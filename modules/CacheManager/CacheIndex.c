#include "CacheIndex.h"
#include "CacheUtils.h"
#define CACHE_MAX_ENTRIES 1000

// Agrega una entrada al índice de caché (tabla hash) a partir de un archivo en disco
// Pide: cache - puntero al CacheManager; cacheKey - clave del caché (MD5 hash); timestamp - tiempo de creación del caché
// Retorna: true si se agregó correctamente, false si hubo error o la entrada ya existe
bool CacheAddFromDisk(CacheManager *cache, const char *cacheKey, time_t timestamp)
{
    if (cache == NULL || cacheKey == NULL || cacheKey[0] == '\0' || timestamp <= 0)
        return false;

    if (cache->entryCount >= CACHE_MAX_ENTRIES)
        return false;

    if (strlen(cacheKey) != 32)
        return false;

    // Verificar que la entrada no exista ya en la tabla hash
    CacheEntry *existing = NULL;
    HASH_FIND_STR(cache->table, cacheKey, existing);
    if (existing != NULL)
        return false;

    // Crear nueva entrada y agregar a la tabla hash
    CacheEntry *entry = malloc(sizeof(CacheEntry));
    if (!entry)
        return false;

    // Construye la entidad con los datos obtenidos del disco
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->key, cacheKey, sizeof(entry->key) - 1);
    entry->key[sizeof(entry->key) - 1] = '\0';
    entry->timestamp = timestamp;

    HASH_ADD_STR(cache->table, key, entry);
    cache->entryCount++;

    return true;
}

// Busca una entrada en la tabla hash por clave
// Pide: cache - puntero al CacheManager; key - clave a buscar
// Retorna: puntero a CacheEntry si se encuentra, NULL si no existe
CacheEntry *CacheFindEntry(CacheManager *cache, const char *key)
{
    CacheEntry *entry = NULL;
    HASH_FIND_STR(cache->table, key, entry);
    return entry;
}

// Verifica si una entrada de caché ha expirado y la elimina si es así
// Pide: cache - puntero al CacheManager; entry - entrada a verificar; cacheKey - clave de la entrada (para eliminar el archivo si expira)
// Retorna: true si la entrada sigue válida, false si expiró y fue eliminada
bool ExpireFileCache(CacheManager *cache, CacheEntry *entry, const char *cacheKey)
{
    // Verifica tiempo de vida
    if (difftime(time(NULL), entry->timestamp) > cache->ttl)
    {
        // Eliminar entrada de la tabla hash
        HASH_DEL(cache->table, entry);
        free(entry);
        cache->entryCount--;

        // Eliminar archivo de disco
        char cachePath[512];
        BuildPath(cache, cacheKey, "", cachePath, sizeof(cachePath));
        remove(cachePath);

        return false;
    }

    return true;
}

// Carga las entradas de caché desde el disco al índice en RAM al iniciar el servidor
// Pide: cache - puntero al CacheManager
// Retorna: nada
void cacheLoadFromDisk(CacheManager *cache)
{
    // Dirección donde se encuentran guardados los datos de caché en disco
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
        // Ignorar entradas de directorio "." y ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char cachePath[512];
        BuildPath(cache, entry->d_name, "", cachePath, sizeof(cachePath));

        // Verificar que el cuerpo del caché exista en disco antes de agregar al índice en RAM
        if (!cacheBodyExists(cache, entry->d_name))
            continue;

        char storedRawKey[512] = {0};
        time_t timestamp = 0;

        // Leer metadata del archivo para obtener la clave original (Datos para formar la clave) y el timestamp
        if (!parseMetaFile(cachePath, storedRawKey, sizeof(storedRawKey), &timestamp))
            continue;

        char expectedCacheKey[33];
        MD5Hash(storedRawKey, expectedCacheKey);

        if (strcmp(expectedCacheKey, entry->d_name) != 0)
        {
            fprintf(stderr, "cacheLoadFromDisk: clave inconsistente en %s\n", entry->d_name);
            continue;
        }

        // Agregar la entrada al índice de caché en RAM
        if (!CacheAddFromDisk(cache, entry->d_name, timestamp))
        {
            fprintf(stderr, "cacheLoadFromDisk: no se pudo agregar %s a RAM\n", entry->d_name);
            continue;
        }
    }

    closedir(dir);
}