#ifndef RESPONSE_BODY_H
#define RESPONSE_BODY_H

#include <stddef.h>

// genera HTML para respuestas de error (400, 404, 500, etc.)
// retorna buffer con malloc — caller debe liberar
unsigned char* GenerateErrorBody(int statusCode, const char* statusMessage, size_t* outLen);

// genera HTML para POST 201 con link al recurso creado
// retorna buffer con malloc — caller debe liberar
unsigned char* GenerateCreatedBody(const char* location, size_t* outLen);

// genera HTML para post 200 
// retorna buffer con malloc — caller debe liberar
unsigned char* GenerateSuccesfulBody(int statusCode, const char* statusMessage, size_t* outLen);

#endif