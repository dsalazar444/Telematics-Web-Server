#include "CacheIndex.h"
#include "CacheUtils.h"
#define CACHE_MAX_ENTRIES 1000

bool CacheAddFromDisk(CacheManager *cache, const char *cacheKey, time_t timestamp)
{
    if (cache == NULL || cacheKey == NULL || cacheKey[0] == '\0' || timestamp <= 0)
        return false;

    if (cache->entryCount >= CACHE_MAX_ENTRIES)
        return false;

    if (strlen(cacheKey) != 32)
        return false;

    CacheEntry *existing = NULL;
    HASH_FIND_STR(cache->table, cacheKey, existing);
    if (existing != NULL)
        return false;

    CacheEntry *entry = malloc(sizeof(CacheEntry));
    if (!entry)
        return false;

    memset(entry, 0, sizeof(*entry));

    strncpy(entry->key, cacheKey, sizeof(entry->key) - 1);
    entry->key[sizeof(entry->key) - 1] = '\0';
    entry->timestamp = timestamp;

    HASH_ADD_STR(cache->table, key, entry);
    cache->entryCount++;

    return true;
}

CacheEntry *CacheFindEntry(CacheManager *cache, const char *key)
{
    CacheEntry *entry = NULL;
    HASH_FIND_STR(cache->table, key, entry);
    return entry;
}

bool ExpireFileCache(CacheManager *cache, CacheEntry *entry, const char *cacheKey)
{
    if (difftime(time(NULL), entry->timestamp) > cache->ttl)
    {
        HASH_DEL(cache->table, entry);
        free(entry);
        cache->entryCount--;

        char cachePath[512];
        BuildPath(cache, cacheKey, "", cachePath, sizeof(cachePath));
        remove(cachePath);

        return false;
    }

    return true;
}

void cacheLoadFromDisk(CacheManager *cache)
{
    DIR *dir = opendir(cache->cacheDir);
    if (!dir)
    {
        fprintf(stderr, "No se pudo abrir %s: %s\n",
                cache->cacheDir, strerror(errno));
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char cachePath[512];
        BuildPath(cache, entry->d_name, "", cachePath, sizeof(cachePath));

        char storedRawKey[512] = {0};
        time_t timestamp = 0;

        if (!parseMetaFile(cachePath, storedRawKey, sizeof(storedRawKey), &timestamp))
            continue;

        char expectedCacheKey[33];
        MD5Hash(storedRawKey, expectedCacheKey);

        if (strcmp(expectedCacheKey, entry->d_name) != 0)
        {
            fprintf(stderr, "cacheLoadFromDisk: clave inconsistente en %s\n", entry->d_name);
            continue;
        }

        if (!cacheBodyExists(cache, entry->d_name))
            continue;

        if (!CacheAddFromDisk(cache, entry->d_name, timestamp))
        {
            fprintf(stderr, "cacheLoadFromDisk: no se pudo agregar %s a RAM\n", entry->d_name);
            continue;
        }
    }

    closedir(dir);
}