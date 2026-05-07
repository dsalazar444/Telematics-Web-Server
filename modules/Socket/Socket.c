#include <sys/socket.h> // socket(), bind(), listen(), accept(), setsockopt()
#include <netinet/in.h> // struct sockaddr_in6, IPPROTO_IPV6
#include <arpa/inet.h>  // inet_pton(), inet_ntop()
#include <unistd.h>     // close()

#include <stdlib.h> // malloc(), free()
#include <stdio.h>  // perror()
#include <string.h>
#include <fcntl.h>

#include "../../Includes/ISocket.h"
#include "Socket.h"
#include <errno.h>
#include <sys/select.h>
#include "../HttpParser/HttpResponseParser.h"
#include "../HttpParser/HttpParser.h"

// Cambie el estado de IPv6ONLY del socket para aceptar o no conexiones IPv4
// Pide: fd - file descriptor del socket; enable - 1 para activar dual-stack, 0 para solo IPv6
// Retorna: 0 si éxito, -1 si error
int SetIPv6Only(int fd, int enable)
{
    int opt = enable;
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        return -1;
    }

    return 0;
}

// Crea un socket IPv6 y cambia el estado a dual-stack (IPv4 y IPv6)
// Pide: nada
// Retorna: file descriptor del socket creado, o -1 si error
int CreateDualStackSocket()
{
    int fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror("socket");
        return -1;
    }

    if (SetIPv6Only(fd, 0) == -1)
    {
        close(fd);
        return -1;
    }

    return fd;
}

// Hace Bind del socket a la dirección y puerto especificados
// Retorna 0 si éxito, -1 si error
int BindSocket(ISocketListener *self, const char *host, int port)
{
    // Estructura de sockets que necesita POSIX para bind()
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));

    // Llena los campos necesarios que aun puede (tener en cuenta que esto solo recibe binarios, por lo que solo puede agregar port y el tipo de red)
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);

    // Es necesario convertir el host a binario para eso se usa esta funcion
    // Parametros: AF_INET6 para IPv6, host es la cadena de texto con la IP, y el ultimo parametro es un puntero a donde se va a guardar la IP en formato binario
    if (inet_pton(AF_INET6, host, &addr.sin6_addr) <= 0)
    {
        perror("inet_pton");
        return -1;
    }

    // Hace la conexion entre el socket y la direccion que se le dio, si es menor a 0 hubo un error
    // (struct sockaddr *)&addr le dice el tipo, esto debido a que no conoce sockaddr_in6, y sizeof(addr) le dice el tamaño de la estructura
    if (bind(self->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return -1;
    }

    return 0;
}

int ListenSocket(ISocketListener *self, int backlog)
{
    if (listen(self->fd, backlog) < 0)
    {
        perror("listen");
        return -1;
    }
    return 0;
}

// Cierra el socket de escucha
// Pide: listener - socket de escucha a cerrar
// Retorna: nada
void CloseListenerSocket(ISocketListener *listener)
{
    if (listener == NULL)
    {
        return;
    }

    close(listener->fd);
}

