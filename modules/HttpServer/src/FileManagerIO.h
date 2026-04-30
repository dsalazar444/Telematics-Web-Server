#ifndef FILE_MANAGER_IO_H
#define FILE_MANAGER_IO_H

#include "FileManagerTypes.h" // porque necesitamos FileResult
#include <sys/stat.h>         // porque declaramos struct stat*
#include <stddef.h> // Para size_t

int HandleDirectory(char* realPath, struct stat* outStat);
int CheckParentDirExists(const char* realPath);
int ReadFile(const char* realPath, const struct stat* pathStat, FileResult* result);
int WriteFile(const char* realPath, const char* body, size_t bodyLen, int* outIsNew);
// obtiene metadata sin leer el body
int GetFileMetadata(const char* absPath, char* outRealPath, struct stat* outStat, FileResult* result);

#endif