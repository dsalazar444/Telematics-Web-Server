#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <stddef.h> // para size_t

typedef struct FileResult FileResult; 

void FileManagerInit(const char* root);

// GET y HEAD — busca y lee el archivo (mandar un bool para saber si head o get?)
FileResult* FileGet(const char* absPath);

// POST — escribe body en disco
FileResult* FilePost(const char* absPath, const char* body, size_t bodyLen);

// Libera memoria del content
void FileResultFree(FileResult* result);

#endif