#include "ResponseBody.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Genera HTML de error para respuestas de error HTTP
// Pide: statusCode - código HTTP; statusMessage - mensaje de estado; outLen - para retornar tamaño
// Retorna: buffer con malloc conteniendo HTML de error
unsigned char* GenerateErrorBody(int statusCode, const char* statusMessage, size_t* outLen) {

    char buffer[1024];
    int len = snprintf(buffer, sizeof(buffer),
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>%d %s</title></head>\n"
        "<body>\n"
        "  <h1>%d %s</h1>\n"
        "  <p>The server could not process your request.</p>\n"
        "</body>\n"
        "</html>\n",
        statusCode, statusMessage,
        statusCode, statusMessage);

    if (len < 0) return NULL;

    unsigned char* body = malloc(len + 1);
    if (body == NULL) return NULL;

    // copiamos el contenido del buffer local al buffer dinamico 
    memcpy(body, buffer, len + 1);
    *outLen = (size_t)len;
    return body;
}

// Genera HTML de éxito para POST 201 Created con link al recurso
// Pide: location - URI del recurso creado; outLen - para retornar tamaño
// Retorna: buffer con malloc conteniendo HTML de confirmación
unsigned char* GenerateCreatedBody(const char* location, size_t* outLen) {

    char buffer[1024];
    int len = snprintf(buffer, sizeof(buffer),
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>201 Created</title></head>\n"
        "<body>\n"
        "  <h1>201 Created</h1>\n"
        "  <p>The resource was created successfully.</p>\n"
        "  <a href=\"%s\">View resource: %s</a>\n"
        "</body>\n"
        "</html>\n",
        location, location);

    if (len < 0) return NULL;

    unsigned char* body = malloc(len + 1);
    if (body == NULL) return NULL;

    memcpy(body, buffer, len + 1);
    *outLen = (size_t)len;
    return body;
}

// Genera HTML de éxito para POST 200 OK
// Pide: statusCode - código HTTP; statusMessage - mensaje de estado; outLen - para retornar tamaño
// Retorna: buffer con malloc conteniendo HTML de confirmación
unsigned char* GenerateSuccesfulBody(int statusCode, const char* statusMessage, size_t* outLen) {

    char buffer[1024];
    int len = snprintf(buffer, sizeof(buffer),
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>%d %s</title></head>\n"
        "<body>\n"
        "  <h1>%d %s</h1>\n"
        "  <p>File successfully updated.</p>\n"
        "</body>\n"
        "</html>\n",
        statusCode, statusMessage,
        statusCode, statusMessage);

    if (len < 0) return NULL;

    unsigned char* body = malloc(len + 1);
    if (body == NULL) return NULL;

    memcpy(body, buffer, len + 1);
    *outLen = (size_t)len;
    return body;
}