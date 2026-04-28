#include "FileManagerIO.h"
#include "FileManagerTypes.h"
#include "FileManager.h"
#include <stdio.h>    // para fopen, fread, fwrite, fclose
#include <sys/stat.h> // para stat(), S_ISDIR(), S_ISREG()
#include <string.h>   // para strlen, strrchr, strncpy, snprintf
#include <stdlib.h>   // para malloc, free
// FUNCIÓN: funciones que abren, leen y escriben archivos:

// retorna 1 si es directorio y encontró index.html
// retorna 0 si no es directorio (no hace nada)
// retorna -1 si es directorio pero no hay index.html
// Verifica si url es una carpeta, si sí, busca si tiene index.html, y actualiza realPath si corresponde
// outsat es el stat() de un archivo o dir, que contiene info básica de él, dado por el OS
int HandleDirectory(char* realPath, struct stat* outStat) {
    // ¿es un directorio?
    if (!S_ISDIR(outStat->st_mode)) return 0;

    // es directorio → buscar index.html
    char indexPath[MAX_PATH_LEN];
    int  pathLen = strlen(realPath);

    if (realPath[pathLen - 1] == '/') {
        snprintf(indexPath, MAX_PATH_LEN, "%sindex.html", realPath);
    } else {
        snprintf(indexPath, MAX_PATH_LEN, "%s/index.html", realPath);
    }

    // stat() del index.html → sobreescribe outStat
    if (stat(indexPath, outStat) != 0) return -1;
    if (!S_ISREG(outStat->st_mode)) return -1;

    // actualizar realPath
    strncpy(realPath, indexPath, MAX_PATH_LEN - 1);
    realPath[MAX_PATH_LEN - 1] = '\0';

    return 1;
}


// pathStat es la info del archivo
int ReadFile(const char* realPath, const struct stat* pathStat, FileResult* result) {
    // tamaño exacto del archivo desde stat
    size_t fileSize = pathStat->st_size;

    // abrir archivo en modo binario
    FILE* file = fopen(realPath, "rb");
    if (file == NULL) return 0;

    // malloc del tamaño exacto
    result->_content = malloc(fileSize); 
    if (result->_content == NULL) {// problema de memoria (malloc falló)
        fclose(file);
        return 0;
    }

    // leer todos los bytes de una vez
    size_t bytesRead = fread(result->_content, 1, fileSize, file);
    fclose(file);

    // verificar que leímos todo
    if (bytesRead != fileSize) {
        free(result->_content);
        result->_content = NULL;
        return 0;
    }

    result->_contentLen = fileSize;
    return 1;
}


// --------------------- POST -------------------------------
int CheckParentDirExists(const char* realPath) {
    char parentPath[MAX_PATH_LEN];
    strncpy(parentPath, realPath, MAX_PATH_LEN - 1);
    parentPath[MAX_PATH_LEN - 1] = '\0';

    // encontrar el último "/" y cortar ahí
    char* lastSlash = strrchr(parentPath, '/');
    if (lastSlash == NULL) return 0;
    *lastSlash = '\0';  // parentPath ahora es solo el directorio

    // verificar que existe y es directorio
    struct stat pathStat;
    if (stat(parentPath, &pathStat) != 0)  return 0;
    if (!S_ISDIR(pathStat.st_mode))        return 0;

    return 1;
}


int WriteFile(const char* realPath, const char* body, size_t bodyLen, int* outIsNew) {
    // verificar si el archivo ya existe
    struct stat pathStat;
    *outIsNew = (stat(realPath, &pathStat) != 0) ? 1 : 0; // si stat falla → archivo no existe → es nuevo

    // abrir en modo binario escritura
    // "wb" → crea si no existe, sobreescribe si existe
    FILE* file = fopen(realPath, "wb");
    if (file == NULL) return 0;

    // escribir body
    size_t bytesWritten = fwrite(body, 1, bodyLen, file);
    fclose(file);

    if (bytesWritten != bodyLen) return 0;

    return 1;
}