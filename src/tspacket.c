#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "tspacket.h"

#define TS_PK_ERR printf("[%s][%d]", __FUNCTION__, __LINE__);printf
#define TS_PK_INF ///printf
#define TS_PK_LEN (188)
#define TRUE (1)
#define FALSE (0)
#define PMT_PID (0x337)
#define VID_PID (0x335)
#define PCR_PID VID_PID
#define AUD_PID (0x336)
typedef unsigned char BYTE;
typedef unsigned int UINT;
typedef unsigned long long UINT64;
typedef unsigned short UINT16;
typedef unsigned long ULONG;
typedef unsigned char BOOL;

typedef struct
{
    BYTE *pFrameBuf;
    UINT u32FrameLen;
    UINT64 u64FrameIdx;
    UINT u32FrameRate;
    UINT64 u64Pts;
    BOOL bRestart;
} H264_FRAME_TO_TS_Params_t;

typedef struct
{
    BYTE *pFrameBuf;
    UINT u32FrameLen;
    UINT64 u64FrameIdx;
    ///AAC 48K
    UINT u32AudioSampleRate;
    ///AAC = 1024
    UINT u32AudioSamplesPerFrame;
    UINT64 u64Pts;
    BOOL bRestart;
} AAC_FRAME_TO_TS_Params_t;

//size = 10 for PTS DTS info
typedef struct
{
    BYTE marker_bit:1;
    BYTE PTS1:3;
    BYTE fix_bit:4;

    BYTE PTS21;

    BYTE marker_bit1:1;
    BYTE PTS22:7;

    BYTE PTS31;

    BYTE marker_bit2:1;
    BYTE PTS32:7;

    /////////////////
    BYTE marker_bit3:1;
    BYTE DTS1:3;
    BYTE fix_bit2:4;

    BYTE  DTS21;

    BYTE marker_bit4:1;
    BYTE DTS22:7;

    BYTE  DTS31;

    BYTE marker_bit5:1;
    BYTE  DTS32:7;
} __attribute__ ((packed)) PTS_tag;

typedef struct//size =9
{
    BYTE packet_start_code_prefix[3];
    BYTE stream_id;
    BYTE PES_packet_length[2];

    BYTE original_or_copy:1;
    BYTE copyright:1;
    BYTE data_alignment_indicator:1;
    BYTE PES_priority:1;
    BYTE PES_scrambling_control:2;
    BYTE fix_bit:2;

    BYTE PES_extension_flag:1;
    BYTE PES_CRC_flag:1;
    BYTE additional_copy_info_flag:1;
    BYTE DSM_trick_mode_flag:1;
    BYTE ES_rate_flag:1;
    BYTE ESCR_flag:1;
    BYTE PTS_DTS_flags:2;

    BYTE PES_header_data_length;
} __attribute__ ((packed)) PES_HEADER_tag;

typedef struct
{
    BYTE sync_byte;
    BYTE pid_hei5:5;
    BYTE transport_priority:1;
    BYTE payload_unit_start_indicator:1;
    BYTE transport_error_indicator:1;
    BYTE pid_low8:8;
    BYTE continuity_counter:4;
    BYTE adaption_field_control:2;
    BYTE transport_scrambling_control:2;
} __attribute__ ((packed)) TS_HEADER_Tag;

typedef enum
{
    ES_FORMAT_H264 = 0,
    ES_FORMAT_AAC,
    ES_FORMAT_MAX,
} __attribute__ ((packed)) ES_FORMAT_e;

typedef enum
{
    ES_FRAME_TYPE_NONE = 0,
    ES_FRAME_TYPE_PB = 1,
    ES_FRAME_TYPE_IDR = 5,
    ES_FRAME_TYPE_SEI = 6,
    ES_FRAME_TYPE_SPS = 7,
    ES_FRAME_TYPE_PPS = 8,
    ES_FRAME_TYPE_AUD = 9,
    ES_FRAME_TYPE_AAC = 0xFFF1,
} __attribute__ ((packed)) ES_FRAME_TYPE_e;

typedef struct
{
    ///in es
    BYTE *pEsBuf;
    UINT u32DataLen;
    UINT u32FrameIndex;
    UINT u32FrameRate;
    ES_FORMAT_e eEsFormat;
    ES_FRAME_TYPE_e eEsFrameType;
    UINT64 u64Pts;

    ///out pes
    BYTE *pPesBuf;
    UINT u32PesBufLen;
}__attribute__ ((packed)) ES_TO_PES_Params_t;

typedef struct
{
    ///in pes
    UINT16 u16Pid:13;
    BYTE *pPesBuf;
    UINT u32PesPkLen;
    BYTE u8ContinueCount;
    ES_FRAME_TYPE_e eEsFrameType;
    UINT u32Pcr;

    ///out ts
    BYTE *pTsBuf;
    UINT u32TsBufLen;
} __attribute__ ((packed)) PES_TO_TS_Params_t;

typedef struct
{
    ///in pes
    UINT16 u16Pid:13;
    BYTE *pSecBuf;
    UINT u32SecLen;
    BYTE u8ContinueCount;

    ///out ts
    BYTE *pTsBuf;
    UINT u32TsBufLen;
} __attribute__ ((packed)) SEC_TO_TS_Params_t;

#define PROGRAM_NUMBER (128)
typedef struct
{
    UINT16 program_number;
    UINT16 program_map_PID;
} __attribute__ ((packed)) PROGRAM_Info_t;

typedef struct
{
    ///in pes
    ///stream id
    UINT16 u16StreamID;
    PROGRAM_Info_t stProgramInfo[PROGRAM_NUMBER];
    BYTE u8ProgramInfoLen;

    ///out ts
    BYTE *pSecBuf;
    UINT u32SecBufLen;
} __attribute__ ((packed)) PAT_SEC_Params_t;

typedef struct
{
    BYTE table_id;
    BYTE section_length_hei4:4;
    BYTE revert:2;
    BYTE force_bit:1;
    BYTE section_syntax_indicator:1;
    BYTE section_length_low8:8;
    BYTE transport_stream_id[2];
    BYTE current_next_indicator:1;
    BYTE version_number:5;
    BYTE reserved:2;
    BYTE section_number;
    BYTE last_section_number;
} __attribute__ ((packed)) PAT_HEADER_Tag;

typedef struct
{
    BYTE table_id;
    BYTE section_length_hei4:4;
    BYTE revert:2;
    BYTE force_bit:1;
    BYTE section_syntax_indicator:1;
    BYTE section_length_low8:8;
    BYTE program_number[2];
    BYTE current_next_indicator:1;
    BYTE version_number:5;
    BYTE reserved:2;
    BYTE section_number;
    BYTE last_section_number;
    BYTE pcr_pid_hei5:5;
    BYTE reserved3:3;
    BYTE pcr_pid_low8:8;
    BYTE program_info_length_hei4:4;
    BYTE reserved4:4;
    BYTE program_info_length_low8:8;
} __attribute__ ((packed)) PMT_HEADER_Tag;

typedef struct
{
    BYTE stream_type;
    UINT16 elementary_PID;
} __attribute__ ((packed)) STREAM_Info_t;

typedef struct
{
    ///in pes
    STREAM_Info_t stStreamInfo[PROGRAM_NUMBER];
    BYTE u8StreamInfoLen;
    UINT16 pcr_pid;
    UINT16 program_number;

    ///out ts
    BYTE *pSecBuf;
    UINT u32SecBufLen;
} __attribute__ ((packed)) PMT_SEC_Params_t;

static ULONG _u32CrcTbl[256] = {
        0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
        0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
        0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
        0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
        0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
        0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
        0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
        0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
        0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
        0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
        0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
        0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
        0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
        0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
        0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
        0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
        0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
        0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
        0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
        0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
        0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
        0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
        0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
        0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
        0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
        0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
        0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
        0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
        0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
        0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
        0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
        0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
        0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
        0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
        0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
        0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
        0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
        0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
        0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
        0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
        0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
        0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
        0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4};

/////////////////////////////////////////////////////////////////////////////////////////////////
//                  local function define
////////////////////////////////////////////////////////////////////////////////////////////////
BOOL PutTsToRingBuffer(BYTE *pu8Data, UINT u32DataLen);
static UINT _calc_crc(BYTE *pData, UINT u32Len)
{
    UINT i;
    UINT u32Crc = 0xffffffff;

    for (i = 0; i < u32Len; i++)
    {
        u32Crc = (u32Crc << 8) ^ _u32CrcTbl[((u32Crc >> 24) ^ *pData++) & 0xff];
    }

    return u32Crc;
}

// define the pes head PTS head for is little end type !!!!
static void _set_pts(PTS_tag *t, UINT64 _ui64PTS, UINT64 _ui64DTS)
{
    t->PTS1 = (_ui64PTS >> 30) & 0x07;
    t->PTS21 = (_ui64PTS >> 22) & 0xFF;
    t->PTS22 = (_ui64PTS >> 15) & 0x7F;
    t->PTS31 = (_ui64PTS >> 7) & 0xFF;
    t->PTS32 = _ui64PTS & 0x7F;

    t->DTS1 = (_ui64DTS >> 30) & 0x07;
    t->DTS21 = (_ui64DTS >> 22) & 0xFF;
    t->DTS22 = (_ui64DTS >> 15) & 0x7F;
    t->DTS31 = (_ui64DTS >> 7) & 0xFF;
    t->DTS32 = _ui64DTS & 0x7F;

    t->fix_bit  = 0x3;///0011
    t->fix_bit2 = 0x1;///0001
    t->marker_bit  = 0x1;///martk_bit_1
    t->marker_bit1 = 0x1;///martk_bit_1
    t->marker_bit2 = 0x1;///martk_bit_1
    t->marker_bit3 = 0x1;///martk_bit_1
    t->marker_bit4 = 0x1;///martk_bit_1
    t->marker_bit5 = 0x1;///martk_bit_1
}

