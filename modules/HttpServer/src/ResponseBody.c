#include "ResponseBody.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

unsigned char* GenerateErrorBody(int statusCode, const char* statusMessage, size_t* outLen) {
    char buffer[1024];//buffer local -> espacio estatico

    // con buffer local, obtenemos el tamaño que debe tener un buffer para guardar nuestro mensaje html
    // necesario para saber de que tamaño hacer malloc para body, que es buffer dinamico, y no desperdiciamos memoria
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

    // copiamos el contenido del buffer local al buffer dinamico body
    memcpy(body, buffer, len + 1);
    *outLen = (size_t)len; // cambiamos valor en memoria donde apuntaba el puntero
    return body;
}

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

// para code 200 en post, en get no se manda nada
unsigned char* GenerateSuccesfulBody(int statusCode, const char* statusMessage, size_t* outLen) {
    char buffer[1024];//buffer local -> espacio estatico

    // con buffer local, obtenemos el tamaño que debe tener un buffer para guardar nuestro mensaje html
    // necesario para saber de que tamaño hacer malloc para body, que es buffer dinamico, y no desperdiciamos memoria
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

    // copiamos el contenido del buffer local al buffer dinamico body
    memcpy(body, buffer, len + 1);
    *outLen = (size_t)len;
    return body;
}