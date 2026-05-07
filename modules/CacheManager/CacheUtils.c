#include "CacheUtils.h"

#define MAX_HEADERS 100

// Función auxiliar para agregar un header a CacheMeta
// Pide: meta - puntero a CacheMeta; key - nombre del header; value - valor del header
// Retorna: true si se agregó correctamente, false si hubo error o se alcanzó el límite de headers
static bool addHeader(CacheMeta *meta, const char *key, const char *value)
{
    if (!meta || !key || !value)
        return false;

    if (meta->extraHeaders.count >= MAX_HEADERS)
        return false;
    int i = meta->extraHeaders.count;

    snprintf(meta->extraHeaders.headers[i].key,
             sizeof(meta->extraHeaders.headers[i].key),
             "%s", key);
    snprintf(meta->extraHeaders.headers[i].value,
             sizeof(meta->extraHeaders.headers[i].value),
             "%s", value);
    meta->extraHeaders.count++;

    return true;
}

// Lee el archivo de metadata del recurso solicitado para obtener la información de los headers y agregarla de forma general o especifica
// Pide: file - puntero al archivo de metadata abierto; meta - puntero a CacheMeta para llenar; rawKey - buffer para almacenar la clave original (Datos para formar la clave); 
//timestamp - para almacenar el tiempo de creación del caché; bodyOffset - para almacenar el offset donde inicia el body en el archivo
// Retorna: true si se leyó correctamente y se encontró la línea vacía que separa headers de body, false si hubo error o formato inválido
static bool readCacheHeader(FILE *file, CacheMeta *meta, char *rawKey, size_t rawKeySize, time_t *timestamp, long *bodyOffset)
{
    if (!file)
        return false;

    if (meta) {
        meta->statusCode = 200;
        meta->contentLength = 0;
        snprintf(meta->contentType, sizeof(meta->contentType), "application/octet-stream");
        meta->extraHeaders.count = 0;
    }
    if (rawKey && rawKeySize > 0)
        rawKey[0] = '\0';
    if (timestamp)
        *timestamp = 0;

    char line[512];

    // Leer el archivo línea por línea hasta encontrar la línea vacía que separa headers de body
    while (fgets(line, sizeof(line), file))
    {
        if (strcmp(line, "\n") == 0 || strcmp(line, "\r\n") == 0)
        {
            if (bodyOffset)
                *bodyOffset = ftell(file);
            return true;
        }

        // Guardar el valor si son casos especiales
        char *eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char *key = line;
        char *value = eq + 1;

        value[strcspn(value, "\r\n")] = '\0';

        // Campos especiales y escritura en variables finales
        if (rawKey && strcmp(key, "key") == 0)
        {
            snprintf(rawKey, rawKeySize, "%s", value);
        }
        else if (timestamp && strcmp(key, "timestamp") == 0)
        {
            *timestamp = (time_t)atol(value);
        }
        else if (meta && strcmp(key, "status") == 0)
        {
            meta->statusCode = atoi(value);
        }
        else if (meta && strcasecmp(key, "Content-Type") == 0)
        {
            snprintf(meta->contentType, sizeof(meta->contentType), "%s", value);
            addHeader(meta, "Content-Type", value);
        }
        else if (meta && strcasecmp(key, "Content-Length") == 0)
        {
            meta->contentLength = (size_t)atol(value);
            addHeader(meta, "Content-Length", value);
        }
        else if (meta)
        {
            // Son headers extras
            addHeader(meta, key, value);
        }
    }

    return false;
}

// Función para generar un hash MD5 de una cadena de entrada
// Pide: input - cadena de entrada, output - buffer para almacenar el hash (debe tener al menos 33 bytes para el hash + null terminator)
// Retorna: nada
void MD5Hash(const char *input, char *output)
{
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((unsigned char *)input, strlen(input), digest);
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
        sprintf(&output[i * 2], "%02x", digest[i]);
    }
    output[MD5_DIGEST_LENGTH * 2] = '\0';
}

// Construye la ruta completa del archivo de caché
// Pide: cache - puntero al CacheManager; cacheKey - clave del caché (MD5 hash); ext - extensión opcional para diferenciar meta/body; 
// out - buffer para almacenar la ruta completa; outLen - tamaño del buffer out
// Retorna: nada
void BuildPath(const CacheManager *cache, const char *cacheKey, const char *ext, char *out, size_t outLen)
{
    if (!cache || !cacheKey || !out)
        return;

    if (ext && ext[0] != '\0')
        snprintf(out, outLen, "%s/%s.%s", cache->cacheDir, cacheKey, ext);
    else
        snprintf(out, outLen, "%s/%s", cache->cacheDir, cacheKey);
}

// Busca el valor de un header por nombre (case-insensitive)
// Pide: headers - lista de headers; key - nombre del header a buscar
// Retorna: valor del header si existe, NULL si no
const char *findHeader(const HTTPHeaders *headers, const char *key)
{
    if (!headers || !key)
        return NULL;

    for (size_t i = 0; i < headers->count; i++)
    {
        if (strcasecmp(headers->headers[i].key, key) == 0)
            return headers->headers[i].value;
    }

    return NULL;
}

