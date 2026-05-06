#include "Response.h"
#include "ResponseBody.h"
#include "FileManagerTypes.h"
#include "../../../Includes/HttpUtils.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// Inicializa una estructura HTTPResponse con el código de estado
// Pide: statusCode - código HTTP de la respuesta
// Retorna: puntero a HTTPResponse inicializado, NULL si falla malloc
static HTTPResponse* InitResponse(int statusCode) {
    HTTPResponse* res = malloc(sizeof(HTTPResponse));
    if (res == NULL) return NULL;

    res->statusCode = statusCode;
    strncpy(res->statusMessage, GetReasonPhrase(statusCode), 63);
    res->statusMessage[63] = '\0';
    res->headers.count     = 0;
    res->body              = NULL;
    res->bodyLength        = 0;

    return res;
}

// Añade un header a la respuesta HTTP
// Pide: res - respuesta HTTP; key - nombre del header; value - valor del header
// Retorna: nada
void AddHeader(HTTPResponse* res, const char* key, const char* value) {
    if (res->headers.count >= 100) return;

    size_t i = res->headers.count;
    strncpy(res->headers.headers[i].key,   key,   255);
    strncpy(res->headers.headers[i].value, value, 255);

    res->headers.headers[i].key[255]   = '\0';
    res->headers.headers[i].value[255] = '\0';

    res->headers.count++;
}

static void BuildDateHeader(char* outDate) {
    time_t now    = time(NULL);
    struct tm* tm = gmtime(&now);
    strftime(outDate, 64, "%a, %d %b %Y %H:%M:%S GMT", tm);
}

// Añade headers comunes a toda respuesta (Date y Server)
// Pide: res - respuesta HTTP
// Retorna: nada
static void AddCommonHeaders(HTTPResponse* res) {
    char date[64];
    BuildDateHeader(date);
    AddHeader(res, "Date", date);
    AddHeader(res, "Server", "Server/1.0");
}

// Construye una respuesta de error HTTP con body HTML
// Pide: statusCode - código de error HTTP
// Retorna: puntero a HTTPResponse con body HTML de error
HTTPResponse* ResponseError(int statusCode) {

    HTTPResponse* res = InitResponse(statusCode);
    if (res == NULL) return NULL;

    AddCommonHeaders(res);

    size_t bodyLen = 0;
    unsigned char* body = GenerateErrorBody(statusCode, res->statusMessage, &bodyLen);

    if (body == NULL) {
        ResponseFree(res);
        return NULL;
    }

    res->body = body;
    res->bodyLength = bodyLen;

    // headers de entidad
    char lenStr[32];
    snprintf(lenStr, sizeof(lenStr), "%zu", bodyLen);

    AddHeader(res, "Content-Length", lenStr);
    AddHeader(res, "Content-Type",   "text/html");
    AddHeader(res, "Connection", "close");

    return res;
}


// Construye una respuesta de error para HEAD (sin body)
// Pide: statusCode - código de error HTTP
// Retorna: puntero a HTTPResponse sin body
HTTPResponse* ResponseErrorHead(int statusCode) {

    HTTPResponse* res = ResponseError(statusCode);
    if (res == NULL) return NULL;
    
    // quitar body
    free(res->body);
    res->body       = NULL;
    res->bodyLength = 0;
    
    return res;
}

// Añade headers de archivo a la respuesta (Content-Type, Content-Length, Last-Modified)
// Pide: res - respuesta HTTP; fileResult - resultado del FileManager
// Retorna: nada
static void AddFileResultHeaders(HTTPResponse* res, FileResult* fileResult) {
    AddHeader(res, "Content-Type", FileResultGetMimeType(fileResult));
    
    char lenStr[32];
    snprintf(lenStr, sizeof(lenStr), "%zu", FileResultGetContentLen(fileResult));
    AddHeader(res, "Content-Length", lenStr);
    
    const char* lastModified = FileResultGetLastModified(fileResult);
    if (lastModified != NULL && lastModified[0] != '\0') {
        AddHeader(res, "Last-Modified", lastModified);
    }
}

