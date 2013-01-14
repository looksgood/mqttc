#ifndef __LOGGER_H
#define __LOGGER_H

#include <stdarg.h>

/**
 * Debug levels.
 */
typedef enum {
    LOG_DEBUG = 0,  /**< Debug levels.              */
    LOG_INFO,     /**< General operation Information. */
    LOG_WARNING,  /**< Warnings.                      */
    LOG_ERROR,    /**< Errors.                        */
    LOG_FATAL /**< Fatal errors.                  */
} LogLevel;

void logger_debug(const char *category, const char *format, ...); 

void logger_info(const char *category, const char *format, ...); 

void logger_warning(const char *category, const char *format, ...); 

void logger_error(const char *category, const char *format, ...); 

void logger_fatal(const char *category, const char *format, ...); 

#endif
