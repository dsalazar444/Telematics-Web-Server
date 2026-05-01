#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Config.h"

Config LoadConfig(const char* filepath) {
    Config config = {0};
    FILE* file = fopen(filepath, "r");
    if (file == NULL) {
        perror("fopen");
        return config;
    }

    // Primero, contar cuántos BACKEND_NODE hay
    int backendCount = 0;
    char line[128];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, "BACKEND_NODE=", 13) == 0) {
            backendCount++;
        }
    }

    // Reservar memoria para los backends
    config.backends = NULL;
    config.backendCount = 0;
    if (backendCount > 0) {
        config.backends = (char**)malloc(sizeof(char*) * backendCount);
    }

    // Volver al inicio del archivo para leer de nuevo
    rewind(file);

    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "PORT=%d", &config.port) == 1) continue;
        if (sscanf(line, "TTL=%d", &config.ttl) == 1) continue;
        if (sscanf(line, "CACHE_DIR=%d", &config.cacheDir) == 1) continue;

        // Leer backends
        if (strncmp(line, "BACKEND_NODE=", 13) == 0 && config.backends) {
            char *value = line + 13;
            // Eliminar salto de línea al final si existe
            size_t len = strcspn(value, "\r\n");
            value[len] = '\0';
            config.backends[config.backendCount] = strdup(value);
            config.backendCount++;
        }
    }

    fclose(file);
    return config;
}

void FreeConfig(Config *config) {
    if (config->backends) {
        for (uint16_t i = 0; i < config->backendCount; ++i) {
            free(config->backends[i]);
        }
        free(config->backends);
        config->backends = NULL;
        config->backendCount = 0;
    }
}