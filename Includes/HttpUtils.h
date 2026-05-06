#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include "http.h"

// Busca el valor de un header por nombre (case-insensitive)
// Pide: headers - lista de headers; key - nombre del header
// Retorna: valor si existe, NULL si no
const char* GetHeaderValue(const HTTPHeaders* headers, const char* key);

// Obtiene la frase descriptiva para un código HTTP
// Pide: statusCode - código HTTP (200, 404, 500, etc.)
// Retorna: string con la descripción
const char* GetReasonPhrase(int statusCode);

// Libera un request HTTP
// Pide: request - puntero a HTTPRequest
// Retorna: nada
void RequestFree(HTTPRequest* request);

// Libera una response HTTP
// Pide: response - puntero a HTTPResponse
// Retorna: nada
void ResponseFree(HTTPResponse* response);

// Analiza buffer de request y extrae tamaño de headers y body
// Pide: requestBuffer - datos recibidos; headerSize - para tamaño headers; contentLength - para tamaño body
// Retorna: 1 si ok, 0 si headers incompletos, -1 si error de formato
int GetRequestSizes(const char *requestBuffer, int *headerSize, int *contentLength);

#endif