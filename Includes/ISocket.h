#ifndef ISOCKET_H
#define ISOCKET_H

#include <stdint.h>

typedef struct IClientSocket IClientSocket;
typedef struct ISocketListener ISocketListener;

// Socket cliente para comunicación con un cliente
struct IClientSocket
{
    int fd;
};

struct ISocketListener
{
    int fd;
};

#endif