UINT make_pes_head(BYTE *pHeader, UINT u32DataLen, UINT u32StreamId, UINT64 u64PTS)
{
    PES_HEADER_tag ePESHeader;

    ePESHeader.packet_start_code_prefix[0] = 0x00;
    ePESHeader.packet_start_code_prefix[1] = 0x00;
    ePESHeader.packet_start_code_prefix[2] = 0x01;
    ePESHeader.stream_id = u32StreamId;

    u32DataLen += 13;
    if (u32DataLen > 0xFFFF)
    {
        u32DataLen = 0;
    }

    ePESHeader.PES_packet_length[0] = (u32DataLen >> 8) & 0xFF;
    ePESHeader.PES_packet_length[1] = u32DataLen & 0xFF;

    ePESHeader.PES_scrambling_control = 0;
    ePESHeader.PES_priority = 0;
    ePESHeader.data_alignment_indicator = 0;
    ePESHeader.copyright = 0;
    ePESHeader.original_or_copy = 0;
    ePESHeader.PTS_DTS_flags = 3;
    ePESHeader.ESCR_flag = 0;
    ePESHeader.ES_rate_flag = 0;
    ePESHeader.DSM_trick_mode_flag = 0;
    ePESHeader.additional_copy_info_flag = 0;
    ePESHeader.PES_CRC_flag = 0;
    ePESHeader.PES_extension_flag = 0;
    ePESHeader.PES_header_data_length = 10;
    ePESHeader.fix_bit = 0x2;///10, must set
    memcpy(pHeader,&ePESHeader,sizeof(PES_HEADER_tag));

    PTS_tag ePTS;
    TS_PK_INF("%s pts:%lld\n", u32StreamId == 0xE0 ? "video" : "audio", u64PTS);
    _set_pts(&ePTS, u64PTS + 3600, u64PTS);
    memcpy(pHeader + sizeof(PES_HEADER_tag),&ePTS,sizeof(PTS_tag));
    return sizeof(PES_HEADER_tag) + sizeof(PTS_tag);
}

static UINT _check_es_to_pes_params_legal(ES_TO_PES_Params_t *pEsToPesParams)
{
    if (!pEsToPesParams)
    {
        TS_PK_INF("pes param is null\n");
        return FALSE;
    }

    if ((!pEsToPesParams->pEsBuf)
     || (pEsToPesParams->u32DataLen == 0)
     || (!pEsToPesParams->pPesBuf)
     || (pEsToPesParams->u32PesBufLen == 0)
     || (pEsToPesParams->eEsFormat >= ES_FORMAT_MAX))
    {
        TS_PK_INF("esbuf:%d, eslen:%d, pesbuf:%p, peslen:%d, format:%d\n",
             (!pEsToPesParams->pEsBuf),
             (pEsToPesParams->u32DataLen == 0),
             (pEsToPesParams->pPesBuf),
             (pEsToPesParams->u32PesBufLen == 0),
             (pEsToPesParams->eEsFormat >= ES_FORMAT_MAX));
        return FALSE;
    }
    return TRUE;
}

static BYTE _es_format_to_stream_type(ES_FORMAT_e eEsFormat)
{
    if (eEsFormat == ES_FORMAT_H264)
    {
        ///video
        return 0x1B;
    }
    else if (eEsFormat == ES_FORMAT_AAC)
    {
        ///audio
        return 0x0F;
    }

    ///audio
    return 0x0F;
}

static BYTE _es_format_to_stream_id(ES_FORMAT_e eEsFormat)
{
    if (eEsFormat == ES_FORMAT_H264)
    {
        ///video
        return 0xE0;
    }
    else if (eEsFormat == ES_FORMAT_AAC)
    {
        ///audio
        return 0xC0;
    }

    ///audio
    return 0xC0;
}

UINT make_es_to_pes(ES_TO_PES_Params_t *pEsToPesParams)
{
    static UINT64 u64Offset = 0;
    static UINT64 u64FrameNum = 0;
    UINT u32PesPkLen = 0;
    UINT u32HdrLen = 0;
    BYTE u8AudData[] = {0x0, 0x0, 0x0, 0x1, 0x9, 0x1};///for apple ipone  and ipad, insert aud
    BYTE u8ForceInsertAudLen = 0;

    if (!_check_es_to_pes_params_legal(pEsToPesParams))
    {
        TS_PK_ERR("pes params ilegal\n");
        return 0;
    }

    if (ES_FORMAT_H264 == pEsToPesParams->eEsFormat)
    {
        ///h264 video
        u8ForceInsertAudLen = sizeof(u8AudData);
    }

    u32HdrLen = make_pes_head(
        pEsToPesParams->pPesBuf,
        pEsToPesParams->u32DataLen + u8ForceInsertAudLen,
        _es_format_to_stream_id(pEsToPesParams->eEsFormat),
        pEsToPesParams->u64Pts);

    if (u8ForceInsertAudLen)
    {
        memcpy(pEsToPesParams->pPesBuf + u32HdrLen, u8AudData, u8ForceInsertAudLen);
    }
    u32HdrLen += u8ForceInsertAudLen;

    if ((pEsToPesParams->pEsBuf[4] & 0x1F) == 1)
    {
        pEsToPesParams->eEsFrameType = ES_FRAME_TYPE_PB;
        u64FrameNum = ((pEsToPesParams->pEsBuf[5] << 3) | (pEsToPesParams->pEsBuf[6] >> 5)) & 0xF;
        TS_PK_INF("P/B Frame u64Offset:0x%llx, u64FrameNum:%lld\n", u64Offset, u64FrameNum);
    }
    else
    if ((pEsToPesParams->pEsBuf[0] == 0xFF) && (pEsToPesParams->pEsBuf[1] == 0xF1))
    {
        TS_PK_INF("AAC Frame\n");
        pEsToPesParams->eEsFrameType = ES_FRAME_TYPE_AAC;
    }
    else
    if ((pEsToPesParams->pEsBuf[4] & 0x1F) == 5)
    {
        pEsToPesParams->eEsFrameType = ES_FRAME_TYPE_IDR;
        u64FrameNum = (pEsToPesParams->pEsBuf[6] >> 3) & 0xF;
        TS_PK_INF("IDR Frame u64Offset:0x%llx, u64FrameNum:%lld\n", u64Offset, u64FrameNum);
    }
    else
    if ((pEsToPesParams->pEsBuf[4] & 0x1F) == 7)
    {
        TS_PK_INF("SPS Frame 0x%llx\n", u64Offset);
        pEsToPesParams->eEsFrameType = ES_FRAME_TYPE_SPS;
    }
    else
    if ((pEsToPesParams->pEsBuf[4] & 0x1F) == 8)
    {
        TS_PK_INF("PPS Frame 0x%llx\n", u64Offset);
        pEsToPesParams->eEsFrameType = ES_FRAME_TYPE_PPS;
    }

    u64Offset += pEsToPesParams->u32DataLen;
    u32PesPkLen = pEsToPesParams->u32DataLen + u32HdrLen;
    if (u32PesPkLen > pEsToPesParams->u32PesBufLen)
    {
        TS_PK_ERR("pes buffer too small\n");
        return 0;
    }

    memcpy(pEsToPesParams->pPesBuf + u32HdrLen, pEsToPesParams->pEsBuf, pEsToPesParams->u32DataLen);
    TS_PK_INF("es to pes done\n");
    return u32PesPkLen;
}

static UINT _check_pes_to_ts_params_legal(PES_TO_TS_Params_t *pPesToTsParams)
{
    UINT u32TsPkLen = 0;
    if (!pPesToTsParams)
    {
        TS_PK_INF("ts param is null\n");
        return FALSE;
    }

    u32TsPkLen = pPesToTsParams->u32TsBufLen / TS_PK_LEN;
    if ((!pPesToTsParams->pPesBuf)
        || (!pPesToTsParams->pTsBuf)
        || (!pPesToTsParams->u32TsBufLen)
        || (!u32TsPkLen)
        || (pPesToTsParams->u16Pid == 0x1FFF))
    {
        TS_PK_INF("pesbuf:%d, tsbuf:%d, tslen:%d, tspklen:%d, pid:%d\n",
            (!pPesToTsParams->pPesBuf),
            (!pPesToTsParams->pTsBuf),
            (!pPesToTsParams->u32TsBufLen),
            (!u32TsPkLen),
            (pPesToTsParams->u16Pid == 0x1FFF));
        return FALSE;
    }

    if (pPesToTsParams->u32PesPkLen % (TS_PK_LEN - 4))
    {
        u32TsPkLen -= 1;
    }

    if ( (pPesToTsParams->u32PesPkLen / (TS_PK_LEN - 4)) > u32TsPkLen )
    {
        TS_PK_INF("pes len:%d, ts len:%d\n", (pPesToTsParams->u32PesPkLen / (TS_PK_LEN - 4)), u32TsPkLen);
        return FALSE;
    }

    return TRUE;
}

UINT make_ts_header(BYTE *pHeader, BYTE u8Len, UINT16 u16Pid, BYTE *pu8ContinuityCounter, BOOL bStartPk)
{
    TS_HEADER_Tag stTsHdrTag;

    if ((u8Len < 4) || (!pu8ContinuityCounter))
    {
        TS_PK_INF("%d %d\n", (u8Len < 4), (!pu8ContinuityCounter));
        return 0;
    }
    memset(&stTsHdrTag, 0x0, sizeof(TS_HEADER_Tag));

    stTsHdrTag.sync_byte = 0x47;
    if (!bStartPk)
    {
        stTsHdrTag.payload_unit_start_indicator = 0;
    }
    else
    {
        stTsHdrTag.payload_unit_start_indicator = 1;
    }
    stTsHdrTag.pid_hei5 = (u16Pid >> 8) & 0x1F;
    stTsHdrTag.pid_low8 = u16Pid & 0xFF;
    ///only playload
    stTsHdrTag.adaption_field_control = 1;
    stTsHdrTag.continuity_counter = (*pu8ContinuityCounter) & 0xF;
    memcpy(pHeader, &stTsHdrTag, sizeof(stTsHdrTag));

    (*pu8ContinuityCounter)++;
    if (*pu8ContinuityCounter > 0xF)
    {
        *pu8ContinuityCounter = 0;
    }

    return sizeof(stTsHdrTag);
}

