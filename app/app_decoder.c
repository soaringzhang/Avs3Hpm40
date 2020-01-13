/* ====================================================================================================================

  The copyright in this software is being made available under the License included below.
  This software may be subject to other third party and contributor rights, including patent rights, and no such
  rights are granted under this license.

  Copyright (c) 2018, HUAWEI TECHNOLOGIES CO., LTD. All rights reserved.
  Copyright (c) 2018, SAMSUNG ELECTRONICS CO., LTD. All rights reserved.
  Copyright (c) 2018, PEKING UNIVERSITY SHENZHEN GRADUATE SCHOOL. All rights reserved.
  Copyright (c) 2018, PENGCHENG LABORATORY. All rights reserved.

  Redistribution and use in source and binary forms, with or without modification, are permitted only for
  the purpose of developing standards within Audio and Video Coding Standard Workgroup of China (AVS) and for testing and
  promoting such standards. The following conditions are required to be met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
      the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
      the following disclaimer in the documentation and/or other materials provided with the distribution.
    * The name of HUAWEI TECHNOLOGIES CO., LTD. or SAMSUNG ELECTRONICS CO., LTD. may not be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

* ====================================================================================================================
*/

#define _CRT_SECURE_NO_WARNINGS

#define DECODING_TIME_TEST 1

#include "com_typedef.h"
#include "app_util.h"
#include "app_args.h"
#include "dec_def.h"
#if LINUX
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>


void handler(int sig)
{
    void *array[10];
    size_t size;
    // get void*'s for all entries on the stack
    size = backtrace(array, 10);
    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}
#endif

#define VERBOSE_FRAME              VERBOSE_1

#define MAX_BS_BUF                 (32*1024*1024) /* byte */

static char op_fname_inp[256] = "\0";
static char op_fname_out[256] = "\0";
static int  op_max_frm_num = 0;
static int  op_use_pic_signature = 0;
static int  op_bit_depth_output = 8;
#if LIBVC_ON
static char op_fname_inp_libpics[256] = "\0"; /* bitstream of libpics */
static char op_fname_out_libpics[256] = "\0"; /* reconstructed yuv of libpics */
#endif

typedef enum _STATES
{
    STATE_DECODING,
    STATE_BUMPING
} STATES;

typedef enum _OP_FLAGS
{
    OP_FLAG_FNAME_INP,
    OP_FLAG_FNAME_OUT,
    OP_FLAG_MAX_FRM_NUM,
    OP_FLAG_USE_PIC_SIGN,
    OP_FLAG_VERBOSE,
#if LIBVC_ON
    OP_FLAG_FNAME_INP_LIBPICS,
    OP_FLAG_FNAME_OUT_LIBPICS,
#endif
    OP_FLAG_MAX
} OP_FLAGS;

static int op_flag[OP_FLAG_MAX] = {0};

static COM_ARGS_OPTION options[] = \
{
    { 'i', "input", ARGS_TYPE_STRING | ARGS_TYPE_MANDATORY, &op_flag[OP_FLAG_FNAME_INP], op_fname_inp, "file name of input bitstream"},
    { 'o', "output", ARGS_TYPE_STRING,                      &op_flag[OP_FLAG_FNAME_OUT], op_fname_out,"file name of decoded output" },
    { 'f', "frames", ARGS_TYPE_INTEGER,                     &op_flag[OP_FLAG_MAX_FRM_NUM], &op_max_frm_num,"maximum number of frames to be decoded" },
    { 's', "signature", COM_ARGS_VAL_TYPE_NONE,             &op_flag[OP_FLAG_USE_PIC_SIGN], &op_use_pic_signature,"conformance check using picture signature (HASH)" },
    {
        'v', "verbose", ARGS_TYPE_INTEGER,                    &op_flag[OP_FLAG_VERBOSE], &op_verbose, "verbose level\n"
        "\t 0: no message\n"
        "\t 1: frame-level messages (default)\n"
        "\t 2: all messages\n"
    },
#if LIBVC_ON
    {
        COM_ARGS_NO_KEY, "input_libpics", ARGS_TYPE_STRING,
        &op_flag[OP_FLAG_FNAME_INP_LIBPICS], op_fname_inp_libpics,
        "file name of input libpics bitstream"
    },
    {
        COM_ARGS_NO_KEY, "output_libpics", ARGS_TYPE_STRING,
        &op_flag[OP_FLAG_FNAME_OUT_LIBPICS], op_fname_out_libpics,
        "file name of decoded libpics output"
    },
#endif
    { 0, "", COM_ARGS_VAL_TYPE_NONE, NULL, NULL, "" } /* termination */
};

