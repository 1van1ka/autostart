#ifndef LOG_H
#define LOG_H

enum log { LOG_INFO, LOG_WARN, LOG_ERR, LOG_DEF };

static enum log LOG_LEVEL;

void log_init(enum log level);
void log_msg(enum log level, const char *fmt, ...);

#endif
