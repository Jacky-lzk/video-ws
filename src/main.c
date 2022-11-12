// Copyright (c) 2020 Cesanta Software Limited
// All rights reserved
//
// Example Websocket server. See https://mongoose.ws/tutorials/websocket-server/

#include "mgServer.h"


int main(void)
{
	return mongoose_server_loop();
}
