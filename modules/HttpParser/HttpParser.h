#ifndef HTTPPARSER_H
#define HTTPPARSER_H

#include "../../Includes/http.h"
#include "UriParser.h"

// En HTTP/1.1 el header Host es obligatorio.

HTTPRequest *ParseHTTPRequest(const char *buffer, int headerSize, size_t contentLength, unsigned short *statusCode);
HTTPMethod ParseMethod(const char *method);
int SerializeHTTPRequest(const HTTPRequest *request, char *buffer, int bufferSize);

#endif // HTTPPARSER_H