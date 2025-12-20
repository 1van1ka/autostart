#include "config.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 1024

/**
 * Initializes the configuration structure with default values.
 * @param cfg Pointer to the configuration structure to initialize.
 */
void config_init(struct Config *cfg) {
  memset(cfg, 0, sizeof(*cfg));
  cfg->delay_ms = 200;
}

/**
 * Loads configuration from a file.
 * Supports sections: [general], [apps], [dirs].
 * @param cfg Pointer to configuration structure to fill.
 * @param path Path to configuration file.
 * @return 0 on success, -1 on failure to open file.
 */
int config_load(struct Config *cfg, const char *path) {
  FILE *f = fopen(path, "r");
  if (!f)
    return -1;

  char line[MAX_LINE];
  char section[64] = "";

  while (fgets(line, sizeof(line), f)) {
    char *s = trim(line);
    if (*s == 0 || *s == '#')
      continue;

    if (*s == '[') {
      sscanf(s, "[%63[^]]", section);
      continue;
    }

    char key[256], val[256];
    if (sscanf(s, "%255[^=]=%255[^\n]", key, val) != 2)
      continue;

    char *k = trim(key);
    char *v = trim(val);

    if (!strcmp(section, "general")) {
      if (!strcmp(k, "startup_delay"))
        cfg->startup_delay_ms = atoi(v);
      else if (!strcmp(k, "delay"))
        cfg->delay_ms = atoi(v);
    } else if (!strcmp(section, "apps") && cfg->app_count < MAX_CFG_APPS) {
      struct AppRule *app_rule = &cfg->apps[cfg->app_count++];
      strncpy(app_rule->name, k, sizeof(app_rule->name) - 1);
      app_rule->name[sizeof(app_rule->name) - 1] = '\0';
      app_rule->allow = 1; // default policy
      app_rule->delay_ms = -1;     // default delay

      char *token = strtok(v, ",");
      while (token) {
        char *t = trim(token);

        if (!strncmp(t, "allow:", 6)) {
            app_rule->allow = atoi(t + 6);
        } else if (!strncmp(t, "delay:", 6)) {
          app_rule->delay_ms = atoi(t + 6);
        }

        token = strtok(NULL, ",");
      }
    } else if (!strcmp(section, "dirs") && cfg->dir_count < MAX_CFG_DIRS) {
      struct DirRule *dir_rule = &cfg->dirs[cfg->dir_count++];
      strncpy(dir_rule->path, k, sizeof(dir_rule->path) - 1);
      dir_rule->allow = !strcmp(v, "block");
    }
  }

  fclose(f);
  return 0;
}

/**
 * Prints the current configuration to stdout.
 * @param cfg Pointer to configuration structure.
 */
void print_config(const struct Config *cfg) {
  printf("=== Current Config =====================\n");
  printf("Startup delay: %d ms\n", cfg->startup_delay_ms);
  printf("Delay between apps: %d ms\n", cfg->delay_ms);
  printf("Log level: %d\n", cfg->log_level);
  printf("Log file: %s\n", cfg->log_file);

  printf("\nApplications rules (%d):\n", cfg->app_count);
  for (int i = 0; i < cfg->app_count; i++) {
    struct AppRule *app = &cfg->apps[i];
    printf("  - %s: %s", app->name,
           app->allow ? "ALLOW" : "BLOCK");
    if (app->delay_ms >= 0) {
      printf(", delay: %d ms", app->delay_ms);
    }
    printf("\n");
  }

  printf("\nDirectory rules (%d):\n", cfg->dir_count);
  for (int i = 0; i < cfg->dir_count; i++) {
    struct DirRule *dir = &cfg->dirs[i];
    printf("  - %s: %s\n", dir->path, dir->allow ? "ALLOW" : "BLOCK");
  }

  printf("========================================\n");
}

/**
 * Finds an application rule by name.
 * @param cfg Pointer to configuration structure.
 * @param name Name of the application to find.
 * @return Pointer to AppRule if found, NULL otherwise.
 */
struct AppRule *config_find_app(struct Config *cfg, const char *name) {
  for (int i = 0; i < cfg->app_count; i++)
    if (!strcmp(cfg->apps[i].name, name))
      return &cfg->apps[i];
  return NULL;
}

/**
 * Checks if a directory is blocked.
 * @param cfg Pointer to configuration structure.
 * @param path Directory path to check.
 * @return 1 if blocked, 0 otherwise.
 */
int config_dir_allowed(struct Config *cfg, const char *path) {
  for (int i = 0; i < cfg->dir_count; i++)
    if (!strcmp(cfg->dirs[i].path, path))
      return cfg->dirs[i].allow;
  return 0;
}
