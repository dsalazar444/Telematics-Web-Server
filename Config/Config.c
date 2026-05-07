#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Config.h"

// Carga la configuración desde el archivo config.config agregandola a la estructura
// Config. Para ello lee el archivo línea por línea, parsea cada línea y asigna los valores a la estructura.
Config LoadConfig(const char *filepath)
{
    Config config = {0};
    config.cacheDir = strdup("./cache");
    FILE *file = fopen(filepath, "r");
    if (file == NULL)
    {
        perror("fopen");
        return config;
    }

    // Primero, contar cuántos BACKEND_NODE hay
    int backendCount = 0;
    char line[128];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (strncmp(line, "BACKEND_NODE=", 13) == 0)
        {
            backendCount++;
        }
    }

    // Reservar memoria para los backends
    config.backends = NULL;
    config.backendCount = 0;
    if (backendCount > 0)
    {
        config.backends = (char **)malloc(sizeof(char *) * backendCount);
    }

    // Volver al inicio del archivo para leer de nuevo
    rewind(file);

    // Leer el archivo línea por línea y parsear cada línea buscando las palabras
    // clave para obtener su valor y asignarlo a la estructura Config
    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (line[0] == '#' || line[0] == '\n')
            continue;
        if (sscanf(line, "PORT=%d", &config.port) == 1)
            continue;
        if (sscanf(line, "TTL_CACHE=%hu", &config.ttl) == 1)
            continue;

        if (strncmp(line, "CACHE_DIR=", 10) == 0)
        {
            char *value = line + 10;
            size_t len = strcspn(value, "\r\n");
            value[len] = '\0';
            free(config.cacheDir);
            config.cacheDir = strdup(value);
            continue;
        }

        if (strncmp(line, "LOG_FILE_PROXY=", 15) == 0)
        {
            char *value = line + 15;
            size_t len = strcspn(value, "\r\n");
            value[len] = '\0';
            free(config.logFileProxy);
            config.logFileProxy = strdup(value);
            continue;
        }

        // Leer backends
        if (strncmp(line, "BACKEND_NODE=", 13) == 0 && config.backends)
        {
            char *value = line + 13;
            size_t len = strcspn(value, "\r\n");
            value[len] = '\0';
            config.backends[config.backendCount] = strdup(value);
            config.backendCount++;
        }
    }

    fclose(file);
    return config;
}

void FreeConfig(Config *config)
{
    if (config->cacheDir)
    {
        free(config->cacheDir);
        config->cacheDir = NULL;
    }

    if (config->backends)
    {
        for (uint16_t i = 0; i < config->backendCount; ++i)
        {
            free(config->backends[i]);
        }
        free(config->backends);
        config->backends = NULL;
        config->backendCount = 0;
    }
}