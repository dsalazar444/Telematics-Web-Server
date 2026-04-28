

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

    char line[128];
    while (fgets(line, sizeof(line), file) != NULL) {

        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "PORT=%d", &config.port) == 1) continue;
        if (sscanf(line, "TTL=%d", &config.ttl) == 1) continue;

        // Leer backends
        if (strncmp(line, "BACKEND_NODE=", 8) == 0) {
            if (config.backend_count < 10) {
                // Copia el valor después del "backend="
                strncpy(config.backends[config.backend_count], 
                        line + 8,  // salta el "backend="
                        63);
                config.backend_count++;
            }
        }
    }

    fclose(file);
    return config;
}