
#include "HttpResponseParser.h"

// Construye una HTTPResponse a partir de los datos dados y en la estructura definida
// Pide: response - puntero a HTTPResponse a llenar; statusCode - código de estado HTTP; statusMessage - mensaje de estado (si NULL se obtiene automáticamente); headers - headers a agregar a la respuesta; body - cuerpo de la respuesta; bodyLength - tamaño del cuerpo
// Retorna: true si se construyó correctamente, false si hubo error
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