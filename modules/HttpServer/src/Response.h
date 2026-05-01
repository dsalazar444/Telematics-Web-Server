#ifndef RESPONSE_H
#define RESPONSE_H

#include "../../../Includes/http.h"
#include "../../../Includes/HttpUtils.h"
#include "ResponseBody.h"
#include "FileManager.h"
#include "../../HttpParser/UriParser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h> 

HTTPResponse* ResponseGet(FileResult* fileResult);
HTTPResponse* ResponseHead(FileResult* fileResult);
HTTPResponse* ResponsePost(const HTTPRequest* req, FileResult* fileResult);
HTTPResponse* ResponseError(int statusCode);
void ResponseFree(HTTPResponse* response);

#endif