#ifndef CONFIG_H
#define CONFIG_H

#include "../Includes/structs.h"

Config LoadConfig(const char* filepath);
void FreeConfig(Config *config);

#endif // CONFIG_H
