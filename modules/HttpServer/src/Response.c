#include "Response.h"
#include "ResponseBody.h"
#include "../../../Includes/HttpUtils.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char* GetReasonPhrase(int statusCode) {
    switch (statusCode) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 411: return "Length Required";
        case 414: return "Request-URI Too Long";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        default:  return "Unknown";
    }
}

// Inicializa response, asignando valores de statuscode y statusMessage
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

static void AddHeader(HTTPResponse* res, const char* key, const char* value) {
    // verificar que no superamos el límite de headers
    if (res->headers.count >= 100) return;

    size_t i = res->headers.count; // 3 headers -> 3
    strncpy(res->headers.headers[i].key,   key,   255); // añadimos en posición i, 
    strncpy(res->headers.headers[i].value, value, 255);
    res->headers.headers[i].key[255]   = '\0'; // limpiamos resto de espacio
    res->headers.headers[i].value[255] = '\0';
    res->headers.count++;
}

static void BuildDateHeader(char* outDate) {
    time_t now    = time(NULL);
    struct tm* tm = gmtime(&now);
    strftime(outDate, 64, "%a, %d %b %Y %H:%M:%S GMT", tm);
}

static void AddCommonHeaders(HTTPResponse* res) {
    // Date — MUST RFC §14.18
    char date[64];
    BuildDateHeader(date);
    AddHeader(res, "Date", date);
    AddHeader(res, "Server", "Server/1.0"); // TODO: CAMBIAR ESTO EN CADA SERVER
}

HTTPResponse* ResponseError(int statusCode) {
    HTTPResponse* res = InitResponse(statusCode);
    if (res == NULL) return NULL;

    AddCommonHeaders(res); // Date y sever

    // generar body HTML de error
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
    // Convierte el tamaño del body (bodyLen) a string para el header Content-Length
    // el body de un Error es el html que retornamos
    snprintf(lenStr, sizeof(lenStr), "%zu", bodyLen); // convertimos num a text
    // Añade el header Content-Type con valor "text/html"
    AddHeader(res, "Content-Type",   "text/html");
    // Añade el header Content-Length con el valor calculado en lenStr
    AddHeader(res, "Content-Length", lenStr);

    return res;
}

// content-lenght y bodyLen es lo mismo, solo que primero es str, 2do es size_t
// Agrega los headers de entidad basados en FileResult (Content-Type, Content-Length, Last-Modified) compartidos entre get y head
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

// Headers: date, server, content-type, content-len y last_modif
HTTPResponse* ResponseGet(FileResult* fileResult) {
    // si FileManager reportó error → response de error
    if (fileResult == NULL || FileResultGetStatusCode(fileResult) != 200) { // en get, si exito, solo retorno 200s
        int code = fileResult ? FileResultGetStatusCode(fileResult) : 500;
        return ResponseError(code);
    }

    HTTPResponse* res = InitResponse(200); // porque no cayó en if -> status = 200
    if (res == NULL) return NULL;

    // date y server
    AddCommonHeaders(res); 
    AddFileResultHeaders(res, fileResult); //Content-Type, Content-Length, Last-Modified

    // copiar body
    size_t bodyLen = FileResultGetContentLen(fileResult);
    unsigned char* body = malloc(bodyLen);
    if (body == NULL) {
        ResponseFree(res);
        return NULL;
    }

    // Body
    memcpy(body, FileResultGetContent(fileResult), bodyLen);
    res->body = body;
    res->bodyLength = bodyLen;

    return res;
}

// NO RETORNO HTML MESSAGE, pero sí su tamaño
// Headers: date, server, content-type, content-len y last_modif. si archivo existe
HTTPResponse* ResponseHead(FileResult* fileResult) {
    if (fileResult == NULL || FileResultGetStatusCode(fileResult) != 200) {
        int code = fileResult ? FileResultGetStatusCode(fileResult) : 500;
        return ResponseError(code);
    }

    HTTPResponse* res = InitResponse(200);
    if (res == NULL) return NULL;

    AddCommonHeaders(res); //Tdata y server
    if (FileResultGetContentLen(fileResult) != 0) { // porque significa que se le asignó de forma correcta en FM
        AddFileResultHeaders(res, fileResult);

    } else {
        size_t bodyLen = 0;
        unsigned char* tmp = GenerateErrorBody(404, "Not Found", &bodyLen);
        free(tmp); // Solo interesa bodyLen
        
        char lenStr[32];
        snprintf(lenStr, sizeof(lenStr), "%zu", bodyLen); // convertimos num a text
        AddHeader(res, "Content-Type",   "text/html");
        AddHeader(res, "Content-Length", lenStr);
        //last_modified no porque archivo solicitado no existe    
    }
    // sin body — eso es todo lo que diferencia HEAD de GET
    res->body = NULL;
    res->bodyLength = 0; //no son headers, son atributos de struct

    return res;
}

// en fresult, me llega solo status code y location, si aplica, porque el body (body, contenttype (html), contentlenght), se genera acá porque es el html message
HTTPResponse* ResponsePost(const HTTPRequest* req, FileResult* fileResult) {
    if (fileResult == NULL) return ResponseError(500);

    int statusCode = FileResultGetStatusCode(fileResult);

    // error de FileManager → response de error
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
        if (host != NULL) {
            char fullLocation[512];
            snprintf(fullLocation, sizeof(fullLocation), "http://%s%s", host, location);
            AddHeader(res, "Location", fullLocation);
        } else {
            AddHeader(res, "Location", location);
        }
    }

    // body HTML
    size_t bodyLen = 0;
    unsigned char* body = NULL;

    if (statusCode == 201) {
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

void ResponseFree(HTTPResponse* response) {
    if (response) {
        if (response->body) {
            free(response->body);
        }
        free(response);
    }
}