#pragma once
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
void fatal(const char *format, ...);
void pclog_ex(const char *format, va_list args);
#ifdef __cplusplus
}
#endif
