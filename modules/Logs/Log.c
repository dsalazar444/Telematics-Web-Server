#include <string.h>

#include "Log.h"
#include "../Includes/HttpUtils.h"

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
