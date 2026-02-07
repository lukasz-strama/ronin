#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <time.h>

// ANSI color codes
#define ANSI_RESET   "\033[0m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_RED     "\033[31m"

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

void log_output(LogLevel level, const char *file, int line, const char *fmt, ...);

#define LOG_INFO(...)  log_output(LOG_LEVEL_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log_output(LOG_LEVEL_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_output(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#endif
