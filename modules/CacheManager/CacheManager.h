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
#include "../../Includes/http.h"

// TODO: cache_lookup(key)Busca en disco si existe entrada válida
// TODO: cache_store(key, response)Guarda body + meta en disco
// TODO: cache_invalidate(key) Elimina la entrada de disco
// TODO: cache_is_valid(meta)Verifica si el TTL no ha expirado
// TODO: cache_key_from_request(req)Genera la clave a partir del HTTP request
// TODO: cache_init(config)Inicializa la caché con la configuración dada (TTL, directorio, etc.)
// TODO: cache_load

typedef struct
{
    time_t timestamp;
    int ttl;
} CacheMetadata;

typedef struct
{
    char key[512];
    time_t timestamp;
    UT_hash_handle hh;  
} CacheEntry;

typedef struct
{
    char cacheDir[256];
    uint16_t ttl;
    CacheEntry *table;
    uint16_t entryCount;
    pthread_mutex_t lock;
} Cache;

Cache *CacheManagerCreate(const char *cacheDir, uint16_t ttl);
bool cacheKeyFromRequest(const HTTPRequest *request, char *outKey, size_t outKeyLen);
const char* MethodToString(const char *method);
void MD5Hash(const char *input, char *output);

#endif // CACHE_H