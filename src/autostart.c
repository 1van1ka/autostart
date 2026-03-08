/**
 * autostart-launcher.c
 *
 * A simple C program that scans and launches applications
 * from XDG autostart directories (.desktop files)
 *
 * Features:
 * - Parses .desktop files
 * - Checks if executables exist via TryExec
 * - Filters hidden/no-display applications
 * - Launches applications in background
 * - Supports both user (~/.config/autostart) and system (/etc/xdg/autostart)
 */

#include "config.h"
#include "log.h"
#include "util.h"
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_LINE 1024
#define MAX_PATH 2048

struct DesktopEntry {
  char name[256];
  char exec[1024];
  char tryexec[256];
  char icon[256];
  char path[1024];
  int terminal;
  int hidden;
  int nodisplay;
  int valid;
};

struct Array {
  char **values;
  size_t count;
  size_t capacity;
};

struct AppQueue {
  struct DesktopEntry *apps;
  size_t count;
  size_t capacity;
};

static struct AppQueue app_queue;
static struct Config cfg;
static struct Array autostart_dirs;

/*
 * Initialier array of autostart directories
 * @param a dynamic array of autostart dirs
 * @return None
 */
void app_queue_init(struct AppQueue *a) {
  int size = 5;

  a->apps = malloc(size * sizeof(struct DesktopEntry));
  if (!a->apps) {
    perror("malloc");
    exit(1);
  }
  a->count = 0;
  a->capacity = size;
}

/*
 * Initialier array of autostart directories
 * @param a dynamic array of autostart dirs
 * @param path directory to copy in array
 * @return None
 */
void app_queue_add(struct AppQueue *a, struct DesktopEntry entry) {
  if (a->count == a->capacity) {
    a->capacity *= 2;
    struct DesktopEntry *tmp =
        realloc(a->apps, a->capacity * sizeof(struct DesktopEntry));
    if (!tmp) {
      perror("realloc");
      exit(1);
    }
    a->apps = tmp;
  }

  a->apps[a->count++] = entry;
}

/*
 * Cleaner autostart Array
 * @param None
 * @return None
 */
void cleanup_autostart_dirs() {
  for (size_t i = 0; i < autostart_dirs.count; i++)
    free(autostart_dirs.values[i]);
  free(autostart_dirs.values);
}

void cleanup_app_queue() { free(app_queue.apps); }

/*
 * Cleaner all dynamic memory allocated
 * @param None
 * @return None
 */
void cleanup() {
  cleanup_autostart_dirs();
  cleanup_app_queue();
}

/**
 * Parses a .desktop file into a DesktopEntry struct
 * @param filename Path to the .desktop file
 * @param entry Pointer to DesktopEntry struct to populate
 * @return 1 on success, 0 on failure or if not an application
 */
int parse_desktop_file(const char *filename, struct DesktopEntry *entry) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    log_msg(LOG_ERR, "Error opening file: %s\n", filename);
    return 0;
  }

  // Initialize the struct
  memset(entry, 0, sizeof(struct DesktopEntry));
  entry->valid = 0;

  char line[MAX_LINE];
  bool in_desktop_entry = false;
  bool type_is_application = false;

  while (fgets(line, MAX_LINE, file)) {
    char *trimmed = trim(line);

    // Skip comments and empty lines
    if (trimmed[0] == '#' || trimmed[0] == 0)
      continue;

    // Check for [Desktop Entry] section
    if (trimmed[0] == '[') {
      in_desktop_entry = (strstr(trimmed, "[Desktop Entry]") != NULL);
      continue;
    }

    if (!in_desktop_entry)
      continue;

    // Split key and value
    char *sep = strchr(trimmed, '=');
    if (!sep)
      continue;

    *sep = '\0';
    char *key = trim(trimmed);
    char *value = trim(sep + 1);

    // Parse key-value pairs
    if (strcmp(key, "Type") == 0) {
      if (strcmp(value, "Application") != 0) {
        fclose(file);
        return 0; // Not an application, skip
      }
      type_is_application = true;
    } else if (strcmp(key, "Name") == 0) {
      strncpy(entry->name, value, sizeof(entry->name) - 1);
    } else if (strcmp(key, "Exec") == 0) {
      strncpy(entry->exec, value, sizeof(entry->exec) - 1);
    } else if (strcmp(key, "TryExec") == 0) {
      strncpy(entry->tryexec, value, sizeof(entry->tryexec) - 1);
    } else if (strcmp(key, "Path") == 0) {
      strncpy(entry->path, value, sizeof(entry->path) - 1);
    } else if (strcmp(key, "Icon") == 0) {
      strncpy(entry->icon, value, sizeof(entry->icon) - 1);
    } else if (strcmp(key, "Terminal") == 0) {
      entry->terminal = (strcmp(value, "true") == 0);
    } else if (strcmp(key, "Hidden") == 0) {
      entry->hidden = (strcmp(value, "true") == 0);
    } else if (strcmp(key, "NoDisplay") == 0) {
      entry->nodisplay = (strcmp(value, "true") == 0);
    }
  }

  fclose(file);

  // Validate required fields
  if (type_is_application && strlen(entry->name) > 0 &&
      strlen(entry->exec) > 0) {
    entry->valid = 1;
  }

  return entry->valid;
}

