#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "tspacket.h"

static pthread_t _aac_thread = 0;
static pthread_t _avc_thread = 0;
static pthread_t _ts_thread = 0;
static int _ts_thrd_run = 0;
///////////////////////////////////////////////////////////////////////////////////////////////////
#define LATM_AAC_PATERN 0x2B7
#define ADTS_AAC_PATERN 0xFFF1
int get_next_aac_frame(unsigned char *buf, int len)
{
    ///avoid first bound
    unsigned char *cur_pos = buf + 2;
    int find = 0;

    while (cur_pos && ((cur_pos - buf) < (len - 1)))
    {
        if ( (*cur_pos == (ADTS_AAC_PATERN >> 8)) && (*(cur_pos + 1) == (ADTS_AAC_PATERN & 0xFF)) )
        {
            find = 1;
            break;
        }
        cur_pos++;
    }

    if (!find)
    {
        return 0;
    }
    return cur_pos - buf;
}

int is_avc_frame(unsigned char *frame)
{
    if (((frame[4] & 0x1F) == 1) || ((frame[4] & 0x1F) == 5))
    {
        return 1;
    }
    return 0;
}

int get_next_avc_frame(unsigned char *buf, int len)
{
    ///avoid first bound
    unsigned char *cur_pos = buf + 4;
    int find = 0;

    while (cur_pos && ((cur_pos - buf) < (len - 4)))
    {
        if ((cur_pos[0] == 0x00) &&
            (cur_pos[1] == 0x00) &&
            (cur_pos[2] == 0x00) &&
            (cur_pos[3] == 0x01)
            #if (1)
            )
            #else
            && (cur_pos - buf > 188))
            #endif
        {
            find = 1;
            break;
        }
        cur_pos++;
    }

    if (!find)
    {
        return 0;
    }
    return cur_pos - buf;
}

static void *push_aac_frame_to_ringbuffer_thread(void *params)
{
    FILE *fp = fopen("./8185.aac", "rb");
    if (!fp)
    {
        printf("open aac file err\n");
        return NULL;
    }

    unsigned char *buf = (unsigned char *)malloc(512 * 1024);
    if (!buf)
    {
        printf("malloc buffer err\n");
        fclose(fp);
        fp = NULL;
        return NULL;
    }

    int pre_len = 0;
    unsigned long long pts = 0;
    while (1)
    {
        int len = fread(buf + pre_len, 1, 512 * 1024 - pre_len, fp);
        if (len <= 0)
        {
            printf("read aac file done\n");
            break;
        }

        len += pre_len;
        pre_len = len;
        unsigned char *cur = buf;
        while (pre_len)
        {
            int pos = get_next_aac_frame(cur, pre_len);
            if (!pos)
            {
                memcpy(buf, cur, pre_len);
                break;
            }

            if (InsertAudEsToRingBuffer(cur, pos, pts))
            {
                pts = pts + 90000 * 1024 / 48000;
                cur += pos;
                pre_len -= pos;
            }
        }
    }

    free(buf);
    buf = NULL;
    fclose(fp);
    fp = NULL;
    return NULL;
}

void *push_avc_frame_to_ringbuffer_thread(void *params)
{
    FILE *fp = fopen("./222.h264", "rb");
    if (!fp)
    {
        printf("open avc file err\n");
        return NULL;
    }

    unsigned char *buf = (unsigned char *)malloc(1 * 1024 * 1024);
    if (!buf)
    {
        printf("malloc buffer err\n");
        fclose(fp);
        fp = NULL;
        return NULL;
    }

    int pre_len = 0;
    unsigned long long pts = 0;
	int total_frames = 0;
    while (1)
    {
        int len = fread(buf + pre_len, 1, 1 * 1024 * 1024 - pre_len, fp);
        if (len <= 0)
        {
            printf("read avc file done\n");
            break;
        }

        len += pre_len;
        pre_len = len;
        unsigned char *cur = buf;
        while (pre_len)
        {
            int pos = get_next_avc_frame(cur, pre_len);
            if (!pos)
            {
                memcpy(buf, cur, pre_len);
                break;
            }

            if (InsertVidEsToRingBuffer(cur, pos, pts))
            {
				++total_frames;
                if (is_avc_frame(cur))
                {
                    pts = pts + 90000 / 25;
                }
                cur += pos;
                pre_len -= pos;
            }
        }
    }
    
	printf("read avc file to ring buffer. total_frames=%d\n", total_frames);
	

    free(buf);
    buf = NULL;
    fclose(fp);
    fp = NULL;
    return NULL;
}

static void *get_ts_packet_from_ringbuffer_thread(void *params)
{
    FILE *fp = fopen("./222.ts", "w+");
    if (!fp)
    {
        printf("open ts file err\n");
        return NULL;
    }

    unsigned char *buf = (unsigned char *)malloc(2 * 1024 * 1024);
    if (!buf)
    {
        printf("malloc buffer err\n");
        fclose(fp);
        fp = NULL;
        return NULL;
    }

    unsigned int data_len = 0;
    _ts_thrd_run = 1;
    while (_ts_thrd_run)
    {
        if (GetTsPacket(buf, 2 * 1024 * 1024, &data_len))
        {
            fwrite(buf, 1, data_len, fp);
        }
        else
        {
            usleep(5 * 1000);
        }
    }
    while (GetTsPacket(buf, 2 * 1024 * 1024, &data_len))
    {
        fwrite(buf, 1, data_len, fp);
    }

    fflush(fp);
    free(buf);
    buf = NULL;
    fclose(fp);
    fp = NULL;
    return NULL;
}

int ForTest(int argc, char *argv[])
{
    CreateEsToTsThread();
    //pthread_create(&_aac_thread, NULL, push_aac_frame_to_ringbuffer_thread, NULL);
    pthread_create(&_avc_thread, NULL, push_avc_frame_to_ringbuffer_thread, NULL);
    pthread_create(&_ts_thread, NULL, get_ts_packet_from_ringbuffer_thread, NULL);
    //pthread_join(_aac_thread, NULL);
    pthread_join(_avc_thread, NULL);
    printf("push done\n");

    usleep(5 * 1000 * 1000);
    _ts_thrd_run = 0;
    pthread_join(_ts_thread, NULL);
    printf("ts packet done\n");
    DestroyEsToTsThread();
    return 0;
}
