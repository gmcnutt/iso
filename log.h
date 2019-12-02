#ifndef log_h
#define log_h

void log_init(void);
void log_critical(const char *fmt, ...);
void log_error(const char *fmt, ...);
void log_debug(const char *fmt, ...);

#endif
