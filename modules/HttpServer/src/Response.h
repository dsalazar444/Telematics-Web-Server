#ifndef RESPONSE_H
#define RESPONSE_H

#include "../../../Includes/http.h"
#include "FileManager.h"

HTTPResponse* ResponseGet(FileResult* fileResult);
HTTPResponse* ResponseHead(FileResult* fileResult);
HTTPResponse* ResponsePost(const HTTPRequest* req, FileResult* fileResult);
HTTPResponse* ResponseError(int statusCode);
void ResponseFree(HTTPResponse* response);

#endif