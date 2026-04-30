#ifndef FILE_MANAGER_TYPES_H
#define FILE_MANAGER_TYPES_H

#define MAX_PATH_LEN 1024

// definición real del opaque struct
struct FileResult {
    // datos del archivo
    char*  _content;           // body a enviar
    size_t _contentLen;        // → Content-Length

    // headers que response.c necesita construir
    char _mimeType[64];      // → Content-Type
    char _lastModified[64];  // → Last-Modified
    char _location[1024];    // → Location (POST 201, si no, /0)
    int  _isNewResource;     // → decide si es 200 o 201

    // control de errores
    int _statusCode;        // 200, 201, 400, 403, 404, 500
};

#endif