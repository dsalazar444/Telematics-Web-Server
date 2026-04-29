#include "HttpParser.h"
#include "../../Includes/structs.h"
#include <stdlib.h>

HTTPRequest *HttpParse(const char *raw)
{

    // Crear el HTTPRequest
    HTTPRequest *request = malloc(sizeof(HTTPRequest));
    if (request == NULL)
        return NULL;
    memset(request, 0, sizeof(HTTPRequest));

    // Hacer una copia del raw para no modificar el original
    char *rawCopy = strdup(raw);
    if (rawCopy == NULL)
    {
        free(request);
        return NULL;
    }

    // 3. Parsear la Request Line
    char *requestLine = strtok(rawCopy, "\r\n");
    if (requestLine == NULL)
    {
        free(rawCopy);
        free(request);
        return NULL;
    }

    char method[16] = {0};
    if (sscanf(requestLine, "%s %s %s", method, request->path, request->version) != 3)
    {
        free(rawCopy);
        free(request);
        return NULL;
    }
    request->method = ParseMethod(method);

    if (request->method == -1)
    {
        free(rawCopy);
        free(request);
        return NULL;
    }

    // 4. Parsear los Headers

    char *headerLine = strtok(NULL, "\r\n"); // siguiente línea

    while (headerLine != NULL && strlen(headerLine) > 0)
    {

        HTTPHeader *header = &request->headers.headers[request->headers.count];

        // separar key y value por ": "
        char *colon = strchr(headerLine, ':');
        if (colon != NULL)
        {
            // copiar key
            int keyLen = colon - headerLine;
            strncpy(header->key, headerLine, keyLen);
            header->key[keyLen] = '\0';

            // copiar value — salta ": " (dos caracteres)
            strncpy(header->value, colon + 2, sizeof(header->value) - 1);

            request->headers.count++;
        }

        headerLine = strtok(NULL, "\r\n"); // siguiente header
    }

    // 5. Parsear el Body
    request->body = NULL; // por defecto no hay body

    // Buscar el header Content-Length
    for (int i = 0; i < request->headers.count; i++)
    {
        if (strcmp(request->headers.headers[i].key, "Content-Length") == 0)
        {

            int bodyLen = atoi(request->headers.headers[i].value);

            if (bodyLen > 0)
            {
                // buscar donde empieza el body — después del \r\n\r\n
                char *bodyStart = strstr(rawCopy, "\r\n\r\n");
                if (bodyStart != NULL)
                {
                    bodyStart += 4; // salta el \r\n\r\n

                    request->body = malloc(bodyLen + 1);
                    memcpy(request->body, bodyStart, bodyLen);
                    request->body[bodyLen] = '\0';
                }
            }
            break;
        }
    }

    free(rawCopy);
    return request;
}

HTTPMethod ParseMethod(const char *method_str)
{
    if (strcmp(method_str, "GET") == 0)
        return GET;
    if (strcmp(method_str, "POST") == 0)
        return POST;
    if (strcmp(method_str, "PUT") == 0)
        return PUT;
    if (strcmp(method_str, "DELETE") == 0)
        return DELETE;
    if (strcmp(method_str, "HEAD") == 0)
        return HEAD;
    if (strcmp(method_str, "OPTIONS") == 0)
        return OPTIONS;
    if (strcmp(method_str, "CONNECT") == 0)
        return CONNECT;
    if (strcmp(method_str, "TRACE") == 0)
        return TRACE;
    return -1;
}