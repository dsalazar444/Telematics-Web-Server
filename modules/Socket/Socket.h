#ifndef SOCKET_H
#define SOCKET_H

#include "../../Shared/ISocket.h" // Para los tipos usados en los prototipos

int CreateDualStackSocket();
int BindSocket(ISocketListener *self, const char *host, int port);
int ListenSocket(ISocketListener *self, int backlog);
IClientSocket* AcceptSocket(ISocketListener *self);
void CloseListenerSocket(ISocketListener *listener);

int RecvFromClient(IClientSocket *client, char *buf, int size);
int SendToClient(IClientSocket *client, const char *data, int size);
void CloseClientSocket(IClientSocket *client);
void GetPeerName(IClientSocket *client, char *ip_out, int *port_out);
void GetSockName(IClientSocket *client, char *ip_out, int *port_out);
int SetClientNonBlocking(IClientSocket *client, int enable);

#endif // SOCKET_H