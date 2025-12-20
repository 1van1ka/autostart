#ifndef CONFIG_H
#define CONFIG_H

#include <limits.h>

#define MAX_CFG_APPS 128
#define MAX_CFG_DIRS 32

struct AppRule {
  char name[256];
  int allow;
  int delay_ms; // -1 если нет
};

struct DirRule {
  char path[PATH_MAX];
  int allow;
};

struct Config {
  int startup_delay_ms;
  int delay_ms;

  int log_level;
  char log_file[PATH_MAX];

  struct AppRule apps[MAX_CFG_APPS];
  int app_count;

  struct DirRule dirs[MAX_CFG_DIRS];
  int dir_count;
};

/* lifecycle */
void config_init(struct Config *cfg);
int config_load(struct Config *cfg, const char *path);
void print_config(const struct Config *cfg);

/* lookup */
struct AppRule *config_find_app(struct Config *cfg, const char *name);
int config_dir_blocked(struct Config *cfg, const char *path);

#endif