#define NUM_ARG_OPTION   ((int)(sizeof(options)/sizeof(options[0]))-1)
static void print_usage(void)
{
    int i;
    char str[1024];
    v0print("< Usage >\n");
    for(i=0; i<NUM_ARG_OPTION; i++)
    {
        if(com_args_get_help(options, i, str) < 0) return;
        v0print("%s\n", str);
    }
}

static int xFindNextStartCode(FILE * fp, int * ruiPacketSize, unsigned char *pucBuffer)
{
    unsigned int uiDummy = 0;
    char bEndOfStream = 0;
    size_t ret = 0;
    ret = fread(&uiDummy, 1, 2, fp);
    if (ret != 2)
    {
        return -1;
    }

    if (feof(fp))
    {
        return -1;
    }
    assert(0 == uiDummy);

    ret = fread(&uiDummy, 1, 1, fp);
    if (ret != 1)
    {
        return -1;
    }
    if (feof(fp))
    {
        return -1;
    }
    assert(1 == uiDummy);

    int iNextStartCodeBytes = 0;
    unsigned int iBytesRead = 0;
    unsigned int uiZeros = 0;
    unsigned char pucBuffer_Temp[16];
    int iBytesRead_Temp = 0;

    pucBuffer[iBytesRead++] = 0x00;
    pucBuffer[iBytesRead++] = 0x00;
    pucBuffer[iBytesRead++] = 0x01;
    while (1)
    {
        unsigned char ucByte = 0;
        ret = fread(&ucByte, 1, 1, fp);

        if (feof(fp))
        {
            iNextStartCodeBytes = 0;
            bEndOfStream = 1;
            break;
        }
        pucBuffer[iBytesRead++] = ucByte;
        if (1 < ucByte)
        {
            uiZeros = 0;
        }
        else if (0 == ucByte)
        {
            uiZeros++;
        }
        else if (uiZeros > 1)
        {
            iBytesRead_Temp = 0;
            pucBuffer_Temp[iBytesRead_Temp] = ucByte;

            iBytesRead_Temp++;
            ret = fread(&ucByte, 1, 1, fp);
            if (ret != 1)
            {
                return -1;
            }
            pucBuffer_Temp[iBytesRead_Temp] = ucByte;
            pucBuffer[iBytesRead++] = ucByte;
            iBytesRead_Temp++;

            if( pucBuffer_Temp[0] == 0x01 && (pucBuffer_Temp[1] == 0xb3 || pucBuffer_Temp[1] == 0xb6 || pucBuffer_Temp[1] == 0xb0 || pucBuffer_Temp[1] == 0x00 || pucBuffer_Temp[1] == 0xb1) )
            {
                iNextStartCodeBytes = 2 + 1 + 1;
                uiZeros = 0;
                break;
            }
            else
            {
                uiZeros = 0;
                iNextStartCodeBytes = 0;// 2 + 1 + 1;
            }
        }
        else
        {
            uiZeros = 0;
        }
    }
    *ruiPacketSize = iBytesRead - iNextStartCodeBytes;

    if (bEndOfStream)
    {
        return 0;
    }
    if (fseek(fp, -1 * iNextStartCodeBytes, SEEK_CUR) != 0)
    {
        printf("file seek failed!\n");
    };
    return 0;
}

