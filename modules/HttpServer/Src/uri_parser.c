#include "uri_parser.h"
#include <string.h>
#include <stdlib.h>

ParsedURI UriParse(const char* rawUri) {
    // Inicializamos parsedUri
    ParsedURI result;
    result._absPath[0] = '\0';
    result._query[0]   = '\0';
    result._isValid    = 0;
    result._statusCode = 400;

    if (!ValidateLength(rawUri)) {
        result._statusCode = 400;
        return result;
    }

    if (!ExtractPath(rawUri, result._absPath, result._query)) {
        result._statusCode = 400;
        return result;
    }

    HandleEmptyPath(&result);
    DecodePercent(result._absPath);
    NormalizePath(result._absPath);

    if (!CheckTraversal(result._absPath)) {
        result._statusCode = 400;
        return result;
    }

    result._isValid    = 1;
    result._statusCode = 200;
    return result;
}

// UP-08 -> Mide que no pase cierto tamaño
static int ValidateLength(const char* uri) {
    if (uri == NULL) return 0;
    if (strlen(uri) >= URI_MAX_LEN) return 0;
    return 1;
}

// out.. es una notación para indicar que son variables de retorno, ya que en C, no es posible retornar varios valores, y en esta función es necesario, pues buscamos obtener el path y la query
// UP-2 -> salta http://host, y separa path y query
static int ExtractPath(const char* uri, char* outPath, char* outQuery) {
    const char* pathStart = uri;

    // si viene con scheme, saltar "http://host[:puerto]"
    if (strncasecmp(uri, "http://", 7) == 0) {
        // verificar que haya algo después del scheme
        if (strlen(uri) == 7) return 0;  // "http://" solo → malformada

        pathStart = strchr(uri + 7, '/'); // Si empieza con http://, busca el primer / después de eso (ejm: http://example.com:8080/path)
        if (pathStart == NULL) {
            // "http://abc.com" sin slash → tratar como vacío
            // handle_empty_path lo convierte a "/"
            outQuery[0] = '\0';
            return 1;
        }
        // verificar que no sea "http:///path" (host vacío)
        if (pathStart == uri + 7) return 0;
    }

    // separar path y query
    const char* queryStart = strchr(pathStart, '?');

    if (queryStart != NULL) {
        // obtenemos outpath y outquery
        size_t pathLen = queryStart - pathStart;
        strncpy(outPath, pathStart, pathLen);
        outPath[pathLen] = '\0';
        strncpy(outQuery, queryStart + 1, URI_MAX_LEN - 1);
        outQuery[URI_MAX_LEN - 1] = '\0';
    } else {
        // Obtenemos outPath y outQuery lo dejamos vacio
        strncpy(outPath, pathStart, URI_MAX_LEN - 1);
        outPath[URI_MAX_LEN - 1] = '\0';
        outQuery[0] = '\0';
    }

    return 1;
}

// UP-1 -> checa si absPath quedó vacio (para tomarlo como root /)
static void HandleEmptyPath(ParsedURI* result) {
    if (strlen(result->_absPath) == 0) {
        result->_absPath[0] = '/';
        result->_absPath[1] = '\0';
    }
}

// UP-3 -> Convierte secuencias %HEX a su caracter real.
static void DecodePercent(char* path) {
    char decoded[URI_MAX_LEN];
    int  i = 0;
    int  j = 0;

    while (path[i] != '\0' && j < URI_MAX_LEN - 1) {
        if (path[i] == '%' &&
            path[i+1] != '\0' &&
            path[i+2] != '\0' &&
            isxdigit(path[i+1]) &&
            isxdigit(path[i+2])) {

            char hex[3] = { path[i+1], path[i+2], '\0' };
            decoded[j]  = (char) strtol(hex, NULL, 16);
            i += 3;
        } else {
            decoded[j] = path[i];
            i++;
        }
        j++;
    }
    decoded[j] = '\0';

    // copiar resultado de vuelta al path original
    strncpy(path, decoded, URI_MAX_LEN - 1);
    path[URI_MAX_LEN - 1] = '\0';
}

// UP-4 -> Resuelve . y .., elimina dobles slashes.
static void NormalizePath(char* path) {
    char  temp[URI_MAX_LEN];
    char* segments[URI_MAX_LEN / 2];
    int   depth = 0;

    strncpy(temp, path, URI_MAX_LEN - 1);
    temp[URI_MAX_LEN - 1] = '\0';

    char* token = strtok(temp, "/");
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // ignorar — punto actual
        } else if (strcmp(token, "..") == 0) {
            // subir nivel si podemos
            if (depth > 0) depth--;
        } else if (strlen(token) > 0) {
            segments[depth++] = token;
        }
        token = strtok(NULL, "/");
    }

    // reconstruir path normalizado
    if (depth == 0) {
        strncpy(path, "/", URI_MAX_LEN - 1);
        return;
    }

    path[0] = '\0';
    for (int k = 0; k < depth; k++) {
        strncat(path, "/", URI_MAX_LEN - strlen(path) - 1);
        strncat(path, segments[k], URI_MAX_LEN - strlen(path) - 1);
    }
}

// Verifica que el path normalizado no contenga .. ni empiece sin /.
static int CheckTraversal(const char* path) {
    // después de normalizar, un path válido nunca
    // debería contener ".." — si lo tiene, es un ataque
    if (strstr(path, "..") != NULL) return 0;

    // tampoco debería empezar sin "/"
    if (path[0] != '/') return 0;

    return 1;
}