/**
 * Checks if a program exists in PATH via TryExec field
 * @param tryexec Program name to check
 * @return 1 if executable exists, 0 otherwise
 */
int check_tryexec(const char *tryexec) {
  if (strlen(tryexec) == 0)
    return 1;

  // Use which command to check existence in PATH
  char command[MAX_PATH];
  snprintf(command, sizeof(command), "command -v %s > /dev/null 2>&1", tryexec);
  return (system(command) == 0);
}

/**
 * Executes a command using fork() and execvp()
 * Uses wordexp() for proper shell expansion and argument parsing
 * @param exec_cmd Command string to execute
 * @param work_dir Working directory for the command (NULL for current)
 */
int run_command(const char *exec_cmd, const char *work_dir) {
  if (!exec_cmd || !*exec_cmd) {
    return 0;
  }

  char cmd[MAX_PATH];
  strncpy(cmd, exec_cmd, sizeof(cmd) - 1);
  cmd[sizeof(cmd) - 1] = '\0';

  // Remove desktop file specifiers
  remove_desktop_specifiers(cmd);

  pid_t pid = fork();

  if (pid == 0) {
    // Ignore signals that could cause coredump (из оригинального кода)
    signal(SIGSEGV, SIG_IGN);
    signal(SIGABRT, SIG_IGN);
    signal(SIGILL, SIG_IGN);

    // Start new session to detach from terminal
    setsid();

    // Change working directory if specified
    if (work_dir && *work_dir) {
      if (chdir(work_dir) != 0) {
        // Error message before closing descriptors
        log_msg(LOG_ERR, "Failed to chdir to %s: %s\n", work_dir,
                strerror(errno));
      }
    }

    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Execute with sh
    execlp("sh", "sh", "-c", cmd, (char *)NULL);
    execlp("bash", "bash", "-c", cmd, (char *)NULL);

    // Exec failed
    _exit(EXIT_FAILURE);
  }

  return (pid > 0);
}

/**
 * Scans an autostart directory and queues valid .desktop applications
 * @param autostart_dir Directory to scan for .desktop files
 * @param dir_index Index of directory for reporting
 * @return Number of applications queued from this directory
 */
int scan_autostart_dir(const char *autostart_dir, int dir_index) {
  DIR *dir = opendir(autostart_dir);

  if (!dir) {
    log_msg(LOG_WARN, "Autostart directory does not exist: %s\n",
            autostart_dir);
    return 0;
  }

  log_msg(LOG_INFO, "[Directory %d] Scanning: %s\n", dir_index + 1,
          autostart_dir);

  struct dirent *entry;
  int total_found = 0;
  int queued = 0;

  while ((entry = readdir(dir)) != NULL) {
    // Only process .desktop files
    const char *ext = strrchr(entry->d_name, '.');
    if (!ext || strcmp(ext, ".desktop") != 0) {
      continue;
    }

    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", autostart_dir,
             entry->d_name);

    struct DesktopEntry de;
    if (parse_desktop_file(full_path, &de)) {
      total_found++;

      // Get app from config
      struct AppRule *CfgApp;
      if ((CfgApp = config_find_app(&cfg, de.name))) {
        if (!CfgApp->allow) {
          log_msg(LOG_INFO, "\tSkipped (disallowed by config): %s\n", de.name);
          continue;
        }
      }

      // Skip hidden or no-display entries
      if (de.hidden || de.nodisplay) {
        log_msg(LOG_INFO, "\tSkipped (hidden/no-display): %s\n", de.name);
        continue;
      }

      // Check if TryExec exists
      if (!check_tryexec(de.tryexec)) {
        log_msg(LOG_INFO, "\tSkipped (TryExec not found): %s\n", de.name);
        continue;
      }

      queued++;

      // Add to queue if there's space
      app_queue_add(&app_queue, de);
      log_msg(LOG_INFO, "\tQueued: %s\n", de.name);
    }
  }

  closedir(dir);

  log_msg(LOG_INFO, "\t--- Summary for %s ---\n", autostart_dir);
  log_msg(LOG_INFO, "\tQueued for launch: %d of %d founded\n\n", queued,
          total_found);

  return queued;
}