static unsigned int initParsingConvertPayloadToRBSP(const unsigned int uiBytesRead, unsigned char* pBuffer, unsigned char* pBuffer2)
{
    unsigned int uiZeroCount = 0;
    unsigned int uiBytesReadOffset = 0;
    unsigned int uiBitsReadOffset = 0;
    const unsigned char *pucRead = pBuffer;
    unsigned char *pucWrite = pBuffer2;
    unsigned int uiWriteOffset = uiBytesReadOffset;
    unsigned char ucCurByte = pucRead[uiBytesReadOffset];

    for (uiBytesReadOffset = 0; uiBytesReadOffset < uiBytesRead; uiBytesReadOffset++)
    {
        ucCurByte = pucRead[uiBytesReadOffset];
        if (2 <= uiZeroCount && 0x02 == pucRead[uiBytesReadOffset])
        {
            pucWrite[uiWriteOffset] = ((pucRead[uiBytesReadOffset] >> 2) << (uiBitsReadOffset + 2));
            uiBitsReadOffset += 2;
            uiZeroCount = 0;
            if (uiBitsReadOffset >= 8)
            {
                uiBitsReadOffset = 0;
                continue;
            }
            if (uiBytesReadOffset >= uiBytesRead)
            {
                break;
            }
        }
        else if (2 <= uiZeroCount && 0x01 == pucRead[uiBytesReadOffset])
        {
            uiBitsReadOffset = 0;
            pucWrite[uiWriteOffset] = pucRead[uiBytesReadOffset];
        }
        else
        {
            pucWrite[uiWriteOffset] = (pucRead[uiBytesReadOffset] << uiBitsReadOffset);
        }

        if (uiBytesReadOffset + 1 < uiBytesRead)
        {
            pucWrite[uiWriteOffset] |= (pucRead[uiBytesReadOffset + 1] >> (8 - uiBitsReadOffset));
        }
        uiWriteOffset++;

        if (0x00 == ucCurByte)
        {
            uiZeroCount++;
        }
        else
        {
            uiZeroCount = 0;
        }
    }

    // th just clear the remaining bits in the buffer
    for (unsigned int ui = uiWriteOffset; ui < uiBytesRead; ui++)
    {
        pucWrite[ui] = 0;
    }
    memcpy(pBuffer, pBuffer2, uiWriteOffset);
    pBuffer[uiWriteOffset] = 0x00;
    pBuffer[uiWriteOffset + 1] = 0x00;
    pBuffer[uiWriteOffset + 2] = 0x01;
    return uiBytesRead;
}

static int read_a_bs(FILE * fp, int * pos, unsigned char * bs_buf, unsigned char * bs_buf2)
{
    int read_size, bs_size;
    unsigned char b = 0;
    bs_size = 0;
    read_size = 0;
    if (!fseek(fp, *pos, SEEK_SET))
    {
        int ret = 0;
        ret = xFindNextStartCode(fp, &bs_size, bs_buf);
        if (ret == -1)
        {
            v2print("End of file\n");
            return -1;
        }
        read_size = initParsingConvertPayloadToRBSP(bs_size, bs_buf, bs_buf2);
    }
    else
    {
        v0print("Cannot seek bitstream!\n");
        return -1;
    }
    return read_size;
}

