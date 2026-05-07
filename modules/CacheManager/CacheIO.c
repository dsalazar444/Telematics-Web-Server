#include "CacheIO.h"
#include "CacheUtils.h"

// Escribe en el archivo del caché los datos del metada de control
// Pide: file - puntero al archivo abierto para escritura; cache - puntero al CacheManager
// para obtener configuración; rawKey - clave original (Datos para formar la clave); response - respuesta HTTP a cachear
// Retorna: true si se escribió correctamente, false si hubo error
static bool CacheWriteMeta(FILE *file, CacheManager *cache, const char *rawKey, const HTTPResponse *response)
{
    const char *contentType = findHeader(&response->headers, "Content-Type");
    if (!contentType)
        contentType = "application/octet-stream";

    // Escribir los campos de control al inicio del archivo
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

// Escribe los headers del recurso en el archivo
// Pide: file - puntero al archivo abierto para escritura; response - respuesta HTTP a cachear (de donde se obtienen los headers)
// Retorna: true si se escribió correctamente, false si hubo error
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

// Escribe el cuerpo del recurso cacheado en el archivo
// Pide: file - puntero al archivo abierto para escritura; response - respuesta HTTP a cachear (de donde se obtiene el body)
// Retorna: true si se escribió correctamente, false si hubo error
static bool CacheWriteBody(FILE *file, const HTTPResponse *response)
{
    if (response->bodyLength > 0 && response->body)
    {
        if (fwrite(response->body, 1, response->bodyLength, file) != response->bodyLength)
            return false;
    }
    return true;
}

// Orquesta la escritura completa del archivo en el cache
// Pide: file - puntero al archivo abierto para escritura; cache - puntero al CacheManager
// para obtener configuración; rawKey - clave original (Datos para formar la clave); response - respuesta HTTP a cachear
// Retorna: true si se escribió correctamente, false si hubo error
bool CacheWriteFile(FILE *file, CacheManager *cache, const char *rawKey, const HTTPResponse *response)
{
    if (!CacheWriteMeta(file, cache, rawKey, response))
        return false;

    if (!CacheWriteExtraHeaders(file, response))
        return false;

    if (fputc('\n', file) == EOF)
        return false;

    if (!CacheWriteBody(file, response))
        return false;

    return true;
}