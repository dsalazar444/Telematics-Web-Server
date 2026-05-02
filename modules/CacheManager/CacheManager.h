#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <openssl/md5.h>
#include "uthash.h"
#include <sys/stat.h> 
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>

#include "../../Includes/http.h"
#include "../HttpParser/HttpResponseParser.h"
#include "../../Includes/messages.h"
typedef struct {
    int statusCode;
    char contentType[128];
    size_t contentLength;
    time_t timestamp;
    size_t ttl;
    HTTPHeaders extraHeaders;
} CacheMeta;

typedef struct
{
    char key[512];
    time_t timestamp;
    UT_hash_handle hh;  
} CacheEntry;

typedef struct
{
    char *cacheDir;
    uint16_t ttl;
    CacheEntry *table;
    uint16_t entryCount;
    pthread_mutex_t lock;
} CacheManager;

typedef struct {
    CacheManager *cacheManager;
    char cacheKey[33];
    char rawKey[512];
    HTTPResponse response;
} CacheStoreArgs;


bool cacheKeyFromRequest(const HTTPRequest *request, char *outKey, size_t outKeyLen);
CacheManager *CacheManagerCreate(const char *cacheDir, uint16_t ttl);
bool CacheStore(CacheManager *cache, const char *cacheKey, const char *rawKey, const HTTPResponse *response);
bool CacheLookUp(CacheManager *cache, const char *cacheKey, HTTPResponse *response);
void CacheStoreAsync(CacheManager *cacheManager, ProxyMessage *proxyMessage);
void CacheInvalidateByRequest(CacheManager *cache, const HTTPRequest *request);
void FreeCacheManager(CacheManager *cacheManager);


#endif // CACHE_H