static int print_stat(DEC_STAT * stat, int ret)
{
#if LIBVC_ON
    char *stype = NULL;
#else
    char stype = 0;
#endif
    int i, j;
    if(COM_SUCCEEDED(ret))
    {
        if (stat->ctype == COM_CT_SLICE || stat->ctype == COM_CT_PICTURE)
        {
#if LIBVC_ON
            if (stat->is_RLpic_flag)
            {
                stype = "RL";
            }
            else
            {
                switch (stat->stype)
                {
                case COM_ST_I:
                    stype = "I";
                    break;
                case COM_ST_P:
                    stype = "P";
                    break;
                case COM_ST_B:
                    stype = "B";
                    break;
                case COM_ST_UNKNOWN:
                default:
                    stype = "U";
                    break;
                }
            }
#else
            switch (stat->stype)
            {
            case COM_ST_I:
                stype = 'I';
                break;
            case COM_ST_P:
                stype = 'P';
                break;
            case COM_ST_B:
                stype = 'B';
                break;
            case COM_ST_UNKNOWN:
            default:
                stype = 'U';
                break;
            }
#endif
        }

        if(stat->ctype == COM_CT_SLICE)
        {
#if LIBVC_ON
            v1print("%2s-slice", stype);
#else
            v1print("%c-slice", stype);
#endif
            v1print(" (poc=%d) ", (int)stat->poc);
        }
        else if(stat->ctype == COM_CT_SQH)
        {
            v1print("Sequence header");
#if PRINT_SQH_PARAM_DEC
            printf( "\n*** HLS   Params: bitDep %d", stat->internal_bit_depth );
            printf( "\n*** Intra  tools:  TSCPM %d   IPF %d  IntraDT %d  IPCM %d", 
                (stat->intra_tools >> 0) & 0x1, (stat->intra_tools >> 1) & 0x1, (stat->intra_tools >> 2) & 0x1, (stat->intra_tools >> 3) & 0x1 );
            printf( "\n*** Inter  tools: AFFINE %d  AMVR %d     UMVE %d  EMVR %d   SMVD %d   HMVP %d ", 
                (stat->inter_tools >> 0) & 0x1, (stat->inter_tools >> 1) & 0x1, (stat->inter_tools >> 2) & 0x1, (stat->inter_tools >> 3) & 0x1,
                (stat->inter_tools >> 4) & 0x1, (stat->inter_tools >> 10) & 0xf );
            printf( "\n*** Trans  tools:   NSST %d   PBT %d                     ",   
                (stat->trans_tools >> 0) & 0x1, (stat->trans_tools >> 1) & 0x1 );
            printf( "\n*** Filter tools:    SAO %d   ALF %d                     ", 
                (stat->filte_tools >> 0) & 0x1, (stat->filte_tools >> 1) & 0x1 );
#endif
        }
        else if (stat->ctype == COM_CT_PICTURE)
        {
#if LIBVC_ON
            v1print("%2s-picture header", stype);
#else
            v1print("%c-picture header", stype);
#endif
        }
        else if( stat->ctype == COM_CT_SEQ_END )
        {
            v1print( "video_sequence_end_code" );
        }
        else
        {
            v0print("Unknown bitstream");
        }


        if (stat->ctype == COM_CT_SLICE)
        {
            for (i = 0; i < 2; i++)
            {
#if LIBVC_ON
                if (stat->is_RLpic_flag)
                {
                    v1print("[Lib%d ", i);
                }
                else
#endif
                {
                    v1print("[L%d ", i);
                }
                for (j = 0; j < stat->refpic_num[i]; j++) v1print("%d ", stat->refpic[i][j]);
                v1print("] ");
            }
        }
        if (ret == COM_OK)
        {
            v1print("\n");
        }
        else if(ret == COM_OK_FRM_DELAYED)
        {
            v1print("->Frame delayed\n");
        }
        else if(ret == COM_OK_DIM_CHANGED)
        {
            v1print("->Resolution changed\n");
        }
        else
        {
            v1print("->Unknown OK code = %d\n", ret);
        }
    }
    else
    {
        v0print("Decoding error = %d\n", ret);
    }
    return 0;
}

static int set_extra_config(DEC id)
{
    int  ret, size, value;
    if(op_use_pic_signature)
    {
        value = 1;
        size = 4;
        ret = dec_config(id, DEC_CFG_SET_USE_PIC_SIGNATURE, &value, &size);
        if(COM_FAILED(ret))
        {
            v0print("failed to set config for picture signature\n");
            return -1;
        }
    }
    return 0;
}

