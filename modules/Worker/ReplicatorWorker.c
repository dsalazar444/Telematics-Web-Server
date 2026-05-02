#include "ReplicatorWorker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// Helper: parse status code from the start of an HTTP response buffer
static int ParseStatusCodeFromStatusLine(const char *line)
{
	// Expect: HTTP/1.1 200 OK
	const char *p = strchr(line, ' ');
	if (!p) return -1;
	// skip space
	p++;
	int code = atoi(p);
	return code;
}

void *replicatorWorker(void *arg)
{
	ReplicatorWorkerArgs *args = (ReplicatorWorkerArgs*)arg;
	if (args == NULL) return NULL;

	HTTPRequest *req = args->request;
	BackendNode target = args->targetNode;
	LoadBalancer *lb = args->lb;
	int timeout = args->timeout_ms > 0 ? args->timeout_ms : 3000;

	char host[64];
	snprintf(host, sizeof(host), "%u.%u.%u.%u",
			 target.id.ip[0], target.id.ip[1], target.id.ip[2], target.id.ip[3]);

	int attempts = 0;
	int success = 0;

	while (attempts < 3 && !success)
	{
		attempts++;

		IClientSocket *backendSock = CreateClientSocket(host, target.id.port, timeout);
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

		// Read only status line to decide success
		// Read into small buffer until CRLF
		char linebuf[1024];
		size_t filled = 0;
		int got_line = 0;

		while (filled < sizeof(linebuf) - 1)
		{
			int r = RecvFromClient(backendSock, linebuf + filled, 1);
			if (r < 0)
			{
				// read error
				break;
			}
			if (r == 0)
			{
				// EOF
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
			// Replace CRLF with NUL
			if (filled >= 2) linebuf[filled-2] = '\0';
			status_code = ParseStatusCodeFromStatusLine(linebuf);
		}

		CloseClientSocket(backendSock);

		if (status_code == 200)
		{
			success = 1;
			break;
		}

		// otherwise retry
		usleep(100 * 1000);
	}

	if (!success)
	{
		// Mark node as unhealthy in load balancer
		if (lb != NULL && target.index < lb->BackendCount)
		{
			pthread_mutex_lock(&lb->lock);
			lb->BackendNodes[target.index].healthy = 0;
			lb->BackendNodes[target.index].FailureCount++;
			pthread_mutex_unlock(&lb->lock);
		}
	}

	// Free request memory as worker owns it
	if (req != NULL)
	{
		free(req->body);
		free(req);
	}

	free(args);
	return NULL;
}