#include "FileManager.h"
#include "../../../Shared/http.h"
#include <sys/stat.h>  // para stat()
#include <time.h>  // para strftime, gmtime
#include <stdio.h>   // para fopen, fread, fclose
#include <stdlib.h>  // para malloc
#include <string.h>


#define MAX_PATH_LEN 1024

static char _documentRoot[MAX_PATH_LEN] = "./www";

struct FileResult {
    // datos del archivo
    char*  _content;           // body a enviar
    size_t _contentLen;        // → Content-Length

    // headers que response.c necesita construir
    char _mimeType[64];      // → Content-Type
    char _lastModified[64];  // → Last-Modified
    char _location[1024];    // → Location (POST 201)
    int  _isNewResource;     // → decide si es 200 o 201

    // control de errores
    int _statusCode;        // 200, 201, 400, 403, 404, 500
};

// Inicialización configurable
void FileManagerInit(const char* root) {
    if (root != NULL) {
        strncpy(_documentRoot, root, MAX_PATH_LEN - 1);
        _documentRoot[MAX_PATH_LEN - 1] = '\0';
    }
}

// Genera path completo, añadiendo ./www al absPath -> retorna 1 si éxito, 0 si falla
static int BuildRealPath(const char* absPath, char* outPath) {
    int rootLen = strlen(_documentRoot);
    int pathLen = strlen(absPath);

    // verificar que no superamos el límite
    if (rootLen + pathLen >= MAX_PATH_LEN) return 0;

    // construir ruta real
    strncpy(outPath, _documentRoot, MAX_PATH_LEN - 1);
    strncat(outPath, absPath, MAX_PATH_LEN - rootLen - 1);
    outPath[MAX_PATH_LEN - 1] = '\0';

    return 1;
}

// Obtener tipo del body request
static void GetMimeType(const char* path, char* outMime) {
    // buscar el último punto en el path
    const char* ext = strrchr(path, '.');

    if (ext == NULL) {
        // sin extensión -> tipo genérico binario
        strncpy(outMime, "application/octet-stream", 63);
        outMime[63] = '\0';
        return;
    }

    // comparar extensión (case-insensitive)
    if      (strcasecmp(ext, ".html") == 0) strncpy(outMime, "text/html", 63);
    else if (strcasecmp(ext, ".htm")  == 0) strncpy(outMime, "text/html", 63);
    else if (strcasecmp(ext, ".css")  == 0) strncpy(outMime, "text/css", 63);
    else if (strcasecmp(ext, ".js")   == 0) strncpy(outMime, "application/javascript", 63);
    else if (strcasecmp(ext, ".json") == 0) strncpy(outMime, "application/json", 63);
    else if (strcasecmp(ext, ".png")  == 0) strncpy(outMime, "image/png", 63);
    else if (strcasecmp(ext, ".jpg")  == 0) strncpy(outMime, "image/jpeg", 63);
    else if (strcasecmp(ext, ".jpeg") == 0) strncpy(outMime, "image/jpeg", 63);
    else if (strcasecmp(ext, ".gif")  == 0) strncpy(outMime, "image/gif", 63);
    else if (strcasecmp(ext, ".txt")  == 0) strncpy(outMime, "text/plain", 63);
    else if (strcasecmp(ext, ".pdf")  == 0) strncpy(outMime, "application/pdf", 63);
    else if (strcasecmp(ext, ".xml")  == 0) strncpy(outMime, "application/xml", 63);
    else    strncpy(outMime, "application/octet-stream", 63);

    outMime[63] = '\0';
}



// retorna 1 si es directorio y encontró index.html
// retorna 0 si no es directorio (no hace nada)
// retorna -1 si es directorio pero no hay index.html
// Verifica si url es una carpeta, si sí, busca si tiene index.html
// outsat es el stat() de un archivo o dir, que contiene info básica de él, dado por el OS
static int HandleDirectory(char* realPath, struct stat* outStat) {
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

// Obtener fecha de ultima modificación de recurso solicitado
static void GetLastModified(const struct stat* pathStat, char* outDate) {
    struct tm* tm = gmtime(&pathStat->st_mtime);
    if (tm == NULL) {
        outDate[0] = '\0';
        return;
    }
    strftime(outDate, 64, "%a, %d %b %Y %H:%M:%S GMT", tm);
}

// pathStat es la info del archivo
static int ReadFile(const char* realPath, const struct stat* pathStat, FileResult* result) {
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
    if (!BuildRealPath(absPath, realPath)) {
        result->_statusCode = 400;
        return result;
    }

    // 2. stat() — una sola vez -> si no se puede obtener -> archivo no existe -> 404
    struct stat pathStat;
    if (stat(realPath, &pathStat) != 0) {
        result->_statusCode = 404;
        return result;
    }

    // 3. manejar directorio — puede modificar realPath y pathStat
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

// --------------------- POST -------------------------------
static int CheckParentDirExists(const char* realPath) {
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

static int WriteFile(const char* realPath, const char* body, size_t bodyLen, int* outIsNew) {
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