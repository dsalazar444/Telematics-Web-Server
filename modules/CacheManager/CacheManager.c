#include "CacheManager.h"
#include "CacheCleanWorker.h"
#include "CacheStoreWorker.h"
#include "CacheIO.h"
#include "CacheIndex.h"
#include "../Logs/Log.h"

#define CACHE_MAX_ENTRIES 1000

#include "CacheUtils.h"

// Crea un nuevo CacheManager con la configuración dada
// Pide: cacheDir - directorio donde se almacenará la caché; ttl -
// tiempo en segundos para que un recurso expire
// Retorna: puntero al CacheManager creado, o NULL si hubo error
CacheManager *CacheManagerCreate(const char *cacheDir, uint16_t ttl)
{
    CacheManager *cacheManager = malloc(sizeof(*cacheManager));
    if (cacheManager == NULL)
        return NULL;

    // Copiar configuración
    cacheManager->cacheDir = strdup(cacheDir);
    if (cacheManager->cacheDir == NULL)
    {
        free(cacheManager);
        return NULL;
    }

    cacheManager->ttl = ttl;
    cacheManager->entryCount = 0;

    fprintf(stderr, "CacheManager creado con directorio '%s' y TTL %d segundos\n", cacheDir, ttl);

    // Reservar memoria para el índice en RAM
    cacheManager->table = NULL;
    cacheManager->entryCount = 0;

    // Crear directorio si no existe
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

    // Carga datos desde el disco a la RAM
    cacheLoadFromDisk(cacheManager);

    // Inicializar mutex
    if (pthread_mutex_init(&cacheManager->lock, NULL) != 0)
    {
        free(cacheManager->cacheDir);
        free(cacheManager->table);
        free(cacheManager);
        return NULL;
    }

    // Inicializa worker de limpieza
    pthread_t cleanUpThread;
    if (pthread_create(&cleanUpThread, NULL, CacheCleanupWorker, cacheManager) != 0)
    {
        fprintf(stderr, "cache_init: no se pudo crear hilo de limpieza: %s\n", strerror(errno));
        FreeCacheManager(cacheManager);
        return NULL;
    }
    pthread_detach(cleanUpThread); 

    return cacheManager;
}

// Genera el cacheKey a partir de la información de la request (método, host, path) y validando que sea cacheable
// Pide: request - puntero a la HTTPRequest; outKey - buffer para almacenar
// el cacheKey generado; outKeyLen - tamaño del buffer outKey (debe ser al menos 33 bytes para MD5)
// Retorna: true si se pudo generar un cacheKey válido, false si no es cache
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

    // Construir raw key
    char key[512];
    snprintf(key, sizeof(key), "%s|%s|%s",
             MethodToString(request->method),
             host,
             request->path);

    // Hashear con MD5
    MD5Hash(key, outKey);

    return true;
}


// Almacena en caché una respuesta HTTP
// Pide: cache - puntero al CacheManager; cacheKey - clave de caché a usar; rawKey - clave sin hashear (para metadata); response - respuesta HTTP a almacenar
// Retorna: true si se almacenó correctamente, false si hubo error
bool CacheStore(CacheManager *cache, const char *cacheKey, const char *rawKey, const HTTPResponse *response)
{
    if (!cache || !cacheKey || !response)
        return false;

    pthread_mutex_lock(&cache->lock);

    char cachePath[512];
    BuildPath(cache, cacheKey, "", cachePath, sizeof(cachePath));

    // Abre el archivo en escritura de binario
    FILE *file = fopen(cachePath, "wb");
    if (!file)
    {
        perror("fopen");
        pthread_mutex_unlock(&cache->lock);
        return false;
    }

    // Escribe la metadata y el body en el archivo de caché
    if (!CacheWriteFile(file, cache, rawKey, response))
        goto error;

    fclose(file);
    file = NULL;

    // Agrega a RAM para que futuras consultas puedan encontrarlo sin ir al disco
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

// Sirve archivos desde el cache
// Pide: cache - puntero al CacheManager; cacheKey - clave de caché a buscar; response - puntero a HTTPResponse donde se almacenará la respuesta leída de caché
// Retorna: true si se encontró una entrada válida en caché y se llenó response
bool CacheLookUp(CacheManager *cache, const char *cacheKey, HTTPResponse *response)
{
    if (!cache || !cacheKey || !response)
        return false;

    pthread_mutex_lock(&cache->lock);

    // Buscar la entrada en RAM
    CacheEntry *entry = CacheFindEntry(cache, cacheKey);
    if (!entry)
        goto miss;

    // Verificar si la entrada en RAM ha expirado. Si expiró, eliminarla de RAM y disco y tratar como miss
    if (!ExpireFileCache(cache, entry, cacheKey))
        goto miss;

    // Lee metadata y headers
    CacheMeta meta;
    if (!CacheReadMeta(cache, cacheKey, &meta))
        goto miss;

    // Lee el body del archivo de caché
    unsigned char *body = CacheReadBody(cache, cacheKey, meta.contentLength);
    if (!body)
        goto miss;

    // Construye la respuesta HTTP a partir de la metadata y el body leídos de caché
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

// Almacena en caché una respuesta HTTP de forma asíncrona, delegando el trabajo a un worker
// Pide: cacheManager - puntero al CacheManager; proxyMessage - mensaje del proxy
// Retorna: nada
void CacheStoreAsync(CacheManager *cacheManager, ProxyMessage *proxyMessage)
{
    if (cacheManager == NULL || proxyMessage == NULL || proxyMessage->request == NULL)
        return;

    const char *host = findHeader(&proxyMessage->request->headers, "Host");
    if (host == NULL)
        return;

    // Recostruye la cacheKey original a partir de la request para que el worker pueda generar el mismo cacheKey y almacenarlo correctamente
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


// Invalida una entrada de caché tanto en RAM como en disco
// Pide: cache - puntero al CacheManager; cacheKey - clave de caché a invalidar
// Retorna: nada
void cacheInvalidate(CacheManager *cache, const char *cacheKey)
{
    pthread_mutex_lock(&cache->lock);

    // Buscar la entrada en RAM y eliminarla de RAM y disco
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

    // Para POST/PUT/DELETE: invalidar caché del mismo path completo
    // Esto asume que GET /path puede tener datos que POST/PUT/DELETE /path modifican
    char rawKey[512];
    snprintf(rawKey, sizeof(rawKey), "GET|%s|%s",
             host,
             request->path);

    char cacheKey[33];
    MD5Hash(rawKey, cacheKey);

    fprintf(stderr, "[Cache] Invalidando entrada por %s %s -> key=%s\n",
            MethodToString(request->method), request->path, cacheKey);
    cacheInvalidate(cache, cacheKey);
}

// Libera toda la memoria asociada al CacheManager, incluyendo el índice en RAM y el directorio en disco
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