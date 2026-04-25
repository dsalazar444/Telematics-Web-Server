#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include "../../shared/messages.h"

// struct de cola que contendrá ForwardRequestsMessage
typedef struct MessageQueue MessageQueue; 

// Crea e inicializa la cola thread-safe
// Retorna NULL si falla
MessageQueue* queue_create(void);

// Encola un mensaje en struct — lo llama el thread lector
// Retorna 0 si éxito, -1 si falla
int queue_enqueue(MessageQueue* q, ForwardRequestMessage* msg);

// Desencola el siguiente mensaje — lo llama el thread worker
// Si está vacía, BLOQUEA hasta que llegue algo
ForwardRequestMessage* queue_dequeue(MessageQueue* q);

// Cuántos mensajes hay actualmente
int queue_size(MessageQueue* q);

// Libera toda la memoria
void queue_destroy(MessageQueue* q);

#endif