#include <SDL2/SDL_log.h>


void log_init(void)
{
	SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION,
			   SDL_LOG_PRIORITY_DEBUG);
}

void log_critical(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION,
			SDL_LOG_PRIORITY_CRITICAL, fmt, args);
	va_end(args);
}

void log_error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION,
			SDL_LOG_PRIORITY_ERROR, fmt, args);
	va_end(args);
}

void log_debug(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION,
			SDL_LOG_PRIORITY_DEBUG, fmt, args);
	va_end(args);
}
