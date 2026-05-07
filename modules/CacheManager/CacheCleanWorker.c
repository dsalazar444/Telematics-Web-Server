#include "CacheCleanWorker.h"

// Worker que se encarga de limpiar la caché periódicamente
// Pide: arg - puntero a CacheManager
// Retorna: NULL
void *CacheCleanupWorker(void *arg)
{
    CacheManager *cache = (CacheManager *)arg;
    while (1)
    {
        // Espera el TTL antes de iniciar la limpieza
        sleep(cache->ttl);
        printf("CacheCleanupWorker: iniciando limpieza de caché...\n");
        pthread_mutex_lock(&cache->lock);

        // Itera en cada registro de la tabla hash verificando que no haya expirado.
        // Si expiró, borra el archivo del disco y elimina la entrada de la tabla hash
        CacheEntry *entry, *tmp;
        HASH_ITER(hh, cache->table, entry, tmp)
        {
            if (difftime(time(NULL), entry->timestamp) > cache->ttl)
            {
                // Limpiar disco 
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