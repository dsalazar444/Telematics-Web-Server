#include "CacheStoreWorker.h"

// Worker que se encarga de almacenar en caché la respuesta HTTP
// Pide: arg - puntero a CacheStoreArgs con toda la información necesaria para almacenar en caché
// Retorna: NULL
void *CacheStoreWorker(void *arg) {
    // Recuperar los argumentos
    CacheStoreArgs *args = (CacheStoreArgs *)arg;
    
    // Guardar en disco y RAM
    CacheStore(args->cacheManager, args->cacheKey, args->rawKey, &args->response);
    
    // Liberar todo lo que se le pasó
    free(args->response.body);  // El body de la respuesta
    free(args);                 // Los argumentos
    return NULL;
}