#define _POSIX_C_SOURCE 200112L // para clock_Realtime

#include "FileManagerUtils.h"
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <stdlib.h>

// FUNCIÓN: funciones que trabajan con strings y metadata, no tocan contenido del archivo
// _documentRoot es local a este módulo (static)

typedef struct {
    const char* extension;
    const char* mimeType;
} MimeEntry; // "dict" para tipo de dato segun extensión, y viceversa

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

static char _documentRoot[MAX_PATH_LEN] = "./modules/HttpServer/www";

void FileManagerSetRoot(const char* root) {
    strncpy(_documentRoot, root, MAX_PATH_LEN - 1);
    _documentRoot[MAX_PATH_LEN - 1] = '\0';
}

// Genera path completo, añadiendo ./www al absPath -> retorna 1 si éxito, 0 si falla (porque ruta muy larga)
int BuildRealPath(const char* absPath, char* outPath) {

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

// Obtener tipo del body request con method get o head, puesto que estos no traen obligatoriamente content-type
void GetMimeTypeByExtension(const char* path, char* outMime) {

    // buscar el último punto en el path
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

// Generamos nombre de archivo a crear con post, usando el timestamp, random byte y la extensión
// Retorna 1 si éxito, 0 si el nombre excede MAX_PATH_LEN
int GenerateFileName(const char* contentType, char* outFileName) {

    const char* ext = ".bin";  // default

    // generamos extensión (basado en contenttype, no en uri)
    for (int i = 0; MIME_TABLE[i].mimeType != NULL; i++) {
        if (strcasecmp(contentType, MIME_TABLE[i].mimeType) == 0) {
            ext = MIME_TABLE[i].extension;
            break;
        }
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int randomByte = rand() % 256;  // Genera byte aleatorio (0-255)
    // verificamos que nueva url no exceda max_path_len porque habria desborde de buffer
    int written = snprintf(outFileName, MAX_PATH_LEN, "%ld%ld%d%s", (long)ts.tv_sec, (long)ts.tv_nsec, randomByte, ext);
    if (written < 0 || written >= MAX_PATH_LEN) {
        outFileName[0] = '\0';
        return 0;
    }
    return 1;
}

// Obtener fecha de ultima modificación de recurso solicitado
void GetLastModified(const struct stat* pathStat, char* outDate) {
    struct tm* tm = gmtime(&pathStat->st_mtime);

    if (tm == NULL) {
        outDate[0] = '\0';
        return;
    }
    strftime(outDate, 64, "%a, %d %b %Y %H:%M:%S GMT", tm);
}