UINT make_pes_to_ts(PES_TO_TS_Params_t *pPesToTsParams)
{
    BYTE *pPesPkPos = NULL;
    BYTE *pEndOfPesPk = NULL;
    BYTE *pTsPkBuf = NULL;
    BYTE u8CurWriLen = 0;
    BYTE u8HdrLen = 0;

    if (!_check_pes_to_ts_params_legal(pPesToTsParams))
    {
        TS_PK_ERR("ts params ilegal\n");
        return 0;
    }

    pPesPkPos = pPesToTsParams->pPesBuf;
    pEndOfPesPk = pPesToTsParams->pPesBuf + pPesToTsParams->u32PesPkLen;
    pTsPkBuf = pPesToTsParams->pTsBuf;
    while (pPesPkPos < pEndOfPesPk)
    {
        memset(pTsPkBuf, 0xFF, TS_PK_LEN);
        if (pPesPkPos != pPesToTsParams->pPesBuf)
        {
            u8HdrLen = make_ts_header(pTsPkBuf, TS_PK_LEN, pPesToTsParams->u16Pid, &(pPesToTsParams->u8ContinueCount), FALSE);
        }
        else
        {
            u8HdrLen = make_ts_header(pTsPkBuf, TS_PK_LEN, pPesToTsParams->u16Pid, &(pPesToTsParams->u8ContinueCount), TRUE);
            ///pes, no have pointer_field
            ///first i frame must set pcr
            if ((pPesToTsParams->eEsFrameType == ES_FRAME_TYPE_IDR)
                #if (1)
                || (pPesToTsParams->eEsFrameType == ES_FRAME_TYPE_SPS)
                || (pPesToTsParams->eEsFrameType == ES_FRAME_TYPE_PPS))
                #else
                )
                #endif
            {
                ///insert pcr
                if (pPesToTsParams->u32Pcr != (UINT64)(-1))
                {
                    ///all set feild data
                    pTsPkBuf[3] |= 0x30;
                    ///adaptation_field_length 6+1
                    pTsPkBuf[4] = 7;
                    ///set PCR_flag = 1
                    pTsPkBuf[5] = 0x10;
                    ///set pcr value

                    #if (1)
                        /* 90KHZ */
                        pTsPkBuf[6] = (unsigned char)(pPesToTsParams->u32Pcr>>25);
                        pTsPkBuf[7] = (unsigned char)(pPesToTsParams->u32Pcr>>17);
                        pTsPkBuf[8] = (unsigned char)(pPesToTsParams->u32Pcr>>9);
                        pTsPkBuf[9] = (unsigned char)(pPesToTsParams->u32Pcr>>1);
                    #else
                        /* 45KHZ */
                        pTsPkBuf[6] = (unsigned char)(pPesToTsParams->u32Pcr>>24);
                        pTsPkBuf[7] = (unsigned char)(pPesToTsParams->u32Pcr>>16);
                        pTsPkBuf[8] = (unsigned char)(pPesToTsParams->u32Pcr>>8);
                        pTsPkBuf[9] = (unsigned char)(pPesToTsParams->u32Pcr);
                    #endif
                        pTsPkBuf[10] = (unsigned char)(((pPesToTsParams->u32Pcr&0x1)<<7) | 0x7e);
                    u8HdrLen = u8HdrLen + pTsPkBuf[4] + 1;

                    if ((pEndOfPesPk - pPesPkPos) < (TS_PK_LEN - u8HdrLen))
                    {
                        memcpy(pTsPkBuf + u8HdrLen, pPesPkPos, (pEndOfPesPk - pPesPkPos));
                        pPesPkPos += (pEndOfPesPk - pPesPkPos);
                        pTsPkBuf += TS_PK_LEN;
                        break;
                    }
                }
            }
        }

        u8CurWriLen = ((pEndOfPesPk - pPesPkPos) >= (TS_PK_LEN - u8HdrLen)) ? (TS_PK_LEN - u8HdrLen) : (pEndOfPesPk - pPesPkPos);

        if (u8CurWriLen < (TS_PK_LEN - u8HdrLen))
        {
            ///fill stuffing_byte
            ///set adaption_field_control to 0x3
            pTsPkBuf[3] |= 0x30;
            ///adaptation_field_length
            pTsPkBuf[4] = (TS_PK_LEN - u8HdrLen) - 1 - u8CurWriLen;
            ///adaptation_field_flag_set to 0x0
            if (pTsPkBuf[4])
            {
                ///playload less than 183
                pTsPkBuf[5] = 0x0;
                ///set all byte include stuffing_byte to 0xFF
                memset(pTsPkBuf + u8HdrLen + 2, 0xFF, (TS_PK_LEN - u8HdrLen - 2));
            }
            else
            {
                ///playload equal 183, pTsPkBuf[4] == 0
                ///memset(pTsPkBuf + u8HdrLen + 1, 0xFF, (TS_PK_LEN - u8HdrLen - 1));
            }

            ///calc new header len
            u8HdrLen = u8HdrLen + pTsPkBuf[4] + 1;
        }

        memcpy(pTsPkBuf + u8HdrLen, pPesPkPos, u8CurWriLen);
        pPesPkPos += u8CurWriLen;
        pTsPkBuf += TS_PK_LEN;
    }

    TS_PK_INF("pes to ts done\n");
    return pTsPkBuf - pPesToTsParams->pTsBuf;
}

static UINT _check_sec_to_ts_params_legal(SEC_TO_TS_Params_t *pSecToTsParams)
{
    UINT u32TsPkLen = 0;
    if (!pSecToTsParams)
    {
        TS_PK_INF("ts param is null\n");
        return FALSE;
    }

    u32TsPkLen = pSecToTsParams->u32TsBufLen / TS_PK_LEN;
    if ((!pSecToTsParams->pSecBuf)
        || (!pSecToTsParams->pTsBuf)
        || (!pSecToTsParams->u32TsBufLen)
        || (!u32TsPkLen)
        || (pSecToTsParams->u16Pid == 0x1FFF))
    {
        TS_PK_INF("pesbuf:%d, tsbuf:%d, tslen:%d, tspklen:%d, pid:%d\n",
            (!pSecToTsParams->pSecBuf),
            (!pSecToTsParams->pTsBuf),
            (!pSecToTsParams->u32TsBufLen),
            (!u32TsPkLen),
            (pSecToTsParams->u16Pid == 0x1FFF));
        return FALSE;
    }

    if (pSecToTsParams->u32SecLen % (TS_PK_LEN - 4))
    {
        u32TsPkLen -= 1;
    }

    if ( (pSecToTsParams->u32SecLen / (TS_PK_LEN - 4)) > u32TsPkLen )
    {
        TS_PK_INF("pes len:%d, ts len:%d\n", (pSecToTsParams->u32SecLen / (TS_PK_LEN - 4)), u32TsPkLen);
        return FALSE;
    }

    return TRUE;
}

UINT make_section_to_ts(SEC_TO_TS_Params_t *pSecToTsParams)
{
    BYTE *pSecPkPos = NULL;
    BYTE *pEndOfSecPk = NULL;
    BYTE *pTsPkBuf = NULL;
    BYTE u8CurWriLen = 0;
    BYTE u8HdrLen = 0;
    BYTE *pu8ContinueCount = NULL;

    if (!_check_sec_to_ts_params_legal(pSecToTsParams))
    {
        TS_PK_ERR("ts params ilegal\n");
        return 0;
    }

    pSecPkPos = pSecToTsParams->pSecBuf;
    pEndOfSecPk = pSecToTsParams->pSecBuf + pSecToTsParams->u32SecLen;
    pTsPkBuf = pSecToTsParams->pTsBuf;

    while (pSecPkPos < pEndOfSecPk)
    {
        memset(pTsPkBuf, 0xFF, TS_PK_LEN);
        if (pSecPkPos != pSecToTsParams->pSecBuf)
        {
            u8HdrLen = make_ts_header(pTsPkBuf, TS_PK_LEN, pSecToTsParams->u16Pid, &(pSecToTsParams->u8ContinueCount), FALSE);
        }
        else
        {
            u8HdrLen = make_ts_header(pTsPkBuf, TS_PK_LEN, pSecToTsParams->u16Pid, &(pSecToTsParams->u8ContinueCount), TRUE);
            ///psi si, need pointer_field
            pTsPkBuf[u8HdrLen++] = 0x0;
        }

        u8CurWriLen = ((pEndOfSecPk - pSecPkPos) >= (TS_PK_LEN - u8HdrLen)) ? (TS_PK_LEN - u8HdrLen) : (pEndOfSecPk - pSecPkPos);
        memcpy(pTsPkBuf + u8HdrLen, pSecPkPos, u8CurWriLen);
        pSecPkPos += u8CurWriLen;
        pTsPkBuf += TS_PK_LEN;
    }

    TS_PK_INF("sec to ts done\n");
    return pTsPkBuf - pSecToTsParams->pTsBuf;
}

static UINT _check_pat_sec_params_legal(PAT_SEC_Params_t *pPatSecParams)
{
    UINT16 u16SecLen = 0;

    if (!pPatSecParams)
    {
        TS_PK_INF("pPatSecParams is null err\n");
        return FALSE;
    }

    if ((!pPatSecParams->u8ProgramInfoLen) || (!pPatSecParams->pSecBuf))
    {
        TS_PK_INF("pat sec params err, %d %d\n", (!pPatSecParams->u8ProgramInfoLen), (!pPatSecParams->pSecBuf));
        return FALSE;
    }

    u16SecLen = sizeof(PAT_HEADER_Tag) + (pPatSecParams->u8ProgramInfoLen * 4) + 4;
    if (pPatSecParams->u32SecBufLen < u16SecLen)
    {
        TS_PK_INF("pat table to small err\n");
        return FALSE;
    }

    return TRUE;
}

