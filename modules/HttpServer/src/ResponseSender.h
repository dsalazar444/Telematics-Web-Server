#ifndef RESPONSE_SENDER_H
#define RESPONSE_SENDER_H

#include "Response.h"
#include "../../Socket/Socket.h"

int SendHTTPResponse(IClientSocket *client, HTTPResponse *res);

#endif // RESPONSE_SENDER_H
