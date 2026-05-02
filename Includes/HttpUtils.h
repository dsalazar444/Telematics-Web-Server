#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include "http.h"

// busca un header por nombre (case-insensitive)
// retorna el valor si existe, NULL si no
const char* GetHeaderValue(const HTTPHeaders* headers, const char* key);
const char* GetReasonPhrase(int statusCode);

#endif