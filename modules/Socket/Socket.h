#ifndef SOCKET_H
#define SOCKET_H

#include "../../Includes/ISocket.h" // Para los tipos usados en los prototipos
#include "../../Includes/http.h"   // Para HTTPRequest

int CreateDualStackSocket();
int BindSocket(ISocketListener *self, const char *host, int port);
int ListenSocket(ISocketListener *self, int backlog);
IClientSocket* CreateClientSocket(const uint8_t ip[4], int port, int timeout_ms);
IClientSocket* AcceptSocket(ISocketListener *self);
void CloseListenerSocket(ISocketListener *listener);

int RecvFromClient(IClientSocket *client, char *buf, int size);
int SendToClient(IClientSocket *client, const char *data, int size);
void CloseClientSocket(IClientSocket *client);
void GetPeerName(IClientSocket *client, char *ip_out, int *port_out);
void GetSockName(IClientSocket *client, char *ip_out, int *port_out);
int SetClientNonBlocking(IClientSocket *client, int enable);
int SendHTTPRequest(IClientSocket *socket, const HTTPRequest *request);
HTTPResponse ReadHTTPResponse(IClientSocket *backend);

#endif // SOCKET_H