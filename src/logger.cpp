#include "logger.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

void log_info(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fprintf(stdout, "[INFO]: ");
    vfprintf(stdout, fmt, args);
    putc('\n', stdout);

    va_end(args);
}

void log_warn(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fprintf(stdout, "[WARNING]: ");
    vfprintf(stdout, fmt, args);
    putc('\n', stdout);

    va_end(args);

}

void log_err(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fprintf(stdout, "[ERROR]: ");
    vfprintf(stdout, fmt, args);
    putc('\n', stdout);

    va_end(args);

}
void log_fatal(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fprintf(stdout, "[FATAL]: ");
    vfprintf(stdout, fmt, args);
    putc('\n', stdout);

    va_end(args);

    fflush(stdout);
    exit(1);
}
