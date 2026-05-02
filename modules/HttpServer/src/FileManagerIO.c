#include "FileManagerIO.h"
#include "FileManagerTypes.h"
#include "FileManagerUtils.h"
#include "FileManager.h"
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
// para debugg
#include <errno.h>
#include <unistd.h>

// Heredados desde FileManagerIO.h (sys/stat.h, stddef.h)

// FUNCIÓN: funciones que abren, leen y escriben archivos:

// Verifica si url es una carpeta, si sí, busca si tiene index.html, y actualiza realPath si corresponde
// retorna 1 si es directorio y encontró index.html
// retorna 0 si no es directorio (no hace nada)
// retorna -1 si es directorio pero no hay index.html
// retorna -2 si uri too long
// outsat es el stat() de un archivo o dir, que contiene info básica de él, dado por el OS
int HandleDirectory(char* realPath, struct stat* outStat) {
    printf("llegué a handledirectory en ws\n");
    // ¿es un directorio?
    printf("hdir realpath llega: %s\n", realPath);

    if (!S_ISDIR(outStat->st_mode)) return 0;
    printf("es dir\n");
    // es directorio → buscar index.html
    char indexPath[MAX_PATH_LEN];
    int  pathLen = strlen(realPath);

    // Construir la ruta con index
    int written;
    if (realPath[pathLen - 1] == '/') {
        written = snprintf(indexPath, MAX_PATH_LEN, "%sindex.html", realPath); // concatenamos real path con index.html, y guardamos en indexPath
    } else {
        written = snprintf(indexPath, MAX_PATH_LEN, "%s/index.html", realPath);
    }

    // Verificar si fue truncada
    if (written < 0 || written >= MAX_PATH_LEN) {
        printf("ruta muy larga");
        return -2; // Ruta demasiado larga
        // no se daña realPath porque se "copio" nueva ruta en indexPath, que no ha sido asignada a realPath
    }

    // stat() del index.html → sobreescribe outStat
    if (stat(indexPath, outStat) != 0) return -1;
    printf("indexPath existe");

    if (!S_ISREG(outStat->st_mode)) return -1;

    printf("indexPath es una carpeta");

    // actualizar realPath
    strncpy(realPath, indexPath, MAX_PATH_LEN - 1);
    realPath[MAX_PATH_LEN - 1] = '\0';

    printf("hdir realpath queda, despues de copy indexpath: %s\n", realPath);
    
    return 1;
}

int GetFileMetadata(const char* absPath, char* outRealPath, struct stat* outStat, FileResult* result){
    printf("llegué a getfilemetadata en ws\n");
   
    // 1. construir ruta real
    //char realPath[MAX_PATH_LEN];
    if (!BuildRealPath(absPath, outRealPath)) { // 1 exit, 0 -> ruta muy larga
        printf("falló buildrealpath en ws\n");
        result->_statusCode = 414; //al formar ruta completa pasada por client, esta es muy larga -> culpa de client
        return 0;
    }
    
    printf("abspath que llega %s, realpath queda: %s\n", absPath, outRealPath);

    // 2. stat() — una sola vez -> si no se puede obtener -> archivo/dir no existe -> 404
    //struct stat pathStat;
    if (stat(outRealPath, outStat) != 0) {
        printf("archivo no existe en ws\n");
        result->_statusCode = 404;
        return 0;
    }

    // 3. manejar directorio — puede modificar realPath y pathStat (1 si index, 0 si ~dir, -1 si no index, -2 si uri too long)
    int dirResult = HandleDirectory(outRealPath, outStat);
    if (dirResult == -1) {
        printf("dir no tiene index.html en ws\n");
        result->_statusCode = 404; // Dir no tenia index.html -> not found
        return 0;
    } else if (dirResult == -2){
        printf("falló uri too long en ws\n");
        result->_statusCode = 414; // uri too long
        return 0;
    }

    // 4. verificar que es archivo regular
    if (!S_ISREG(outStat->st_mode)) {
        printf("no es archivo reg en ws\n");
        result->_statusCode = 404;
        return 0;
    }

    // si llega hasta acá, y era una ruta de dir, ya estamos trabajando con el index de esa dir
    // 5. Metadata
    GetMimeTypeByExtension(outRealPath, result->_mimeType); // porque retornaré recurso solicitado, no html message
    GetLastModified(outStat, result->_lastModified);
    
    printf("path que sale de getmetadata %s\n", outRealPath);

    return 1;

}