/**
 * Launches all queued applications using threads with staggered delays
 */
void launch_queued_apps() {
  int success_count = 0;

  if (app_queue.count == 0) {
    log_msg(LOG_WARN, "No applications to launch.\n");
    return;
  }

  log_msg(LOG_INFO, "Launching %ld apps with %dms delay\n", app_queue.count,
          cfg.delay_ms);

  // Create a thread for each application
  for (size_t i = 0; i < app_queue.count; i++) {
    int delay = i ? cfg.delay_ms : cfg.startup_delay_ms;

    // Sleep for the calculated delay
    struct timespec ts = {.tv_sec = delay / 1000,
                          .tv_nsec = (delay % 1000) * 1000000L};
    nanosleep(&ts, NULL);

    int status = run_command(app_queue.apps[i].exec, app_queue.apps[i].path);

    if (status)
      success_count++;

    log_msg(LOG_INFO, "\t[%ld/%ld] %s %s\n", i + 1, app_queue.count,
            status ? "Access" : "Deny", app_queue.apps[i].name);
  }

  log_msg(LOG_DEF, "Launch completed (%d of %zu)\n", success_count,
          app_queue.count);
}

/*
 * Initialier array of autostart directories
 * @param a dynamic array of autostart dirs
 * @return None
 */
void autostart_dirs_init(struct Array *a) {
  int size = 5;

  a->values = malloc(size * sizeof(char *));
  if (!a->values) {
    perror("malloc");
    exit(1);
  }
  a->count = 0;
  a->capacity = size;
}

/*
 * Initialier array of autostart directories
 * @param a dynamic array of autostart dirs
 * @param path directory to copy in array
 * @return None
 */
void autostart_dirs_add(struct Array *a, const char *path) {
  if (a->count == a->capacity) {
    a->capacity *= 2;
    char **tmp = realloc(a->values, a->capacity * sizeof(char *));
    if (!tmp) {
      perror("realloc");
      exit(1);
    }
    a->values = tmp;
  }

  a->values[a->count] = strdup(path);
  if (!a->values[a->count]) {
    perror("strdup");
    exit(1);
  }
  a->count++;
}

const char *get_config_file(const char *config_file, const char *home) {
  static char path[MAX_PATH];

  if (!cfg.use_config)
    return NULL;

  if (config_file)
    return config_file;

  snprintf(path, MAX_PATH, "%s/.config/autostart.conf", home);
  if (access(path, F_OK) == 0)
    return path;

  snprintf(path, MAX_PATH, "/etc/xdg/autostart.conf");
  if (access(path, F_OK) == 0)
    return path;

  return NULL;
}

void setup(const char *config_file) {
  // Get home directory
  const char *home = getenv("HOME");
  if (!home) {
    struct passwd *pw = getpwuid(getuid());
    home = pw->pw_dir;
  }

  char buf[MAX_PATH];

  if (cfg.use_config)
    if (!config_load(&cfg, get_config_file(config_file, home)))
      log_msg(LOG_WARN, "No configuration file found\n");

  snprintf(buf, MAX_PATH, "%s/.local/state/autostart.log", home);
  log_init(cfg.log_level, buf);

  autostart_dirs_init(&autostart_dirs);
  app_queue_init(&app_queue);

  snprintf(buf, MAX_PATH, "%s/.config/autostart", home);
  autostart_dirs_add(&autostart_dirs, buf);
  autostart_dirs_add(&autostart_dirs, "/etc/xdg/autostart");
}

void run() {
  print_config(&cfg);

  // Scan directories and queue applications
  for (size_t i = 0; i < autostart_dirs.count; i++) {
    scan_autostart_dir(autostart_dirs.values[i], i);
  }

  // Launch queued applications with staggered delays
  launch_queued_apps();
}

/*
 * Printer text of right using
 * @param None
 * @return None
 */
static void usage() {
  printf("usage: autostart <options> [...]\n"
         "options:\n"
         "  {-v  --version}                Show autostart version\n"
         "  {-c  --config}     <file>      Use custom configuration file\n"
         "  {-N  --no-config}              Disable configuration file\n");
}

int main(int argc, char **argv) {
  const char *config_file = NULL;

  config_init(&cfg);

  for (int i = 1; i < argc; i++)
    /* options without arguments */
    if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version"))
      puts("autostart version 0.1"), exit(0);
    else if (!strcmp(argv[i], "-N") || !strcmp(argv[i], "--no-config"))
      cfg.use_config = 0;
    else if (i + 1 == argc) /* option expects an argument but none left */
      usage(), exit(1);
    /* options with argument */
    else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--config"))
      config_file = argv[++i];
    else
      usage(), exit(1);

  setup(config_file);
  run();

  cleanup();

  return 0;
}
