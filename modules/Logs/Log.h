#ifndef Log_H
#define Log_H

#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "../LoadBalancer/loadBalancer.h"

#include "../Includes/http.h"

void PrintHttpRequest(const HTTPRequest *request);
void PrintHttpResponse(const HTTPResponse *res);
char* HttpResponseToString(const HTTPResponse* res);
char* HttpRequestToString(const HTTPRequest* request);
char* LoadBalancerToString(LoadBalancer* lb);
void LogWrite(int file, const char *level, const char *msg);
int LogInit(const char *path);

#endif // Log_H