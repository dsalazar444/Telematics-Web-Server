#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <stddef.h>

typedef struct FileResult FileResult; 

void FileManagerInit(const char* root);
FileResult* FileGet(const char* absPath);
FileResult* FileHead(const char* absPath);  
FileResult* FilePost(const char* absPath, const char* body, size_t bodyLen, const char* contentType);
void FileResultFree(FileResult* result);

int            FileResultGetStatusCode(const FileResult* result);
const char*    FileResultGetMimeType(const FileResult* result);
const char*    FileResultGetLastModified(const FileResult* result);
const char*    FileResultGetLocation(const FileResult* result);
unsigned char* FileResultGetContent(const FileResult* result);
size_t         FileResultGetContentLen(const FileResult* result);

#endif