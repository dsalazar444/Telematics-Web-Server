#include "ResponseSender.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <strings.h>

// Envía todos los datos al cliente mediante bucle (maneja fragmentación)
// Pide: client - socket del cliente; data - datos a enviar; length - tamaño de datos
// Retorna: 0 si éxito, -1 si error
static int SendAll(IClientSocket *client, const char *data, size_t length)
{
    size_t sentTotal = 0;
    while (sentTotal < length)
    {
        int sent = SendToClient(client, data + sentTotal, (int)(length - sentTotal));
        if (sent <= 0)
        {
            return -1;
        }
        sentTotal += (size_t)sent;
    }

    return 0;
}

// Envía una línea de header con formato "key: value\r\n"
// Pide: client - socket del cliente; key - nombre del header; value - valor del header
// Retorna: 0 si éxito, -1 si error
static int SendHeaderLine(IClientSocket *client, const char *key, const char *value)
{
    if (SendAll(client, key, strlen(key)) < 0)
    {
        return -1;
    }

    if (SendAll(client, ": ", 2) < 0)
    {
        return -1;
    }

    if (SendAll(client, value, strlen(value)) < 0)
    {
        return -1;
    }

    return SendAll(client, "\r\n", 2);
}

// Envía una respuesta HTTP completa (status line + headers + body) al cliente
// Pide: client - socket del cliente; response - respuesta HTTP a enviar
// Retorna: 0 si éxito, -1 si error
int SendHTTPResponse(IClientSocket *client, HTTPResponse *response)
{
    if (!client || !response) return -1;

    char statusLine[128];
    int statusLen = snprintf(statusLine, sizeof(statusLine), "HTTP/1.1 %d %s\r\n", response->statusCode, response->statusMessage);
    if (statusLen < 0 || statusLen >= (int)sizeof(statusLine))
    {
        return -1;
    }

    if (SendAll(client, statusLine, (size_t)statusLen) < 0)
    {
        return -1;
    }

    bool hasContentLength = false;
    for (size_t i = 0; i < response->headers.count; ++i)
    {
        if (strcasecmp(response->headers.headers[i].key, "Content-Length") == 0)
        {
            hasContentLength = true;
            break;
        }
    }

    for (size_t i = 0; i < response->headers.count; ++i)
    {
        if (SendHeaderLine(client,
                           response->headers.headers[i].key,
                           response->headers.headers[i].value) < 0)
        {
            return -1;
        }
    }

    if (!hasContentLength && response->body && response->bodyLength > 0)
    {
        char lenStr[32];
        snprintf(lenStr, sizeof(lenStr), "%zu", response->bodyLength);
        if (SendHeaderLine(client, "Content-Length", lenStr) < 0)
        {
            return -1;
        }
    }

    if (SendAll(client, "\r\n", 2) < 0)
    {
        return -1;
    }

    if (response->body && response->bodyLength > 0)
    {
        const char *ptr = (const char*)response->body;
        size_t remaining = response->bodyLength;
        while (remaining > 0)
        {
            int chunk = remaining > 4096 ? 4096 : (int)remaining;
            if (SendAll(client, ptr, (size_t)chunk) < 0)
            {
                return -1;
            }
            ptr += chunk;
            remaining -= (size_t)chunk;
        }
    }

    return 0;
}
