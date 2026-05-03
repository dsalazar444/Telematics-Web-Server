#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

typedef struct  {
    int  port;
    uint16_t  ttl;
    char* cacheDir;
    char* logFileWs;
    char* logFileProxy;
    char** backends;
    uint16_t  backendCount;
} Config;

Config LoadConfig(const char* filepath);
void FreeConfig(Config *config);

#endif // CONFIG_H
