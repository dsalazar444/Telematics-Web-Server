#ifndef MESSAGEQUEUE_H
#define MESSAGEQUEUE_H

#include "../../../Includes/http.h"

// struct de cola que contendrá HTTPRequest
typedef struct MessageQueue MessageQueue; 

// Crea e inicializa la cola thread-safe
// Retorna NULL si falla
MessageQueue* QueueCreate(void);

// Encola un mensaje en struct — lo llama el thread lector
// Retorna 0 si éxito, -1 si falla
int QueueEnqueue(MessageQueue* queue, HTTPRequest* msg);

// Desencola el siguiente mensaje — lo llama el thread worker
// Si está vacía, BLOQUEA hasta que llegue algo
HTTPRequest* QueueDequeue(MessageQueue* queue);

// Cuántos mensajes hay actualmente
int QueueSize(MessageQueue* queue);

// Libera toda la memoria
void QueueDestroy(MessageQueue* queue);

#endif