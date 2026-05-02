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

static pthread_mutex_t _writeMutex = PTHREAD_MUTEX_INITIALIZER;
// FUNCIÓN: solo orquesta, no hace trabajo sucio
//absPath es ruta absoluta, que es desde raiz de sistema de archivos (sin .www/, realPath es con esta, entre otras cosas)

// Inicialización configurable
void FileManagerInit(const char* root) {
    if (root != NULL) FileManagerSetRoot(root);
}

static FileResult* InitResult(){
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
    result->_statusCode = 0;

    return result;

}

// Orquesta todo lo del get
// Headers que llena: 
//getMetadata: status, si existe url -> contenttype, last_modified
// read: si exite url -> content, contentlen
FileResult* FileGet(const char* absPath) {
    printf("llegué a fileget en ws\n");
   
    FileResult* result = InitResult();
    if (result == NULL) return NULL;

    char realPath[MAX_PATH_LEN];
    struct stat pathStat;

    if (!GetFileMetadata(absPath, realPath, &pathStat, result)) {
        printf("falló en getfilemetada, fileget en ws\n");

        return result;  // statusCode ya seteado adentro si error
    }
    printf("ANTES DE READFILE: %s\n", realPath);

    // GET lee el body
    if (!ReadFile(realPath, &pathStat, result)) {
        printf("falló en readfile, fileget en ws\n");
        result->_statusCode = 500;
        return result;
    }

    result->_statusCode = 200;
    printf("ok fileget\n");

    return result;
}

// si archivo no existe me llena solo su statuscode
FileResult* FileHead(const char* absPath) {
    printf("llegué a filehead en ws\n");

    FileResult* result = InitResult();
    if (result == NULL) return NULL;

    char realPath[MAX_PATH_LEN];
    struct stat pathStat;

    // HEAD solo necesita metadata — no llama ReadFile
    if(!GetFileMetadata(absPath, realPath, &pathStat, result)){
        printf("falló en getfilemetada, filehead en ws\n");

        return result;  // statusCode ya seteado adentro si error
    }

    result->_statusCode = 200;
    printf("AAAAAA STATUS CODE: %d\n", result->_statusCode);
    printf("filehead status code: %d\n", result->_statusCode);
    result->_contentLen = pathStat.st_size;
    printf("ok filehead\n");

    return result;
}

// Orquesta todo del post (path de post es una carpeta, para crear archivo ahi, o un archivo a modificar/crear)
// atributos que asigna a fileresult (no headers):
// statusCode, si 201 -> location, 
// atributos que no: content, contentLen, mime, last_mod, si 200 -> location, 
FileResult* FilePost(const char* absPath, const char* body, size_t bodyLen, const char* contentType) {
    printf("llegué a filepost en ws\n");
    FileResult* result = InitResult();
    if (result == NULL) return NULL;

    // 1. construir ruta real (añadimos .www)
    char realPath[MAX_PATH_LEN];
    if (!BuildRealPath(absPath, realPath)) {
        printf("falle buildpath en filepost\n");

        result->_statusCode = 414; //uri too long
        return result;
    }
    
    printf("abspath que llega %s, realpath queda: %s\n", absPath, realPath);
    int isNew = 0;

    // 2. stat() para ver si es directorio
    struct stat pathStat;
    
    // CASO 1: Ruta existe y es directorio → generar nombre aleatorio -> solo s_isdir si stat retorna 0 -> no riesgo de comportamiento innesaperado
    if (stat(realPath, &pathStat) == 0 && S_ISDIR(pathStat.st_mode)) {
        printf("ruta existe y soy un dir\n");

        char fileName[MAX_PATH_LEN];
        pthread_mutex_lock(&_writeMutex);
        if(!GenerateFileName(contentType, fileName)){
            printf("falle generatename en filepost\n");
            result->_statusCode = 500; // error de server porque fue por limitaciones nuestras que no pudimos procesar la peticion -> nueva uri muy larga
            return result;
        } //mutex porque dos workers pueden tener mismo timestamp, poco probable, pero ajá

        // 4. construir ruta completa del archivo nuevo (ejm: ./www/uploads/1714392000123.html)
        char newFilePath[MAX_PATH_LEN];
        snprintf(newFilePath, MAX_PATH_LEN, "%s/%s", realPath, fileName);
        
        if (!WriteFile(newFilePath, body, bodyLen, &isNew)) {
            printf("falle writefile en filepost\n");

            pthread_mutex_unlock(&_writeMutex);
            result->_statusCode = 500;
            return result;
        }
        
        pthread_mutex_unlock(&_writeMutex); // despues de escribir, porque si dos hilos tienen diferencia menor a un nanosegundo, generarán mismo nombre -> editarán él uno al otro
        
        // 6. construir location → URI del nuevo recurso
        // Ejm: dirPath = "/uploads" + "/" + fileName = "/uploads/1714392000123.html" -> sin .www/
        snprintf(result->_location, MAX_PATH_LEN, "%s/%s", absPath, fileName); 
    }
    // CASO 2: Archivo existente o nuevo (ruta general puede no existir, pero carpeta padre debe existir)
    else {
        printf("no soy dir en filepost\n");
        // Verificar que la carpeta padre existe
        char parentDirPath[MAX_PATH_LEN];
        if (!GetParentDir(realPath, parentDirPath) || !CheckDirExists(parentDirPath)) {
            printf("falle getparentdir || checkdirexist en filepost\n");
            
            result->_statusCode = 404;
            return result;
        }
        
        // WriteFile crea el archivo si no existe, o append si existe
        if (!WriteFile(realPath, body, bodyLen, &isNew)) {
            printf("falle writefile en filepost, soy archivo\n");
            result->_statusCode = 500;
            return result;
        }

        if (isNew){ // aunque me den el nombre, debo devolver el location porque 201
            snprintf(result->_location, MAX_PATH_LEN, "%s", absPath); 
        }
    }

    // 4. llenar resultado
    result->_statusCode = isNew ? 201 : 200;
    printf("oj filepost\n");

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


// getter
int            FileResultGetStatusCode(const FileResult* r)   { return r->_statusCode; }
const char*    FileResultGetMimeType(const FileResult* r)     { return r->_mimeType; }
const char*    FileResultGetLastModified(const FileResult* r) { return r->_lastModified; }
const char*    FileResultGetLocation(const FileResult* r)     { return r->_location; }
unsigned char* FileResultGetContent(const FileResult* r)      { return r->_content; }
size_t         FileResultGetContentLen(const FileResult* r)   { return r->_contentLen; }
