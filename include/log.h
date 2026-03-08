#ifndef LOG_H
#define LOG_H

enum log { LOG_INFO, LOG_WARN, LOG_ERR, LOG_DEF };

void log_init(enum log level, char *file);
void log_end();
void log_msg(enum log level, const char *fmt, ...);

#endif
