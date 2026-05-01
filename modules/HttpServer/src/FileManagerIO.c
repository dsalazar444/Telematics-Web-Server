#include "FileManagerIO.h"
#include "FileManagerTypes.h"
#include "FileManagerUtils.h"
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

int GetFileMetadata(const char* absPath, char* outRealPath, struct stat* outStat, FileResult* result){
   
    // 1. construir ruta real
    char realPath[MAX_PATH_LEN];
    if (!BuildRealPath(absPath, realPath)) { // 1 exit, 0 -> ruta muy larga
        result->_statusCode = 414; //al crear ruta completa pasada por client, esta es muy larga -> culpa de client
        return 0;
    }

    // 2. stat() — una sola vez -> si no se puede obtener -> archivo/dir no existe -> 404
    struct stat pathStat;
    if (stat(realPath, &pathStat) != 0) {
        result->_statusCode = 404;
        return 0;
    }

    // 3. manejar directorio — puede modificar realPath y pathStat (1 si index, 0 si ~dir, -1 si no index)
    int dirResult = HandleDirectory(realPath, &pathStat);
    if (dirResult == -1) {
        result->_statusCode = 404; // Dir no tenia index.html -> not found
        return 0;
    }

    // 4. verificar que es archivo regular
    if (!S_ISREG(pathStat.st_mode)) {
        result->_statusCode = 404;
        return 0;
    }

    // si llega hasta acá, y era una ruta de dir, ya estamos trabajando con el index de esa dir
    // 5. Metadata
    GetMimeTypeByExtension(realPath, result->_mimeType);
    GetLastModified(&pathStat, result->_lastModified);

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
// Verifica que, la carpeta indicada en la uri, en donde nos piden hacer el post, exista
int CheckDirExists(const char* realDirPath) {
    struct stat pathStat;
    if (stat(realDirPath, &pathStat) != 0) return 0;
    if (!S_ISDIR(pathStat.st_mode)) return 0;
    return 1;
}

// Extrae la carpeta padre del path (busca el último '/')
// Ejm: "./www/uploads/archivo.txt" -> "./www/uploads"
// Retorna 1 si éxito, 0 si no encuentra '/' o ruta muy corta
int GetParentDir(const char* filePath, char* outParentDir) {

    // Buscar el último '/'
    const char* lastSlash = strrchr(filePath, '/');
    if (lastSlash == NULL) return 0; // No hay '/' → sin carpeta padre válida
    
    // copiamos en outparentDir
    int parentLen = lastSlash - filePath;     
    strncpy(outParentDir, filePath, parentLen);
    outParentDir[parentLen] = '\0';
    
    return 1;
}


int WriteFile(const char* realPath, const char* body, size_t bodyLen, int* outIsNew) {
    // verificar si el archivo ya existe
    struct stat pathStat;
    *outIsNew = (stat(realPath, &pathStat) != 0) ? 1 : 0; // si stat falla → archivo no existe → es nuevo

    // abrir en modo binario append
    // "ab" → crea si no existe, añade si existe
    FILE* file = fopen(realPath, "ab");
    if (file == NULL) return 0;

    // escribir body
    size_t bytesWritten = fwrite(body, 1, bodyLen, file);
    fclose(file);

    if (bytesWritten != bodyLen) return 0;
    
    return 1;
}