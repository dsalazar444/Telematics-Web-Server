#ifndef ISOCKET_H
#define ISOCKET_H

#include <stdint.h>

typedef struct IClientSocket IClientSocket;
typedef struct ISocketListener ISocketListener;

// Estructura para representar un socket del cliente
struct IClientSocket
{
    int fd;
};

// Estructura para representar un socket de escucha
struct ISocketListener
{
    int fd;
};

#endif