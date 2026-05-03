#include <string.h>
#include "Log.h"
#include "../Includes/HttpUtils.h"
#include <pthread.h>
#include <stdlib.h>
#include "../LoadBalancer/loadBalancer.h"

static pthread_mutex_t _logMutex = PTHREAD_MUTEX_INITIALIZER;

void PrintHttpRequest(const HTTPRequest *request)
{
    printf("Method: %d\n", request->method);
    printf("Path: %s\n", request->path);
    printf("Version: %s\n", request->version);
    printf("Headers:\n");
    for (size_t i = 0; i < request->headers.count; i++)
    {
        printf("  %s: %s\n", request->headers.headers[i].key, request->headers.headers[i].value);
    }
    if (request->body)
    {
        printf("Body length: %zu\n", request->bodyLength);
    }
    else
    {
        printf("No Body\n");
    }
}

void PrintHttpResponse(const HTTPResponse *res) {
    // Status line
    printf("HTTP/1.1 %d %s\n", res->statusCode, res->statusMessage);

    // Imprimir primero Date y Server si existen
    const char* date = GetHeaderValue(&res->headers, "Date");
    if (date) printf("Date: %s\n", date);
    const char* server = GetHeaderValue(&res->headers, "Server");
    if (server) printf("Server: %s\n", server);

    // Imprimir el resto de headers (excepto Date y Server)
    for (size_t i = 0; i < res->headers.count; i++) {
        const char* key = res->headers.headers[i].key;
        if (strcasecmp(key, "Date") == 0 || strcasecmp(key, "Server") == 0) continue;
        printf("%s: %s\n", key, res->headers.headers[i].value);
    }

    printf("\n"); // Separador headers-body

    // Imprimir el body si existe
    if (res->body && res->bodyLength > 0) {
        fwrite(res->body, 1, res->bodyLength, stdout); //fwrite por binarios, print no funcionaria
        printf("\n");
    }
}
char* HttpRequestToString(const HTTPRequest* request) {
    char buffer[4096];
    int  offset = 0;

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Method: %d\n", request->method);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Path: %s\n", request->path);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Version: %s\n", request->version);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Headers:\n");

    for (size_t i = 0; i < request->headers.count; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                           "  %s: %s\n",
                           request->headers.headers[i].key,
                           request->headers.headers[i].value);
    }

    if (request->body) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Body length: %zu\n", request->bodyLength);
    } else {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "No Body\n");
    }

    // malloc del tamaño exacto
    char* result = malloc(offset + 1);
    if (result == NULL) return NULL;
    memcpy(result, buffer, offset + 1);
    return result;
}

char* HttpResponseToString(const HTTPResponse* res) {
    char buffer[4096];
    int  offset = 0;

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "HTTP/1.1 %d %s\n", res->statusCode, res->statusMessage);

    // Date y Server primero
    const char* date = GetHeaderValue(&res->headers, "Date");
    if (date) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Date: %s\n", date);
    }

    const char* server = GetHeaderValue(&res->headers, "Server");
    if (server) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                           "Server: %s\n", server);
    }

    // resto de headers
    for (size_t i = 0; i < res->headers.count; i++) {
        const char* key = res->headers.headers[i].key;
        if (strcasecmp(key, "Date") == 0 || strcasecmp(key, "Server") == 0) continue;
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                           "%s: %s\n", key, res->headers.headers[i].value);
    }

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\n");

    // body — solo indicamos tamaño, no incluimos bytes binarios en el string
    if (res->body && res->bodyLength > 0) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[Body: %zu bytes]\n", res->bodyLength);
    }

    char* result = malloc(offset + 1);
    if (result == NULL) return NULL;
    memcpy(result, buffer, offset + 1);
    return result;
}

char* LoadBalancerToString(LoadBalancer* lb) {
    if (lb == NULL) {
        char* result = malloc(32);
        if (result == NULL) return NULL;
        strncpy(result, "LoadBalancer is NULL\n", 31);
        return result;
    }

    // calcular tamaño necesario dinámicamente
    // header + footer fijos + línea por cada backend
    int bufSize = 256 + (lb->BackendCount * 128);
    char* buffer = malloc(bufSize);
    if (buffer == NULL) return NULL;

    int offset = 0;

    offset += snprintf(buffer + offset, bufSize - offset,
                       "\n=== LoadBalancer Status ===\n");
    offset += snprintf(buffer + offset, bufSize - offset,
                       "Total backends: %u\n", lb->BackendCount);
    offset += snprintf(buffer + offset, bufSize - offset,
                       "Current RR index: %u\n", lb->rrIndex);
    offset += snprintf(buffer + offset, bufSize - offset,
                       "\nBackends:\n");

    for (uint16_t i = 0; i < lb->BackendCount; i++) {
        BackendNode* bn = &lb->BackendNodes[i];
        offset += snprintf(buffer + offset, bufSize - offset,
                           "  [%u] %u.%u.%u.%u:%u | healthy=%s | active_conn=%u\n",
                           bn->index,
                           bn->id.ip[0], bn->id.ip[1],
                           bn->id.ip[2], bn->id.ip[3],
                           bn->id.port,
                           bn->healthy ? "YES" : "NO",
                           bn->ActiveConnections);
    }

    offset += snprintf(buffer + offset, bufSize - offset,
                       "============================\n\n");

    // ajustar al tamaño exacto
    char* result = malloc(offset + 1);
    if (result == NULL) {
        free(buffer);
        return NULL;
    }
    memcpy(result, buffer, offset + 1);
    free(buffer);
    return result;
}



void LogWrite(int file, const char *level, const char *msg) {
    char buf[1024];
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    int len = snprintf(buf, sizeof(buf), "[%s] [%s] %s\n", timestamp, level, msg);

        // escritura atómica
    pthread_mutex_lock(&_logMutex);
    write(file, buf, len);
    pthread_mutex_unlock(&_logMutex);
}

int LogInit(const char *path) {

    int logFile = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (logFile < 0) {
        perror("Error abriendo log");
        return -1;
    }
    return logFile; 
}