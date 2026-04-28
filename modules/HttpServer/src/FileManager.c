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

// Orquesta todo lo del get
FileResult* FileGet(const char* absPath) {
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

    // 1. construir ruta real
    char realPath[MAX_PATH_LEN];
    if (!BuildRealPath(absPath, realPath)) { // 1 exit, 0 -> ruta muy larga
        result->_statusCode = 400;
        return result;
    }

    // 2. stat() — una sola vez -> si no se puede obtener -> archivo/dir no existe -> 404
    struct stat pathStat;
    if (stat(realPath, &pathStat) != 0) {
        result->_statusCode = 404;
        return result;
    }

    // 3. manejar directorio — puede modificar realPath y pathStat (1 si index, 0 si ~dir, -1 si no index)
    int dirResult = HandleDirectory(realPath, &pathStat);
    if (dirResult == -1) {
        result->_statusCode = 403; // Dir no tenia index.html
        return result;
    }

    // 4. verificar que es archivo regular
    if (!S_ISREG(pathStat.st_mode)) {
        result->_statusCode = 403;
        return result;
    }
    // si llega hasta acá, y era una ruta de dir, ya estamos trabajando con el index de esa dir
    // 5. MIME type
    GetMimeType(realPath, result->_mimeType);

    // 6. Last-Modified — reutiliza pathStat
    GetLastModified(&pathStat, result->_lastModified);

    // 7. leer archivo
    if (!ReadFile(realPath, &pathStat, result)) {
        result->_statusCode = 500;
        return result;
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