#include "FileManagerUtils.h"
#include <sys/stat.h>  // para stat()
#include <stdio.h>     // para NULL
#include <string.h>    // para strlen, strncpy, strncat, strrchr, strcasecmp
#include <time.h>      // para gmtime, strftime

// FUNCIÓN: funciones que trabajan con strings y metadata, no tocan contenido del archivo
// _documentRoot es local a este módulo (static)

static char _documentRoot[MAX_PATH_LEN] = "./www";

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

// Obtener tipo del body request
void GetMimeType(const char* path, char* outMime) {
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

// Obtener fecha de ultima modificación de recurso solicitado
void GetLastModified(const struct stat* pathStat, char* outDate) {
    struct tm* tm = gmtime(&pathStat->st_mtime);
    if (tm == NULL) {
        outDate[0] = '\0';
        return;
    }
    strftime(outDate, 64, "%a, %d %b %Y %H:%M:%S GMT", tm);
}
