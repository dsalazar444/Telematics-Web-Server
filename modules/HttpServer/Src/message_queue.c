#include "message_queue.h"
#include <stdlib.h>
#include <pthread.h>

// Nodo de la lista enlazada
typedef struct QueueNode {
    ForwardRequestMessage* msg;
    struct QueueNode*      next;
} QueueNode;

// Definición real del opaque struct
struct MessageQueue {
    QueueNode*      _head;
    QueueNode*      _tail;
    int             _size;
    pthread_mutex_t _mutex;
    pthread_cond_t  _notEmpty;
};

MessageQueue* QueueCreate(void) {
    // Reservamos memoria   
    MessageQueue* queue = malloc(sizeof(MessageQueue));
    if (queue == NULL) return NULL; // Si la memoria esta llena -> malloc no nos reserva espacio para queue -> queue = NULL -> retornamos para evitar errores

    queue->_head = NULL;
    queue->_tail = NULL;
    queue->_size = 0;
    pthread_mutex_init(&queue->_mutex, NULL); // Inicializamos mutex para queue
    pthread_cond_init(&queue->_notEmpty, NULL); // Inicializamos variable de condición -> hilos puedan dormir mientras esperan que cola tenga mensajes

    return queue;
}

int QueueEnqueue(MessageQueue* queue, ForwardRequestMessage* msg) {
    QueueNode* node = malloc(sizeof(QueueNode));
    if (node == NULL) return -1;

    node->msg  = msg;
    node->next = NULL;

    pthread_mutex_lock(&queue->_mutex);
    // Añadir nodo a lista (modificamos lista -> mutex)
    if (queue->_tail == NULL) {
        // cola vacía, head y tail apuntan al mismo nodo
        queue->_head = node;
        queue->_tail = node;
    } else {
        // Actualizamos nodo último (antes de actualizarlo), para que su next sea nuestro nuevo nodo
        queue->_tail->next = node;
        queue->_tail = node; // Actualizamos a tail el nuevo nodo
    }
    queue->_size++;

    pthread_cond_signal(&queue->_notEmpty);  // despierta al worker
    pthread_mutex_unlock(&queue->_mutex);

    return 0;
}

ForwardRequestMessage* QueueDequeue(MessageQueue* queue) {
    pthread_mutex_lock(&queue->_mutex); // antes de while porque obtendremos dato de queue (su size)

    // mientras esté vacía, dormirse y soltar el mutex
    while (queue->_size == 0) {
        pthread_cond_wait(&queue->_notEmpty, &queue->_mutex);
    }

    // sacar el nodo del frente
    QueueNode* node = queue->_head;
    ForwardRequestMessage* msg  = node->msg;

    queue->_head = node->next; //Actualizamos head de queue, para que apunte a next de anterior node
    if (queue->_head == NULL) {
        // la cola quedó vacía
        queue->_tail = NULL;
    }
    queue->_size--;

    pthread_mutex_unlock(&queue->_mutex);
    free(node);  // liberar el nodo, NO el msg (el worker lo libera)

    return msg;
}

int QueueSize(MessageQueue* queue) {
    pthread_mutex_lock(&queue->_mutex);
    int size = queue->_size;
    pthread_mutex_unlock(&queue->_mutex);
    return size;
}

void QueueDestroy(MessageQueue* queue) {
    pthread_mutex_lock(&queue->_mutex);

    // liberar todos los nodos que queden
    QueueNode* current = queue->_head;
    while (current != NULL) {
        QueueNode* next = current->next;
        free(current->msg);
        free(current);
        current = next;
    }

    pthread_mutex_unlock(&queue->_mutex);

    // Liberamos recursos de mutex y de var de condición
    pthread_mutex_destroy(&queue->_mutex);
    pthread_cond_destroy(&queue->_notEmpty);
    free(queue); //Destruimos los campos que no sean punteros -> como ya habiamos eliminado punteros, destruimos toda la queue
}