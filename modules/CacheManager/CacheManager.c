#include "CacheManager.h"
#define CACHE_MAX_ENTRIES 1000

static const char *findHeader(const HTTPHeaders *headers, const char *key);
const char *MethodToString(HTTPMethod method);

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

    // 3. Reservar memoria para el índice en RAM
    cacheManager->table = NULL;
    cacheManager->entryCount = 0;

    // 4. Crear directorio si no existe
    struct stat st = {0};
    if (stat(cacheDir, &st) == -1)
    {
        if (mkdir(cacheDir, 0755) == -1)
        {
            free(cacheManager->cacheDir);
            free(cacheManager->table);
            free(cacheManager);
            return NULL;
        }
    }

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

    // Buscamos el host, porque es parte de la clave, y si no esta lo ponemos como "unknown" para que no afecte la generacion de la clave
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


bool cache_store(CacheManager *cache, const char *cacheKey, const char *rawKey, const HTTPResponse *response) {
    if (cache == NULL || cacheKey == NULL || response == NULL) return false;

    pthread_mutex_lock(&cache->lock);

    // 1. Construir paths de los archivos
    char bodyPath[512];
    char metaPath[512];
    snprintf(bodyPath, sizeof(bodyPath), "%s/%s.body", cache->cacheDir, cacheKey);
    snprintf(metaPath, sizeof(metaPath), "%s/%s.meta", cache->cacheDir, cacheKey);

    // 2. Escribir el body
    FILE *bodyFile = fopen(bodyPath, "wb");  // wb = write binary
    if (bodyFile == NULL) {
        fprintf(stderr, "cache_store: no se pudo crear body %s: %s\n", bodyPath, strerror(errno));
        pthread_mutex_unlock(&cache->lock);
        return false;
    }
    fwrite(response->body, 1, response->bodyLength, bodyFile);
    fclose(bodyFile);

    // 3. Buscar Content-Type en los headers
    const char *contentType = "application/octet-stream";  // default
    
    contentType = GetHeaderValue(&response->headers, "Content-Type");

    // 4. Escribir el meta
    FILE *metaFile = fopen(metaPath, "w");
    if (metaFile == NULL) {
        fprintf(stderr, "cache_store: no se pudo crear meta %s: %s\n", metaPath, strerror(errno));
        // Limpiar el body que ya escribimos
        remove(bodyPath);
        pthread_mutex_unlock(&cache->lock);
        return false;
    }
    fprintf(metaFile, "key=%s\n",            rawKey);
    fprintf(metaFile, "timestamp=%ld\n",     (long)time(NULL));
    fprintf(metaFile, "ttl=%d\n",            cache->ttl);
    fprintf(metaFile, "status=%d\n",         response->statusCode);
    fprintf(metaFile, "content_type=%s\n",   contentType);
    fprintf(metaFile, "content_length=%zu\n",response->bodyLength);
    fclose(metaFile);

    // 5. Actualizar índice en RAM
    CacheEntry *existing = NULL;
    HASH_FIND_STR(cache->table, cacheKey, existing);

    if (existing != NULL) {
        // Ya existe → solo actualizar timestamp
        existing->timestamp = time(NULL);
    } else {
        // Nueva entrada
        CacheEntry *entry = malloc(sizeof(CacheEntry));
        if (entry == NULL) {
            pthread_mutex_unlock(&cache->lock);
            return false;
        }
        strncpy(entry->key, cacheKey, sizeof(entry->key) - 1);
        entry->key[sizeof(entry->key) - 1] = '\0';
        entry->timestamp = time(NULL);
        HASH_ADD_STR(cache->table, key, entry);
        cache->entryCount++;
    }

    pthread_mutex_unlock(&cache->lock);
    return true;
}



const char *MethodToString(HTTPMethod method)
{   
    if (method == GET)
        return "GET";
    if (method == HEAD)
        return "HEAD";
    return NULL;
}

const char *findHeader(const HTTPHeaders *headers, const char *key)
{
    for (size_t i = 0; i < headers->count; i++)
    {
        if (strcasecmp(headers->headers[i].key, key) == 0)
        {
            return headers->headers[i].value;
        }
    }
    return NULL;
}

void MD5Hash(const char *input, char *output)
{
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((unsigned char *)input, strlen(input), digest);
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
        sprintf(&output[i * 2], "%02x", digest[i]);
    }
}
