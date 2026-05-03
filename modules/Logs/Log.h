#ifndef Log_H
#define Log_H

#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "../Includes/http.h"

void PrintHttpRequest(const HTTPRequest *request);
void PrintHttpResponse(const HTTPResponse *res);
void logWrite(int fd, const char *level, const char *msg);


#endif // Log_H