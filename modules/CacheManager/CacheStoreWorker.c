#include "CacheStoreWorker.h"

void *CacheStoreWorker(void *arg) {
    // 1. Recuperar los argumentos
    CacheStoreArgs *args = (CacheStoreArgs *)arg;
    
    // 2. Guardar en disco y RAM
    CacheStore(args->cacheManager, args->cacheKey, args->rawKey, &args->response);
    
    // 3. Liberar todo lo que se le pasó
    free(args->response.body);  // El body de la respuesta
    free(args);                 // Los argumentos
    return NULL;
}