#include "mgServer.h"
#include "mongoose.h"
#include <thread>
#include "tspacket.h"

using namespace std;

static const char *s_websocket_listen = "ws://0.0.0.0:8001";

static char*  get_exe_path(char* out_buf, int buf_len)
{
	if (NULL == out_buf || buf_len < 2)
	{
		return NULL;
	}
	int i;
	int rslt = readlink("/proc/self/exe", out_buf, buf_len);
	if (rslt < 0 || (rslt >= 1024))
	{
		return NULL;
	}
	out_buf[rslt] = '\0';
	for (i = rslt; i >= 0; i--)
	{
		if (out_buf[i] == '/')
		{
			out_buf[i + 1] = '\0';
			break;
		}
	}
	return out_buf;
}

FILE *g_pSaveInFp = NULL;
const char* SAVE_FILE_NAME = "upload-ffmepg.ts";

bool g_hasWebsocket = false;

void send_data_ws(struct mg_connection *c, int times = 1)
{
	unsigned char *buf = (unsigned char *)malloc(2 * 1024 * 1024);
	if (!buf)
	{
		printf("malloc buffer err\n");
		return;
	}

	for (int ts_frame_nun = 0; ts_frame_nun < times; ++ts_frame_nun)
	{
		int i = 0;
		if (!g_hasWebsocket)
		{
			printf("no websocket connection in \n");
			usleep(1000000);
			continue;
		}
		unsigned int data_len = 0;
		if (GetTsPacket(buf, 2 * 1024 * 1024, &data_len))
		{
			printf("GetTsPacket ts_frame_nun=%d, data_len=%d \n", ts_frame_nun, data_len);
			printf("i=%d, c->is_websocket=%d, c->is_connecting=%d \n",
				i, c->is_websocket, c->is_connecting);

			if (c->is_websocket)
			{
				mg_ws_send(c, (const char*)buf, data_len, WEBSOCKET_OP_BINARY);
				//mg_ws_send(c, "ÄãºÃ \r\n", 5, WEBSOCKET_OP_TEXT);
			}
		}
		else
		{
			printf("no ts data \n");
		}

	}

}

static void fn_websocket(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
	struct mg_http_message *hm = (struct mg_http_message *) ev_data;
	struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;

	switch (ev)
	{
	case MG_EV_OPEN:
		printf("fn_ws_out, MG_EV_OPEN fn_websocket \n");
		break;


	case MG_EV_ACCEPT:
		printf("fn_ws_out, MG_EV_ACCEPT fn_websocket \n");
		//mg_ws_upgrade(c, hm, NULL);
		break;

	case MG_EV_CLOSE:
		printf("fn_ws_out, MG_EV_CLOSE fn_websocket \n");
		//g_pOutC = NULL;
		break;

	case MG_EV_HTTP_MSG:
		if (mg_http_match_uri(hm, "/websocket"))
		{
			// Upgrade to websocket. From now on, a connection is a full-duplex
			// Websocket connection, which will receive MG_EV_WS_MSG events.
			mg_ws_upgrade(c, hm, NULL);
			g_hasWebsocket = true;

		}
		else if (mg_http_match_uri(hm, "/rest"))
		{
			// Serve REST response
			mg_http_reply(c, 200, "", "{\"result\": %d}\n", 123);
		}
		else
		{
			// Serve REST response
			//mg_http_reply(c, 200, "", "{\"result\": %d}\n", 123);
			// Serve static files
			char buf[256] = { 0 };

			struct mg_http_serve_opts opts = { .root_dir = get_exe_path(buf, sizeof(buf)) };
			mg_http_serve_dir(c, (struct mg_http_message *)ev_data, &opts);
		}
		break;
	case MG_EV_WS_MSG:
		// Got websocket frame. Received data is wm->data. Echo it back!
		//mg_ws_send(c, wm->data.ptr, wm->data.len, WEBSOCKET_OP_TEXT);
		//send_data_ws(c, 100);

		break;

	default:
		break;
	}

	if (fn_data != NULL)
	{
		(void)fn_data;
	}

}


static void send_data_thread(void *arg) {
	struct mg_mgr *mgr = (struct mg_mgr *)arg;
	struct mg_connection *c;

	unsigned char *buf = (unsigned char *)malloc(2 * 1024 * 1024);
	if (!buf)
	{
		printf("malloc buffer err\n");
		return;
	}
	FILE *fp = fopen("./bbbb.ts", "w+");
	if (!fp)
	{
		printf("open ts file err\n");
		return;
	}

	int ts_frame_nun = 0;
	for (;;)
	{
		bool bHasSent = false;
		int i = 0;
		if (!g_hasWebsocket)
		{
			printf("no websocket connection in \n");
			usleep(1000000);
			continue;
		}
		unsigned int data_len = 0;
		if (GetTsPacket(buf, 2 * 1024 * 1024, &data_len))
		{
			if (data_len == 0)
			{
				printf("No ts data \n");
				fflush(fp);
				free(buf);
				buf = NULL;
				fclose(fp);
				fp = NULL;
				return;
			}
			fwrite(buf, 1, data_len, fp);

			++ts_frame_nun;
			printf("GetTsPacket ts_frame_nun=%d, data_len=%d \n", ts_frame_nun, data_len);
			for (c = mgr->conns; c != NULL; c = c->next)
			{
				++i;
				printf("i=%d, c->is_websocket=%d, c->is_connecting=%d \n",
					i, c->is_websocket, c->is_connecting);


				if (c->is_websocket)
				{
					bHasSent = true;
					//mg_send(c, buf, data_len);

					mg_ws_send(c, (const char*)buf, data_len, WEBSOCKET_OP_BINARY);
					//mg_ws_send(c, "ÄãºÃ \r\n", 5, WEBSOCKET_OP_TEXT);
				}

			}
		}
		else
		{
			printf("GetTsPacket failed! \n");
			return;
		}

		if (bHasSent)
		{
			usleep(40000);
			printf("bHasSent is true, sleep 10 ms \n");
		}
		else
		{
			usleep(500000);
			printf("bHasSent is false, sleep 200 ms \n");
		}
	}

	

}

int mongoose_server_loop()
{

	struct mg_mgr mgr;  // Event manager
	mg_mgr_init(&mgr);  // Initialise event manager

	//struct mg_timer t1;

	printf("Starting websocket listener on %s \n", s_websocket_listen);
	mg_http_listen(&mgr, s_websocket_listen, fn_websocket, NULL);  // Create HTTP listener

	//mg_timer_init(&t1, 100, MG_TIMER_REPEAT, timer_callback, &mgr);
	thread t(send_data_thread, &mgr);
	t.detach();

	CreateEsToTsThread();
	thread t2(push_avc_frame_to_ringbuffer_thread, nullptr);
	t2.detach();

	for (;;)
	{
		mg_mgr_poll(&mgr, 10);             // Infinite event loop
	}

	mg_mgr_free(&mgr);
	//mg_timer_free(&t1);

	return 0;
}

int mg_server_loop_in_thread()
{

	return 0;
}