#include "FileManager.h"
#include "FileManagerTypes.h"
#include "FileManagerUtils.h"
#include "FileManagerIO.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdio.h>
// Heredados desde FileManagerIO.h (sys/stat.h, pthread.h)

// FUNCIÓN: solo orquesta, no hace trabajo sucio.
// Aclaración: absPath es ruta absoluta, que es desde raiz de sistema de archivos (sin .www/, realPath es con esta, entre otras cosas)

static pthread_mutex_t _writeMutex = PTHREAD_MUTEX_INITIALIZER;

// Inicializa el FileManager con la ruta raíz del documento
// Pide: root - ruta raíz del servidor
// Retorna: nada
void FileManagerInit(const char* root) {
    if (root != NULL) FileManagerSetRoot(root);
}

// Inicializa una estructura FileResult vacía
// Pide: nada
// Retorna: puntero a FileResult inicializado, NULL si falla malloc
static FileResult* InitResult(){
    FileResult* result = malloc(sizeof(FileResult));
    if (result == NULL) {
        return NULL;
    }

    result->_content = NULL;
    result->_contentLen = 0;
    result->_mimeType[0] = '\0';
    result->_lastModified[0] = '\0';
    result->_location[0] = '\0';
    result->_statusCode = 0;

    return result;

}

// Obtiene un archivo completo (contenido + metadata)
// Pide: absPath - ruta absoluta del archivo
// Retorna: FileResult con campos statusCode, mimeType, lastModified, content y contentLen
FileResult* FileGet(const char* absPath) {

    FileResult* result = InitResult();
    if (result == NULL) return NULL;

    char realPath[MAX_PATH_LEN];
    struct stat pathStat;

    if (!GetFileMetadata(absPath, realPath, &pathStat, result)) {
        return result;
    }

    if (!ReadFile(realPath, &pathStat, result)) {
        result->_statusCode = 500;
        return result;
    }

    result->_statusCode = 200;
    return result;
}

// Obtiene metadata del archivo sin leer su contenido
// Pide: absPath - ruta absoluta del archivo
// Retorna: FileResult con statusCode, mimeType, lastModified y contentLen (sin content)
FileResult* FileHead(const char* absPath) {

    FileResult* result = InitResult();
    if (result == NULL) return NULL;

    char realPath[MAX_PATH_LEN];
    struct stat pathStat;

    if(!GetFileMetadata(absPath, realPath, &pathStat, result)){
        return result;
    }

    result->_statusCode = 200;
    result->_contentLen = pathStat.st_size;
    return result;
}

// Escribe contenido en archivo (nuevo, si pasan carpeta,  o existente, si pasan archivo) 
// Pide: absPath - ruta del archivo o directorio; body - contenido; bodyLen - tamaño del contenido; contentType - tipo MIME
// Retorna: FileResult con statusCode (200/201), y location si es nuevo (201)
FileResult* FilePost(const char* absPath, const char* body, size_t bodyLen, const char* contentType) {

    FileResult* result = InitResult();
    if (result == NULL) return NULL;

    // 1. construir ruta real (añadimos ....www)
    char realPath[MAX_PATH_LEN];
    if (!BuildRealPath(absPath, realPath)) {
        result->_statusCode = 414;
        return result;
    }
    
    int isNew = 0;
    struct stat pathStat;
    
    // CASO 1: Ruta existe y es directorio → generar nombre aleatorio -> solo s_isdir si stat retorna 0 -> no riesgo de comportamiento innesaperado
    if (stat(realPath, &pathStat) == 0 && S_ISDIR(pathStat.st_mode)) {

        char fileName[MAX_PATH_LEN];
        pthread_mutex_lock(&_writeMutex);
        if(!GenerateFileName(contentType, fileName)){
            result->_statusCode = 500;
            return result;
        }

        // Construir ruta completa del archivo nuevo (ejm: ./www/uploads/1714392000123.html)
        char newFilePath[MAX_PATH_LEN];
        snprintf(newFilePath, MAX_PATH_LEN, "%s/%s", realPath, fileName);
        
        if (!WriteFile(newFilePath, body, bodyLen, &isNew)) {
            pthread_mutex_unlock(&_writeMutex);
            result->_statusCode = 500;
            return result;
        }
        
        pthread_mutex_unlock(&_writeMutex);
        
        if (!EnsureTrailingSlash(realPath)) {
            result->_statusCode = 414;
            return result;
        }
        // Llenar location con ruta de nuevo archivo
        snprintf(result->_location, MAX_PATH_LEN, "%s%s", absPath, fileName);
    }
    // CASO 2: Archivo existente o nuevo (ruta general puede no existir, pero carpeta padre debe existir)
    else {
        // Verificar que la carpeta padre existe
        char parentDirPath[MAX_PATH_LEN];
        if (!GetParentDir(realPath, parentDirPath) || !CheckDirExists(parentDirPath)) {
            result->_statusCode = 404;
            return result;
        }
        
        // WriteFile crea el archivo si no existe, o append si existe
        if (!WriteFile(realPath, body, bodyLen, &isNew)) {
            result->_statusCode = 500;
            return result;
        }

        if (isNew){ 
            snprintf(result->_location, MAX_PATH_LEN, "%s", absPath); 
        }
    }

    result->_statusCode = isNew ? 201 : 200;
    return result;
}

// Libera la memoria asociada a un FileResult
// Pide: result - puntero a FileResult
// Retorna: nada
void FileResultFree(FileResult* result) {
    if (result == NULL) return;
    if (result->_content != NULL) {
        free(result->_content);
        result->_content = NULL;
    }
    free(result);
}

// getters
int            FileResultGetStatusCode(const FileResult* r)   { return r->_statusCode; }
const char*    FileResultGetMimeType(const FileResult* r)     { return r->_mimeType; }
const char*    FileResultGetLastModified(const FileResult* r) { return r->_lastModified; }
const char*    FileResultGetLocation(const FileResult* r)     { return r->_location; }
unsigned char* FileResultGetContent(const FileResult* r)      { return r->_content; }
size_t         FileResultGetContentLen(const FileResult* r)   { return r->_contentLen; }
