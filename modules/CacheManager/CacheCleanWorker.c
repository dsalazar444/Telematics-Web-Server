#include "CacheCleanWorker.h"

void *CacheCleanupWorker(void *arg)
{
    CacheManager *cache = (CacheManager *)arg;
    while (1)
    {
        sleep(cache->ttl);
        printf("CacheCleanupWorker: iniciando limpieza de caché...\n");
        pthread_mutex_lock(&cache->lock);

        CacheEntry *entry, *tmp;
        HASH_ITER(hh, cache->table, entry, tmp)
        {
            if (difftime(time(NULL), entry->timestamp) > cache->ttl)
            {
                // Limpiar disco — sin extensión
                char cachePath[512];
                BuildPath(cache, entry->key, "", cachePath, sizeof(cachePath));
                remove(cachePath);

                // Limpiar RAM
                HASH_DEL(cache->table, entry);
                free(entry);
                cache->entryCount--;
            }
        }

        pthread_mutex_unlock(&cache->lock);
    }
    return NULL;
}