static int write_dec_img(DEC id, char * fname, COM_IMGB * img, int bit_depth_internal)
{
    COM_IMGB* imgb_t = NULL;
    if (imgb_t == NULL)
    {
        imgb_t = imgb_alloc(img->width[0], img->height[0], COM_COLORSPACE_YUV420, bit_depth_internal);
        if (imgb_t == NULL)
        {
            v0print("failed to allocate temporay image buffer\n");
            return -1;
        }
    }

    imgb_cpy_conv_rec(imgb_t, img, bit_depth_internal);

#if LIBVC_ON
    if (imgb_write(fname, imgb_t, bit_depth_internal)) return -1;
#else
    if (imgb_write(op_fname_out, img)) return -1;
#endif
    if (imgb_t)
    {
        imgb_free(imgb_t);
    }

    return COM_OK;
}

#if LIBVC_ON
void set_livcdata_dec(DEC id_lib, LibVCData *libvc_data)
{
    DEC_CTX      * tmp_ctx = (DEC_CTX *)id_lib;
    tmp_ctx->dpm.libvc_data = libvc_data;
}

int decode_libpics(DEC_CDSC * cdsc, LibVCData* libvc_data)
{
    STATES            state_lib = STATE_DECODING;
    unsigned char   * bs_buf_lib = NULL;
    unsigned char   * bs_buf_lib2 = NULL;
    DEC               id_lib = NULL;
    COM_BITB          bitb_lib;
    COM_IMGB        * imgb_lib;
    DEC_STAT          stat_lib;
    int               ret;
    COM_CLK           clk_beg_lib, clk_tot_lib;
    int               bs_cnt_lib, pic_cnt_lib;
    int               bs_size_lib, bs_read_pos_lib = 0;
    int               width_lib, height_lib;
    FILE            * fp_bs_lib = NULL;
#if LINUX
    signal(SIGSEGV, handler);   // install our handler
#endif
#if DECODING_TIME_TEST
    clk_beg_lib = com_clk_get();
#endif

    /* open input bitstream */
    fp_bs_lib = fopen(op_fname_inp_libpics, "rb");
    if (fp_bs_lib == NULL)
    {
        v0print("ERROR: cannot open libpics bitstream file = %s\n", op_fname_inp_libpics);
        print_usage();
        return -1;
    }
    if (op_flag[OP_FLAG_FNAME_OUT_LIBPICS])
    {
        /* remove decoded file contents if exists */
        FILE * fp;
        fp = fopen(op_fname_out_libpics, "wb");
        if (fp == NULL)
        {
            v0print("ERROR: cannot create a decoded libpics file\n");
            print_usage();
            return -1;
        }
        fclose(fp);
    }
    bs_buf_lib = malloc(MAX_BS_BUF);
    if (bs_buf_lib == NULL)
    {
        v0print("ERROR: cannot allocate bit buffer, size=%d\n", MAX_BS_BUF);
        return -1;
    }

    bs_buf_lib2 = malloc(MAX_BS_BUF);
    if (bs_buf_lib2 == NULL)
    {
        v0print("ERROR: cannot allocate bit buffer, size2=%d\n", MAX_BS_BUF);
        return -1;
    }

    id_lib = dec_create(cdsc, NULL);
    if (id_lib == NULL)
    {
        v0print("ERROR: cannot create decoder\n");
        return -1;
    }
    set_livcdata_dec(id_lib, libvc_data);

    if (set_extra_config(id_lib))
    {
        v0print("ERROR: cannot set extra configurations\n");
        return -1;
    }
    pic_cnt_lib = 0;
    clk_tot_lib = 0;
    bs_cnt_lib = 0;
    width_lib = height_lib = 0;
    while (1)
    {
        if (state_lib == STATE_DECODING)
        {
            bs_size_lib = read_a_bs(fp_bs_lib, &bs_read_pos_lib, bs_buf_lib, bs_buf_lib2);
            if (bs_size_lib <= 0)
            {
                state_lib = STATE_BUMPING;
                v1print("bumping process starting...\n");
                continue;
            }
            bs_read_pos_lib += (bs_size_lib);
            bitb_lib.addr = bs_buf_lib;
            bitb_lib.ssize = bs_size_lib;
            bitb_lib.bsize = MAX_BS_BUF;

#if !DECODING_TIME_TEST
            clk_beg_lib = com_clk_get();
#endif
            /* main decoding block */
            ret = dec_cnk((DEC_CTX *)id_lib, &bitb_lib, &stat_lib);
            if (COM_FAILED(ret))
            {
                v0print("failed to decode bitstream\n");
                return -1;
            }
#if !DECODING_TIME_TEST
            clk_tot_lib += com_clk_from(clk_beg_lib);
#endif
        }
        if (stat_lib.fnum >= 0 || state_lib == STATE_BUMPING)
        {
            ret = dec_pull_frm((DEC_CTX *)id_lib, &imgb_lib, state_lib);
            if (ret == COM_ERR_UNEXPECTED)
            {
                v1print("bumping process completed\n");
                goto END;
            }
            else if (COM_FAILED(ret))
            {
                v0print("failed to pull the decoded image\n");
                return -1;
            }
        }
        else
        {
            imgb_lib = NULL;
        }
        if (imgb_lib)
        {
            width_lib = imgb_lib->width[0];
            height_lib = imgb_lib->height[0];
            if (op_flag[OP_FLAG_FNAME_OUT_LIBPICS])
            {
                write_dec_img(id_lib, op_fname_out_libpics, imgb_lib, ((DEC_CTX *)id_lib)->info.bit_depth_internal);
            }
            imgb_lib->release(imgb_lib);
            pic_cnt_lib++;
        }
    }

END:
#if DECODING_TIME_TEST
    clk_tot_lib += com_clk_from(clk_beg_lib);
#endif
    libvc_data->num_lib_pic = pic_cnt_lib;

    if (id_lib) dec_delete(id_lib);
    if (fp_bs_lib) fclose(fp_bs_lib);
    if (bs_buf_lib) free(bs_buf_lib);
    return 0;
}
#endif

