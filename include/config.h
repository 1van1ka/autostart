#ifndef CONFIG_H
#define CONFIG_H

#include <limits.h>
#include "log.h"

#define MAX_CFG_APPS 128

struct AppRule {
  char entry_id[256];
  int allow;
  int delay_ms;
};

struct DirRule {
  char path[PATH_MAX];
  int allow;
};

struct Config {
  int use_config;

  int startup_delay_ms;
  int delay_ms;

  enum log log_level;

  struct AppRule apps[MAX_CFG_APPS];
  int app_count;
};

/* lifecycle */
void config_init(struct Config *cfg);
int config_load(struct Config *cfg, const char *path);
void print_config(const struct Config *cfg);

/* lookup */
struct AppRule *config_find_app(struct Config *cfg, const char *name);

#endif
