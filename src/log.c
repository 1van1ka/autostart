#include "log.h"
#include <stdarg.h>
#include <stdio.h>

static enum log LOG_LEVEL = LOG_INFO;

void log_init(enum log level) { LOG_LEVEL = level; }

void log_msg(enum log level, const char *fmt, ...) {
  if (level < LOG_LEVEL)
    return;
  va_list args;
  va_start(args, fmt);

  FILE *out = (level == LOG_ERR) ? stderr : stdout;

  switch (level) {
  case LOG_INFO:
    fprintf(out, "\033[32m[INFO]\033[0m ");
    break;
  case LOG_WARN:
    fprintf(out, "\033[33m[WARN]\033[0m ");
    break;
  case LOG_ERR:
    fprintf(out, "\033[31m[ERR ]\033[0m ");
    break;
  case LOG_DEF:
    break;
  }

  vfprintf(out, fmt, args);
  va_end(args);
}
