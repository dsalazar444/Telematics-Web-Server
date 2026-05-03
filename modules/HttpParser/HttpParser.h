#ifndef HTTPPARSER_H
#define HTTPPARSER_H

#include "../../Includes/http.h"
#include "UriParser.h"

// En HTTP/1.1 el header Host es obligatorio.

HTTPRequest *ParseHTTPRequest(const char *buffer, int headerSize, size_t contentLength, unsigned short *statusCode);
//void PrintHttpRequest(const HTTPRequest *request);
HTTPMethod ParseMethod(const char *method);
//char *BuildErrorResponse(unsigned short statusCode, size_t *outSize);

#endif // HTTPPARSER_H