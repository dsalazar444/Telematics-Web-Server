#ifndef URIPARSER_H
#define URIPARSER_H

#include <ctype.h> 

#define URI_MAX_LEN 2048

typedef struct {
    char _absPath[URI_MAX_LEN];  // path normalizado y decodificado
    char _query[URI_MAX_LEN];    // lo que viene después de "?"
    int  _isValid;                   // 0 si debe rechazarse (403/400)
    int  _statusCode;                // 400, 403, o 200 si ok
} ParsedURI;

// Parsea y normaliza una URI cruda
ParsedURI UriParse(const char* rawUri);

#endif