// Construye una respuesta GET con contenido del archivo
// Pide: fileResult - resultado del FileManager con contenido
// Retorna: puntero a HTTPResponse con el archivo como body
HTTPResponse* ResponseGet(FileResult* fileResult) {

    if (fileResult == NULL || FileResultGetStatusCode(fileResult) != 200) {
        int code = fileResult ? FileResultGetStatusCode(fileResult) : 500;
        return ResponseError(code);
    }

    HTTPResponse* res = InitResponse(200);
    if (res == NULL) return NULL;

    AddCommonHeaders(res); 
    AddFileResultHeaders(res, fileResult);

    size_t bodyLen = FileResultGetContentLen(fileResult);
    if (bodyLen == 0) {
        ResponseFree(res);
        return ResponseError(500);
    }

    unsigned char* body = malloc(bodyLen);
    if (body == NULL) {
        ResponseFree(res);
        return NULL;
    }

    memcpy(body, FileResultGetContent(fileResult), bodyLen);
    res->body = body;
    res->bodyLength = bodyLen;
    return res;
}

// Construye una respuesta HEAD con headers pero sin body
// Pide: fileResult - resultado del FileManager
// Retorna: puntero a HTTPResponse con headers pero sin contenido
HTTPResponse* ResponseHead(FileResult* fileResult) {

    if (fileResult == NULL) return ResponseErrorHead(500);

    int statusCode = FileResultGetStatusCode(fileResult);

    if (statusCode != 200) return ResponseErrorHead(statusCode);
    
    HTTPResponse* res = InitResponse(200);
    if (res == NULL) return NULL;

    AddCommonHeaders(res);
    AddFileResultHeaders(res, fileResult);

    res->body = NULL;
    res->bodyLength = 0;

    return res;
}

// Construye una respuesta POST (201 Created o 200 OK)
// Pide: req - request original del cliente; fileResult - resultado del FileManager
// Retorna: puntero a HTTPResponse con Location header y HTML body
HTTPResponse* ResponsePost(const HTTPRequest* req, FileResult* fileResult) {

    if (fileResult == NULL) return ResponseError(500);

    int statusCode = FileResultGetStatusCode(fileResult);

    if (statusCode != 200 && statusCode != 201) {
        return ResponseError(statusCode);
    }

    HTTPResponse* res = InitResponse(statusCode);
    if (res == NULL) return NULL;

    AddCommonHeaders(res); //sever y date

    // Location — siempre en POST exitoso
    const char* location = FileResultGetLocation(fileResult);
    if (location != NULL && location[0] != '\0') { // o sea, code es 201

        // construir URL completa con Host del request
        const char* host = GetHeaderValue(&req->headers, "Host"); 
        
        int written;
        if (host != NULL) {

            char fullLocation[MAX_PATH_LEN];
            written = snprintf(fullLocation, sizeof(fullLocation), "http://%s%s", host, location);

            if (written < 0 || written >= MAX_PATH_LEN) {
                AddHeader(res, "Location", location);

            } else {
                AddHeader(res, "Location", fullLocation);
            }
        } else {
            AddHeader(res, "Location", location);

        }

    }

    // body HTML
    size_t bodyLen = 0;
    unsigned char* body = NULL;

    if (statusCode == 201) {
        if (location == NULL || location[0] == '\0' ) {
            ResponseFree(res);
            return ResponseError(500);
        }
        // 201 → página de confirmación con link
        body = GenerateCreatedBody(location, &bodyLen);
    } else {
        // 200 → confirmación simple
        body = GenerateSuccesfulBody(200, "OK", &bodyLen);
    }

    if (body == NULL) {
        ResponseFree(res);
        return NULL;
    }

    char lenStr[32];
    snprintf(lenStr, sizeof(lenStr), "%zu", bodyLen);
    AddHeader(res, "Content-Type",   "text/html");
    AddHeader(res, "Content-Length", lenStr);

    res->body = body;
    res->bodyLength = bodyLen;

    return res;
}