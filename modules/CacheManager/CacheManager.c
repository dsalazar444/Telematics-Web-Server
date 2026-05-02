#include "CacheManager.h"
#include "CacheWorker.h"
#define CACHE_MAX_ENTRIES 1000

#include "CacheUtils.h"

bool CacheAddFromDisk(CacheManager *cache, const char *cacheKey, time_t timestamp);
bool ExpireFileCache(CacheManager *cache, CacheEntry *entry, const char *cacheKey);
static CacheEntry *CacheFindEntry(CacheManager *cache, const char *key);
void CacheDestroy(CacheManager *cache);

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
        CacheDestroy(cacheManager);
        return NULL;
    }
    pthread_detach(cleanUpThread); // Corre independient

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

static bool CacheWriteMeta(FILE *file, CacheManager *cache, const char *rawKey, const HTTPResponse *response)
{
    const char *contentType = findHeader(&response->headers, "Content-Type");
    if (!contentType)
        contentType = "application/octet-stream";

    if (fprintf(file,
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
                response->bodyLength) < 0)
    {
        return false;
    }

    return true;
}

static bool CacheWriteExtraHeaders(FILE *file, const HTTPResponse *response)
{
    for (size_t i = 0; i < response->headers.count; i++)
    {
        const char *key = response->headers.headers[i].key;
        const char *value = response->headers.headers[i].value;

        if (strcasecmp(key, "Content-Type") == 0 ||
            strcasecmp(key, "Content-Length") == 0)
            continue;

        if (fprintf(file, "%s=%s\n", key, value) < 0)
            return false;
    }

    return true;
}

static bool CacheWriteBody(FILE *file, const HTTPResponse *response)
{
    if (response->bodyLength > 0 && response->body)
    {
        if (fwrite(response->body, 1, response->bodyLength, file) != response->bodyLength)
            return false;
    }
    return true;
}

bool cacheStore(CacheManager *cache, const char *cacheKey, const char *rawKey, const HTTPResponse *response)
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

    // 🔹 Metadata
    if (!CacheWriteMeta(file, cache, rawKey, response))
        goto error;

    // 🔹 Headers extra
    if (!CacheWriteExtraHeaders(file, response))
        goto error;

    // 🔹 Separador
    if (fputc('\n', file) == EOF)
        goto error;

    // 🔹 Body
    if (!CacheWriteBody(file, response))
        goto error;

    fclose(file);

    // 🔹 Índice RAM (reutiliza tu función)
    if (!CacheAddFromDisk(cache, cacheKey, time(NULL)))
    {
        fprintf(stderr, "cache_store: no se pudo agregar %s al indice en RAM\n", cacheKey);
        goto error;
    }

    pthread_mutex_unlock(&cache->lock);
    return true;

error:
    fprintf(stderr, "cache_store: error escribiendo archivo\n");
    fclose(file);
    remove(cachePath);
    pthread_mutex_unlock(&cache->lock);
    return false;
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
        // 1. Filtrar archivos de caché sin extensión
        if (entry->d_name[0] == '.' || strchr(entry->d_name, '.') != NULL)
            continue;

        // 2. Path del archivo único
        char cachePath[512];
        BuildPath(cache, entry->d_name, "", cachePath, sizeof(cachePath));

        // 3. Parsear metadata y validar que el rawKey corresponde al cacheKey
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

        // 4. Verificar que el archivo siga existiendo y tenga cuerpo
        if (!cacheBodyExists(cache, entry->d_name))
            continue;

        // 5. Insertar en RAM
        if (!CacheAddFromDisk(cache, entry->d_name, timestamp))
        {
            fprintf(stderr, "cacheLoadFromDisk: no se pudo agregar %s a RAM\n", entry->d_name);
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

        char cachePath[512];
        BuildPath(cache, cacheKey, "", cachePath, sizeof(cachePath));
        remove(cachePath);

        pthread_mutex_unlock(&cache->lock);
        return false;
    }

    return true;
}

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

void CacheDestroy(CacheManager *cache)
{
    if (cache == NULL)
        return;

    pthread_mutex_lock(&cache->lock);

    CacheEntry *entry, *tmp;
    HASH_ITER(hh, cache->table, entry, tmp)
    {
        HASH_DEL(cache->table, entry);
        free(entry);
    }

    pthread_mutex_unlock(&cache->lock);
    pthread_mutex_destroy(&cache->lock);
    free(cache);
}