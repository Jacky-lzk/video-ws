#ifndef _TS_PACKET_H_
#define _TS_PACKET_H_
#include <stdio.h>

typedef struct
{
    unsigned int u32VidFrameRate;///25
    unsigned int u32AudSampleRate;///48000
    unsigned int u32AudSamplePerFrame;///1024
} PRE_SET_AVINFO_t;

int InsertAudEsToRingBuffer(unsigned char *pu8Data, unsigned int u32DataLen, unsigned long long u64Pts);
int InsertVidEsToRingBuffer(unsigned char *pu8Data, unsigned int u32DataLen, unsigned long long u64Pts);
int CreateEsToTsThread(void);
int DestroyEsToTsThread(void);
void PreSetAVInfo(PRE_SET_AVINFO_t *pstPreSetAVInfo);
int GetTsPacket(unsigned char *pu8Buf, unsigned int u32BufLen, unsigned int *pu32DataLen);

void *push_avc_frame_to_ringbuffer_thread(void *params);
int ForTest(int argc, char *argv[]);

#endif ///_TS_PACKET_H_
