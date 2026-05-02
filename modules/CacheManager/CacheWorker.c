#include "CacheWorker.h"

void *CacheCleanupWorker(void *arg)
{
    CacheManager *cache = (CacheManager *)arg;

    while (1)
    {
        sleep(cache->ttl); // Espera un TTL completo

        pthread_mutex_lock(&cache->lock);

        CacheEntry *entry, *tmp;
        HASH_ITER(hh, cache->table, entry, tmp)
        {
            if (difftime(time(NULL), entry->timestamp) > cache->ttl)
            {
                // Limpiar disco
                char bodyPath[512], metaPath[512];
                BuildPath(cache, entry->key, "body", bodyPath, sizeof(bodyPath));
                BuildPath(cache, entry->key, "meta", metaPath, sizeof(metaPath));
                remove(bodyPath);
                remove(metaPath);

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

