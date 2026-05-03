#include "CacheUtils.h"

#define MAX_HEADERS 100

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

static bool readCacheHeader(FILE *file, CacheMeta *meta, char *rawKey, size_t rawKeySize, time_t *timestamp, long *bodyOffset)
{
    if (!file)
        return false;

    /* Inicializaciones seguras: sólo escribir en punteros válidos */
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

    while (fgets(line, sizeof(line), file))
    {
        if (strcmp(line, "\n") == 0 || strcmp(line, "\r\n") == 0)
        {
            if (bodyOffset)
                *bodyOffset = ftell(file);
            return true;
        }

        char *eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char *key = line;
        char *value = eq + 1;

        value[strcspn(value, "\r\n")] = '\0';

        /* Campos especiales y escritura sólo si los punteros son válidos */
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
            addHeader(meta, key, value);
        }
    }

    return false;
}

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

void BuildPath(const CacheManager *cache, const char *cacheKey, const char *ext, char *out, size_t outLen)
{
    if (!cache || !cacheKey || !out)
        return;

    if (ext && ext[0] != '\0')
        snprintf(out, outLen, "%s/%s.%s", cache->cacheDir, cacheKey, ext);
    else
        snprintf(out, outLen, "%s/%s", cache->cacheDir, cacheKey);
}

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

bool parseMetaFile(const char *metaPath, char *rawKey, size_t keySize, time_t *timestamp)
{
    if (!metaPath || !rawKey || keySize == 0 || !timestamp)
        return false;

    FILE *file = fopen(metaPath, "r");
    if (!file)
        return false;

    bool ok = readCacheHeader(file, NULL, rawKey, keySize, timestamp, NULL);
    fclose(file);
    return ok && rawKey[0] != '\0' && *timestamp != 0;
}

bool cacheBodyExists(CacheManager *cache, const char *cacheKey)
{
    if (!cache || !cacheKey)
        return false;

    char cachePath[512];
    BuildPath(cache, cacheKey, "", cachePath, sizeof(cachePath));
    return access(cachePath, F_OK) == 0;
}

bool CacheReadMeta(CacheManager *cache, const char *cacheKey, CacheMeta *meta)
{
    if (!cache || !cacheKey || !meta)
        return false;

    char cachePath[512];
    BuildPath(cache, cacheKey, "", cachePath, sizeof(cachePath));

    FILE *file = fopen(cachePath, "r");
    if (!file)
        return false;

    bool ok = readCacheHeader(file, meta, NULL, 0, NULL, NULL);

    fclose(file);
    return ok;
}

unsigned char *CacheReadBody(CacheManager *cache, const char *cacheKey, size_t contentLength)
{
    if (!cache || !cacheKey || contentLength == 0)
        return NULL;

    char cachePath[512];
    BuildPath(cache, cacheKey, "", cachePath, sizeof(cachePath));

    FILE *file = fopen(cachePath, "rb");
    if (!file)
        return NULL;

    long bodyOffset = 0;
    if (!readCacheHeader(file, NULL, NULL, 0, NULL, &bodyOffset))
    {
        fclose(file);
        return NULL;
    }

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

    size_t readBytes = fread(buffer, 1, contentLength, file);
    fclose(file);

    if (readBytes != contentLength)
    {
        free(buffer);
        return NULL;
    }

    return buffer;
}
