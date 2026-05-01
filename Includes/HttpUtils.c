#include "HttpUtils.h"
#include <strings.h>

const char* GetHeaderValue(const HTTPHeaders* headers, const char* key) {
    if (headers == NULL || key == NULL) return NULL;

    for (size_t i = 0; i < headers->count; i++) {
        if (strcasecmp(headers->headers[i].key, key) == 0) {
            return headers->headers[i].value;
        }
    }
    return NULL;
}