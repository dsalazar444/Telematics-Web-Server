#include "WorkerWS.h"
#include "ResponseSender.h"
#include "FileManagerTypes.h"
#include "../../HttpParser/HttpParser.h"
#include "../../../Includes/HttpUtils.h"
#include "../../Logs/Log.h"

#define LEVEL "Server"

static HTTPRequest* RecvRequest(IClientSocket* client);
static HTTPResponse* ProcessRequest(const HTTPRequest* req);
static HTTPResponse* HandleGet(const char* path);
static HTTPResponse* HandleHead(const char* path);
static HTTPResponse* HandlePost(const HTTPRequest* req, const char* path);
void PrintHttpResponse(const HTTPResponse *res);

// Ejecuta el thread worker: recibe request, procesa y envía response
// Pide: arg - puntero a WorkerWSArgs con cliente y logFile
// Retorna: NULL al terminar el thread
void* WorkerRun(void* arg) {

    WorkerWSArgs *workerArgs = (WorkerWSArgs*)arg;
    IClientSocket *client = workerArgs->client;
    int logFile = workerArgs->logFile;
    free(arg);

    // recibir request
    HTTPRequest* req = RecvRequest(client);
    PrintHttpRequest(req);
    const char *reqString = HttpRequestToString(req);
    LogWrite(logFile, LEVEL, reqString);

    HTTPResponse* res = ProcessRequest(req);
    if (res == NULL) {
        res = ResponseError(500);
    }

    PrintHttpResponse(res);
    const char *resString = HttpResponseToString(res);
    LogWrite(logFile, LEVEL, resString);
    
    // enviar
    int sent = SendHTTPResponse(client, res);
    if (sent < 0) {
        perror("Fallo al enviar respuesta al cliente");
    }

    RequestFree(req);
    ResponseFree(res);
    CloseClientSocket(client);
    return NULL;
}

// Recibe un request HTTP completo del cliente con manejo de fragmentación
// Pide: client - socket del cliente
// Retorna: puntero a HTTPRequest parseado y validado
static HTTPRequest* RecvRequest(IClientSocket* client) {

    char buffer[4096]; // recibe datos de socket
    char requestBuffer[4096]; // acumulador de petición completa
    int requestLen = 0; // cant de bits acumulados

    while (1)
    {
        memset(buffer, 0, sizeof(buffer)); //limpiar buffer
        int bytes = RecvFromClient(client, buffer, sizeof(buffer));
        if (bytes <= 0) break;

        memcpy(requestBuffer + requestLen, buffer, bytes); //copiamos nuevo bloque
        requestLen += bytes;
        requestBuffer[requestLen] = '\0';

        int headerSize = 0;
        int contentLength = 0;
        int requestInfoStatus = GetRequestSizes(requestBuffer, &headerSize, &contentLength); // mira si ya tiene headers completos, y cuanto body debe esperar

        if (requestInfoStatus == 0) continue; // faltan headers

        int expectedSize = headerSize + contentLength;
        if (requestLen < expectedSize) continue;
        
        // ya tenemos request completo
        unsigned short statusCode = 0;
        HTTPRequest *request = ParseHTTPRequest(requestBuffer, headerSize, contentLength, &statusCode);
     
        if (request == NULL || statusCode != 0)
        {
            // limpiamos
            requestLen = 0;
            memset(requestBuffer, 0, sizeof(requestBuffer));
            continue;
         }
        
        // Limpiar buffer para la siguiente petición
        requestLen = 0;
        memset(requestBuffer, 0, sizeof(requestBuffer));

        return request;
    }
}

// Procesa el request y delega al handler según el método HTTP
// Pide: req - request HTTP parseado
// Retorna: puntero a HTTPResponse apropiado
static HTTPResponse* ProcessRequest(const HTTPRequest* req) {
    
    // obtenemos path
    char path[256];
    strncpy(path, req->path, sizeof(path) - 1);
    path[255] = '\0';

    switch (req->method) {
        case GET:     return HandleGet(path);
        case HEAD:    return HandleHead(path);
        case POST:    return HandlePost(req, path);
        default:      return ResponseError(501);
    }
}

// Maneja un request GET: obtiene archivo y construye response
// Pide: path - ruta del archivo solicitado
// Retorna: puntero a HTTPResponse con el contenido
static HTTPResponse* HandleGet(const char* path) {
    FileResult* fileResult = FileGet(path);
    HTTPResponse* res = ResponseGet(fileResult);
    FileResultFree(fileResult);
    return res;
}

// Maneja un request HEAD: obtiene metadata y construye response sin body
// Pide: path - ruta del archivo solicitado
// Retorna: puntero a HTTPResponse sin contenido
static HTTPResponse* HandleHead(const char* path) {
    FileResult* fileResult = FileHead(path);
    HTTPResponse* res = ResponseHead(fileResult);
    FileResultFree(fileResult);
    return res;
}

// Maneja un request POST: valida headers, escribe archivo y construye response
// Pide: req - request HTTP completo; path - ruta donde guardar
// Retorna: puntero a HTTPResponse con status 201/200 o error
static HTTPResponse* HandlePost(const HTTPRequest* req, const char* path) {
    const char* contentType = GetHeaderValue(&req->headers, "Content-Type");
    if (contentType == NULL) return ResponseError(400);

    const char* contentLen = GetHeaderValue(&req->headers, "Content-Length");
    if (contentLen == NULL) return ResponseError(411);
    
    FileResult* fileResult = FilePost(path, (const char*)req->body, req->bodyLength, contentType);
    HTTPResponse* res = ResponsePost(req, fileResult);
    FileResultFree(fileResult);
    return res;
}