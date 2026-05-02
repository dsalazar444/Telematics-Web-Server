#ifndef CACHE_IO_H
#define CACHE_IO_H

#include "CacheManager.h"

bool CacheWriteFile(FILE *file, CacheManager *cache, const char *rawKey, const HTTPResponse *response);

#endif