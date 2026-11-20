#pragma once
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
FILE *rom_fopen(const char *name, char *mode);
#ifdef __cplusplus
}
#endif
