#include "CacheManager.h"
#include "CacheCleanWorker.h"
#include "CacheStoreWorker.h"
#include "CacheIO.h"
#include "CacheIndex.h"

#define CACHE_MAX_ENTRIES 1000

#include "CacheUtils.h"

CacheManager *CacheManagerCreate(const char *cacheDir, uint16_t ttl)
{
    CacheManager *cacheManager = malloc(sizeof(*cacheManager));
    if (cacheManager == NULL)
        return NULL;

    // 2. Copiar configuración
    cacheManager->cacheDir = strdup(cacheDir);
    if (cacheManager->cacheDir == NULL)
    {
        free(cacheManager);
        return NULL;
    }

    cacheManager->ttl = ttl;
    cacheManager->entryCount = 0;

    fprintf(stderr, "CacheManager creado con directorio '%s' y TTL %d segundos\n", cacheDir, ttl);

    // 3. Reservar memoria para el índice en RAM
    cacheManager->table = NULL;
    cacheManager->entryCount = 0;

    // 4. Crear directorio si no existe
    struct stat st = {0};
    if (stat(cacheDir, &st) == -1)
    {
        if (mkdir(cacheDir, 0755) == -1)
        {
            fprintf(stderr, "Error al crear directorio de caché '%s': %s\n", cacheDir, strerror(errno));
            free(cacheManager->cacheDir);
            free(cacheManager->table);
            free(cacheManager);
            return NULL;
        }
    }

    char absoluteDir[512];
    if (realpath(cacheDir, absoluteDir) != NULL)
    {
        char *resolvedDir = strdup(absoluteDir);
        if (resolvedDir == NULL)
        {
            free(cacheManager->cacheDir);
            free(cacheManager->table);
            free(cacheManager);
            return NULL;
        }

        free(cacheManager->cacheDir);
        cacheManager->cacheDir = resolvedDir;
    }

    cacheLoadFromDisk(cacheManager);

    // 5. Inicializar mutex
    if (pthread_mutex_init(&cacheManager->lock, NULL) != 0)
    {
        free(cacheManager->cacheDir);
        free(cacheManager->table);
        free(cacheManager);
        return NULL;
    }

    pthread_t cleanUpThread;
    if (pthread_create(&cleanUpThread, NULL, CacheCleanupWorker, cacheManager) != 0)
    {
        fprintf(stderr, "cache_init: no se pudo crear hilo de limpieza: %s\n", strerror(errno));
        FreeCacheManager(cacheManager);
        return NULL;
    }
    pthread_detach(cleanUpThread); // Corre independientemente

    return cacheManager;
}

bool cacheKeyFromRequest(const HTTPRequest *request, char *outKey, size_t outKeyLen)
{
    // MD5 hash siempre produce 32 caracteres + null terminator
    if (!request || !outKey || outKeyLen < 33)
        return false;

    // Verificamos que el metodo sea cacheable (GET o HEAD)
    if (request->method != GET && request->method != HEAD)
    {
        return false;
    }

    // Verificamos que no tenga headers que indiquen que no es cacheable (Authorization, Cookie)
    // Porque estos son privados entonces no deberian de cachearse
    if (findHeader(&request->headers, "Authorization") != NULL || findHeader(&request->headers, "Cookie") != NULL)
    {
        return false;
    }

    const char *host = findHeader(&request->headers, "Host");
    if (host == NULL)
    {
        return false; // No se puede generar clave sin host
    }

    // 4. Construir raw key
    char key[512];
    snprintf(key, sizeof(key), "%s|%s|%s",
             MethodToString(request->method),
             host,
             request->path);

    // 5. Hashear con MD5
    MD5Hash(key, outKey);

    return true;
}


bool CacheStore(CacheManager *cache, const char *cacheKey, const char *rawKey, const HTTPResponse *response)
{
    if (!cache || !cacheKey || !response)
        return false;

    pthread_mutex_lock(&cache->lock);

    char cachePath[512];
    BuildPath(cache, cacheKey, "", cachePath, sizeof(cachePath));

    FILE *file = fopen(cachePath, "wb");
    if (!file)
    {
        perror("fopen");
        pthread_mutex_unlock(&cache->lock);
        return false;
    }

    if (!CacheWriteFile(file, cache, rawKey, response))
        goto error;

    fclose(file);
    file = NULL;

    if (!CacheAddFromDisk(cache, cacheKey, time(NULL)))
    {
        fprintf(stderr, "cache_store: no se pudo agregar %s al indice en RAM\n", cacheKey);
        goto error;
    }

    pthread_mutex_unlock(&cache->lock);
    return true;

error:
    fprintf(stderr, "cache_store: error escribiendo archivo\n");
    if (file != NULL)
    {
        fclose(file);
        remove(cachePath);
    }
    pthread_mutex_unlock(&cache->lock);
    return false;
}