// pathStat es la info del archivo
int ReadFile(const char* realPath, const struct stat* pathStat, FileResult* result) {
    // tamaño exacto del archivo desde stat
    printf("entre a readfile en ws\n");
    size_t fileSize = pathStat->st_size;

    // abrir archivo en modo binario
    printf("path que llega a readfile, realpath: %s\n", realPath);
    FILE* file = fopen(realPath, "rb");
    if (file == NULL){
        printf("file null");
        return 0;
    } 
        printf("file  no null");

    // malloc del tamaño exacto
    result->_content = malloc(fileSize); 
    if (result->_content == NULL) {// problema de memoria (malloc falló)
        printf("falló malloc en readfile en ws\n");
        fclose(file);
        return 0;
    }

    // leer todos los bytes de una vez
    size_t bytesRead = fread(result->_content, 1, fileSize, file);
    fclose(file);

    // verificar que leímos todo
    if (bytesRead != fileSize) {
        printf("falló leer todo en ws\n");
        free(result->_content);
        result->_content = NULL;
        // contentLen es por default 0, por eso no se pone
        return 0;
    }

    result->_contentLen = fileSize;
    return 1;
}

// --------------------- POST -------------------------------
// Verifica que, la carpeta indicada en la uri, en donde nos piden hacer el post, exista
int CheckDirExists(const char* realDirPath) {
    printf("entré a checkdirexists en ws\n");

    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    printf("cwd: %s\n", cwd);
    struct stat pathStat;
    //if (stat(realDirPath, &pathStat) != 0) return 0;
    if (stat(realDirPath, &pathStat) != 0) {
        printf("stat falló: %s\n", strerror(errno));
        return 0;
    }
    printf("archivo existe en ws\n");
    if (!S_ISDIR(pathStat.st_mode)) return 0;
    printf("archivo es dir exists en ws\n");
    return 1;
}

// Extrae la carpeta padre del path (busca el último '/')
// Ejm: "./www/uploads/archivo.txt" -> "./www/uploads"
// Retorna 1 si éxito, 0 si no encuentra '/' o ruta muy corta
int GetParentDir(const char* filePath, char* outParentDir) {
    printf("entré a getparentdir en ws\n");
    // Buscar el último '/'
    const char* lastSlash = strrchr(filePath, '/');
    printf("filepath que llega %s\n", filePath);
    if (lastSlash == NULL) return 0; // No hay '/' → sin carpeta padre válida
    printf("encontré ultimo / en ws\n");
    // copiamos en outparentDir
    int parentLen = lastSlash - filePath;     
    strncpy(outParentDir, filePath, parentLen);
    outParentDir[parentLen] = '\0';
    printf("outparendir quedó %s\n", outParentDir);
    
    return 1;
}


int WriteFile(const char* realPath, const char* body, size_t bodyLen, int* outIsNew) {
    printf("entré a writefile en ws\n");
    // verificar si el archivo ya existe

    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    printf("cwd: %s\n", cwd);
    printf("realPath: %s\n", realPath);
    struct stat pathStat;
    *outIsNew = (stat(realPath, &pathStat) != 0) ? 1 : 0; // si stat falla → archivo no existe → es nuevo
    printf("isNew? %d\n", *outIsNew);
    // abrir en modo binario append
    // "ab" → crea si no existe, añade si existe
    FILE* file = fopen(realPath, "ab");
    if (file == NULL) return 0;

    // escribir body
    size_t bytesWritten = fwrite(body, 1, bodyLen, file);
    fflush(file);
    fsync(fileno(file));
    fclose(file);

    if (bytesWritten != bodyLen) return 0;
    printf("todo bien en writefile\n");
    return 1;
}