
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

// Elimina espacios al inicio y al final de una cadena (usando punteros para no modificar la cadena original)
// Pide: start - puntero al inicio de la cadena; end - puntero al final de la cadena (justo después del último carácter)
// Retorna: Nada, pero modifica los punteros start y end para que apunten al inicio y final de la cadena sin espacios.
static void TrimSpaces(const char **start, const char **end)
{
    // Mueve el puntero start hacia adelante mientras haya espacios o tabs
    while (*start < *end && (**start == ' ' || **start == '\t'))
    {
        (*start)++;
    }

    // Mueve el puntero end hacia atrás mientras haya espacios o tabs
    while (*end > *start && ((*(*end - 1) == ' ') || (*(*end - 1) == '\t')))
    {
        (*end)--;
    }
}

// Parsea el status line y headers de un HTTP response desde un buffer de texto
// Pide: buffer - puntero al buffer de texto que contiene el status line y headers; headerLen - longitud del buffer; statusCode - puntero donde se va a guardar el código de estado; statusMessage - buffer donde se va a guardar el mensaje de estado; statusMessageSize - tamaño del buffer para el mensaje de estado; headers - puntero a la estructura HTTPHeaders donde se van a guardar los headers
// Retorna: 0 si el parseo fue exitoso, o -1 si hubo un error
int ParseHTTPResponseHead(const char *buffer,
                          size_t headerLen,
                          int *statusCode,
                          char *statusMessage,
                          size_t statusMessageSize,
                          HTTPHeaders *headers)
{
    if (buffer == NULL || statusCode == NULL || statusMessage == NULL || headers == NULL)
    {
        return -1;
    }

    headers->count = 0;
    statusMessage[0] = '\0';

    const char *cursor = buffer;
    const char *headEnd = buffer + headerLen;

    const char *firstLineEnd = NULL;
    for (const char *p = cursor; p + 1 < headEnd; p++)
    {
        if (p[0] == '\r' && p[1] == '\n')
        {
            firstLineEnd = p;
            break;
        }
    }

    if (firstLineEnd == NULL)
    {
        return -1;
    }

    size_t firstLineLen = (size_t)(firstLineEnd - cursor);
    if (firstLineLen >= 512)
    {
        firstLineLen = 511;
    }

    char statusLine[512];
    memcpy(statusLine, cursor, firstLineLen);
    statusLine[firstLineLen] = '\0';

    int major = 0;
    int minor = 0;
    int code = 0;
    char reason[256] = {0};
    int parsed = sscanf(statusLine, "HTTP/%d.%d %d %255[^\r\n]", &major, &minor, &code, reason);
    if (parsed < 3)
    {
        return -1;
    }

    *statusCode = code;
    if (parsed >= 4)
    {
        snprintf(statusMessage, statusMessageSize, "%s", reason);
    }

    cursor = firstLineEnd + 2;
    while (cursor < headEnd)
    {
        if ((headEnd - cursor) >= 2 && cursor[0] == '\r' && cursor[1] == '\n')
        {
            break;
        }

        const char *lineEnd = NULL;
        for (const char *p = cursor; p + 1 < headEnd; p++)
        {
            if (p[0] == '\r' && p[1] == '\n')
            {
                lineEnd = p;
                break;
            }
        }

        if (lineEnd == NULL)
        {
            break;
        }

        const char *colon = NULL;
        for (const char *p = cursor; p < lineEnd; p++)
        {
            if (*p == ':')
            {
                colon = p;
                break;
            }
        }

        if (colon != NULL && headers->count < 100)
        {
            const char *keyStart = cursor;
            const char *keyEnd = colon;
            const char *valueStart = colon + 1;
            const char *valueEnd = lineEnd;

            TrimSpaces(&keyStart, &keyEnd);
            TrimSpaces(&valueStart, &valueEnd);

            if (keyEnd > keyStart)
            {
                size_t keyLen = (size_t)(keyEnd - keyStart);
                size_t valueLen = (size_t)(valueEnd - valueStart);

                if (keyLen > 255)
                {
                    keyLen = 255;
                }
                if (valueLen > 255)
                {
                    valueLen = 255;
                }

                HTTPHeader *header = &headers->headers[headers->count];
                memcpy(header->key, keyStart, keyLen);
                header->key[keyLen] = '\0';
                memcpy(header->value, valueStart, valueLen);
                header->value[valueLen] = '\0';
                headers->count++;
            }
        }

        cursor = lineEnd + 2;
    }

    return 0;
}