#include "FileManager.h"
#include "FileManagerTypes.h"
#include "FileManagerUtils.h"
#include "FileManagerIO.h"
#include <stdlib.h> // para malloc()
#include <string.h> // para strncpy()
#include <sys/stat.h> // para stat()..

// FUNCIÓN: solo orquesta, no hace trabajo sucio:

// Inicialización configurable
void FileManagerInit(const char* root) {
    if (root != NULL) FileManagerSetRoot(root);
}

FileResult* InitResult(){
    FileResult* result = malloc(sizeof(FileResult));
    if (result == NULL) {
        // malloc falló
        return NULL;
    }

    // inicializar
    result->_content = NULL;
    result->_contentLen = 0;
    result->_mimeType[0] = '\0';
    result->_lastModified[0] = '\0';
    result->_location[0] = '\0';
    result->_isNewResource = 0;
    result->_statusCode = 200;

    return result;

}

// Orquesta todo lo del get
FileResult* FileGet(const char* absPath) {
   
    FileResult* result = InitResult();
    if (result == NULL) return NULL;

    char realPath[MAX_PATH_LEN];
    struct stat pathStat;

    if (!GetFileMetadata(absPath, realPath, &pathStat, result)) {
        return result;  // statusCode ya seteado adentro
    }

    // GET lee el body
    if (!ReadFile(realPath, &pathStat, result)) {
        result->_statusCode = 500;
    }

    return result;
}

FileResult* FileHead(const char* absPath) {
    FileResult* result = InitResult();
    if (result == NULL) return NULL;

    char realPath[MAX_PATH_LEN];
    struct stat pathStat;

    // HEAD solo necesita metadata — no llama ReadFile
    GetFileMetadata(absPath, realPath, &pathStat, result);

    // contentLen sí debe estar presente en HEAD
    // para que Response pueda construir Content-Length correcto
    if (result->_statusCode == 200) {
        // RFC 9.4 dice que Content-Length debe ser el mismo que tendría un GET equivalente 
        result->_contentLen = pathStat.st_size;
    }

    return result;
}

// Orquesta todo del post
FileResult* FilePost(const char* absPath, const char* body, size_t bodyLen) {
    FileResult* result = malloc(sizeof(FileResult));
    if (result == NULL) return NULL;

    // inicializar
    result->_content = NULL;
    result->_contentLen = 0;
    result->_mimeType[0] = '\0';
    result->_lastModified[0] = '\0';
    result->_location[0] = '\0';
    result->_isNewResource = 0;
    result->_statusCode = 200;

    // validar body (que no este vacio)
    if (body == NULL || bodyLen == 0) {
        result->_statusCode = 400;
        return result;
    }

    // 1. construir ruta real
    char realPath[MAX_PATH_LEN];
    if (!BuildRealPath(absPath, realPath)) {
        result->_statusCode = 400;
        return result;
    }

    // 2. verificar directorio padre
    if (!CheckParentDirExists(realPath)) {
        result->_statusCode = 404;
        return result;
    }

    // 3. escribir archivo
    int isNew = 0;
    if (!WriteFile(realPath, body, bodyLen, &isNew)) {
        result->_statusCode = 500;
        return result;
    }

    // 4. llenar resultado
    result->_isNewResource = isNew;
    result->_statusCode    = isNew ? 201 : 200;

    // 5. Location header → apunta al absPath del recurso
    strncpy(result->_location, absPath, MAX_PATH_LEN - 1);
    result->_location[MAX_PATH_LEN - 1] = '\0';

    return result;
}

void FileResultFree(FileResult* result) {
    if (result == NULL) return;
    if (result->_content != NULL) {
        free(result->_content);
        result->_content = NULL;
    }
    free(result);
}