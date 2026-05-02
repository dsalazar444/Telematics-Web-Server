#ifndef Log_H
#define Log_H

#include <stdio.h>
#include "../Includes/http.h"

void PrintHttpRequest(const HTTPRequest *request);
void PrintHttpResponse(const HTTPResponse *res);


#endif // Log_H