#include "HttpParser.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

HTTPRequest *ParseHTTPRequest(const char *buffer, int headerSize, size_t contentLength)
{
    HTTPRequest *request = malloc(sizeof(HTTPRequest));
    if (request == NULL)
        return NULL;
    memset(request, 0, sizeof(HTTPRequest));

    const char *headersEnd = buffer + headerSize;

    const char *requestLineEnd = strstr(buffer, "\r\n");
    if (requestLineEnd == NULL || requestLineEnd > headersEnd)
    {
        free(request);
        return NULL;
    }

    char requestLine[1024];
    size_t requestLineLen = (size_t)(requestLineEnd - buffer);
    if (requestLineLen == 0 || requestLineLen >= sizeof(requestLine))
    {
        free(request);
        return NULL;
    }
    memcpy(requestLine, buffer, requestLineLen);
    requestLine[requestLineLen] = '\0';

    char method[16] = {0};
    if (sscanf(requestLine, "%15s %255s %9s", method, request->path, request->version) != 3)
    {
        free(request);
        return NULL;
    }
    request->method = ParseMethod(method);

    if (request->method == -1)
    {
        free(request);
        return NULL;
    }

    char headers_copy[headerSize + 1];
    memcpy(headers_copy, buffer, headerSize); 
    headers_copy[headerSize] = '\0';

    const char *lineStart = strstr(headers_copy, "\r\n") + 2; 
    const char *headersEndCopy = headers_copy + headerSize;  

    while (lineStart < headersEndCopy)
    {
        const char *lineEnd = strstr(lineStart, "\r\n");
        if (lineEnd == NULL || lineEnd > headersEndCopy)
        {
            free(request);
            return NULL;
        }

        if (lineEnd == lineStart)
        {
            break;
        }

        if (request->headers.count >= 100)
        {
            free(request);
            return NULL;
        }

        HTTPHeader *header = &request->headers.headers[request->headers.count];

        const char *colon = memchr(lineStart, ':', (size_t)(lineEnd - lineStart));
        if (colon != NULL)
        {
            size_t keyLen = (size_t)(colon - lineStart);
            if (keyLen >= sizeof(header->key))
            {
                keyLen = sizeof(header->key) - 1;
            }
            memcpy(header->key, lineStart, keyLen);
            header->key[keyLen] = '\0';

            const char *valueStart = colon + 1;
            while (valueStart < lineEnd && (*valueStart == ' ' || *valueStart == '\t'))
            {
                valueStart++;
            }

            size_t valueLen = (size_t)(lineEnd - valueStart);
            if (valueLen >= sizeof(header->value))
            {
                valueLen = sizeof(header->value) - 1;
            }
            memcpy(header->value, valueStart, valueLen);
            header->value[valueLen] = '\0';

            request->headers.count++;
        }

        lineStart = lineEnd + 2;
    }

    request->body = NULL;
    request->bodyLength = 0; 

    if (contentLength > 0 && contentLength < 1024 * 1024) 
    {
        const unsigned char *bodyStart = (const unsigned char *)buffer + headerSize;

        request->body = malloc(contentLength); 
        if (request->body == NULL)
        {
            free(request);
            return NULL;
        }

        memcpy(request->body, bodyStart, contentLength); 
        request->bodyLength = contentLength; 
    }

    return request;
}

HTTPMethod ParseMethod(const char *method)
{
    if (strcasecmp(method, "GET") == 0)
        return GET;
    if (strcasecmp(method, "POST") == 0)
        return POST;
    if (strcasecmp(method, "PUT") == 0)
        return PUT;
    if (strcasecmp(method, "DELETE") == 0)
        return DELETE;
    if (strcasecmp(method, "HEAD") == 0)
        return HEAD;
    if (strcasecmp(method, "OPTIONS") == 0)
        return OPTIONS;
    if (strcasecmp(method, "CONNECT") == 0)
        return CONNECT;
    if (strcasecmp(method, "TRACE") == 0)
        return TRACE;
    return -1;
}