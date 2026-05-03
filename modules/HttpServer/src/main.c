#include "WorkerWS.h"
#include "../../Socket/Socket.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "../../Logs/Log.h"
#include "FileManager.h"

#define LEVEL "Server"
// Heredados desde Worker.h (ISocket.h)

void print_usage(const char *program_name) {
    printf("Uso: %s [-l LOGFILE] [-p PORT]\n", program_name);
    printf("  -l, --logfile FILE    Archivo de log (por defecto: logs.log)\n");
    printf("  -p, --port PORT       Puerto de escucha (por defecto: 8083)\n");
    printf("  -d, --dir DIR         Directorio de trabajo (por defecto: modules/HttpServer/www)\n");
    printf("  -h, --help            Mostrar esta ayuda\n");
}

int main(int argc, char *argv[]) {
    int port = 8083;
    char logfile[256] = "logs.log";
    char dir[256] = "modules/HttpServer/www";
    
    // Definir opciones largas
    struct option long_options[] = {
        {"logfile", required_argument, 0, 'l'},
        {"port", required_argument, 0, 'p'},
        {"dir", required_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    // Parsear argumentos
    while ((opt = getopt_long(argc, argv, "l:p:d:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'l':
                strncpy(logfile, optarg, sizeof(logfile) - 1);
                logfile[sizeof(logfile) - 1] = '\0';
                break;
            case 'p':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Error: puerto inválido %s. Debe estar entre 1 y 65535\n", optarg);
                    return 1;
                }
                break;
            case 'd':
                strncpy(dir, optarg, sizeof(dir) - 1);
                dir[sizeof(dir) - 1] = '\0';
                FileManagerInit(dir);
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    printf("Archivo de log: %s\n", logfile);
    printf("Puerto: %d\n", port);
    printf("Directorio de trabajo: %s\n", dir); // TODO: usar el dir pasado por argumento
    
    ISocketListener listener;
    listener.fd = CreateDualStackSocket();
    if (listener.fd < 0)
        return 1;

    if (BindSocket(&listener, "::", port) < 0)
        return 1;
    if (ListenSocket(&listener, 128) < 0)
        return 1;

    printf("Server/1.0 HTTP escuchando en puerto %d\n", port);

    const char *path = "../modules/HttpServer/www/health/logs.log";
    int logFile = LogInit(path);

    while (1)
    {
        IClientSocket* client = AcceptSocket(&listener);
        if (client == NULL) continue;

        WorkerWSArgs* args = malloc(sizeof(WorkerWSArgs));
        if (args == NULL) {
            CloseClientSocket(client);
            continue;
        }
        args->client = client;
        args->logFile = logFile;

        pthread_t thread;
        if (pthread_create(&thread, NULL, WorkerRun, args) != 0) {
            fprintf(stderr, "Error creando thread\n");
            free(args);
            continue;
        }
        pthread_detach(thread);
    }
    // Cuando ya no se necesite la configuración ni el load balancer:
    //FreeLoadBalancer(lb); // Si tienes una función para liberar el load balancer TODO
    return 0;
}