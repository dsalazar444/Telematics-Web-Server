#include "messages_queue.h"
#include <stdlib.h>
#include <pthread.h>

// Nodo de la lista enlazada
typedef struct QueueNode {
    ForwardRequestMessage* msg;
    struct QueueNode*      next;
} QueueNode;

// Definición real del opaque struct
struct MessagesQueue {
    QueueNode*      head;
    QueueNode*      tail;
    int             size;
    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;
};

MessagesQueue* queue_create(void) {
    // Reservamos memoria   
    MessagesQueue* q = malloc(sizeof(MessagesQueue));
    if (q == NULL) return NULL; // Si la memoria esta llena -> malloc no nos reserva espacio para q -> q = NULL -> retornamos para evitar errores

    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    pthread_mutex_init(&q->mutex, NULL); // Inicializamos mutex para queue
    pthread_cond_init(&q->not_empty, NULL); // Inicializamos variable de condición -> hilos puedan dormir mientras esperan que cola tenga mensajes

    return q;
}

int queue_enqueue(MessagesQueue* q, ForwardRequestMessage* msg) {
    QueueNode* node = malloc(sizeof(QueueNode));
    if (node == NULL) return -1;

    node->msg  = msg;
    node->next = NULL;

    pthread_mutex_lock(&q->mutex);
    // Añadir nodo a lista (modificamos lista -> mutex)
    if (q->tail == NULL) {
        // cola vacía, head y tail apuntan al mismo nodo
        q->head = node;
        q->tail = node;
    } else {
        // Actualizamos nodo último (antes de actualizarlo), para que su next sea nuestro nuevo nodo
        q->tail->next = node;
        q->tail = node; // Actualizamos a tail el nuevo nodo
    }
    q->size++;

    pthread_cond_signal(&q->not_empty);  // despierta al worker
    pthread_mutex_unlock(&q->mutex);

    return 0;
}

ForwardRequestMessage* queue_dequeue(MessagesQueue* q) {
    pthread_mutex_lock(&q->mutex); // antes de while porque obtendremos dato de q (su size)

    // mientras esté vacía, dormirse y soltar el mutex
    while (q->size == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    // sacar el nodo del frente
    QueueNode* node = q->head;
    ForwardRequestMessage* msg  = node->msg;

    q->head = node->next; //Actualizamos head de q, para que apunte a next de anterior node
    if (q->head == NULL) {
        // la cola quedó vacía
        q->tail = NULL;
    }
    q->size--;

    pthread_mutex_unlock(&q->mutex);
    free(node);  // liberar el nodo, NO el msg (el worker lo libera)

    return msg;
}

int queue_size(MessagesQueue* q) {
    pthread_mutex_lock(&q->mutex);
    int size = q->size;
    pthread_mutex_unlock(&q->mutex);
    return size;
}

void queue_destroy(MessagesQueue* q) {
    pthread_mutex_lock(&q->mutex);

    // liberar todos los nodos que queden
    QueueNode* current = q->head;
    while (current != NULL) {
        QueueNode* next = current->next;
        free(current->msg);
        free(current);
        current = next;
    }

    pthread_mutex_unlock(&q->mutex);

    // Liberamos recursos de mutex y de var de condición
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    free(q); //Destruimos los campos que no sean punteros -> como ya habiamos eliminado punteros, destruimos toda la q
}