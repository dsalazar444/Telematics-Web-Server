#ifndef FILE_MANAGER_UTILS_H
#define FILE_MANAGER_UTILS_H

#include "FileManagerTypes.h"
#include <sys/stat.h>

void FileManagerSetRoot(const char* root);
int  BuildRealPath(const char* absPath, char* outPath);
void GetMimeTypeByExtension(const char* path, char* outMime);
void GetLastModified(const struct stat* pathStat, char* outDate);
int GenerateFileName(const char* contentType, char* outFileName);
int EnsureTrailingSlash(char* path);

#endif