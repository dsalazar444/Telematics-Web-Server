#include "CacheUtils.h"

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
        {
            return headers->headers[i].value;
        }
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

    rawKey[0] = '\0';
    *timestamp = 0;

    char line[512];
    while (fgets(line, sizeof(line), file))
    {
        if (strncmp(line, "key=", 4) == 0)
        {
            strncpy(rawKey, line + 4, keySize - 1);
            rawKey[strcspn(rawKey, "\n")] = '\0';
        }
        else if (strncmp(line, "timestamp=", 10) == 0)
        {
            *timestamp = (time_t)atol(line + 10);
        }
    }

    fclose(file);

    return rawKey[0] != '\0' && *timestamp != 0;
}

bool cacheBodyExists(CacheManager *cache, const char *cacheKey)
{
    if (!cache || !cacheKey)
        return false;

    char bodyPath[512];
    BuildPath(cache, cacheKey, "body", bodyPath, sizeof(bodyPath));

    return access(bodyPath, F_OK) == 0;
}

bool CacheReadMeta(CacheManager *cache, const char *cacheKey, CacheMeta *meta)
{
    if (!cache || !cacheKey || !meta)
        return false;

    char metaPath[512];
    BuildPath(cache, cacheKey, "meta", metaPath, sizeof(metaPath));

    FILE *f = fopen(metaPath, "r");
    if (!f)
        return false;

    meta->statusCode = 200;
    meta->contentLength = 0;
    strncpy(meta->contentType, "application/octet-stream", sizeof(meta->contentType) - 1);
    meta->extraHeaders.count = 0;

    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        char *eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char *key = line;
        char *value = eq + 1;

        value[strcspn(value, "\n")] = '\0';

        if (strcmp(key, "status") == 0)
            meta->statusCode = atoi(value);
        else if (strcmp(key, "Content-Type") == 0 || strcmp(key, "content_type") == 0)
            strncpy(meta->contentType, value, sizeof(meta->contentType) - 1);
        else if (strcmp(key, "Content-Length") == 0 || strcmp(key, "content_length") == 0)
            meta->contentLength = (size_t)atol(value);
        else
        {
            if (meta->extraHeaders.count < 100)
            {
                int i = meta->extraHeaders.count;
                strncpy(meta->extraHeaders.headers[i].key, key, 255);
                strncpy(meta->extraHeaders.headers[i].value, value, 255);
                meta->extraHeaders.headers[i].key[255] = '\0';
                meta->extraHeaders.headers[i].value[255] = '\0';
                meta->extraHeaders.count++;
            }
        }
    }

    fclose(f);
    return true;
}

unsigned char *CacheReadBody(CacheManager *cache,const char *cacheKey,size_t contentLength)
{
    if (!cache || !cacheKey || contentLength == 0)
        return NULL;

    char bodyPath[512];
    BuildPath(cache, cacheKey, "body", bodyPath, sizeof(bodyPath));

    FILE *file = fopen(bodyPath, "rb");
    if (!file)
        return NULL;

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
