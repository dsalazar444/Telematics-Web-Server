#ifndef FILE_MANAGER_TYPES_H
#define FILE_MANAGER_TYPES_H

#define MAX_PATH_LEN 256
#include <stddef.h>

// definición real del opaque struct
typedef struct FileResult {
    // datos del archivo
    char*  _content;           // body a enviar
    size_t _contentLen;        // → Content-Length

    // headers que response.c necesita construir
    char _mimeType[64];      // → Content-Type si body
    char _lastModified[64];  // → Last-Modified
    char _location[1024];    // → Location (POST 201, si no, /0)
    // control de errores
    int _statusCode;        // 200, 201, 400, 403, 404, 500
} FileResult;

#endif