/* 
**
** logger.c - logger
**
** Copyright (c) 2013  Ery Lee <ery.lee at gmail dot com>
** All rights reserved.
**
*/

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "logger.h"

int log_level;

char *log_file;

static void _log(LogLevel level, const char *category, const char *format, va_list ap);

void logger_debug(const char *category, const char *format, ...) {
    va_list args;
    va_start(args, format);
    _log(LOG_DEBUG, category, format, args);
    va_end(args);
}

void logger_info(const char *category, const char *format, ...) {
    va_list args;
    va_start(args, format);
    _log(LOG_INFO, category, format, args);
    va_end(args);
}

void logger_warning(const char *category, const char *format, ...) {
    va_list args;
    va_start(args, format);
    _log(LOG_WARNING, category, format, args);
    va_end(args);
}

void logger_error(const char *category, const char *format, ...) {
    va_list args;
    va_start(args, format);
    _log(LOG_ERROR, category, format, args);
    va_end(args);
}

void logger_fatal(const char *category, const char *format, ...) {
    va_list args;
    va_start(args, format);
    _log(LOG_FATAL, category, format, args);
    va_end(args);
}

void _log(LogLevel level, const char *category, const char *format, va_list ap) {
    const char *levels[] = {"DEBUG", "INFO", "WARNING", "ERROR", "FATAL"};
    time_t now = time(NULL);
    char buf[64];
    FILE *fp;

    if (level < log_level) return;

    fp = (log_file == NULL) ? stdout : fopen(log_file,"a");
    if (!fp) return;

    strftime(buf,sizeof(buf),"%d %b %H:%M:%S", localtime(&now)); 
    fprintf(fp,"[%s] %s %s ", levels[level], category, buf);
    vfprintf(fp, format, ap);
    fprintf(fp,"\n");
    fflush(fp);

    if (log_file) fclose(fp);
}

