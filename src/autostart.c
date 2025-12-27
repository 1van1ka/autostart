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
#define DELAY_MS 200

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
    fprintf(stderr, "Error opening file: %s\n", filename);
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
        fprintf(stderr, "Failed to chdir to %s: %s\n", work_dir,
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
    fprintf(stderr, "\nWarning: Autostart directory does not exist: %s\n",
            autostart_dir);
    return 0;
  }

  printf("\n[Directory %d] Scanning: %s\n", dir_index + 1, autostart_dir);

  struct dirent *entry;
  int total_found = 0;
  int queued = 0;

  while ((entry = readdir(dir)) != NULL) {
    // Only process .desktop files
    const char *ext = strrchr(entry->d_name, '.');
    if (!ext || strcmp(ext, ".desktop") != 0) {
      continue;
    }

    total_found++;

    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", autostart_dir,
             entry->d_name);

    struct DesktopEntry de;
    if (parse_desktop_file(full_path, &de) && de.valid) {
      // Skip hidden or no-display entries
      if (de.hidden || de.nodisplay) {
        printf("  Skipped (hidden/no-display): %s\n", de.name);
        continue;
      }

      int allowed = 1;
      for (int i = 0; i < cfg.app_count; i++) {
        if (strcmp(cfg.apps[i].name, de.name) == 0) {
          allowed = cfg.apps[i].allow;
          break;
        }
      }

      if (!allowed) {
        printf("  Skipped (disallowed by config): %s\n", de.name);
        continue;
      }

      // Check if TryExec exists
      if (!check_tryexec(de.tryexec)) {
        printf("  Skipped (TryExec not found): %s\n", de.name);
        continue;
      }

      // Add to queue if there's space
      app_queue_add(&app_queue, de);
      printf("  Queued: %s\n", de.name);
    }
  }

  closedir(dir);

  printf("\n  --- Summary for %s ---\n", autostart_dir);
  printf("  Total .desktop files found: %d\n", total_found);
  printf("  Queued for launch: %d\n", queued);
  printf("  Skipped: %d\n", total_found - queued);

  return queued;
}

/**
 * Launches all queued applications using threads with staggered delays
 */
void launch_queued_apps() {
  int success_count = 0;

  if (app_queue.count == 0) {
    printf("\nNo applications to launch.\n");
    return;
  }

  printf("\n========================================\n");
  printf("Launching %ld applications with %dms delay\n", app_queue.count,
         DELAY_MS);

  // Create a thread for each application
  for (size_t i = 0; i < app_queue.count; i++) {
    int delay = i ? cfg.delay_ms : cfg.startup_delay_ms;
    // Sleep for the calculated delay
    struct timespec ts = {.tv_sec = delay / 1000,
                          .tv_nsec = (delay % 1000) * 1000000L};
    nanosleep(&ts, NULL);

    printf("[%ld/%ld] ", i + 1, app_queue.count);

    if (run_command(app_queue.apps[i].exec, app_queue.apps[i].path)) {
      printf("Access ");
      success_count++;
    } else {
      printf("Deny ");
    }
    printf("launching: %s\n", app_queue.apps[i].name);
  }

  // Calculate maximum expected time
  printf("========================================\n");
  printf("Launch completed\n");
  printf("Total:      %ld\n", app_queue.count);
  printf("Successful: %d\n", success_count);
  printf("Failed:     %ld\n", app_queue.count - success_count);
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

int main(int argc, char **argv) {
  // Get home directory
  const char *home = getenv("HOME");
  if (!home) {
    struct passwd *pw = getpwuid(getuid());
    home = pw->pw_dir;
  }

  config_init(&cfg);

  if (argc > 1)
    config_load(&cfg, argv[1]);

  autostart_dirs_init(&autostart_dirs);
  app_queue_init(&app_queue);

  char buf[MAX_PATH];

  snprintf(buf, MAX_PATH, "%s/.config/autostart", home);
  autostart_dirs_add(&autostart_dirs, buf);
  autostart_dirs_add(&autostart_dirs, "/etc/xdg/autostart");
  autostart_dirs_add(&autostart_dirs, "/usr/share/autostart");

  print_config(&cfg);
  printf("\nScanning directories:\n");
  for (size_t i = 0; i < autostart_dirs.count; i++) {
    printf("  %zu. %s\n", i + 1, autostart_dirs.values[i]);
  }
  printf("\n");

  // Scan directories and queue applications
  for (size_t i = 0; i < autostart_dirs.count; i++) {
    scan_autostart_dir(autostart_dirs.values[i], i);
  }

  // Launch queued applications with staggered delays
  launch_queued_apps();

  cleanup();

  return 0;
}
