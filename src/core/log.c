#include "log.h"
#include <stdarg.h>
#include <string.h>

static const char *get_basename(const char *path)
{
    const char *base = strrchr(path, '/');
    return base ? base + 1 : path;
}

void log_output(LogLevel level, const char *file, int line, const char *fmt, ...)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    char time_buf[16];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm);

    const char *level_str;
    const char *color;

    switch (level)
    {
    case LOG_LEVEL_INFO:
        level_str = "INFO";
        color = ANSI_CYAN;
        break;
    case LOG_LEVEL_WARN:
        level_str = "WARN";
        color = ANSI_YELLOW;
        break;
    case LOG_LEVEL_ERROR:
        level_str = "ERROR";
        color = ANSI_RED;
        break;
    default:
        level_str = "????";
        color = ANSI_RESET;
        break;
    }

    fprintf(stderr, "%s[%s][%s][%s:%d]%s ",
            color, level_str, time_buf, get_basename(file), line, ANSI_RESET);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}