IClientSocket *CreateClientSocket(const uint8_t ip[4], int port, int timeout_ms)
{
    // PASO 1: Crear socket IPv4 TCP
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0)
    {
        perror("socket");
        return NULL;
    }

    // PASO 2: Reservar estructura IClientSocket en heap
    IClientSocket *clientSocket = malloc(sizeof(IClientSocket));
    if (clientSocket == NULL)
    {
        close(socketFD);
        return NULL;
    }
    clientSocket->fd = socketFD;

    // PASO 3: Poner socket en modo NO-BLOQUEANTE
    // (así el connect no se queda esperando indefinidamente)
    if (SetClientNonBlocking(clientSocket, 1) < 0)
    {
        CloseClientSocket(clientSocket);
        return NULL;
    }

    // PASO 4: Preparar dirección del servidor remoto
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;   // IPv4
    serverAddr.sin_port = htons(port); // puerto convertido a network byte order

    if (ip == NULL)
    {
        fprintf(stderr, "CreateClientSocket: ip es NULL\n");
        CloseClientSocket(clientSocket);
        return NULL;
    }

    memcpy(&serverAddr.sin_addr, ip, sizeof(serverAddr.sin_addr));

    // PASO 4b: Intentar connect
    int connectResult = connect(clientSocket->fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));

    // Hay dos casos de error con EINPROGRESS:
    // - connectResult == 0: conectó inmediatamente
    // - connectResult < 0 pero errno == EINPROGRESS: conexión en progreso, hay que esperar
    if (connectResult < 0 && errno != EINPROGRESS)
    {
        perror("connect - error fatal");
        CloseClientSocket(clientSocket);
        return NULL;
    }

    // PASO 5: Si el connect está "en progreso", esperar con timeout
    if (connectResult < 0)
    {
        // Crear conjunto de file descriptors para monitorear
        fd_set writeableSockets;
        FD_ZERO(&writeableSockets);                  // limpiar el conjunto
        FD_SET(clientSocket->fd, &writeableSockets); // agregar nuestro socket

        // Convertir timeout de ms a segundos + microsegundos
        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;           // segundos
        timeout.tv_usec = (timeout_ms % 1000) * 1000; // microsegundos restantes

        // Bucle para reintentar si select() es interrumpido por una señal (EINTR)
        // Esto es importante en entornos multihilo: otras señales pueden despertar select()
        // sin que el socket esté listo; el POSIX estándar dice que debemos reintentar.
        int selectResult = -1;
        while (selectResult < 0)
        {
            // select() espera a que el socket sea escribible O se agote el timeout
            // - NULL para read set (no nos importa si puede leer)
            // - &writeableSockets para write set (nos importa estar listo para escribir)
            // - &timeout para el máximo tiempo a esperar
            selectResult = select(clientSocket->fd + 1, NULL, &writeableSockets, NULL, &timeout);

            // Si select retorna -1, verificar si es por una señal (EINTR)
            if (selectResult < 0)
            {
                if (errno == EINTR)
                {
                    // EINTR: select() fue interrumpido por una señal
                    // Solución: reintentar el loop (el timeout se ha actualizado automáticamente)
                    fprintf(stderr, "select() interrumpido por señal, reintentando...\n");
                    continue; // volver a intentar
                }
                else
                {
                    // Otro error (EBADF, ENOMEM, etc): es un error fatal
                    perror("select - error fatal al esperar conexión");
                    CloseClientSocket(clientSocket);
                    return NULL;
                }
            }

            // Si selectResult == 0, ha habido timeout
            if (selectResult == 0)
            {
                fprintf(stderr, "Connect timeout - no se conectó al backend %u.%u.%u.%u:%d en %d ms\n",
                    ip[0], ip[1], ip[2], ip[3], port, timeout_ms);
                CloseClientSocket(clientSocket);
                return NULL;
            }

            // selectResult > 0: algún descriptor está listo, salir del loop
            break;
        }

        // PASO 6: El socket debe estar listo; verificar que está en writeableSockets
        // (select() puede indicar que algo está listo, pero no necesariamente nuestro socket)
        if (!FD_ISSET(clientSocket->fd, &writeableSockets))
        {
            // El descriptor no está en el conjunto "escribible"
            fprintf(stderr, "Socket no está listo para escribir (select inconsistencia)\n");
            CloseClientSocket(clientSocket);
            return NULL;
        }

        // PASO 6b: Verificar si la conexión fue exitosa
        // Aunque select() diga que el socket está "listo", la conexión pudo haber fallado
        // Por eso usamos getsockopt para obtener el error real del socket
        int connectError = 0;
        socklen_t errorLength = sizeof(connectError);

        if (getsockopt(clientSocket->fd, SOL_SOCKET, SO_ERROR, &connectError, &errorLength) < 0)
        {
            perror("getsockopt - no se pudo verificar estado de conexión");
            CloseClientSocket(clientSocket);
            return NULL;
        }

        // Si connectError != 0, la conexión falló
        if (connectError != 0)
        {
            errno = connectError;
            perror("connect - falló conexión remota");
            CloseClientSocket(clientSocket);
            return NULL;
        }
    }

    // PASO 7: Restaurar socket a modo BLOQUEANTE
    // (ahora queremos que los sockets se bloqueen normalmente en send/recv)
    if (SetClientNonBlocking(clientSocket, 0) < 0)
    {
        perror("SetClientNonBlocking - aviso: no se pudo restaurar a bloqueante");
        // No retornar NULL aquí porque la conexión está OK, solo es una advertencia
    }

    // Conexión exitosa, devolver socket listo para enviar/recibir
    return clientSocket;
}

// Acepta una conexión entrante en el socket de escucha, retorna un 
// nuevo IClientSocket para la conexión o NULL si hubo error
IClientSocket *AcceptSocket(ISocketListener *self)
{
    // Crea la estructura necesaria para guardar la informacion del cliente que se va a conectar, y un entero con el tamaño de esta estructura
    struct sockaddr_in6 clientAddr;
    socklen_t addrLen = sizeof(clientAddr); // Usa este tipo de dato ya que es el estandar y adiccional evita errores

    // Acepta la conexion entrante, y guarda la informacion del cliente en clientAddr, si es menor a 0 hubo un error
    int clientFd = accept(self->fd, (struct sockaddr *)&clientAddr, &addrLen);
    if (clientFd < 0)
    {
        perror("accept");
        return NULL;
    }

    // Crea un nuevo IClientSocket para el cliente conectado
    IClientSocket *client = malloc(sizeof(IClientSocket));
    if (client == NULL)
    {
        close(clientFd);
        return NULL;
    }
    client->fd = clientFd;

    return client;
}

