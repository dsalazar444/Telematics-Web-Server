#ifndef CACHE_INDEX_H
#define CACHE_INDEX_H

#include "CacheManager.h"

bool CacheAddFromDisk(CacheManager *cache, const char *cacheKey, time_t timestamp);
bool ExpireFileCache(CacheManager *cache, CacheEntry *entry, const char *cacheKey);
void cacheLoadFromDisk(CacheManager *cache);
CacheEntry *CacheFindEntry(CacheManager *cache, const char *key);

#endif