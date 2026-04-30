#include "FileManager.h"
#include "FileManagerTypes.h"
#include "FileManagerUtils.h"
#include "FileManagerIO.h"
#include <stdlib.h> // para malloc()
#include <string.h> // para strncpy()
#include <sys/stat.h> // para stat()..
#include <pthread.h>

static pthread_mutex_t _writeMutex = PTHREAD_MUTEX_INITIALIZER;
// FUNCIÓN: solo orquesta, no hace trabajo sucio
//absPath es ruta absoluta, que es desde raiz de sistema de archivos (sin .www/, realPath es con esta, entre otras cosas)

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

    // TO DO ----------------------------------------------- SE MANDA SIEMPRE, INCLUSO SI FALLA 
    if (result->_statusCode == 200) { //si archivo no existe, o carpeta sin index, da codigo diferente -> ~contentLen
        // RFC 9.4 dice que Content-Length debe ser el mismo que tendría un GET equivalente 
        result->_contentLen = pathStat.st_size;
    }

    return result;
}

// Orquesta todo del post (path de post es una carpeta, para crear archivo ahi, o un archivo a modificar)
FileResult* FilePost(const char* absPath, const char* body, size_t bodyLen, const char* contentType) {
    FileResult* result = InitResult();
    if (result == NULL) return NULL;
    
    // validar body (que no este vacio)
    // to do --------------------- LAURA YA LO HACE
    if (body == NULL || bodyLen == 0) {
        result->_statusCode = 400;
        return result;
    }

    // 1. construir ruta real (añadimos .www)
    char realPath[MAX_PATH_LEN];
    if (!BuildRealPath(absPath, realPath)) {
        result->_statusCode = 414; //uri too long
        return result;
    }

    // 2. verificar qué exista, sea lo que sea (archivo o directorio)
    struct stat pathStat;
    if (stat(realPath, &pathStat) != 0) {
        result->_statusCode = 404; // ESO NO LO PUEDE MANEJAR LAURA
        return result;
    }
    
    int isNew = 0;
    result->_contentLen = 0; // para todos los casos, será 0, pues no devolveremos entidad descriptiva (json, o html)

    // 3. CASO 1: es un directorio → generar nombre y crear archivo nuevo
    if (S_ISDIR(pathStat.st_mode)) {
        
        // 3. generar nombre de archivo -> 1714392000123.html
        char fileName[256];

        pthread_mutex_lock(&_writeMutex); // TODO -------- usar tambien generaodr de random -> unit8 para un byte -> porque dos servers al mismo tiempo
        GenerateFileName(contentType, fileName); //Porque pueden tener mismo timestamp, poco probable, pero ajá

        // 4. construir ruta completa del archivo nuevo (ejm: ./www/uploads/1714392000123.html)
        //TODO ------- post si esun archivo, debo verificar si existe o no, si sí, append, si no, crear, para poder replicador servir
        char newFilePath[MAX_PATH_LEN];
        snprintf(newFilePath, MAX_PATH_LEN, "%s/%s", realPath, fileName);

        // 3. escribir archivo
        if (!WriteFile(newFilePath, body, bodyLen, &isNew)) {
            pthread_mutex_unlock(&_writeMutex);
            result->_statusCode = 500;
            return result;
        }
        
         pthread_mutex_unlock(&_writeMutex); // despues de escribir, porque si dos hilos tienen diferencia menor a un nanosegundo, generarán mismo nombre -> editarán él uno al otro
        // 6. construir location → URI del nuevo recurso
        // Ejm: dirPath = "/uploads" + "/" + fileName = "/uploads/1714392000123.html"
        snprintf(result->_location, MAX_PATH_LEN, "%s/%s", absPath, fileName);
    }

    else if (S_ISREG(pathStat.st_mode)) {

        if (!WriteFile(realPath, body, bodyLen, &isNew)) { // es modificar archivo de uri
            result->_statusCode = 500;
            return result;
        }

        // 5. Location header vacio porque como fue edición, no hay que mandarlo
        result->_location[0] = '\0';
        
   
    } else {
        // No es ni dir ni archivo
        result->_statusCode = 403; // Forbidden -> tampoco lo puede hacer Lau
        return result;
    }

    // 4. llenar resultado
    result->_isNewResource = isNew;
    result->_statusCode = isNew ? 201 : 200;
 
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