
#include "HttpResponseParser.h"

bool BuildHTTPResponse(HTTPResponse *response,int statusCode,const char *statusMessage,HTTPHeaders *headers,unsigned char *body,size_t bodyLength)
{

    response->statusCode = statusCode;

    const char *msg = statusMessage ? statusMessage : GetReasonPhrase(statusCode);

    snprintf(response->statusMessage, sizeof(response->statusMessage), "%s", msg);

    response->headers.count = 0;
    if (headers)
    {
        for (size_t i = 0; i < headers->count; i++)
        {
            AddHeader(response,
                      headers->headers[i].key,
                      headers->headers[i].value);
        }
    }

    response->body = body;
    response->bodyLength = bodyLength;

    return 1;
}