bool CacheLookUp(CacheManager *cache, const char *cacheKey, HTTPResponse *response)
{
    if (!cache || !cacheKey || !response)
        return false;

    pthread_mutex_lock(&cache->lock);

    CacheEntry *entry = CacheFindEntry(cache, cacheKey);
    if (!entry)
        goto miss;

    if (!ExpireFileCache(cache, entry, cacheKey))
        goto miss;

    CacheMeta meta;
    if (!CacheReadMeta(cache, cacheKey, &meta))
        goto miss;

    unsigned char *body = CacheReadBody(cache, cacheKey, meta.contentLength);
    if (!body)
        goto miss;

    if (!BuildHTTPResponse(response, meta.statusCode, NULL, &meta.extraHeaders, body, meta.contentLength))
    {
        free(body);
        goto miss;
    }

    pthread_mutex_unlock(&cache->lock);
    return true;

miss:
    pthread_mutex_unlock(&cache->lock);
    return false;
}

void CacheStoreAsync(CacheManager *cacheManager, ProxyMessage *proxyMessage)
{
    if (cacheManager == NULL || proxyMessage == NULL || proxyMessage->request == NULL)
        return;

    const char *host = findHeader(&proxyMessage->request->headers, "Host");
    if (host == NULL)
        return;

    char rawKey[512];
    snprintf(rawKey, sizeof(rawKey), "%s|%s|%s",
             MethodToString(proxyMessage->request->method),
             host,
             proxyMessage->request->path);

    CacheStoreArgs *storeArgs = malloc(sizeof(CacheStoreArgs));
    if (storeArgs == NULL)
        return;

    storeArgs->cacheManager = cacheManager;
    storeArgs->response = proxyMessage->response;
    strncpy(storeArgs->cacheKey, proxyMessage->cacheKey, sizeof(storeArgs->cacheKey));
    strncpy(storeArgs->rawKey, rawKey, sizeof(storeArgs->rawKey));

    pthread_t storeThread;
    if (pthread_create(&storeThread, NULL, CacheStoreWorker, storeArgs) != 0)
    {
        free(storeArgs);
        return;
    }
    pthread_detach(storeThread);
}


void cacheInvalidate(CacheManager *cache, const char *cacheKey)
{
    pthread_mutex_lock(&cache->lock);

    CacheEntry *entry = NULL;
    HASH_FIND_STR(cache->table, cacheKey, entry);
    if (entry != NULL)
    {
        // Borrar disco
        char path[512];
        BuildPath(cache, cacheKey, "", path, sizeof(path));
        remove(path);

        // Borrar RAM
        HASH_DEL(cache->table, entry);
        free(entry);
        cache->entryCount--;
    }

    pthread_mutex_unlock(&cache->lock);
}

void CacheInvalidateByRequest(CacheManager *cache, const HTTPRequest *request) {
    if (cache == NULL || request == NULL)
        return;

    const char *host = findHeader(&request->headers, "Host");
    if (host == NULL)
        return;

    char rawKey[512];
    snprintf(rawKey, sizeof(rawKey), "GET|%s|%s",
             host,
             request->path);

    char cacheKey[33];
    MD5Hash(rawKey, cacheKey);

    cacheInvalidate(cache, cacheKey);
}

// Falta destruir el mutex correctamente
// Actualmente llama pthread_mutex_destroy pero NO hace lock/unlock
// Debería ser así:

void FreeCacheManager(CacheManager *cacheManager)
{
    if (cacheManager == NULL)
        return;

    // No hace falta lock aquí porque es destrucción final,
    // pero sí hay que asegurarse que nadie más lo usa antes de llamar esto
    pthread_mutex_destroy(&cacheManager->lock);

    if (cacheManager->table != NULL)
    {
        CacheEntry *entry, *tmp;
        HASH_ITER(hh, cacheManager->table, entry, tmp)
        {
            HASH_DEL(cacheManager->table, entry);
            free(entry);
        }
    }

    free(cacheManager->cacheDir);
    free(cacheManager);
}