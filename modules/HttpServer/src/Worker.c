#include "Worker.h"
//#include <../../Socket/Socket.h> // se obtiene de responsesender
#include "ResponseSender.h"
#include "../HttpParser/HttpParser.h"
#include "FileManagerTypes.h"
#include "../../Worker/Worker.h"


static HTTPRequest* RecvRequest(IClientSocket* client) {

    char buffer[4096]; // recibe datos de socket
    char requestBuffer[4096]; // acumulador de petición completa
    int requestLen = 0; // cant de bits acumulados

    while (1)
    {

        memset(buffer, 0, sizeof(buffer)); //limpiamos buffer con \0 desde i= tamaño
        int bytes = RecvFromClient(client, buffer, sizeof(buffer));
        if (bytes <= 0) break;

        memcpy(requestBuffer + requestLen, buffer, bytes); //copiamos nuevo bloque (de buffer) en requesBuffer despues de lo que tenia ya "escrito"
        requestLen += bytes;
        requestBuffer[requestLen] = '\0';

        int headerSize = 0;
        int contentLength = 0;
        int requestInfoStatus = GetRequestSizes(requestBuffer, &headerSize, &contentLength); // mira si ya tiene headers completos, y cuanto body debe esperar

        // no hay más validaciones pues las hace el LB
        if (requestInfoStatus == 0) // todavia faltan headers (no hay secuencia \r\n\r\)
        {
            continue;
        }

        int expectedSize = headerSize + contentLength;
        if (requestLen < expectedSize) // todavia falta info
        {
            continue;
        }

        // Ya tenemos la petición completa (headers)
        unsigned short statusCode = 0;
        HTTPRequest *request = ParseHTTPRequest(requestBuffer, headerSize, contentLength, &statusCode);

     
        if (request == NULL || statusCode != 0)
        {
            printf("error recibiendo request acá en ws");
            // limpiamos
            requestLen = 0;
            memset(requestBuffer, 0, sizeof(requestBuffer));
            continue; //esperamos otra petición
         }
        
         
        // Limpiar buffer para la siguiente petición
        requestLen = 0;
        memset(requestBuffer, 0, sizeof(requestBuffer));

        return request;
    }
 
}


void* WorkerRun(void* arg) {

    // casteamos arg a tipo real
    WorkerWSArgs *workerArgs = (WorkerWSArgs*)arg;
    IClientSocket *client = workerArgs->client;

    //Socket* clientSocket = *(Socket**)arg;
    free(arg);  // liberar el malloc de WorkerStart

    // recibir request
    HTTPRequest* req = RecvRequest(client);
    if (req == NULL) {
        socket_close(client);
        return NULL;
    }

    PrintHttpRequest(req);

    // procesar
    HTTPResponse* res = ProcessRequest(req);
    if (res == NULL) {
        res = ResponseError(500);
    }

    PrintHttpResponse(res);
    
    // enviar response
    int sent = SendHTTPResponse(client, res);
    if (sent < 0) {
        // Hubo un error al enviar la respuesta
        log_error("Fallo al enviar respuesta al cliente");
        // Puedes cerrar el socket o intentar reenviar, etc.
    }

    // limpiar
    RequestFree(req);
    ResponseFree(res);
    CloseClientSocket(client);
    return NULL;
}

static HTTPResponse* ProcessRequest(const HTTPRequest* req) {
    //const char* host = GetHeaderValue(&req->headers, "Host");

    char path[256];
    strncpy(path, req->path, sizeof(path) - 1);
    path[255] = '\0';

    switch (req->method) {
        case GET:     return HandleGet(&path);
        case HEAD:    return HandleHead(&path);
        case POST:    return HandlePost(req, &path);
        default:      return ResponseError(501);
    }
}

static HTTPResponse* HandleGet(const char* path) {
    FileResult* fileResult = FileGet(path); // obtiene struct con atributos necesarios para construir el response
    HTTPResponse* res = ResponseGet(fileResult);
    FileResultFree(fileResult);
    return res;
}

static HTTPResponse* HandleHead(const char* path) {
    FileResult* fileResult = FileHead(path);
    HTTPResponse* res = ResponseHead(fileResult);
    FileResultFree(fileResult);
    return res;
}

static HTTPResponse* HandlePost(const HTTPRequest* req, const char* path) {
    // Content-Type es obligatorio en POST — RFC §7.2.1
    const char* contentType = GetHeaderValue(&req->headers, "Content-Type");
    if (contentType == NULL) return ResponseError(400);

    // Content-Length es obligatorio en POST — RFC §4.4
    const char* contentLen = GetHeaderValue(&req->headers, "Content-Length");
    if (contentLen == NULL) return ResponseError(411);  // 411 Length Required

    FileResult* fileResult = FilePost(path, (const char*)req->body, req->bodyLength, contentType);
    HTTPResponse* res = ResponsePost(req, fileResult);
    FileResultFree(fileResult);
    return res;
}

void PrintHttpResponse(const HTTPResponse *res) {
    // Status line
    printf("HTTP/1.1 %d %s\n", res->statusCode, res->statusMessage);

    // Imprimir primero Date y Server si existen
    const char* date = GetHeaderValue(&res->headers, "Date");
    if (date) printf("Date: %s\n", date);
    const char* server = GetHeaderValue(&res->headers, "Server");
    if (server) printf("Server: %s\n", server);

    // Imprimir el resto de headers (excepto Date y Server)
    for (size_t i = 0; i < res->headers.count; i++) {
        const char* key = res->headers.headers[i].key;
        if (strcasecmp(key, "Date") == 0 || strcasecmp(key, "Server") == 0) continue;
        printf("%s: %s\n", key, res->headers.headers[i].value);
    }

    printf("\n"); // Separador headers-body

    // Imprimir el body si existe
    if (res->body && res->bodyLength > 0) {
        fwrite(res->body, 1, res->bodyLength, stdout); //fwrite por binarios, print no funcionaria
        printf("\n");
    }
}