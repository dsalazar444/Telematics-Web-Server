#include "CacheManager.h"
#define CACHE_MAX_ENTRIES 1000

#include "CacheUtils.h"

bool CacheAddFromDisk(CacheManager *cache, const char *cacheKey, time_t timestamp);
bool ExpireFileCache(CacheManager *cache, CacheEntry *entry, const char *cacheKey);
static CacheEntry *CacheFindEntry(CacheManager *cache, const char *key);

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
    realpath(cacheDir, absoluteDir);
    strncpy(cacheManager->cacheDir, absoluteDir, sizeof(cacheManager->cacheDir) - 1);
    cacheManager->cacheDir[sizeof(cacheManager->cacheDir) - 1] = '\0';

    // 5. Inicializar mutex
    if (pthread_mutex_init(&cacheManager->lock, NULL) != 0)
    {
        free(cacheManager->cacheDir);
        free(cacheManager->table);
        free(cacheManager);
        return NULL;
    }

    return cacheManager;
}

bool cacheKeyFromRequest(const HTTPRequest *request, char *outKey, size_t outKeyLen)
{
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

bool cacheStore(CacheManager *cache, const char *cacheKey, const char *rawKey, const HTTPResponse *response)
{
    if (cache == NULL || cacheKey == NULL || response == NULL)
        return false;

    pthread_mutex_lock(&cache->lock);

    // 1. Construir paths de los archivos
    char bodyPath[512];
    char metaPath[512];
    BuildPath(cache, cacheKey, "body", bodyPath, sizeof(bodyPath));
    BuildPath(cache, cacheKey, "meta", metaPath, sizeof(metaPath));

    // 2. Escribir el body
    if (!writeFile(bodyPath, response->body, response->bodyLength, "wb"))
    {
        fprintf(stderr, "cache_store: error escribiendo body %s\n", bodyPath);
        pthread_mutex_unlock(&cache->lock);
        return false;
    }

    // 3. Buscar Content-Type en los headers
    const char *contentType = "application/octet-stream"; // default
    contentType = findHeader(&response->headers, "Content-Type");

    char metaBuffer[512];

    int len = snprintf(metaBuffer, sizeof(metaBuffer),
                       "key=%s\n"
                       "timestamp=%ld\n"
                       "ttl=%d\n"
                       "status=%d\n"
                       "Content-Type=%s\n"
                       "Content-Length=%zu\n",
                       rawKey,
                       (long)time(NULL),
                       cache->ttl,
                       response->statusCode,
                       contentType,
                       response->bodyLength);

    if (len < 0 || len >= sizeof(metaBuffer))
    {
        fprintf(stderr, "cache_store: error construyendo meta\n");
        remove(bodyPath);
        pthread_mutex_unlock(&cache->lock);
        return false;
    }

    if (!writeFile(metaPath, metaBuffer, len, "w"))
    {
        fprintf(stderr, "cache_store: no se pudo escribir meta\n");
        remove(bodyPath);
        pthread_mutex_unlock(&cache->lock);
        return false;
    }

    // 5. Actualizar índice en RAM
    CacheEntry *existing = NULL;
    HASH_FIND_STR(cache->table, cacheKey, existing);

    if (existing != NULL)
    {
        // Ya existe → solo actualizar timestamp
        existing->timestamp = time(NULL);
    }
    else
    {
        // Nueva entrada
        CacheEntry *entry = malloc(sizeof(CacheEntry));
        if (entry == NULL)
        {
            pthread_mutex_unlock(&cache->lock);
            return false;
        }

        time_t timestamp = 0;
        CacheAddFromDisk(cache, cacheKey, time(NULL));
    }

    pthread_mutex_unlock(&cache->lock);
    return true;
}

static void cacheLoadFromDisk(CacheManager *cache)
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
        // 1. Filtrar .meta
        char *ext = strrchr(entry->d_name, '.');
        if (!ext || strcmp(ext, ".meta") != 0)
            continue;

        // 2. Path del meta
        char metaPath[512];
        BuildPath(cache, entry->d_name, "", metaPath, sizeof(metaPath));

        // 3. Parsear
        char rawKey[512] = {0};
        time_t timestamp = 0;

        if (!parseMetaFile(metaPath, rawKey, sizeof(rawKey), &timestamp))
            continue;

        // 4. Generar cacheKey (hash)
        char cacheKey[33];
        MD5Hash(rawKey, cacheKey);

        // 5. Verificar body
        if (!cacheBodyExists(cache, cacheKey))
            continue;

        // 6. Insertar en RAM
        if (CacheAddFromDisk(cache, cacheKey, timestamp) == false)
        {
            fprintf(stderr, "cacheLoadFromDisk: no se pudo agregar %s a RAM\n", cacheKey);
            continue;
        }
    }

    closedir(dir);
}

bool CacheLookUp(CacheManager *cache, const char *cacheKey, HTTPResponse *response)
{
    if (!cache || !cacheKey || !response)
        return false;

    pthread_mutex_lock(&cache->lock);

    // 1. Buscar
    CacheEntry *entry = CacheFindEntry(cache, cacheKey);
    if (!entry)
    {
        pthread_mutex_unlock(&cache->lock);
        return false;
    }

    // 2. TTL
    if (!ExpireFileCache(cache, entry, cacheKey))
    {
        pthread_mutex_unlock(&cache->lock);
        return false;
    }

    // 3. Meta
    CacheMeta meta;
    if (!CacheReadMeta(cache, cacheKey, &meta))
    {
        pthread_mutex_unlock(&cache->lock);
        return false;
    }

    // 4. Body
    unsigned char *body = CacheReadBody(cache, cacheKey, meta.contentLength);
    if (!body)
    {
        pthread_mutex_unlock(&cache->lock);
        return false;
    }

    // 5. Build response
    if (!BuildHTTPResponse(response, meta.statusCode, NULL, &meta.extraHeaders, body, meta.contentLength))
    {
        free(body);
        pthread_mutex_unlock(&cache->lock);
        return false;
    }

    pthread_mutex_unlock(&cache->lock);
    return true;
}

void FreeCacheManager(CacheManager *cacheManager)
{
    if (cacheManager == NULL)
        return;

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




static CacheEntry *CacheFindEntry(CacheManager *cache, const char *key)
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

        char bodyPath[512];
        char metaPath[512];
        BuildPath(cache, cacheKey, "body", bodyPath, sizeof(bodyPath));
        BuildPath(cache, cacheKey, "meta", metaPath, sizeof(metaPath));
        remove(bodyPath);
        remove(metaPath);

        pthread_mutex_unlock(&cache->lock);
        return false;
    }
}


bool CacheAddFromDisk(CacheManager *cache, const char *cacheKey, time_t timestamp)
{
    CacheEntry *existing = NULL;
    HASH_FIND_STR(cache->table, cacheKey, existing);
    if (existing)
        return false;

    CacheEntry *entry = malloc(sizeof(CacheEntry));
    if (!entry)
        return false;

    strncpy(entry->key, cacheKey, sizeof(entry->key) - 1);
    entry->key[sizeof(entry->key) - 1] = '\0';
    entry->timestamp = timestamp;

    HASH_ADD_STR(cache->table, key, entry);
    cache->entryCount++;

    return true;
}