UINT make_pat_table(PAT_SEC_Params_t *pPatSecParams)
{
    PAT_HEADER_Tag stPatHdr;
    UINT16 u16SecLen = 0;
    int i = 0;
    int pos = 0;
    UINT u32Crc = 0;

    u16SecLen = sizeof(stPatHdr) + (pPatSecParams->u8ProgramInfoLen * 4) + 4;
    if (!(_check_pat_sec_params_legal(pPatSecParams)))
    {
        TS_PK_ERR("pat params err\n");
        return 0;
    }

    memset(&stPatHdr, 0x0, sizeof(stPatHdr));
    stPatHdr.table_id = 0x0;
    stPatHdr.section_syntax_indicator = 1;
    stPatHdr.section_length_hei4 = ((u16SecLen - 3) >> 8) & 0xF;
    stPatHdr.section_length_low8 = (u16SecLen - 3) & 0xFF;
    stPatHdr.transport_stream_id[0] = (pPatSecParams->u16StreamID >> 8) & 0xFF;
    stPatHdr.transport_stream_id[1] = pPatSecParams->u16StreamID & 0xFF;
    stPatHdr.version_number = 0;
    stPatHdr.current_next_indicator = 1;
    stPatHdr.section_number = 0;
    stPatHdr.last_section_number = 0;
    memcpy(pPatSecParams->pSecBuf, &stPatHdr, sizeof(stPatHdr));

    pos = sizeof(stPatHdr);
    for (i = 0; i < pPatSecParams->u8ProgramInfoLen; ++i)
    {
        pPatSecParams->pSecBuf[pos++] = (pPatSecParams->stProgramInfo[i].program_number >> 8) & 0xFF;
        pPatSecParams->pSecBuf[pos++] = (pPatSecParams->stProgramInfo[i].program_number) & 0xFF;
        pPatSecParams->pSecBuf[pos++] = (pPatSecParams->stProgramInfo[i].program_map_PID >> 8) & 0x1F;
        pPatSecParams->pSecBuf[pos++] = (pPatSecParams->stProgramInfo[i].program_map_PID) & 0xFF;
    }

    u32Crc = _calc_crc(pPatSecParams->pSecBuf, u16SecLen - 4);
    pPatSecParams->pSecBuf[pos++] = (u32Crc >> 24) & 0xFF;
    pPatSecParams->pSecBuf[pos++] = (u32Crc >> 16) & 0xFF;
    pPatSecParams->pSecBuf[pos++] = (u32Crc >> 8) & 0xFF;
    pPatSecParams->pSecBuf[pos++] = u32Crc & 0xFF;

    TS_PK_INF("make pat table done\n");
    return u16SecLen;
}

static UINT _check_pmt_sec_params_legal(PMT_SEC_Params_t *pPmtSecParams)
{
    UINT16 u16SecLen = 0;

    if (!pPmtSecParams)
    {
        TS_PK_INF("pPmtSecParams is null err\n");
        return FALSE;
    }

    if ((!pPmtSecParams->u8StreamInfoLen) || (!pPmtSecParams->pSecBuf))
    {
        TS_PK_INF("pat sec params err, %d %d\n", (!pPmtSecParams->u8StreamInfoLen), (!pPmtSecParams->pSecBuf));
        return FALSE;
    }

    u16SecLen = sizeof(PAT_HEADER_Tag) + (pPmtSecParams->u8StreamInfoLen * 5) + 4;
    if (pPmtSecParams->u32SecBufLen < u16SecLen)
    {
        TS_PK_INF("pat table to small err\n");
        return FALSE;
    }

    return TRUE;
}

