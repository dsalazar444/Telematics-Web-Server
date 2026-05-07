#include "FileManagerIO.h"
#include "FileManagerTypes.h"
#include "FileManagerUtils.h"
#include "FileManager.h"
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// FUNCIÓN: hace llamadas a SO

// Verifica si una ruta es directorio y busca index.html dentro
// Pide: realPath - ruta a verificar; outStat - estructura stat del recurso
// Retorna: 1 si es directorio con index.html, 0 si no es directorio, -1 si es directorio pero sin index.html, -2 si ruta muy larga
int HandleDirectory(char* realPath, struct stat* outStat) {

    if (!S_ISDIR(outStat->st_mode)) return 0;

    char indexPath[MAX_PATH_LEN];
    int  pathLen = strlen(realPath);

    // Construir la ruta con index
    int written;
    if (realPath[pathLen - 1] == '/') {
        written = snprintf(indexPath, MAX_PATH_LEN, "%sindex.html", realPath); // concatenamos real path con index.html, y guardamos en indexPath
    } else {
        written = snprintf(indexPath, MAX_PATH_LEN, "%s/index.html", realPath);
    }

    if (written < 0 || written >= MAX_PATH_LEN) {
        return -2;
    }

    // stat() del index.html → sobreescribe outStat
    if (stat(indexPath, outStat) != 0) return -1;
    if (!S_ISREG(outStat->st_mode)) return -1;

    // actualizar realPath
    strncpy(realPath, indexPath, MAX_PATH_LEN - 1);
    realPath[MAX_PATH_LEN - 1] = '\0';
    
    return 1;
}

// Obtiene metadata del archivo (ruta real, stat, tipo MIME, última modificación)
// Pide: absPath - ruta absoluta; outRealPath - ruta real construida; outStat - estructura stat; result - estructura para llenar
// Retorna: 1 si éxito, 0 si hay error (statusCode se asigna en result)
int GetFileMetadata(const char* absPath, char* outRealPath, struct stat* outStat, FileResult* result){
   
    if (!BuildRealPath(absPath, outRealPath)) {
        result->_statusCode = 414;
        return 0;
    }
    
    if (stat(outRealPath, outStat) != 0) {
        result->_statusCode = 404; // Archivo no existe
        return 0;
    }

    int dirResult = HandleDirectory(outRealPath, outStat);
    if (dirResult == -1) {
        result->_statusCode = 404; //Index no existe
        return 0;
    } else if (dirResult == -2){
        result->_statusCode = 414;
        return 0;
    }

    if (!S_ISREG(outStat->st_mode)) {
        result->_statusCode = 404;
        return 0;
    }

    GetMimeTypeByExtension(outRealPath, result->_mimeType); 
    GetLastModified(outStat, result->_lastModified);
    
    return 1;

}

// Lee el contenido completo del archivo
// Pide: realPath - ruta real del archivo; pathStat - estructura stat del archivo; result - estructura para llenar
// Retorna: 1 si éxito, 0 si hay error
int ReadFile(const char* realPath, const struct stat* pathStat, FileResult* result) {
    size_t fileSize = pathStat->st_size;

    FILE* file = fopen(realPath, "rb");
    if (file == NULL){
        return 0;
    } 

    result->_content = malloc(fileSize); 
    if (result->_content == NULL) {
        fclose(file);
        return 0;
    }

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

// --------------------- POST FUNCTIONS -------------------------------

// Verifica si una carpeta existe
// Pide: realDirPath - ruta real de la carpeta
// Retorna: 1 si existe y es directorio, 0 si no
int CheckDirExists(const char* realDirPath) {

    struct stat pathStat;
    if (stat(realDirPath, &pathStat) != 0)  return 0;
    if (!S_ISDIR(pathStat.st_mode)) return 0;

    return 1;
}

// Extrae la carpeta padre de una ruta
// Pide: filePath - ruta completa del archivo
// Retorna: 1 si éxito, 0 si no hay carpeta padre válida
// Nota: outParentDir contiene la ruta padre sin el nombre del archivo
int GetParentDir(const char* filePath, char* outParentDir) {

    const char* lastSlash = strrchr(filePath, '/');
    if (lastSlash == NULL) return 0;
    
    // copiamos en outparentDir
    int parentLen = lastSlash - filePath;
    strncpy(outParentDir, filePath, parentLen);
    outParentDir[parentLen] = '\0';
    
    return 1;
}

// Escribe contenido en un archivo (crea si no existe, añade si existe)
// Pide: realPath - ruta del archivo; body - contenido a escribir; bodyLen - tamaño del contenido; outIsNew - indica si fue nuevo
// Retorna: 1 si éxito, 0 si hay error
int WriteFile(const char* realPath, const char* body, size_t bodyLen, int* outIsNew) {
    
    struct stat pathStat;
    *outIsNew = (stat(realPath, &pathStat) != 0) ? 1 : 0;

    // abrir en modo binario append
    FILE* file = fopen(realPath, "ab");
    if (file == NULL) return 0;

    size_t bytesWritten = fwrite(body, 1, bodyLen, file);
    fflush(file);
    fsync(fileno(file));
    fclose(file);

    if (bytesWritten != bodyLen) return 0;
    return 1;
}