// Recibe datos del cliente, retorna número de bytes leídos o -1 si error
int RecvFromClient(IClientSocket *client, char *buf, int size)
{
    int bytesRead = recv(client->fd, buf, size, 0);
    if (bytesRead < 0)
    {
        perror("recv");
        return -1;
    }
    return bytesRead;
}

// Envía datos al cliente, retorna número de bytes enviados o -1 si error
int SendToClient(IClientSocket *client, const char *data, int size)
{
    int bytesSent = send(client->fd, data, size, 0);
    if (bytesSent < 0)
    {
        perror("send");
        return -1;
    }
    return bytesSent;
}

// Cierra el socket del cliente y libera la estructura
void CloseClientSocket(IClientSocket *client)
{
    close(client->fd);
    free(client);
}

// Cambia el modo de bloqueo del socket del cliente
// Pide: client - socket del cliente; enable - 1 para no bloqueante, 0 para bloqueante
// Retorna: 0 si éxito, -1 si error
int SetClientNonBlocking(IClientSocket *client, int enable)
{
    if (client == NULL)
    {
        fprintf(stderr, "SetClientNonBlocking: client es NULL\n");
        return -1;
    }

    int flags = fcntl(client->fd, F_GETFL, 0);
    if (flags < 0)
    {
        perror("fcntl(F_GETFL)");
        return -1;
    }

    if (enable)
    {
        flags |= O_NONBLOCK;
    }
    else
    {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(client->fd, F_SETFL, flags) < 0)
    {
        perror("fcntl(F_SETFL)");
        return -1;
    }

    return 0;
}

// Envía un HTTPRequest completo a través del socket (headers + body)
// Pide: socket - socket del cliente; request - puntero al HTTPRequest a enviar
// Retorna: 0 si éxito, -1 si error
int SendHTTPRequest(IClientSocket *socket, const HTTPRequest *request)
{
    if (socket == NULL || request == NULL)
    {
        fprintf(stderr, "SendHTTPRequest: socket o request es NULL\n");
        return -1;
    }

    // Buffer para construir la petición HTTP
    char httpBuffer[8192];

    // PASO 1: Serializar el request a buffer
    int bufferSize = SerializeHTTPRequest(request, httpBuffer, sizeof(httpBuffer));
    if (bufferSize < 0)
    {
        fprintf(stderr, "SendHTTPRequest: fallo al serializar request\n");
        return -1;
    }

    // PASO 2: Enviar headers
    if (SendToClient(socket, httpBuffer, bufferSize) < 0)
    {
        fprintf(stderr, "SendHTTPRequest: fallo al enviar headers\n");
        return -1;
    }

    // PASO 3: Si hay body, enviarlo
    // El body es binario, así que se envía tal cual sin procesamiento
    if (request->body != NULL && request->bodyLength > 0)
    {
        if (SendToClient(socket, (const char *)request->body, (int)request->bodyLength) < 0)
        {
            fprintf(stderr, "SendHTTPRequest: fallo al enviar body\n");
            return -1;
        }
    }

    // Todo enviado correctamente
    return 0;
}

// Helper: buscar "\r\n\r\n" en un buffer
// Pide: buffer - puntero al buffer de texto; len - longitud del buffer
// Retorna: puntero a la posición del buffer donde termina el header (justo antes de "\r\n\r\n"), o NULL si no se encuentra
static char *FindHeadersEnd(const char *buffer, size_t len)
{
    for (size_t i = 0; i + 3 < len; i++)
    {
        if (buffer[i] == '\r' && buffer[i + 1] == '\n' &&
            buffer[i + 2] == '\r' && buffer[i + 3] == '\n')
        {
            return (char *)(buffer + i);
        }
    }
    return NULL;
}

