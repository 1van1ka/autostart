#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static enum log LOG_LEVEL = LOG_INFO;
static FILE *FILE_LOG = NULL;

void log_init(enum log level, char *file) {
  LOG_LEVEL = level;
  FILE_LOG = fopen(file, "a");
  if (!FILE_LOG) {
    log_msg(LOG_ERR, "Error opening file: %s\n", file);
  }
}

void log_end() { fclose(FILE_LOG); }

static void log_file(enum log level, va_list args, const char *fmt) {
  if (!FILE_LOG)
    return;

  char ts[20];
  time_t now = time(NULL);
  struct tm tm;
  localtime_r(&now, &tm);
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

  fprintf(FILE_LOG, "%s ", ts);


  switch (level) {
  case LOG_INFO:
    fprintf(FILE_LOG, "[INFO] ");
    break;
  case LOG_WARN:
    fprintf(FILE_LOG, "[WARN] ");
    break;
  case LOG_ERR:
    fprintf(FILE_LOG, "[ERR ] ");
    break;
  case LOG_DEF:
    break;
  }

  vfprintf(FILE_LOG, fmt, args);
  fflush(FILE_LOG);

  va_end(args);
}

void log_msg(enum log level, const char *fmt, ...) {
  if (level < LOG_LEVEL)
    return;

  va_list args;
  va_start(args, fmt);

  // for using to output in file_log
  va_list args_copy;
  va_copy(args_copy, args);

  FILE *out = (level == LOG_ERR) ? stderr : stdout;

  // stdout
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

  log_file(level, args_copy, fmt);
}
