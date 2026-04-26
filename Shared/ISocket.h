#ifndef ISOCKET_H
#define ISOCKET_H

#include <stdint.h>

typedef struct IClientSocket IClientSocket;
typedef struct ISocketListener ISocketListener;

struct IClientSocket
{
    int fd;
    int (*recv)(IClientSocket *self, char *buf, int size);
    int (*send)(IClientSocket *self, const char *data, int size);
    void (*close)(IClientSocket *self);
    void (*getpeername)(IClientSocket *self, char *ip_out, int *port_out);
    void (*getsockname)(IClientSocket *self, char *ip_out, int *port_out);
    int (*set_nonblocking)(IClientSocket *self, int enable);
};

struct ISocketListener
{
    int fd;
    void (*fdbind)(ISocketListener *self, const char *host, int port);
    void (*listen)(ISocketListener *self, int backlog);
    IClientSocket *(*accept)(ISocketListener *self);
    void (*close)(ISocketListener *self);
};

#endif // ISOCKET_H