#include "HttpUtils.h"
#include <strings.h>

const char* GetHeaderValue(const HTTPHeaders* headers, const char* key) {
    if (headers == NULL || key == NULL) return NULL;

    for (size_t i = 0; i < headers->count; i++) {
        if (strcasecmp(headers->headers[i].key, key) == 0) {
            return headers->headers[i].value;
        }
    }
    return NULL;
}

const char* GetReasonPhrase(int statusCode) {
    switch (statusCode) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 411: return "Length Required";
        case 414: return "Request-URI Too Long";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        default:  return "Unknown";
    }
}