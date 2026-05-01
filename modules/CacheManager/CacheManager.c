#include "CacheManager.h"
#define CACHE_MAX_ENTRIES 1000

CacheManager *CacheManagerCreate(const char *cacheDir, uint16_t ttl)
{
    CacheManager *CacheManager = malloc(sizeof(CacheManager));
    if (CacheManager == NULL)
        return NULL;

    // 2. Copiar configuración
    strncpy(CacheManager->cacheDir, cacheDir, sizeof(CacheManager->cacheDir) - 1);
    CacheManager->cacheDir[sizeof(CacheManager->cacheDir) - 1] = '\0';
    CacheManager->ttl = ttl;
    CacheManager->entryCount = 0;

    printf("CacheManager: Configuración - cacheDir=%s, ttl=%u\n", CacheManager->cacheDir, CacheManager->ttl);

    // 3. Reservar memoria para el índice en RAM
    CacheManager->table = NULL;
    CacheManager->entryCount = 0;

    // 4. Crear directorio si no existe
    struct stat st = {0};
    if (stat(cacheDir, &st) == -1)
    {
        if (mkdir(cacheDir, 0755) == -1)
        {
            free(CacheManager->table);
            free(CacheManager);
            return NULL;
        }
    }

    // 5. Inicializar mutex
    if (pthread_mutex_init(&CacheManager->lock, NULL) != 0)
    {
        free(CacheManager->table);
        free(CacheManager);
        return NULL;
    }

    return CacheManager;
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

const char *MethodToString(const char *method)
{
    if (strcasecmp(method, "GET") == 0)
        return "GET";
    if (strcasecmp(method, "HEAD") == 0)
        return "HEAD";
    return NULL;
}

static const char *findHeader(const HTTPHeaders *headers, const char *key)
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
