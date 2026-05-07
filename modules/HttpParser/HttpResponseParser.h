#ifndef HTTP_RESPONSE_PARSER_H
#define HTTP_RESPONSE_PARSER_H

#include <stdbool.h>
#include "../../Includes/http.h"
#include "../../Includes/HttpUtils.h"
#include "../HttpServer/src/Response.h"


bool BuildHTTPResponse(HTTPResponse *response,int statusCode,const char *statusMessage,HTTPHeaders *headers,unsigned char *body,size_t bodyLength);
int ParseHTTPResponseHead(const char *buffer,
						  size_t headerLen,
						  int *statusCode,
						  char *statusMessage,
						  size_t statusMessageSize,
						  HTTPHeaders *headers);

#endif // HTTP_RESPONSE_PARSER_H