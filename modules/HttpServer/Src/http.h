#ifndef HTTP_H
#define HTTP_H

#include "socket.h"   // para usar ClientInfo y Socket*

// Para no mexclar dos niveles de abstracción, al reemplazar por esto clientInfo
// Útil para rawRequestMessage, que necesita ambos datos, entonces no necesitamos pasarle dos obtejos (socket e infoclient), sino solo este, que es la agrupación de ambos
typedef struct {
    Socket*    socket;
    ClientInfo info;
} ConnectedClient;

#endif