#ifndef HTTPPARSER_H
#define HTTPPARSER_H

#include "../../Includes/structs.h"

HTTPRequest *ParseHTTPRequest(const char *buffer, int headerSize, size_t contentLength);
void PrintHttpRequest(const HTTPRequest *request);
HTTPMethod ParseMethod(const char *method);

#endif // HTTPPARSER_H