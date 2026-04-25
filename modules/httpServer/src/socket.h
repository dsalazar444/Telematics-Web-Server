#ifndef SOCKET_H
#define SOCKET_H

typedef struct Socket Socket;

typedef struct {
    char ip[64]; // Permite almacenar string de ipv4, e ipv6
    int port;
} ClientInfo;

// Crea el socket servidor, hace bind y listen. Retorna el fd del servidor, o -1 si algo falla
Socket* server_socket_create(int port); // Funciones para crear sockets retornan uno -> el cliente no se debe preocupar en saber cómo crearlo

// Bloquea hasta que llegue un cliente. Genera nuevo socket usando la info del socket del servidor, y del cliente.
Socket* server_socket_accept(Socket* server, ClientInfo* client);

void socket_close(Socket* s);

#endif