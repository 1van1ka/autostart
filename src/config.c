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
  cfg->use_config = 1;
  cfg->delay_ms = 200;
}

/**
 * Loads configuration from a file.
 * Supports sections: [general], [apps], [dirs].
 * @param cfg Pointer to configuration structure to fill.
 * @param path Path to configuration file.
 * @return 1 on success, 0 on failure to open file.
 */
int config_load(struct Config *cfg, const char *path) {
  FILE *f = fopen(path, "r");
  if (!f)
    return 0;

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

    // section general
    if (!strcmp(section, "general")) {
      if (!strcmp(k, "startup_delay"))
        cfg->startup_delay_ms = atoi(v);

      else if (!strcmp(k, "delay"))
        cfg->delay_ms = atoi(v);

      else if (!strcmp(k, "log_level")) {
        cfg->log_level = atoi(v);
        if (cfg->log_level < LOG_INFO)
          cfg->log_level = LOG_INFO;
        else if (cfg->log_level > LOG_DEF)
          cfg->log_level = LOG_DEF;
      }

      // section apps
    } else if (!strcmp(section, "apps") && cfg->app_count < MAX_CFG_APPS) {
      struct AppRule *app_rule = &cfg->apps[cfg->app_count++];

      strncpy(app_rule->name, k, sizeof(app_rule->name) - 1);
      app_rule->name[sizeof(app_rule->name) - 1] = '\0';

      app_rule->allow = 1;    // default policy
      app_rule->delay_ms = 0; // default delay

      char *token = strtok(v, ",");
      while (token) {
        char *t = trim(token);

        char *colon = strchr(t, ':');
        if (!colon) {
          token = strtok(NULL, ",");
          continue;
        }

        *colon = '\0';

        char *key = trim(t);
        char *val = trim(colon + 1);

        if (strcmp(key, "allow") == 0) {
          app_rule->allow = atoi(val);
        } else if (strcmp(key, "delay") == 0) {
          app_rule->delay_ms = atoi(val);
        }

        token = strtok(NULL, ",");
      }
    }
  }

  fclose(f);
  return 1;
}

/**
 * Prints the current configuration to stdout.
 * @param cfg Pointer to configuration structure.
 */
void print_config(const struct Config *cfg) {
  log_msg(LOG_INFO, "=== Current Config =====================\n");
  log_msg(LOG_INFO, "Startup delay: %d ms\n", cfg->startup_delay_ms);
  log_msg(LOG_INFO, "Delay between apps: %d ms\n", cfg->delay_ms);
  log_msg(LOG_INFO, "Log level: %d\n", cfg->log_level);
  log_msg(LOG_INFO, "Log file: %s\n", cfg->log_file);

  log_msg(LOG_INFO, "Applications rules (%d):\n", cfg->app_count);
  for (int i = 0; i < cfg->app_count; i++) {
    const struct AppRule *app = &cfg->apps[i];
    log_msg(LOG_INFO, "\t- %s: %s with delay: %d\n", app->name,
            app->allow ? "ALLOW" : "BLOCK", app->delay_ms);
  }
  log_msg(LOG_INFO, "========================================\n\n");
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