// Transforma un método HTTP a su representación en string
// Pide: method - método HTTP a convertir
// Retorna: string con el nombre del método (ej: "GET", "HEAD"),
const char *MethodToString(HTTPMethod method)
{
    if (method == GET)
        return "GET";
    if (method == HEAD)
        return "HEAD";
    return "";
}


bool writeFile(const char *path, const void *data, size_t size, const char *mode)
{
    if (!path || (!data && size > 0) || !mode)
        return false;

    FILE *file = fopen(path, mode);
    if (!file)
    {
        perror("fopen");
        return false;
    }

    if (size > 0)
    {
        size_t written = fwrite(data, 1, size, file);
        if (written != size)
        {
            fprintf(stderr, "writeFile: escritura incompleta (%zu/%zu)\n", written, size);
            fclose(file);
            return false;
        }
    }

    if (fclose(file) != 0)
    {
        perror("fclose");
        return false;
    }

    return true;
}

// Lee el archivo de metadata de caché para obtener la clave original, el timestamp
// Pide: metaPath - ruta al archivo de metadata, rawKey - buffer para almacenar la clave original, keySize -
// tamaño del buffer rawKey, timestamp - puntero para almacenar el timestamp
// Retorna: true si se pudo leer correctamente, false si hubo error o datos faltantes
bool parseMetaFile(const char *metaPath, char *rawKey, size_t keySize, time_t *timestamp)
{
    if (!metaPath || !rawKey || keySize == 0 || !timestamp)
        return false;

    // Abrir el archivo de metadata en modo lectura
    FILE *file = fopen(metaPath, "r");
    if (!file)
        return false;

    // Aquí obtienen solamente el rawKey y el timestamp para validar la entrada de caché, no los metadatos completos
    bool ok = readCacheHeader(file, NULL, rawKey, keySize, timestamp, NULL);
    fclose(file);
    return ok && rawKey[0] != '\0' && *timestamp != 0;
}

// Verifica si el .cuerpo del archivo en caché existe en disco para una clave dada
// Pide: cache - puntero al CacheManager, cacheKey - clave del caché
// Retorna: true si el body existe, false si no existe o hubo error
bool cacheBodyExists(CacheManager *cache, const char *cacheKey)
{
    if (!cache || !cacheKey)
        return false;

    char cachePath[512];
    BuildPath(cache, cacheKey, "", cachePath, sizeof(cachePath));
    return access(cachePath, F_OK) == 0;
}

// Lee la metadata de un recurso cacheado para obtener los headers
// Pide: cache - puntero al CacheManager; cacheKey - clave del caché; meta - puntero a CacheMeta para llenar
// Retorna: true si se pudo leer la metadata correctamente, false si hubo error o formato inválido
bool CacheReadMeta(CacheManager *cache, const char *cacheKey, CacheMeta *meta)
{
    if (!cache || !cacheKey || !meta)
        return false;

    char cachePath[512];
    BuildPath(cache, cacheKey, "", cachePath, sizeof(cachePath));

    // Abrir el archivo de metadata en modo lectura  
    FILE *file = fopen(cachePath, "r");
    if (!file)
        return false;

    // Lee ahora los datos completos de los headers para llenar CacheMeta
    bool ok = readCacheHeader(file, meta, NULL, 0, NULL, NULL);

    fclose(file);
    return ok;
}

// Lee el cuerpo del recurso cacheado desde el archivo en disco
// Pide: cache - puntero al CacheManager; cacheKey - clave del caché
// contentLength - tamaño del contenido a leer (obtenido de CacheMeta)
// Retorna: buffer con el contenido leído 
unsigned char *CacheReadBody(CacheManager *cache, const char *cacheKey, size_t contentLength)
{
    if (!cache || !cacheKey || contentLength == 0)
        return NULL;

    char cachePath[512];
    BuildPath(cache, cacheKey, "", cachePath, sizeof(cachePath));

    // Abre el archivo en modo lectura binaria para leer el cuerpo del caché
    FILE *file = fopen(cachePath, "rb");
    if (!file)
        return NULL;

    // Leer el header para obtener el offset donde inicia el body en el archivo
    long bodyOffset = 0;
    if (!readCacheHeader(file, NULL, NULL, 0, NULL, &bodyOffset))
    {
        fclose(file);
        return NULL;
    }

    // Mueve el cursor de lectura a esa posición para leer el body
    if (fseek(file, bodyOffset, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }

    unsigned char *buffer = malloc(contentLength);
    if (!buffer)
    {
        fclose(file);
        return NULL;
    }

    // Leer el contenido del body y lo almacena en el buffer
    size_t readBytes = fread(buffer, 1, contentLength, file);
    fclose(file);

    if (readBytes != contentLength)
    {
        free(buffer);
        return NULL;
    }

    return buffer;
}
