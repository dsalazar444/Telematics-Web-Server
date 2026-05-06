#ifndef FILE_MANAGER_IO_H
#define FILE_MANAGER_IO_H

#include "FileManagerTypes.h"
#include <sys/stat.h>
#include <stddef.h>

int HandleDirectory(char* realPath, struct stat* outStat);
int GetFileMetadata(const char* absPath, char* outRealPath, struct stat* outStat, FileResult* result);
int ReadFile(const char* realPath, const struct stat* pathStat, FileResult* result);
int CheckDirExists(const char* realDirPath);
int GetParentDir(const char* filePath, char* outParentDir);
int WriteFile(const char* realPath, const char* body, size_t bodyLen, int* outIsNew);

#endif