int main(int argc, const char **argv)
{
    STATES            state = STATE_DECODING;
    unsigned char   * bs_buf = NULL;
    unsigned char   * bs_buf2 = NULL;
    DEC               id = NULL;
    DEC_CDSC          cdsc;
    COM_BITB          bitb;
    COM_IMGB        * imgb;
    DEC_STAT          stat;
    int               ret;
    COM_CLK           clk_beg, clk_tot;
    int               bs_cnt, pic_cnt;
    int               bs_size, bs_read_pos = 0;
    int               width, height;
    FILE            * fp_bs = NULL;

#if LINUX
    signal(SIGSEGV, handler);   // install our handler
#endif
#if DECODING_TIME_TEST
    clk_beg = com_clk_get();
#endif
    /* parse options */
    ret = com_args_parse_all(argc, argv, options);
    if(ret != 0)
    {
        if(ret > 0) v0print("-%c argument should be set\n", ret);
        print_usage();
        return -1;
    }
    /* open input bitstream */
    fp_bs = fopen(op_fname_inp, "rb");
    if(fp_bs == NULL)
    {
        v0print("ERROR: cannot open bitstream file = %s\n", op_fname_inp);
        print_usage();
        return -1;
    }
    if(op_flag[OP_FLAG_FNAME_OUT])
    {
        /* remove decoded file contents if exists */
        FILE * fp;
        fp = fopen(op_fname_out, "wb");
        if(fp == NULL)
        {
            v0print("ERROR: cannot create a decoded file\n");
            print_usage();
            return -1;
        }
        fclose(fp);
    }
    bs_buf = malloc(MAX_BS_BUF);
    if(bs_buf == NULL)
    {
        v0print("ERROR: cannot allocate bit buffer, size=%d\n", MAX_BS_BUF);
        return -1;
    }
    bs_buf2 = malloc(MAX_BS_BUF);
    if (bs_buf2 == NULL)
    {
        v0print("ERROR: cannot allocate bit buffer, size=%d\n", MAX_BS_BUF);
        return -1;
    }
#if LIBVC_ON
    LibVCData libvc_data;
    init_libvcdata(&libvc_data);

    if (op_flag[OP_FLAG_FNAME_INP_LIBPICS])
    {

        int err = decode_libpics(&cdsc, &libvc_data);
        if (err)
        {
            v0print("Error when decode lib pic!");
            return -1;
        }

        libvc_data.is_libpic_prepared = 1;
    }
#endif
    id = dec_create(&cdsc, NULL);
    if(id == NULL)
    {
        v0print("ERROR: cannot create decoder\n");
        return -1;
    }
#if LIBVC_ON
    set_livcdata_dec(id, &libvc_data);
#endif
    if (set_extra_config(id))
    {
        v0print("ERROR: cannot set extra configurations\n");
        return -1;
    }
    pic_cnt = 0;
    clk_tot = 0;
    bs_cnt  = 0;
    width = height   = 0;
    g_CountDOICyCleTime = 0;                    // initialized the number .
    g_DOIPrev = 0;
    while(1)
    {
        if(state == STATE_DECODING)
        {
            bs_size = read_a_bs(fp_bs, &bs_read_pos, bs_buf,bs_buf2);
            if(bs_size <= 0)
            {
                state = STATE_BUMPING;
                v1print("bumping process starting...\n");
                continue;
            }
            bs_read_pos +=  bs_size;
            bitb.addr  = bs_buf;
            bitb.ssize = bs_size;
            bitb.bsize = MAX_BS_BUF;
            v1print("[%4d]-th BS (%07dbytes)   --> ", bs_cnt++, bs_size);
#if !DECODING_TIME_TEST
            clk_beg = com_clk_get();
#endif
            /* main decoding block */
            ret = dec_cnk((DEC_CTX *)id, &bitb, &stat);
            if(COM_FAILED(ret))
            {
                v0print("failed to decode bitstream\n");
                return -1;
            }
#if !DECODING_TIME_TEST
            clk_tot += com_clk_from(clk_beg);
#endif
            print_stat(&stat, ret);
        }
        if(stat.fnum >= 0 || state == STATE_BUMPING)
        {
            ret = dec_pull_frm((DEC_CTX *)id, &imgb, state);
            if(ret == COM_ERR_UNEXPECTED)
            {
                v1print("bumping process completed\n");
                goto END;
            }
            else if(COM_FAILED(ret))
            {
                v0print("failed to pull the decoded image\n");
                return -1;
            }
        }
        else
        {
            imgb = NULL;
        }
        if(imgb)
        {
            width = imgb->width[0];
            height = imgb->height[0];
            if(op_flag[OP_FLAG_FNAME_OUT])
            {
                write_dec_img(id, op_fname_out, imgb, ((DEC_CTX *)id)->info.bit_depth_internal);
            }
            imgb->release(imgb);
            pic_cnt++;
        }
    }
END:
#if DECODING_TIME_TEST
    clk_tot += com_clk_from(clk_beg);
#endif
    v1print("===========================================================\n");
    v1print("Resolution                        = %d x %d\n", width, height);
    v1print("Processed BS count                = %d\n", bs_cnt);
    v1print("Decoded frame count               = %d\n", pic_cnt);
    if(pic_cnt > 0)
    {
        v1print("total decoding time               = %d msec,",
                (int)com_clk_msec(clk_tot));
        v1print(" %.3f sec\n",
                (float)(com_clk_msec(clk_tot) /1000.0));
        v1print("Average decoding time for a frame = %d msec\n",
                (int)com_clk_msec(clk_tot)/pic_cnt);
        v1print("Average decoding speed            = %.3f frames/sec\n",
                ((float)pic_cnt*1000)/((float)com_clk_msec(clk_tot)));
    }
    v1print("===========================================================\n");
    if (op_flag[OP_FLAG_USE_PIC_SIGN] && pic_cnt > 0)
    {
        v1print("Decode Match: 1 (HASH)\n");
        v1print("===========================================================\n");
    }

    if(id) dec_delete(id);
    if(fp_bs) fclose(fp_bs);
    if(bs_buf) free(bs_buf);
#if LIBVC_ON
    delete_libvcdata(&libvc_data);
#endif
    return 0;
}
