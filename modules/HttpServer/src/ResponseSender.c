#include "ResponseSender.h"
#include <stdio.h>
#include <string.h>

// Serialize headers and send response (headers + body). Returns 0 on success, -1 on failure.
int SendHTTPResponse(IClientSocket *client, HTTPResponse *response)
{
    if (!client || !response) return -1;

    char headersBuf[1024];
    int off = snprintf(headersBuf, sizeof(headersBuf), "HTTP/1.1 %d %s\r\n", response->statusCode, response->statusMessage);
    if (off < 0) return -1;

    for (size_t i = 0; i < response->headers.count && off < (int)sizeof(headersBuf) - 1; ++i) {
        int n = snprintf(headersBuf + off, sizeof(headersBuf) - off, "%s: %s\r\n",
                         response->headers.headers[i].key,
                         response->headers.headers[i].value);
        if (n < 0) return -1;
        off += n;
    }

    // add separator
    if (off + 2 < (int)sizeof(headersBuf)) {
        memcpy(headersBuf + off, "\r\n", 2);
        off += 2;
    } else {
        // headers buffer full — send what we have and continue
        if (SendToClient(client, headersBuf, off) <= 0) return -1;
        // send separator
        if (SendToClient(client, "\r\n", 2) <= 0) return -1;
        off = 0;
    }

    // send headers
    if (off > 0) {
        if (SendToClient(client, headersBuf, off) <= 0) return -1;
    }

    // send body in chunks (response->bodyLength may be large or binary)
    if (response->body && response->bodyLength > 0) {
        const char *ptr = (const char*)response->body;
        size_t remaining = response->bodyLength;
        while (remaining > 0) {
            int chunk = remaining > 4096 ? 4096 : (int)remaining;
            int sent = SendToClient(client, ptr, chunk);
            if (sent <= 0) return -1;
            ptr += sent;
            remaining -= (size_t)sent;
        }
    }

    return 0;
}
