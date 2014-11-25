#ifndef _COMMON_H
#define _COMMON_H

#include "unp.h"
#include <stdarg.h>

static void printdebuginfo(const char *format, ...)
{
#ifdef NDEBUGINFO
    return;
#endif
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}
