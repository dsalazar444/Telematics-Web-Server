#ifndef CACHE_UTILS_H
#define CACHE_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <openssl/md5.h>

#include "CacheManager.h"

void MD5Hash(const char *input, char *output);
void BuildPath(const CacheManager *cache, const char *cacheKey, const char *ext, char *out, size_t outLen);
const char *findHeader(const HTTPHeaders *headers, const char *key);
const char *MethodToString(HTTPMethod method);
bool writeFile(const char *path, const void *data, size_t size, const char *mode);
bool parseMetaFile(const char *metaPath, char *rawKey, size_t keySize, time_t *timestamp);
bool cacheBodyExists(CacheManager *cache, const char *cacheKey);
bool CacheReadMeta(CacheManager *cache, const char *cacheKey, CacheMeta *meta);
unsigned char *CacheReadBody(CacheManager *cache,const char *cacheKey,size_t contentLength);

#endif // CACHE_UTILS_H
