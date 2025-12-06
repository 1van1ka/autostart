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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <glob.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_LINE 1024
#define MAX_PATH 2048
#define MAX_APPS 100
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

struct ThreadArg {
  struct DesktopEntry entry;
  int thread_num;
  int total_threads;
};

struct AppQueue {
  struct DesktopEntry apps[MAX_APPS];
  int count;
  pthread_mutex_t lock;
};

struct AppQueue app_queue = {.count = 0};

static volatile sig_atomic_t child_exited = 0;

void handle_child() {
  child_exited = 1;
  // Reap child processes to avoid zombies
  while (waitpid(-1, NULL, WNOHANG) > 0) {
  }
}

/**
 * Removes leading and trailing whitespace from a string
 * @param str String to trim (modified in place)
 * @return Pointer to the trimmed string
 */
char *trim(char *str) {
  char *end;

  // Trim leading whitespace
  while (isspace((unsigned char)*str))
    str++;

  // All spaces?
  if (*str == 0)
    return str;

  // Trim trailing whitespace
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;

  // Write new null terminator
  *(end + 1) = 0;
  return str;
}

/**
 * Removes desktop entry specifiers (%u, %f, etc.) from command string
 * @param cmd Command string to clean (modified in place)
 */
void remove_desktop_specifiers(char *cmd) {
  char *src = cmd;
  char *dst = cmd;

  while (*src) {
    if (*src == '%') {
      // Skip the % and the next character
      src++;
      if (*src) {
        // Skip the specifier character (u, f, F, i, etc.)
        src++;
      }
      continue;
    }
    *dst++ = *src++;
  }
  *dst = '\0';
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
void run_command(const char *exec_cmd, const char *work_dir) {
  char cmd[MAX_PATH];
  strncpy(cmd, exec_cmd, sizeof(cmd) - 1);
  cmd[sizeof(cmd) - 1] = '\0';

  // Remove desktop file specifiers
  remove_desktop_specifiers(cmd);

  // Use wordexp for proper expansion
  glob_t p = {0};
#ifdef GLOB_TILDE
  if (glob(cmd, GLOB_NOCHECK | GLOB_TILDE, NULL, &p) != 0) {
#else
  if (glob(cmd, GLOB_NOCHECK, NULL, &p) != 0) {
#endif
    fprintf(stderr, "  Failed to parse command: %s\n", cmd);
    return;
  }

  if (p.gl_pathc == 0) {
    globfree(&p);
    fprintf(stderr, "  Empty command after parsing\n");
    return;
  }

  pid_t pid = fork();

  if (pid == 0) {
    // Start new session to detach from terminal
    setsid();

    if (work_dir && strlen(work_dir) > 0) {
      if (chdir(work_dir) != 0) {
        fprintf(stderr, "  Failed to change directory to %s\n", work_dir);
      }
    }

    // Close standard file descriptors to detach from terminal
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Execute the command
    execvp(p.gl_pathv[0], p.gl_pathv);

    // If execvp returns, there was an error
    // We can't use fprintf here because we closed stderr
    exit(EXIT_FAILURE);
  } else if (pid > 0) {
    // Parent process - don't wait for child
  } else {
    fprintf(stderr, "  Fork failed: %s\n", strerror(errno));
  }
  globfree(&p);
}

/**
 * Thread function to launch a single application with delay
 * @param arg ThreadArg structure containing application details
 * @return NULL
 */
void *launch_app_thread(void *arg) {
  struct ThreadArg *thread_arg = (struct ThreadArg *)arg;
  struct DesktopEntry *entry = &thread_arg->entry;

  // Calculate delay based on thread number
  int delay_ms = thread_arg->thread_num * DELAY_MS;

  // Sleep for the calculated delay
  struct timespec ts = {.tv_sec = delay_ms / 1000,
                        .tv_nsec = (delay_ms % 1000) * 1000000L};
  nanosleep(&ts, NULL);

  printf("Thread %d: Launching: %s\n", thread_arg->thread_num, entry->name);
  run_command(entry->exec, entry->path);

  free(thread_arg);
  return NULL;
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

      // Check if TryExec exists
      if (!check_tryexec(de.tryexec)) {
        printf("  Skipped (TryExec not found): %s\n", de.name);
        continue;
      }

      // Add to queue if there's space
      pthread_mutex_lock(&app_queue.lock);
      if (app_queue.count < MAX_APPS) {
        app_queue.apps[app_queue.count] = de;
        app_queue.count++;
        queued++;
        printf("  Queued: %s\n", de.name);
      } else {
        fprintf(stderr, "  Queue full, cannot queue: %s\n", de.name);
      }
      pthread_mutex_unlock(&app_queue.lock);
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
  if (app_queue.count == 0) {
    printf("\nNo applications to launch.\n");
    return;
  }

  printf("\n========================================\n");
  printf("Launching %d applications with %dms delay\n", app_queue.count,
         DELAY_MS);
  printf("========================================\n");

  pthread_t threads[app_queue.count];
  int threads_created = 0;

  // Create a thread for each application
  for (int i = 0; i < app_queue.count; i++) {
    struct ThreadArg *arg = malloc(sizeof(struct ThreadArg));
    if (!arg) {
      fprintf(stderr, "Failed to allocate memory for thread argument\n");
      continue;
    }

    arg->entry = app_queue.apps[i];
    arg->thread_num = i;
    arg->total_threads = app_queue.count;

    if (pthread_create(&threads[i], NULL, launch_app_thread, arg) != 0) {
      fprintf(stderr, "Failed to create thread for: %s\n", arg->entry.name);
      free(arg);
      continue;
    }

    threads_created++;

    // Small delay between thread creation to avoid overwhelming the system
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000L}; // 10ms
    nanosleep(&ts, NULL);
  }

  // Give threads time to start their delays
  sleep(1);

  // Calculate maximum expected time
  printf("All launch threads initiated\n");

  // We don't join threads because they run independently
  // but we need to ensure they complete
  for (int i = 0; i < threads_created; i++) {
    pthread_detach(threads[i]);
  }
}

int main() {
  if (pthread_mutex_init(&app_queue.lock, NULL) != 0) {
    fprintf(stderr, "Mutex initialization failed\n");
    return 1;
  }

  // Get home directory
  const char *home = getenv("HOME");
  if (!home) {
    struct passwd *pw = getpwuid(getuid());
    home = pw->pw_dir;
  }

  // Define autostart directories to scan
  char autostart_dirs[3][MAX_PATH];
  int dir_count = 0;

  // User-specific autostart directory (highest priority)
  snprintf(autostart_dirs[dir_count++], MAX_PATH, "%s/.config/autostart", home);

  // System-wide autostart directory
  snprintf(autostart_dirs[dir_count++], MAX_PATH, "/etc/xdg/autostart");

  // Alternative location (less common)
  snprintf(autostart_dirs[dir_count++], MAX_PATH, "/usr/share/autostart");

  printf("Autostart Launcher\n");
  printf("=============================================\n");
  printf("Configuration:\n");
  printf("  Delay between application starts: %dms\n", DELAY_MS);
  printf("  Maximum applications: %d\n", MAX_APPS);
  printf("\nScanning directories:\n");
  for (int i = 0; i < dir_count; i++) {
    printf("  %d. %s\n", i + 1, autostart_dirs[i]);
  }
  printf("\n");

  // Scan directories and queue applications
  int total_queued = 0;
  for (int i = 0; i < dir_count; i++) {
    total_queued += scan_autostart_dir(autostart_dirs[i], i);
  }

  // Launch queued applications with staggered delays
  launch_queued_apps();

  // Cleanup
  pthread_mutex_destroy(&app_queue.lock);

  printf("\n========================================\n");
  printf("All autostart applications processed.\n");
  printf("========================================\n");

  return 0;
}