UINT make_pmt_table(PMT_SEC_Params_t *pPmtSecParams)
{
    PMT_HEADER_Tag stPmtHdr;
    UINT16 u16SecLen = 0;
    int i = 0;
    int pos = 0;
    UINT u32Crc = 0;

    u16SecLen = sizeof(stPmtHdr) + (pPmtSecParams->u8StreamInfoLen * 5) + 4;
    if (!(_check_pmt_sec_params_legal(pPmtSecParams)))
    {
        TS_PK_ERR("pat params err\n");
        return 0;
    }

    memset(&stPmtHdr, 0x0, sizeof(stPmtHdr));
    stPmtHdr.table_id = 0x2;
    stPmtHdr.section_syntax_indicator = 1;
    stPmtHdr.section_length_hei4 = ((u16SecLen - 3) >> 8) & 0xF;
    stPmtHdr.section_length_low8 = (u16SecLen - 3) & 0xFF;
    stPmtHdr.program_number[0] = (pPmtSecParams->program_number >> 8) & 0xFF;
    stPmtHdr.program_number[1] = (pPmtSecParams->program_number) & 0xFF;
    stPmtHdr.version_number = 0;
    stPmtHdr.current_next_indicator = 1;
    stPmtHdr.section_number = 0;
    stPmtHdr.last_section_number = 0;

    stPmtHdr.pcr_pid_hei5 = (pPmtSecParams->pcr_pid >> 8) & 0x1F;
    stPmtHdr.pcr_pid_low8 = (pPmtSecParams->pcr_pid) & 0xFF;
    memcpy(pPmtSecParams->pSecBuf, &stPmtHdr, sizeof(stPmtHdr));

    pos = sizeof(stPmtHdr);
    for (i = 0; i < pPmtSecParams->u8StreamInfoLen; ++i)
    {
        pPmtSecParams->pSecBuf[pos++] = pPmtSecParams->stStreamInfo[i].stream_type;
        pPmtSecParams->pSecBuf[pos++] = (pPmtSecParams->stStreamInfo[i].elementary_PID >> 8) & 0x1F;
        pPmtSecParams->pSecBuf[pos++] = (pPmtSecParams->stStreamInfo[i].elementary_PID) & 0xFF;
        pPmtSecParams->pSecBuf[pos++] = 0x0;
        pPmtSecParams->pSecBuf[pos++] = 0x0;
    }

    u32Crc = _calc_crc(pPmtSecParams->pSecBuf, u16SecLen - 4);
    pPmtSecParams->pSecBuf[pos++] = (u32Crc >> 24) & 0xFF;
    pPmtSecParams->pSecBuf[pos++] = (u32Crc >> 16) & 0xFF;
    pPmtSecParams->pSecBuf[pos++] = (u32Crc >> 8) & 0xFF;
    pPmtSecParams->pSecBuf[pos++] = u32Crc & 0xFF;

    TS_PK_INF("make pmt table done\n");
    return u16SecLen;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
//                  gobal function define
////////////////////////////////////////////////////////////////////////////////////////////////

void dbg_set_8185_ts_pat(FILE *pFile)
{
    BYTE sec[188];
    static BYTE u8ContinuteCounter = 0;
    u8ContinuteCounter++;
    if (u8ContinuteCounter >= 16)
    {
        u8ContinuteCounter = 0;
    }
    memset(sec, 0xFF, sizeof(sec));
    int pos = 0;
    sec[pos++] = 0x47;
    sec[pos++] = 0x40;
    sec[pos++] = 0x00;
    sec[pos++] = 0x10 | u8ContinuteCounter;
    sec[pos++] = 0x00;
    sec[pos++] = 0x00;
    sec[pos++] = 0xB0;
    sec[pos++] = 0x15;
    sec[pos++] = 0x00;
    sec[pos++] = 0x01;
    sec[pos++] = 0xc9;
    sec[pos++] = 0x00;
    sec[pos++] = 0x00;
    sec[pos++] = 0x00;
    sec[pos++] = 0x52;
    sec[pos++] = 0xe3;
    sec[pos++] = 0x34;
    sec[pos++] = 0x00;
    sec[pos++] = 0x53;
    sec[pos++] = 0xe3;
    sec[pos++] = 0x3e;
    sec[pos++] = 0x00;
    sec[pos++] = 0x55;
    sec[pos++] = 0xe3;
    sec[pos++] = 0x52;
    sec[pos++] = 0xdc;
    sec[pos++] = 0xd8;
    sec[pos++] = 0x65;
    sec[pos++] = 0x0c;

    if (188 != fwrite(sec, 1, 188, pFile))
    {
        TS_PK_INF("write pat table err\n");
    }
}

void dbg_set_8185_ts_pmt(FILE *pFile)
{
    BYTE sec[188];
    static BYTE u8ContinuteCounter = 0;
    u8ContinuteCounter++;
    if (u8ContinuteCounter >= 16)
    {
        u8ContinuteCounter = 0;
    }
    memset(sec, 0xFF, sizeof(sec));
    int pos = 0;
    sec[pos++] = 0x47;
    sec[pos++] = 0x43;
    sec[pos++] = 0x34;
    sec[pos++] = 0x10 | u8ContinuteCounter;
    sec[pos++] = 0x00;
    sec[pos++] = 0x02;
    sec[pos++] = 0xB0;
    sec[pos++] = 0x75;
    sec[pos++] = 0x00;
    sec[pos++] = 0x52;
    sec[pos++] = 0xcb;
    sec[pos++] = 0x00;
    sec[pos++] = 0x00;
    sec[pos++] = 0xe3;
    sec[pos++] = 0x35;
    sec[pos++] = 0xf0;
    sec[pos++] = 0x00;
    sec[pos++] = 0x1b;
    sec[pos++] = 0xe3;
    sec[pos++] = 0x35;

    sec[pos++] = 0xf0;
    sec[pos++] = 0x12;
    sec[pos++] = 0x11;
    sec[pos++] = 0x01;
    sec[pos++] = 0xff;
    sec[pos++] = 0x28;
    sec[pos++] = 0x04;
    sec[pos++] = 0x4d;
    sec[pos++] = 0x40;
    sec[pos++] = 0x03;
    sec[pos++] = 0x3f;
    sec[pos++] = 0x2a;
    sec[pos++] = 0x07;
    sec[pos++] = 0xef;
    sec[pos++] = 0xFF;
    sec[pos++] = 0x00;
    sec[pos++] = 0x10;
    sec[pos++] = 0x7a;
    sec[pos++] = 0xc0;
    sec[pos++] = 0xFF;

    sec[pos++] = 0x06;
    sec[pos++] = 0xe3;
    sec[pos++] = 0x36;
    sec[pos++] = 0xf0;
    sec[pos++] = 0x09;
    sec[pos++] = 0x05;
    sec[pos++] = 0x04;
    sec[pos++] = 0x41;
    sec[pos++] = 0x43;
    sec[pos++] = 0x2d;
    sec[pos++] = 0x33;
    sec[pos++] = 0x6a;
    sec[pos++] = 0x01;
    sec[pos++] = 0x00;
    sec[pos++] = 0x06;
    sec[pos++] = 0xe3;
    sec[pos++] = 0x37;
    sec[pos++] = 0xf0;
    sec[pos++] = 0x09;
    sec[pos++] = 0x05;

    sec[pos++] = 0x04;
    sec[pos++] = 0x41;
    sec[pos++] = 0x43;
    sec[pos++] = 0x2d;
    sec[pos++] = 0x33;
    sec[pos++] = 0x6a;
    sec[pos++] = 0x01;
    sec[pos++] = 0x00;
    sec[pos++] = 0x06;
    sec[pos++] = 0xe3;
    sec[pos++] = 0x39;
    sec[pos++] = 0xf0;
    sec[pos++] = 0x0a;
    sec[pos++] = 0x59;
    sec[pos++] = 0x08;
    sec[pos++] = 0x43;
    sec[pos++] = 0x48;
    sec[pos++] = 0x49;
    sec[pos++] = 0x10;
    sec[pos++] = 0x00;

    sec[pos++] = 0x01;
    sec[pos++] = 0x00;
    sec[pos++] = 0x01;
    sec[pos++] = 0x0b;
    sec[pos++] = 0xe3;
    sec[pos++] = 0xe9;
    sec[pos++] = 0xf0;
    sec[pos++] = 0x21;
    sec[pos++] = 0x52;
    sec[pos++] = 0x01;
    sec[pos++] = 0x0b;
    sec[pos++] = 0x14;
    sec[pos++] = 0x0d;
    sec[pos++] = 0x00;
    sec[pos++] = 0x0b;
    sec[pos++] = 0x00;
    sec[pos++] = 0x00;
    sec[pos++] = 0x08;
    sec[pos++] = 0x80;
    sec[pos++] = 0x01;

    sec[pos++] = 0x00;
    sec[pos++] = 0x00;
    sec[pos++] = 0xFF;
    sec[pos++] = 0xFF;
    sec[pos++] = 0xFF;
    sec[pos++] = 0xFF;
    sec[pos++] = 0x66;
    sec[pos++] = 0x06;
    sec[pos++] = 0x01;
    sec[pos++] = 0x06;
    sec[pos++] = 0x01;
    sec[pos++] = 0x01;
    sec[pos++] = 0x00;
    sec[pos++] = 0x00;
    sec[pos++] = 0x13;
    sec[pos++] = 0x05;
    sec[pos++] = 0x00;
    sec[pos++] = 0x00;
    sec[pos++] = 0x00;
    sec[pos++] = 0x01;

    sec[pos++] = 0x00;
    sec[pos++] = 0x14;
    sec[pos++] = 0xf4;
    sec[pos++] = 0x86;
    sec[pos++] = 0xa1;

    if (188 != fwrite(sec, 1, 188, pFile))
    {
        TS_PK_INF("write pmt table err\n");
    }
}

static UINT _check_frame_to_ts_params_legal(H264_FRAME_TO_TS_Params_t *pstFrameToTsParams)
{
    if (!pstFrameToTsParams)
    {
        TS_PK_INF("pstFrameToTsParams is null\n");
        return FALSE;
    }

    if ((!pstFrameToTsParams->pFrameBuf) || (!pstFrameToTsParams->u32FrameLen))
    {
        TS_PK_INF("frame to ts params ilegal%d %d, %d\n",
            (!pstFrameToTsParams->pFrameBuf),
            (!pstFrameToTsParams->u32FrameLen));
        return FALSE;
    }

    return TRUE;
}

#define SAVE_TS_TO_FILE (0)

#if (SAVE_TS_TO_FILE)
static FILE *pFile = NULL;
#endif
UINT h264_frame_to_ts(H264_FRAME_TO_TS_Params_t *pstFrameToTsParams)
{
    static BYTE u8PesLastContinityCounter = 0;
    static BYTE u8PmtLastContinityCounter = 0;
    static BYTE u8PatLastContinityCounter = 0;

    UINT u32PkLen = 0;
    ES_TO_PES_Params_t stVesToPesParams;
    PES_TO_TS_Params_t stPesToTsParams;
    UINT u32FrameRate = 0;
    UINT64 u64FrameIdx = 0;
    UINT64 u64Pts = 0;

    if (!_check_frame_to_ts_params_legal(pstFrameToTsParams))
    {
        TS_PK_ERR("frame to ts params err\n");
        return 0;
    }

    #if (SAVE_TS_TO_FILE)
    if (!pFile)
    {
        pFile = fopen("newpk.ts", "w+");
    }
    #endif

    if (pstFrameToTsParams->bRestart)
    {
        u8PesLastContinityCounter = 0;
        u8PmtLastContinityCounter = 0;
        u8PatLastContinityCounter = 0;
    }

    u32FrameRate = pstFrameToTsParams->u32FrameRate;
    u64FrameIdx = pstFrameToTsParams->u64FrameIdx;
    u64Pts = pstFrameToTsParams->u64Pts;
    ///set pcr
    if (u64Pts == (UINT64)(-1))
    {
        u64Pts = u64FrameIdx * 90000.0 / u32FrameRate;
    }

    u32FrameRate = u32FrameRate ? u32FrameRate : 1;
    if (0 == ((u64FrameIdx * 3) % u32FrameRate))
    {
        UINT u16ProgramNumber = 0;
        static BYTE auSecBuf[100 * TS_PK_LEN];
        static BYTE auTsBuf[100 * TS_PK_LEN];
        PAT_SEC_Params_t stPatSecParams;
        PMT_SEC_Params_t stPmtSecParams;
        SEC_TO_TS_Params_t stSecToTsParams;

        u16ProgramNumber = 0x1;
        ///fill pat and pmt
        memset(&stPatSecParams, 0x0, sizeof(stPatSecParams));
        stPatSecParams.u16StreamID = 0;
        stPatSecParams.u8ProgramInfoLen = 1;
        stPatSecParams.stProgramInfo[0].program_number = u16ProgramNumber;
        stPatSecParams.stProgramInfo[0].program_map_PID = PMT_PID;
        stPatSecParams.pSecBuf = auSecBuf;
        stPatSecParams.u32SecBufLen = sizeof(auSecBuf);
        u32PkLen = make_pat_table(&stPatSecParams);
        if (!u32PkLen)
        {
            TS_PK_INF("make pat table err\n");
            return 0;
        }

        memset(&stSecToTsParams, 0x0, sizeof(stSecToTsParams));
        stSecToTsParams.u16Pid = 0x0;
        stSecToTsParams.pSecBuf = stPatSecParams.pSecBuf;
        stSecToTsParams.u32SecLen = u32PkLen;
        stSecToTsParams.pTsBuf = auTsBuf;
        stSecToTsParams.u32TsBufLen = sizeof(auTsBuf);
        stSecToTsParams.u8ContinueCount = u8PatLastContinityCounter;
        u32PkLen = make_section_to_ts(&stSecToTsParams);
        u8PatLastContinityCounter = stSecToTsParams.u8ContinueCount;

#if (SAVE_TS_TO_FILE)
        if (u32PkLen != fwrite(stSecToTsParams.pTsBuf, 1, u32PkLen, pFile))
        {
            TS_PK_INF("write pat table err\n");
            return 0;
        }
#endif
        if (!PutTsToRingBuffer(stSecToTsParams.pTsBuf, u32PkLen))
        {
            TS_PK_ERR("no ts buffer to save header data\n");
        }

        memset(&stPmtSecParams, 0x0, sizeof(stPmtSecParams));
        stPmtSecParams.u8StreamInfoLen = 2;
        stPmtSecParams.stStreamInfo[0].stream_type = _es_format_to_stream_type(ES_FORMAT_H264);
        stPmtSecParams.stStreamInfo[0].elementary_PID = VID_PID;
        stPmtSecParams.stStreamInfo[1].stream_type = _es_format_to_stream_type(ES_FORMAT_AAC);
        stPmtSecParams.stStreamInfo[1].elementary_PID = AUD_PID;
        stPmtSecParams.pcr_pid = PCR_PID;
        stPmtSecParams.program_number = u16ProgramNumber;
        stPmtSecParams.pSecBuf = auSecBuf;
        stPmtSecParams.u32SecBufLen = sizeof(auSecBuf);
        u32PkLen = make_pmt_table(&stPmtSecParams);

        if (!u32PkLen)
        {
            TS_PK_INF("make pmt table err\n");
            return 0;
        }

        memset(&stSecToTsParams, 0x0, sizeof(stSecToTsParams));
        stSecToTsParams.u16Pid = PMT_PID;
        stSecToTsParams.pSecBuf = stPmtSecParams.pSecBuf;
        stSecToTsParams.u32SecLen = u32PkLen;
        stSecToTsParams.pTsBuf = auTsBuf;
        stSecToTsParams.u32TsBufLen = sizeof(auTsBuf);
        stSecToTsParams.u8ContinueCount = u8PmtLastContinityCounter;
        u32PkLen = make_section_to_ts(&stSecToTsParams);
        u8PmtLastContinityCounter = stSecToTsParams.u8ContinueCount;

        #if (SAVE_TS_TO_FILE)
        if (u32PkLen != fwrite(stSecToTsParams.pTsBuf, 1, u32PkLen, pFile))
        {
            TS_PK_INF("write pmt table err\n");
            return 0;
        }
        #endif
        
        if (!PutTsToRingBuffer(stSecToTsParams.pTsBuf, u32PkLen))
        {
            TS_PK_ERR("no ts buffer to save header data\n");
        }
    }
    else if (0)
    {
        #if (SAVE_TS_TO_FILE)
        dbg_set_8185_ts_pat(pFile);
        dbg_set_8185_ts_pmt(pFile);
        #endif
    }

    ///pes make
    memset(&stVesToPesParams, 0x0, sizeof(stVesToPesParams));
    stVesToPesParams.pEsBuf = pstFrameToTsParams->pFrameBuf;
    stVesToPesParams.u32DataLen = pstFrameToTsParams->u32FrameLen;
    stVesToPesParams.u32FrameIndex = u64FrameIdx;
    stVesToPesParams.u32FrameRate = u32FrameRate;
    stVesToPesParams.eEsFormat = ES_FORMAT_H264;
    stVesToPesParams.u32PesBufLen = 2 * 1024 * 1024;
    stVesToPesParams.pPesBuf = (BYTE *)malloc(stVesToPesParams.u32PesBufLen);
    stVesToPesParams.u64Pts = u64Pts;
    u32PkLen = make_es_to_pes(&stVesToPesParams);

    #if (SAVE_TS_TO_FILE)
    static FILE *pPesFile = NULL;
    if (!pPesFile)
    {
        pPesFile = fopen("newpk.pes", "w+");
    }
    fwrite(stVesToPesParams.pPesBuf, 1, u32PkLen, pPesFile);
    #endif

    memset(&stPesToTsParams, 0x0, sizeof(stPesToTsParams));
    stPesToTsParams.u16Pid = VID_PID;
    stPesToTsParams.pPesBuf = stVesToPesParams.pPesBuf;
    stPesToTsParams.u32PesPkLen = u32PkLen;
    stPesToTsParams.u32TsBufLen = 2 * 1024 * 1024;;
    stPesToTsParams.pTsBuf = (BYTE *)malloc(stPesToTsParams.u32TsBufLen);
    stPesToTsParams.u8ContinueCount = u8PesLastContinityCounter;
    stPesToTsParams.eEsFrameType = stVesToPesParams.eEsFrameType;

    ///stPesToTsParams.u32Pcr = -1;
    stPesToTsParams.u32Pcr = u64Pts;
    TS_PK_INF("make pcr:%d\n", stPesToTsParams.u32Pcr);
    u32PkLen = make_pes_to_ts(&stPesToTsParams);
    u8PesLastContinityCounter = stPesToTsParams.u8ContinueCount;

    #if (SAVE_TS_TO_FILE)
    if (u32PkLen != fwrite(stPesToTsParams.pTsBuf, 1, u32PkLen, pFile))
    {
        TS_PK_INF("write pmt table err\n");
        return 0;
    }
    #endif
    if (!PutTsToRingBuffer(stPesToTsParams.pTsBuf, u32PkLen))
    {
        TS_PK_ERR("no ts buffer to save video data\n");
    }

    free(stPesToTsParams.pTsBuf);
    stPesToTsParams.pTsBuf = NULL;

    free(stVesToPesParams.pPesBuf);
    stVesToPesParams.pPesBuf = NULL;

    TS_PK_INF("h264 to ts done\n");
    return u32PkLen;
}

UINT aac_frame_to_ts(AAC_FRAME_TO_TS_Params_t *pstFrameToTsParams)
{
    UINT u32PkLen = 0;
    static BYTE u8PesLastContinityCounter = 0;

    ES_TO_PES_Params_t stEsToPesParams;
    PES_TO_TS_Params_t stPesToTsParams;
    UINT u32AudioSampleRate = 0;
    UINT u32AudioSamplesPerFrame = 0;
    UINT64 u64FrameIdx = 0;
    UINT64 u64Pts = 0;

    //if (!_check_frame_to_ts_params_legal(pstFrameToTsParams))
    //{
    //    TS_PK_ERR("frame to ts params err\n");
    //    return 0;
    //}

    if (pstFrameToTsParams->bRestart)
    {
        u8PesLastContinityCounter = 0;
    }

    u32AudioSampleRate = pstFrameToTsParams->u32AudioSampleRate;
    u32AudioSamplesPerFrame = pstFrameToTsParams->u32AudioSamplesPerFrame;
    u64FrameIdx = pstFrameToTsParams->u64FrameIdx;
    u64Pts = pstFrameToTsParams->u64Pts;
    ///set pcr
    if (u64Pts == (UINT64)(-1))
    {
        ///48k ->  1000ms * 1024 (sample) / 48000 (48k) = 21.333333
        ///audio_pts = (90000 * audio_samples_per_frame) / audio_sample_rate = (90000 * 1024) / 48000
        u64Pts = u64FrameIdx * (90000 * u32AudioSamplesPerFrame / u32AudioSampleRate);
    }

    ///pes make
    memset(&stEsToPesParams, 0x0, sizeof(stEsToPesParams));
    stEsToPesParams.pEsBuf = pstFrameToTsParams->pFrameBuf;
    stEsToPesParams.u32DataLen = pstFrameToTsParams->u32FrameLen;
    stEsToPesParams.eEsFormat = ES_FORMAT_AAC;
    stEsToPesParams.u32PesBufLen = 2 * 1024 * 1024;
    stEsToPesParams.pPesBuf = (BYTE *)malloc(stEsToPesParams.u32PesBufLen);
    stEsToPesParams.u64Pts = u64Pts;
    u32PkLen = make_es_to_pes(&stEsToPesParams);

    #if (SAVE_TS_TO_FILE)
    static FILE *pPesFile = NULL;
    if (!pPesFile)
    {
        pPesFile = fopen("aac.pes", "w+");
    }
    fwrite(stEsToPesParams.pPesBuf, 1, u32PkLen, pPesFile);
    #endif

    memset(&stPesToTsParams, 0x0, sizeof(stPesToTsParams));
    stPesToTsParams.u16Pid = AUD_PID;
    stPesToTsParams.pPesBuf = stEsToPesParams.pPesBuf;
    stPesToTsParams.u32PesPkLen = u32PkLen;
    stPesToTsParams.u32TsBufLen = 2 * 1024 * 1024;;
    stPesToTsParams.pTsBuf = (BYTE *)malloc(stPesToTsParams.u32TsBufLen);
    stPesToTsParams.u8ContinueCount = u8PesLastContinityCounter;
    u32PkLen = make_pes_to_ts(&stPesToTsParams);
    u8PesLastContinityCounter = stPesToTsParams.u8ContinueCount;
    #if (SAVE_TS_TO_FILE)
    if (!pFile)
    {
        pFile = fopen("newpk.ts", "w+");
    }
    if (u32PkLen != fwrite(stPesToTsParams.pTsBuf, 1, u32PkLen, pFile))
    {
        TS_PK_INF("write pmt table err\n");
        return 0;
    }
    #endif
    if (!PutTsToRingBuffer(stPesToTsParams.pTsBuf, u32PkLen))
    {
        TS_PK_ERR("no ts buffer to save audio data\n");
    }

    free(stPesToTsParams.pTsBuf);
    stPesToTsParams.pTsBuf = NULL;

    free(stEsToPesParams.pPesBuf);
    stEsToPesParams.pPesBuf = NULL;

    TS_PK_INF("aac frame to ts done\n");
    return u32PkLen;
}

//////////////////////////////////////////////////////////////////////////
///MAIN
#define TRUE (1)
#define FALSE (0)
typedef struct
{
    BYTE *pu8Buf;
    UINT u32BufLen;
    UINT u32WritePoint;
    UINT u32ReadPoint;
    UINT u32PreReadPoint;
    UINT u32PreWritePoint;
}RINGBUFFER_t;

typedef struct
{
    UINT u32Len;
    UINT64 u64Pts;
}BUFFER_HEADER_t;

static RINGBUFFER_t _stVidRingBuf;
static RINGBUFFER_t _stAudRingBuf;
static RINGBUFFER_t _stTsOutputRingBuf;
static BOOL _bRunTsBuilder = FALSE;
///use origninal pts mode TRUE, trans es to ts as soon as possible
static BOOL _bUseOriginalPts = FALSE;
static UINT _u32OutOfSyncMs = 90000 * 5;///5s
static pthread_t _thrEsToTs = 0;
static UINT64 INVALID_PTS  = (UINT64)(-1);
static PRE_SET_AVINFO_t _stPreSetAvInfo = {25, 48000, 1024};
static pthread_mutex_t _muxTsRingBuf = PTHREAD_MUTEX_INITIALIZER;

BOOL InitRingBuffer(RINGBUFFER_t *pstRingBuf, UINT u32BufLen)
{
    pstRingBuf->u32BufLen = u32BufLen;
    pstRingBuf->pu8Buf = (BYTE *)malloc(pstRingBuf->u32BufLen);
    pstRingBuf->u32ReadPoint = pstRingBuf->u32WritePoint = 0;
    pstRingBuf->u32PreReadPoint = pstRingBuf->u32PreWritePoint = 0;
    return (NULL != pstRingBuf->pu8Buf);
}

void DeInitRingBuffer(RINGBUFFER_t *pstRingBuf)
{
    free(pstRingBuf->pu8Buf);
    pstRingBuf->pu8Buf = NULL;
    pstRingBuf->u32ReadPoint = pstRingBuf->u32WritePoint = 0;
    pstRingBuf->u32PreReadPoint = pstRingBuf->u32PreWritePoint = 0;
    pstRingBuf->u32BufLen = 0;
}

BOOL WriteRingBuffer(RINGBUFFER_t *pstRingBuf, BUFFER_HEADER_t *pstBufHdr, BYTE *pu8Data, UINT u32DataLen)
{
    UINT u32WP = pstRingBuf->u32WritePoint;
    UINT u32RP = pstRingBuf->u32ReadPoint;
    UINT REVERT_BYTE = 1;
    UINT CURRENT_WRITE_SIZE = sizeof(BUFFER_HEADER_t) + u32DataLen + REVERT_BYTE;

    if (CURRENT_WRITE_SIZE >= pstRingBuf->u32BufLen)
    {
        TS_PK_INF("Not Buffer Drop A:%p\n", pstRingBuf);
        return FALSE;
    }

    if (u32WP > u32RP)
    {
        ///----------R---------W----
        UINT RingBufFreeSize = pstRingBuf->u32BufLen + u32RP - u32WP;
        if (CURRENT_WRITE_SIZE >= RingBufFreeSize)
        {
            TS_PK_INF("Not Buffer Drop B:%p\n", pstRingBuf);
            return FALSE;
        }
    }
    else if (u32WP < u32RP)
    {
        ///----------W---------R----
        UINT RingBufFreeSize = u32RP - u32WP;
        if (CURRENT_WRITE_SIZE >= RingBufFreeSize)
        {
            TS_PK_INF("Not Buffer Drop C:%p\n", pstRingBuf);
            return FALSE;
        }
    }

    ///Have free point
    UINT u32OffEnd = pstRingBuf->u32BufLen - u32WP;
    if (u32OffEnd >= sizeof(*pstBufHdr))
    {
        ///>= header len
        ///save header
        memcpy(pstRingBuf->pu8Buf + u32WP, pstBufHdr, sizeof(*pstBufHdr));
        ///save data
        if ((u32WP + sizeof(*pstBufHdr)) == pstRingBuf->u32BufLen)
        {
            memcpy(pstRingBuf->pu8Buf, pu8Data, u32DataLen);
            u32WP = u32DataLen;
        }
        else
        {
            UINT u32FirstWrite = pstRingBuf->u32BufLen - u32WP - sizeof(*pstBufHdr);
            if (u32FirstWrite >= u32DataLen)
            {
                memcpy(pstRingBuf->pu8Buf + u32WP + sizeof(*pstBufHdr), pu8Data, u32DataLen);
                u32WP = u32WP + sizeof(*pstBufHdr) + u32DataLen;
            }
            else
            {
                memcpy(pstRingBuf->pu8Buf + u32WP + sizeof(*pstBufHdr), pu8Data, u32FirstWrite);
                memcpy(pstRingBuf->pu8Buf, pu8Data + u32FirstWrite, u32DataLen - u32FirstWrite);
                u32WP = u32DataLen - u32FirstWrite;
            }
        }
    }
    else
    {
        ///< header len
        ///save header
        memcpy(pstRingBuf->pu8Buf + u32WP, pstBufHdr, u32OffEnd);
        memcpy(pstRingBuf->pu8Buf, ((BYTE *)pstBufHdr) + u32OffEnd, sizeof(*pstBufHdr) - u32OffEnd);
        ///save data
        memcpy(pstRingBuf->pu8Buf + sizeof(*pstBufHdr) - u32OffEnd, pu8Data, u32DataLen);
        u32WP = u32DataLen + sizeof(*pstBufHdr) - u32OffEnd;
    }

    pstRingBuf->u32WritePoint = u32WP;
    return TRUE;
}

BOOL ReadRingBuffer(RINGBUFFER_t *pstRingBuf, BUFFER_HEADER_t *pstBufHdr, BYTE *pu8Data, UINT u32Len)
{
    UINT u32WP = pstRingBuf->u32WritePoint;
    UINT u32RP = pstRingBuf->u32ReadPoint;
    ///read header
    if (u32RP == u32WP)
    {
        TS_PK_INF("No Read Data\n");
        return FALSE;
    }

    UINT u32OffEnd = pstRingBuf->u32BufLen - u32RP;
    if (u32OffEnd >= sizeof(*pstBufHdr))
    {
        ///read header, >= header len
        memcpy(pstBufHdr, pstRingBuf->pu8Buf + u32RP, sizeof(*pstBufHdr));
        if (u32Len < pstBufHdr->u32Len)
        {
            TS_PK_ERR("Save Data Buffer Too Small A, %d, %d\n", pstBufHdr->u32Len, u32Len);
            return FALSE;
        }

        if (u32OffEnd == sizeof(*pstBufHdr))
        {
            memcpy(pu8Data, pstRingBuf->pu8Buf, pstBufHdr->u32Len);
            u32RP = pstBufHdr->u32Len;
        }
        else
        {
            UINT u32ReadFirst = u32OffEnd - sizeof(*pstBufHdr);
            if (u32ReadFirst >= pstBufHdr->u32Len)
            {
                memcpy(pu8Data, pstRingBuf->pu8Buf + u32RP + sizeof(*pstBufHdr), pstBufHdr->u32Len);
                u32RP = u32RP + sizeof(*pstBufHdr) + pstBufHdr->u32Len;
            }
            else
            {
                memcpy(pu8Data, pstRingBuf->pu8Buf + u32RP + sizeof(*pstBufHdr), u32ReadFirst);
                memcpy(pu8Data + u32ReadFirst, pstRingBuf->pu8Buf, pstBufHdr->u32Len - u32ReadFirst);
                u32RP = pstBufHdr->u32Len - u32ReadFirst;
            }
        }
    }
    else
    {
        ///read header, < header len
        memcpy(pstBufHdr, pstRingBuf->pu8Buf + u32RP, u32OffEnd);
        memcpy(((BYTE *)pstBufHdr) + u32OffEnd, pstRingBuf->pu8Buf, sizeof(*pstBufHdr) - u32OffEnd);
        if (u32Len < pstBufHdr->u32Len)
        {
            TS_PK_ERR("Save Data Buffer Too Small B, %d, %d\n", pstBufHdr->u32Len, u32Len);
            return FALSE;
        }
        memcpy(pu8Data, pstRingBuf->pu8Buf + sizeof(*pstBufHdr) - u32OffEnd, pstBufHdr->u32Len);
        u32RP = pstBufHdr->u32Len + sizeof(*pstBufHdr) - u32OffEnd;
    }

    pstRingBuf->u32PreReadPoint = u32RP;
    return TRUE;
}

void UpdateRingBufferReadPointer(RINGBUFFER_t *pstRingBuf)
{
    pstRingBuf->u32ReadPoint = pstRingBuf->u32PreReadPoint;
}

void UpdateRingBufferWritePointer(RINGBUFFER_t *pstRingBuf)
{
    pstRingBuf->u32WritePoint = pstRingBuf->u32PreWritePoint;
}


BOOL ResetRingBuffer(RINGBUFFER_t *pstRingBuf)
{
    pstRingBuf->u32ReadPoint = pstRingBuf->u32WritePoint = 0;
    pstRingBuf->u32PreReadPoint = pstRingBuf->u32PreWritePoint = 0;
    return TRUE;
}

BOOL GetNextAudEs(BYTE *pu8Buf, UINT u32BufLen, UINT64 *pu64DataPts, UINT *pu32DataLen)
{
    BUFFER_HEADER_t stBufHdr;

    memset(&stBufHdr, 0x0, sizeof(stBufHdr));
    if (ReadRingBuffer(&_stAudRingBuf, &stBufHdr, pu8Buf, u32BufLen))
    {
        *pu64DataPts = stBufHdr.u64Pts;
        *pu32DataLen = stBufHdr.u32Len;
        return TRUE;
    }

    return FALSE;
}

BOOL GetNextVidEs(BYTE *pu8Buf, UINT u32BufLen, UINT64 *pu64DataPts, UINT *pu32DataLen)
{
    BUFFER_HEADER_t stBufHdr;

    memset(&stBufHdr, 0x0, sizeof(stBufHdr));
    if (ReadRingBuffer(&_stVidRingBuf, &stBufHdr, pu8Buf, u32BufLen))
    {
        *pu64DataPts = stBufHdr.u64Pts;
        *pu32DataLen = stBufHdr.u32Len;
        return TRUE;
    }

    return FALSE;
}

UINT64 ABS(UINT64 u64APts, UINT64 u64BPts)
{
    if ((INVALID_PTS == u64APts) || (INVALID_PTS == u64BPts))
    {
        return 0;
    }
    if (u64APts > u64BPts)
    {
        return u64APts - u64BPts;
    }

    return u64BPts - u64APts;
}

BOOL VidEsToTs(BYTE *pu8Data, UINT u32Len, UINT64 u64Pts)
{
    H264_FRAME_TO_TS_Params_t stFrmToTsParams;

    ///frame align check
    if ((pu8Data[0] != 0x00) || (pu8Data[1] != 0x00) || (pu8Data[2] != 0x00) || (pu8Data[3] != 0x01))
    {
        TS_PK_ERR("H264 Frame Not Align, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
               pu8Data[0], pu8Data[1], pu8Data[2], pu8Data[3]);
        return FALSE;
    }
    memset(&stFrmToTsParams, 0x0, sizeof(stFrmToTsParams));
    stFrmToTsParams.pFrameBuf = pu8Data;
    stFrmToTsParams.u32FrameLen = u32Len;
    stFrmToTsParams.u64Pts = u64Pts;
    stFrmToTsParams.u32FrameRate = _stPreSetAvInfo.u32VidFrameRate;
    stFrmToTsParams.u64FrameIdx = 0;
    stFrmToTsParams.bRestart = 0;
    h264_frame_to_ts(&stFrmToTsParams);
    return TRUE;
}

BOOL AudEsToTs(BYTE *pu8Data, UINT u32Len, UINT64 u64Pts)
{
    AAC_FRAME_TO_TS_Params_t stFrameToTsParams;

    ///frame align check
    if ((pu8Data[0] != 0xFF) || (pu8Data[1] != 0xF1))
    {
        TS_PK_ERR("AAC Frame Not Align, 0x%02x, 0x%02x\n", pu8Data[0], pu8Data[1]);
        return FALSE;
    }
    memset(&stFrameToTsParams, 0x0, sizeof(AAC_FRAME_TO_TS_Params_t));
    stFrameToTsParams.pFrameBuf = pu8Data;
    stFrameToTsParams.u32FrameLen = u32Len;
    stFrameToTsParams.u32AudioSampleRate = _stPreSetAvInfo.u32AudSampleRate;
    stFrameToTsParams.u32AudioSamplesPerFrame = _stPreSetAvInfo.u32AudSamplePerFrame;
    stFrameToTsParams.u64FrameIdx = 0;
    stFrameToTsParams.bRestart = 0;
    ///u64Pts += (90000 * 1024) / 48000;
    stFrameToTsParams.u64Pts = u64Pts;
    aac_frame_to_ts(&stFrameToTsParams);
    return TRUE;
}

static void * TsBuilder(void *params)
{

    ///audio params
    BYTE *pu8AudBuf = NULL;
    UINT u32AudLen = 64 * 1024;
    UINT64 u64CurAudPts = INVALID_PTS;
    UINT u32CurAudLen = 0;
    UINT64 u64PreAudPts = INVALID_PTS;

    ///video params
    BYTE *pu8VidBuf = NULL;
    UINT u32VidLen = 1 * 1024 * 1024;
    UINT64 u64CurVidPts = INVALID_PTS;
    UINT u32CurVidLen = 0;
    UINT64 u64PreVidPts = INVALID_PTS;

    pu8AudBuf = (BYTE *)malloc(u32AudLen);
    pu8VidBuf = (BYTE *)malloc(u32VidLen);

    ///av sync before record
    _bRunTsBuilder = TRUE;
    while(_bRunTsBuilder)
    {
		/*
        if (!GetNextAudEs(pu8AudBuf, u32AudLen, &u64CurAudPts, &u32CurAudLen))
        {
            ResetRingBuffer(&_stVidRingBuf);
            TS_PK_INF("audio not ready, reset video buffer now, wait audio again\n");
            usleep(5 * 1000);
            continue;
        }
		*/

        if (!GetNextVidEs(pu8VidBuf, u32VidLen, &u64CurVidPts, &u32CurVidLen))
        {
            ResetRingBuffer(&_stVidRingBuf);
            TS_PK_INF("video not ready, reset audio buffer now, wait video again\n");
            usleep(5 * 1000);
            continue;
        }

        TS_PK_INF("start sync done, a/v ready\n");
        break;
    }

    ///VIDEO FRAME MASTER
    while (_bRunTsBuilder)
    {
        ///GET VIDEO FRAME
        if (!GetNextVidEs(pu8VidBuf, u32VidLen, &u64CurVidPts, &u32CurVidLen))
        {
            usleep(5 * 1000);
            continue;
        }
        ///VIDEO ES TO TS
        if ((ABS(u64CurVidPts, u64PreAudPts) < _u32OutOfSyncMs) || _bUseOriginalPts)
        {
            VidEsToTs(pu8VidBuf, u32CurVidLen, u64CurVidPts);
            u64PreVidPts = u64CurVidPts;
            UpdateRingBufferReadPointer(&_stVidRingBuf);
            #if (0)
            static FILE *fwp = NULL;
            if (NULL == fwp)
                fwp = fopen("./vides.dump.avc", "w+");
            fwrite(pu8VidBuf, 1, u32CurVidLen, fwp);
            fflush(fwp);
            #endif
        }
        else
        {
            TS_PK_ERR("Av Sync VES Wait AES, V:0x%llx, A:0x%llx\n", u64CurVidPts, u64PreAudPts);
        }

        ///GET AUDIO FRAME
		/*
        do
        {
            if (!GetNextAudEs(pu8AudBuf, u32AudLen, &u64CurAudPts, &u32CurAudLen))
            {
                usleep(5 * 1000);
                break;
            }

            if ((ABS(u64CurAudPts, u64PreVidPts) < _u32OutOfSyncMs) || _bUseOriginalPts)
            {
                ///AUDIO ES TO TS
                AudEsToTs(pu8AudBuf, u32CurAudLen, u64CurAudPts);
                u64PreAudPts = u64CurAudPts;
                UpdateRingBufferReadPointer(&_stAudRingBuf);
            }
            else
            {
                TS_PK_ERR("Av Sync AES Wait VES, V:0x%llx, A:0x%llx\n", u64CurAudPts, u64PreVidPts);
            }
        } while (u64CurAudPts < u64PreVidPts);
		*/
    }

    free(pu8AudBuf);
    pu8AudBuf = NULL;
    free(pu8VidBuf);
    pu8VidBuf = NULL;
    return NULL;
}


void TsBuildInit(void)
{
    InitRingBuffer(&_stVidRingBuf, 2 * 1024 * 1024);
    InitRingBuffer(&_stAudRingBuf, 1 * 1024 * 1024);
    InitRingBuffer(&_stTsOutputRingBuf, 3 * 1024 * 1024);
}

void TsBuildDeInit(void)
{
    DeInitRingBuffer(&_stAudRingBuf);
    DeInitRingBuffer(&_stVidRingBuf);
    DeInitRingBuffer(&_stTsOutputRingBuf);
}

int CreateEsToTsThread(void)
{
    if (0 == pthread_create(&_thrEsToTs, NULL, TsBuilder, NULL))
    {
        TsBuildInit();
        return TRUE;
    }
    return FALSE;
}

int DestroyEsToTsThread(void)
{
    _bRunTsBuilder = FALSE;
    pthread_join(_thrEsToTs, NULL);
    TsBuildDeInit();
    return FALSE;
}

UINT64 CalcAudPts(void)
{
    UINT64 u64Pts = INVALID_PTS;
    return u64Pts;
}

int InsertAudEsToRingBuffer(unsigned char *pu8Data, unsigned int u32DataLen, unsigned long long u64Pts)
{
    BUFFER_HEADER_t stBufHdr;
    if (INVALID_PTS == u64Pts)
    {
        u64Pts = CalcAudPts();
    }

    stBufHdr.u64Pts = u64Pts;
    stBufHdr.u32Len = u32DataLen;
    return WriteRingBuffer(&_stAudRingBuf, &stBufHdr, pu8Data, u32DataLen);
}

int InsertVidEsToRingBuffer(unsigned char *pu8Data, unsigned int u32DataLen, unsigned long long u64Pts)
{
    BUFFER_HEADER_t stBufHdr;
    stBufHdr.u64Pts = u64Pts;
    stBufHdr.u32Len = u32DataLen;
    BOOL bRet = WriteRingBuffer(&_stVidRingBuf, &stBufHdr, pu8Data, u32DataLen);
    #if (0)
    if (bRet)
    {
        static FILE *fwp = NULL;
        if (NULL == fwp)
            fwp = fopen("./vidwries.dump.avc", "w+");
        fwrite(pu8Data, 1, stBufHdr.u32Len, fwp);
    }
    #endif
    return bRet;
}

BOOL PutTsToRingBuffer(BYTE *pu8Data, UINT u32DataLen)
{
    BUFFER_HEADER_t stBufHdr;
    BOOL bRet = FALSE;

    stBufHdr.u64Pts = 0;
    stBufHdr.u32Len = u32DataLen;
    pthread_mutex_lock(&_muxTsRingBuf);
    bRet = WriteRingBuffer(&_stTsOutputRingBuf, &stBufHdr, pu8Data, u32DataLen);
    pthread_mutex_unlock(&_muxTsRingBuf);
    return bRet;
}

int GetTsPacket(unsigned char *pu8Buf, unsigned int u32BufLen, unsigned int *pu32DataLen)
{
    BUFFER_HEADER_t stBufHdr;

    memset(&stBufHdr, 0x0, sizeof(stBufHdr));
    pthread_mutex_lock(&_muxTsRingBuf);
    if (ReadRingBuffer(&_stTsOutputRingBuf, &stBufHdr, pu8Buf, u32BufLen))
    {
        pthread_mutex_unlock(&_muxTsRingBuf);
        *pu32DataLen = stBufHdr.u32Len;
        UpdateRingBufferReadPointer(&_stTsOutputRingBuf);
        return TRUE;
    }

    pthread_mutex_unlock(&_muxTsRingBuf);
    return FALSE;
}

void PreSetAVInfo(PRE_SET_AVINFO_t *pstPreSetAVInfo)
{
    _stPreSetAvInfo.u32AudSamplePerFrame = pstPreSetAVInfo->u32AudSamplePerFrame;
    _stPreSetAvInfo.u32AudSampleRate = pstPreSetAVInfo->u32AudSampleRate;
    _stPreSetAvInfo.u32VidFrameRate = pstPreSetAVInfo->u32VidFrameRate;
}
#if (0)
int main(int argc, char *argv[])
{
    BYTE auFrameBuf[5];
    UINT u32FrameLen = 5;
    UINT64 u64FrameIdx = 0;
    UINT u32FrameRate = 25;
    UINT64 u64Pts = 0x1256FAFB;

    auFrameBuf[0] = 0xFA;
    auFrameBuf[1] = 0xE1;
    auFrameBuf[2] = 0xC2;
    auFrameBuf[3] = 0xB4;
    auFrameBuf[4] = 0xA7;
    int i = 1000;
    while (i-- > 0)
    printf("result:%d\n",
        h264_frame_to_ts(auFrameBuf, u32FrameLen, u64FrameIdx, u32FrameRate, u64Pts)
        );
    return 0;
}
#endif
