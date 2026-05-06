#define _POSIX_C_SOURCE 200112L // para clock_realtime

#include "FileManagerUtils.h"
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <stdlib.h>

typedef struct {
    const char* extension;
    const char* mimeType;
} MimeEntry;

static const MimeEntry MIME_TABLE[] = {
    { ".html", "text/html" },
    { ".htm",  "text/html" },
    { ".css",  "text/css" },
    { ".js",   "application/javascript" },
    { ".json", "application/json" },
    { ".png",  "image/png" },
    { ".jpg",  "image/jpeg" },
    { ".jpeg", "image/jpeg" },
    { ".gif",  "image/gif" },
    { ".txt",  "text/plain" },
    { ".pdf",  "application/pdf" },
    { ".xml",  "application/xml" },
    { NULL, NULL }  // centinela — marca el fin de la tabla
};

static char _documentRoot[MAX_PATH_LEN] = "../modules/HttpServer/www";

// Establece la ruta raíz del documento
// Pide: root - nueva ruta raíz
// Retorna: nada
void FileManagerSetRoot(const char* root) {
    strncpy(_documentRoot, root, MAX_PATH_LEN - 1);
    _documentRoot[MAX_PATH_LEN - 1] = '\0';
}

// Construye la ruta real del archivo (concatena root document con absPath)
// Pide: absPath - ruta absoluta del cliente; outPath - buffer para la ruta real
// Retorna: 1 si éxito, 0 si la ruta resultante excede MAX_PATH_LEN
int BuildRealPath(const char* absPath, char* outPath) {

    int rootLen = strlen(_documentRoot);
    int pathLen = strlen(absPath);

    if (rootLen + pathLen >= MAX_PATH_LEN) return 0;

    // construir ruta real
    strncpy(outPath, _documentRoot, MAX_PATH_LEN - 1);
    strncat(outPath, absPath, MAX_PATH_LEN - rootLen - 1);
    outPath[MAX_PATH_LEN - 1] = '\0';

    return 1;
}

// Obtiene el tipo MIME basado en la extensión del archivo
// Pide: path - ruta del archivo; outMime - buffer para el tipo MIME
// Retorna: nada (outMime contiene el tipo MIME o "application/octet-stream" por defecto)
void GetMimeTypeByExtension(const char* path, char* outMime) {

    const char* ext = strrchr(path, '.');
    
    if (ext != NULL) {
        for (int i = 0; MIME_TABLE[i].extension != NULL; i++) {
            if (strcasecmp(ext, MIME_TABLE[i].extension) == 0) {
                strncpy(outMime, MIME_TABLE[i].mimeType, 63);
                outMime[63] = '\0';
                return;
            }
        }
    }

    strncpy(outMime, "application/octet-stream", 63);
    outMime[63] = '\0';
}

// Genera un nombre de archivo aleatorio basado en timestamp, random byte y extensión
// Pide: contentType - tipo MIME para determinar la extensión; outFileName - buffer para el nombre
// Retorna: 1 si éxito, 0 si el nombre resultante excede MAX_PATH_LEN
int GenerateFileName(const char* contentType, char* outFileName) {

    const char* ext = ".bin";  // default

    // generamos extensión (basado en contenttype)
    for (int i = 0; MIME_TABLE[i].mimeType != NULL; i++) {
        if (strcasecmp(contentType, MIME_TABLE[i].mimeType) == 0) {
            ext = MIME_TABLE[i].extension;
            break;
        }
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int randomByte = rand() % 256;

    int written = snprintf(outFileName, MAX_PATH_LEN, "%ld%ld%d%s", (long)ts.tv_sec, (long)ts.tv_nsec, randomByte, ext);
    if (written < 0 || written >= MAX_PATH_LEN) {
        outFileName[0] = '\0';
        return 0;
    }

    return 1;
}

// Asegura que una ruta termina con barra inclinada
// Pide: path - ruta a modificar
// Retorna: 1 si éxito, 0 si no hay espacio para agregar la barra
int EnsureTrailingSlash(char* path) {
    int len = strlen(path);

    if (len > 0 && path[len - 1] == '/') return 1;

    // verificar que cabe el slash extra
    if (len + 1 >= MAX_PATH_LEN) return 0;

    path[len]     = '/';
    path[len + 1] = '\0';
    return 1;
}

// Obtiene la fecha de última modificación de un archivo en formato HTTP
// Pide: pathStat - estructura stat del archivo; outDate - buffer para la fecha
// Retorna: nada (outDate contiene la fecha o vacío si hay error)
void GetLastModified(const struct stat* pathStat, char* outDate) {
    struct tm* tm = gmtime(&pathStat->st_mtime);

    if (tm == NULL) {
        outDate[0] = '\0';
        return;
    }
    strftime(outDate, 64, "%a, %d %b %Y %H:%M:%S GMT", tm);
}
