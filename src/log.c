#include "log.h"
#include <stdarg.h>
#include <stdio.h>

void log_msg(enum log level, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  switch (level) {
  case LOG_INFO:
    printf("\033[32m[INFO] ");
    break;
  case LOG_WARN:
    printf("\033[33m[WARN] ");
    break;
  case LOG_ERR:
    fprintf(stderr, "\033[31m[ERR ] ");
    break;
  case LOG_DEF:
    break;
  }

  printf("\033[0m");
  vprintf(fmt, args);

  va_end(args);
}