// Lee un HTTPResponse completo desde el socket, incluyendo status line, headers y body
// Pide: backend - socket del cliente conectado al backend
// Retorna: HTTPResponse con los datos leídos; si hubo error, el HTTPResponse tendrá statusCode = 0 y bodyLength = 0
HTTPResponse ReadHTTPResponse(IClientSocket *backend)
{
    // Respuesta final que se devolverá al caller.
    HTTPResponse response = {0};

    if (backend == NULL)
    {
        fprintf(stderr, "ReadHTTPResponse: backend es NULL\n");
        return response;
    }

    // Acumulador grande para juntar bytes hasta encontrar el final de headers.
    char accumulator[65536];
    size_t accumulatedBytes = 0;
    // Datos extraídos del status line y de los headers.
    int statusCode = 0;
    char statusMessage[64] = {0};
    HTTPHeaders parsedHeaders = {0};
    // Flags de control: si ya encontramos el fin de headers y si el parseo salió bien.
    int headersParsed = 0;
    int parseOk = 0;

    // Buffer dinámico para ir guardando el body completo.
    unsigned char *responseBody = NULL;
    size_t responseBodyLength = 0;
    size_t responseBodyCapacity = 0;

    // Buffer temporal para cada lectura del socket.
    char buffer[8192];
    while (1)
    {
        // Leer el siguiente bloque de bytes del backend.
        int n = RecvFromClient(backend, buffer, sizeof(buffer));
        if (n < 0)
        {
            fprintf(stderr, "ReadHTTPResponse: recv error\n");
            break;
        }
        if (n == 0)
        {
            // EOF: backend cerró la conexión
            break;
        }

        // Primero juntamos bytes hasta tener status line + headers completos.
        if (!headersParsed)
        {
            // No seguir acumulando si ya no cabe nada más.
            if (accumulatedBytes + n > sizeof(accumulator))
            {
                fprintf(stderr, "ReadHTTPResponse: respuesta muy grande\n");
                break;
            }

            // Copiar el chunk recién leído al acumulador.
            memcpy(accumulator + accumulatedBytes, buffer, n);
            accumulatedBytes += n;

            // Buscar "\r\n\r\n", que marca el final del bloque de headers HTTP.
            char *headersEnd = FindHeadersEnd(accumulator, accumulatedBytes);
            if (headersEnd)
            {
                // Ya tenemos headers completos, ahora se puede parsear el head.
                headersParsed = 1;

                /* Incluir el CRLF final de la última cabecera para que el parser
                 * vea el '\r\n' que termina esa línea. FindHeadersEnd devuelve
                 * la posición del '\r' de la primera secuencia "\r\n\r\n",
                 * por lo que debemos sumar 2 para incluir el primer CRLF. */
                size_t headerLen = (size_t)(headersEnd - accumulator) + 2;
                if (ParseHTTPResponseHead(accumulator,
                                          headerLen,
                                          &statusCode,
                                          statusMessage,
                                          sizeof(statusMessage),
                                          &parsedHeaders) == 0)
                {
                    parseOk = 1;
                }
                else
                {
                    fprintf(stderr, "ReadHTTPResponse: no se pudo parsear el response head\n");
                }

                // Todo lo que vino después de "\r\n\r\n" pertenece al body.
                char *bodyStart = headersEnd + 4;
                size_t bodyChunkLen = (size_t)((accumulator + accumulatedBytes) - bodyStart);
                if (bodyChunkLen > 0)
                {
                    // Crear el body con lo que ya llegó en este mismo chunk.
                    responseBody = malloc(bodyChunkLen);
                    if (responseBody == NULL)
                    {
                        fprintf(stderr, "ReadHTTPResponse: malloc fallo para body\n");
                        break;
                    }

                    memcpy(responseBody, bodyStart, bodyChunkLen);
                    responseBodyLength = bodyChunkLen;
                    responseBodyCapacity = bodyChunkLen;
                }

                continue;
            }
        }

        // Si los headers ya fueron parseados, todo lo nuevo pertenece al body.
        if (headersParsed)
        {
            size_t incomingLen = (size_t)n;
            if (incomingLen > 0)
            {
                // Si no alcanza el buffer actual, crecerlo de forma exponencial.
                if (responseBodyLength + incomingLen > responseBodyCapacity)
                {
                    size_t newCapacity = responseBodyCapacity == 0 ? incomingLen : responseBodyCapacity;
                    while (newCapacity < responseBodyLength + incomingLen)
                    {
                        newCapacity *= 2;
                    }

                    unsigned char *newBody = realloc(responseBody, newCapacity);
                    if (newBody == NULL)
                    {
                        fprintf(stderr, "ReadHTTPResponse: realloc fallo para body\n");
                        free(responseBody);
                        responseBody = NULL;
                        responseBodyLength = 0;
                        responseBodyCapacity = 0;
                        break;
                    }

                    responseBody = newBody;
                    responseBodyCapacity = newCapacity;
                }

                // Copiar el nuevo fragmento al final del body acumulado.
                memcpy(responseBody + responseBodyLength, buffer, incomingLen);
                responseBodyLength += incomingLen;
            }
        }
    }

    // Si el parseo del head fue correcto, construir la respuesta final.
    if (parseOk)
    {
        BuildHTTPResponse(
            &response,
            statusCode,
            statusMessage[0] != '\0' ? statusMessage : NULL,
            &parsedHeaders,
            responseBody,
            responseBodyLength);
    }
    else
    {
        // Si falló el parseo, liberar cualquier body que ya se haya reservado.
        free(responseBody);
    }

    return response;
}
