#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <stddef.h> // para size_t

typedef struct FileResult FileResult; 

void FileManagerInit(const char* root);

// GET — busca y lee el archivo 
FileResult* FileGet(const char* absPath);

// HEAD - busca y genera encabezados (no lee archivo)
FileResult* FileHead(const char* absPath);  

// POST — escribe body en disco
FileResult* FilePost(const char* absPath, const char* body, size_t bodyLen, const char* contentType);

// Libera memoria del content
void FileResultFree(FileResult* result);

#endif