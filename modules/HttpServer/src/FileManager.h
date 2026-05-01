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

// getters
int            FileResultGetStatusCode(const FileResult* result);
const char*    FileResultGetMimeType(const FileResult* result);
const char*    FileResultGetLastModified(const FileResult* result);
const char*    FileResultGetLocation(const FileResult* result);
unsigned char* FileResultGetContent(const FileResult* result);
size_t         FileResultGetContentLen(const FileResult* result);

#endif