#include "ReplicatorWorker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// Helper: Parsea el código de estado de la línea de estado de una respuesta HTTP
// Recibe: line - la línea de estado completa (ej: "HTTP/1.1 200 OK")
// Retorna: el código de estado como entero (ej: 200), o -1 si no se pudo parsear
static int ParseStatusCodeFromStatusLine(const char *line)
{
	const char *p = strchr(line, ' ');
	if (!p) return -1;
	p++;
	int code = atoi(p);
	return code;
}

// Worker que se encarga de replicar un mensaje HTTP a un backend específico, con reintentos y marcando el backend como unhealthy si falla
// Pide: arg - puntero a ReplicatorWorkerArgs con la información de la petición a replicar, el nodo objetivo, el LoadBalancer para marcar unhealthy y el timeout por intento
// Retorna: NULL (no se espera que termine con un valor)
void *replicatorWorker(void *arg)
{
	ReplicatorWorkerArgs *args = (ReplicatorWorkerArgs*)arg;
	if (args == NULL) return NULL;

	HTTPRequest *req = args->request;
	BackendNode target = args->targetNode;
	LoadBalancer *lb = args->lb;
	int timeout = args->timeout_ms > 0 ? args->timeout_ms : 3000;

	int attempts = 0;
	int success = 0;

	while (attempts < 3 && !success)
	{
		attempts++;

		IClientSocket *backendSock = CreateClientSocket(target.id.ip, target.id.port, timeout);
		if (backendSock == NULL)
		{
			// Could not connect, try again
			usleep(100 * 1000); // 100ms backoff
			continue;
		}

		// Send the HTTP request
		if (SendHTTPRequest(backendSock, req) < 0)
		{
			CloseClientSocket(backendSock);
			usleep(100 * 1000);
			continue;
		}

		char linebuf[1024];
		size_t filled = 0;
		int got_line = 0;

		// Lee la primera línea de la respuesta del backend (status line)
		// Solamente le importa el número de status, así que no necesita leer headers ni body
		while (filled < sizeof(linebuf) - 1)
		{
			int r = RecvFromClient(backendSock, linebuf + filled, 1);
			if (r < 0)
			{
				break;
			}
			if (r == 0)
			{
				break;
			}
			filled += r;
			linebuf[filled] = '\0';
			if (filled >= 2 && linebuf[filled-2] == '\r' && linebuf[filled-1] == '\n')
			{
				got_line = 1;
				break;
			}
		}

		int status_code = -1;
		if (got_line)
		{
			if (filled >= 2) linebuf[filled-2] = '\0';
			status_code = ParseStatusCodeFromStatusLine(linebuf);
		}

		CloseClientSocket(backendSock);

		if (status_code == 200 || status_code == 201)
		{
			success = 1;
			break;
		}

		usleep(100 * 1000);
	}

	if (!success)
	{
		if (lb != NULL && target.index < lb->BackendCount)
		{
			pthread_mutex_lock(&lb->lock);
			lb->BackendNodes[target.index].healthy = 0;
			lb->BackendNodes[target.index].FailureCount++;
			pthread_mutex_unlock(&lb->lock);
		}
	}

	if (req != NULL)
	{
		free(req->body);
		free(req);
	}

	free(args);
	return NULL;
}