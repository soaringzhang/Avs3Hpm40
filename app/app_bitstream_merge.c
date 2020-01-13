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

#define MERGE_BITSTREAM                 1

#include <com_typedef.h>
#include "app_util.h"
#include "app_args.h"
#include "com_def.h"
#include "dec_def.h"
#include "dec_DecAdaptiveLoopFilter.h"

#if MERGE_BITSTREAM
#include "dec_eco.h"
#include "enc_eco.h"
#include "stdio.h"
#include "com_df.h"
#include "com_sao.h"
#endif

#define VERBOSE_FRAME              VERBOSE_1

#define MAX_BS_BUF                 (64*1024*1024) /* byte */

static char op_fname_inp[256] = "\0";
static char op_fname_out[256] = "\0";
static int  op_max_frm_num = 0;
static int  op_use_pic_signature = 0;
static int  op_bit_depth_output = 8;
#if LIBVC_ON
static int  op_merge_libandseq_bitstream_flag = 0;
static char op_fname_inp_lib[256] = "\0";
static char op_fname_inp_seq[256] = "\0";
#endif
#if BBV
static int  op_bbv_check_flag = 0;
#if BBV_LIBVC
static int  op_bbv_check_merge_steam = 0;
#endif

typedef enum _BBV_STATES
{
    BBV_OK = 0,
    BBV_OVERFLOW,
    BBV_DOWNFLOW,
    BBV_DELAY_WRONG,
    BBV_INPUT_SPEED_OVERFLOW
}BBV_STATES;
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
    OP_FLAG_OUT_BIT_DEPTH,
#if LIBVC_ON
    OP_FLAG_MERGE_LIBANDSEQ_BITSTREAM,
    OP_FLAG_FNAME_INP_LIB,
    OP_FLAG_FNAME_INP_SEQ,
#endif
#if BBV
    OP_FLAG_BBV_CHECK,
#if BBV_LIBVC
    OP_FLAG_BBV_CHECK_MERGE_STREAM,
#endif
#endif
    OP_FLAG_VERBOSE,
    OP_FLAG_MAX
} OP_FLAGS;

static int op_flag[OP_FLAG_MAX] = { 0 };

static COM_ARGS_OPTION options[] = \
{
    {
#if LIBVC_ON
        'i', "input", ARGS_TYPE_STRING,
#else
        'i', "input", ARGS_TYPE_STRING | ARGS_TYPE_MANDATORY,
#endif
        &op_flag[OP_FLAG_FNAME_INP], op_fname_inp,
        "file name of input bitstream"
    },
    {
        'o', "output", ARGS_TYPE_STRING,
        &op_flag[OP_FLAG_FNAME_OUT], op_fname_out,
        "file name of decoded output"
    },
    {
        'f', "frames", ARGS_TYPE_INTEGER,
        &op_flag[OP_FLAG_MAX_FRM_NUM], &op_max_frm_num,
        "maximum number of frames to be decoded"
    },
    {
        's', "signature", COM_ARGS_VAL_TYPE_NONE,
        &op_flag[OP_FLAG_USE_PIC_SIGN], &op_use_pic_signature,
        "conformance check using picture signature (HASH)"
    },
    {
        'v', "verbose", ARGS_TYPE_INTEGER,
        &op_flag[OP_FLAG_VERBOSE], &op_verbose,
        "verbose level\n"
        "\t 0: no message\n"
        "\t 1: frame-level messages (default)\n"
        "\t 2: all messages\n"
    },
    {
        COM_ARGS_NO_KEY, "output_bit_depth", ARGS_TYPE_INTEGER,
        &op_flag[OP_FLAG_OUT_BIT_DEPTH], &op_bit_depth_output,
        "output bitdepth (8(default), 10) "
    },
#if LIBVC_ON
    {
        COM_ARGS_NO_KEY, "merge_libandseq_bitstream", ARGS_TYPE_INTEGER,
        &op_flag[OP_FLAG_MERGE_LIBANDSEQ_BITSTREAM], &op_merge_libandseq_bitstream_flag,
        "merge_lib and seq_bitstream flag (0(default), 1) "
    },
    {
        COM_ARGS_NO_KEY, "input_lib_bitstream", ARGS_TYPE_STRING,
        &op_flag[OP_FLAG_FNAME_INP_LIB], &op_fname_inp_lib,
        "file name of input lib bitstream"
    },
    {
        COM_ARGS_NO_KEY, "input_seq_bitstream", ARGS_TYPE_STRING,
        &op_flag[OP_FLAG_FNAME_INP_SEQ], &op_fname_inp_seq,
        "file name of input seq bitstream"
    },
#endif
#if BBV
    {
        COM_ARGS_NO_KEY, "bbv_check", ARGS_TYPE_INTEGER,
        &op_flag[OP_FLAG_BBV_CHECK], &op_bbv_check_flag,
        "bbv check flag (0(default), 1) "
    },
#if BBV_LIBVC
    {
        COM_ARGS_NO_KEY, "bbv_check_merge_stream", ARGS_TYPE_INTEGER,
        &op_flag[OP_FLAG_BBV_CHECK_MERGE_STREAM], &op_bbv_check_merge_steam,
        "whether the stream to do bbv check is a merge stream (0(default), 1) "
    },
#endif
#endif
    { 0, "", COM_ARGS_VAL_TYPE_NONE, NULL, NULL, "" } /* termination */
};
#if MERGE_BITSTREAM
static void sequence_deinit(DEC_CTX * ctx)
{
    if (ctx->map.map_scu)
    {
        com_mfree(ctx->map.map_scu);
        ctx->map.map_scu = NULL;
    }
    if (ctx->map.map_split)
    {
        com_mfree(ctx->map.map_split);
        ctx->map.map_split = NULL;
    }
    if (ctx->map.map_ipm)
    {
        com_mfree(ctx->map.map_ipm);
        ctx->map.map_ipm = NULL;
    }
    if (ctx->map.map_cu_mode)
    {
        com_mfree(ctx->map.map_cu_mode);
        ctx->map.map_cu_mode = NULL;
    }
#if TB_SPLIT_EXT
    if (ctx->map.map_pb_tb_part)
    {
        com_mfree(ctx->map.map_pb_tb_part);
        ctx->map.map_pb_tb_part = NULL;
    }
#endif
    if (ctx->map.map_depth)
    {
        com_mfree(ctx->map.map_depth);
        ctx->map.map_depth = NULL;
    }
    if (ctx->map.map_patch_idx)
    {
        com_mfree(ctx->map.map_patch_idx);
        ctx->map.map_patch_idx = NULL;
    }

    DeleteEdgeFilter_avs2(ctx->ppbEdgeFilter, ctx->info.pic_height);

    if (ctx->pic_sao)
    {
        com_picbuf_free(ctx->pic_sao);
        ctx->pic_sao = NULL;
    }

    com_free_3d_SAOstatdate(ctx->saostatData,
        ((ctx->info.pic_width >> ctx->info.log2_max_cuwh) + (ctx->info.pic_width % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)) * ((
            ctx->info.pic_height >> ctx->info.log2_max_cuwh) + (ctx->info.pic_height % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)), N_C);
    com_free_2d_SAOParameter(ctx->saoBlkParams,
        ((ctx->info.pic_width >> ctx->info.log2_max_cuwh) + (ctx->info.pic_width % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)) * ((
            ctx->info.pic_height >> ctx->info.log2_max_cuwh) + (ctx->info.pic_height % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)));
    com_free_2d_SAOParameter(ctx->rec_saoBlkParams,
        ((ctx->info.pic_width >> ctx->info.log2_max_cuwh) + (ctx->info.pic_width % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)) * ((
            ctx->info.pic_height >> ctx->info.log2_max_cuwh) + (ctx->info.pic_height % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)));

    if (ctx->pic_alf_Rec)
    {
        com_picbuf_free(ctx->pic_alf_Rec);
        ctx->pic_alf_Rec = NULL;
    }
    if (ctx->pic_alf_Dec)
    {
        com_picbuf_free(ctx->pic_alf_Dec);
        ctx->pic_alf_Dec = NULL;
    }
    ReleaseAlfGlobalBuffer(ctx);
    ctx->info.pic_header.pic_alf_on = NULL;
    ctx->info.pic_header.m_alfPictureParam = NULL;
    com_picman_deinit(&ctx->dpm);
}
static int sequence_init(DEC_CTX * ctx, COM_SQH * sqh)
{
    int size;
    int ret;

    ctx->info.bit_depth_internal = (sqh->encoding_precision == 2) ? 10 : 8;
    assert(sqh->sample_precision == 1 || sqh->sample_precision == 2);
    ctx->info.bit_depth_input = (sqh->sample_precision == 1) ? 8 : 10;
    ctx->info.qp_offset_bit_depth = (8 * (ctx->info.bit_depth_internal - 8));

    /* resolution was changed */
    sequence_deinit( ctx );
    ctx->info.pic_width  = (sqh->horizontal_size  + MINI_SIZE - 1) / MINI_SIZE * MINI_SIZE;
    ctx->info.pic_height = (sqh->vertical_size    + MINI_SIZE - 1) / MINI_SIZE * MINI_SIZE;
    ctx->info.max_cuwh = 1 << sqh->log2_max_cu_width_height;
    ctx->info.log2_max_cuwh = CONV_LOG2( ctx->info.max_cuwh );

    size = ctx->info.max_cuwh;
    ctx->info.pic_width_in_lcu = (ctx->info.pic_width + (size - 1)) / size;
    ctx->info.pic_height_in_lcu = (ctx->info.pic_height + (size - 1)) / size;
    ctx->info.f_lcu = ctx->info.pic_width_in_lcu * ctx->info.pic_height_in_lcu;
    ctx->info.pic_width_in_scu = (ctx->info.pic_width + ((1 << MIN_CU_LOG2) - 1)) >> MIN_CU_LOG2;
    ctx->info.pic_height_in_scu = (ctx->info.pic_height + ((1 << MIN_CU_LOG2) - 1)) >> MIN_CU_LOG2;
    ctx->info.f_scu = ctx->info.pic_width_in_scu * ctx->info.pic_height_in_scu;
    /* alloc SCU map */
    if (ctx->map.map_scu == NULL)
    {
        size = sizeof(u32) * ctx->info.f_scu;
        ctx->map.map_scu = (u32 *)com_malloc(size);
        com_assert_gv(ctx->map.map_scu, ret, COM_ERR_OUT_OF_MEMORY, ERR);
        com_mset_x64a(ctx->map.map_scu, 0, size);
    }
    /* alloc cu mode SCU map */
    if (ctx->map.map_cu_mode == NULL)
    {
        size = sizeof(u32) * ctx->info.f_scu;
        ctx->map.map_cu_mode = (u32 *)com_malloc(size);
        com_assert_gv(ctx->map.map_cu_mode, ret, COM_ERR_OUT_OF_MEMORY, ERR);
        com_mset_x64a(ctx->map.map_cu_mode, 0, size);
    }
    /* alloc map for CU split flag */
    if (ctx->map.map_split == NULL)
    {
        size = sizeof(s8) * ctx->info.f_lcu * MAX_CU_DEPTH * NUM_BLOCK_SHAPE * MAX_CU_CNT_IN_LCU;
        ctx->map.map_split = com_malloc(size);
        com_assert_gv(ctx->map.map_split, ret, COM_ERR_OUT_OF_MEMORY, ERR);
        com_mset_x64a(ctx->map.map_split, 0, size);
    }
    /* alloc map for intra prediction mode */
    if (ctx->map.map_ipm == NULL)
    {
        size = sizeof(s8) * ctx->info.f_scu;
        ctx->map.map_ipm = (s8 *)com_malloc(size);
        com_assert_gv(ctx->map.map_ipm, ret, COM_ERR_OUT_OF_MEMORY, ERR);
        com_mset_x64a(ctx->map.map_ipm, -1, size);
    }
    if (ctx->map.map_patch_idx == NULL)
    {
        size = sizeof(s8)* ctx->info.f_scu;
        ctx->map.map_patch_idx = (s8 *)com_malloc(size);
        com_assert_gv(ctx->map.map_patch_idx, ret, COM_ERR_OUT_OF_MEMORY, ERR);
        com_mset_x64a(ctx->map.map_patch_idx, -1, size);
    }
#if TB_SPLIT_EXT
    /* alloc map for pb and tb partition size (for luma and chroma) */
    if (ctx->map.map_pb_tb_part == NULL)
    {
        size = sizeof(u32) * ctx->info.f_scu;
        ctx->map.map_pb_tb_part = (u32 *)com_malloc(size);
        com_assert_gv(ctx->map.map_pb_tb_part, ret, COM_ERR_OUT_OF_MEMORY, ERR);
        com_mset_x64a(ctx->map.map_pb_tb_part, 0xffffffff, size);
    }
#endif
    /* alloc map for intra depth */
    if (ctx->map.map_depth == NULL)
    {
        size = sizeof(s8) * ctx->info.f_scu;
        ctx->map.map_depth = com_malloc_fast(size);
        com_assert_gv(ctx->map.map_depth, ret, COM_ERR_OUT_OF_MEMORY, ERR);
        com_mset(ctx->map.map_depth, -1, size);
    }

    ctx->pa.width = ctx->info.pic_width;
    ctx->pa.height = ctx->info.pic_height;
    ctx->pa.pad_l = PIC_PAD_SIZE_L;
    ctx->pa.pad_c = PIC_PAD_SIZE_C;
    ret = com_picman_init(&ctx->dpm, MAX_PB_SIZE, MAX_NUM_REF_PICS, &ctx->pa);
    com_assert_g(COM_SUCCEEDED(ret), ERR);

    CreateEdgeFilter_avs2(ctx->info.pic_width, ctx->info.pic_height, ctx->ppbEdgeFilter);

    if (ctx->pic_sao == NULL)
    {
        ctx->pic_sao = com_pic_alloc(&ctx->pa, &ret);
        com_assert_rv(ctx->pic_sao != NULL, ret);
    }

    com_malloc_3d_SAOstatdate(&(ctx->saostatData),
        ((ctx->info.pic_width >> ctx->info.log2_max_cuwh) + (ctx->info.pic_width % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)) * ((
            ctx->info.pic_height >> ctx->info.log2_max_cuwh) + (ctx->info.pic_height % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)), N_C, NUM_SAO_NEW_TYPES);
    com_malloc_2d_SAOParameter(&(ctx->saoBlkParams),
        ((ctx->info.pic_width >> ctx->info.log2_max_cuwh) + (ctx->info.pic_width % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)) * ((
            ctx->info.pic_height >> ctx->info.log2_max_cuwh) + (ctx->info.pic_height % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)), N_C);
    com_malloc_2d_SAOParameter(&ctx->rec_saoBlkParams,
        ((ctx->info.pic_width >> ctx->info.log2_max_cuwh) + (ctx->info.pic_width % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)) * ((
            ctx->info.pic_height >> ctx->info.log2_max_cuwh) + (ctx->info.pic_height % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)), N_C);

    ctx->info.pic_header.tool_alf_on = ctx->info.sqh.adaptive_leveling_filter_enable_flag;
    if (ctx->pic_alf_Rec == NULL)
    {
        ctx->pic_alf_Rec = com_pic_alloc(&ctx->pa, &ret);
        com_assert_rv(ctx->pic_alf_Rec != NULL, ret);
    }
    if (ctx->pic_alf_Dec == NULL)
    {
        ctx->pic_alf_Dec = com_pic_alloc(&ctx->pa, &ret);
        com_assert_rv(ctx->pic_alf_Dec != NULL, ret);
    }
    CreateAlfGlobalBuffer(ctx);
    ctx->info.pic_header.pic_alf_on = ctx->pic_alf_on;
    ctx->info.pic_header.m_alfPictureParam = ctx->Dec_ALF->m_alfPictureParam;
    /*initialize patch info */
#if PATCH
    PATCH_INFO * patch = NULL;
    patch = (PATCH_INFO *)com_malloc_fast(sizeof(PATCH_INFO));
    ctx->patch = patch;
    ctx->patch->stable = sqh->patch_stable;
    ctx->patch->cross_patch_loop_filter = sqh->cross_patch_loop_filter;
    ctx->patch->ref_colocated = sqh->patch_ref_colocated;
    if (ctx->patch->stable)
    {
        ctx->patch->uniform = sqh->patch_uniform;
        if (ctx->patch->uniform)
        {
            ctx->patch->width = sqh->patch_width_minus1 + 1;
            ctx->patch->height = sqh->patch_height_minus1 + 1;
            ctx->patch->columns = ctx->info.pic_width_in_lcu / ctx->patch->width;
            ctx->patch->rows = ctx->info.pic_height_in_lcu / ctx->patch->height;
            /*set column_width and row_height*/
            for (int i = 0; i < ctx->patch->columns; i++)
            {
                ctx->patch_column_width[i] = ctx->patch->width;
            }
            if (ctx->info.pic_width_in_lcu%ctx->patch->width != 0)
            {
                if (ctx->patch->columns == 0)
                {
                    ctx->patch_column_width[ctx->patch->columns] = ctx->info.pic_width_in_lcu - ctx->patch->width*ctx->patch->columns;
                    ctx->patch->columns++;
                }
                else
                {
                    ctx->patch_column_width[ctx->patch->columns - 1] = ctx->patch_column_width[ctx->patch->columns - 1] + ctx->info.pic_width_in_lcu - ctx->patch->width*ctx->patch->columns;
                }
            }
            for (int i = 0; i < ctx->patch->rows; i++)
            {
                ctx->patch_row_height[i] = ctx->patch->height;
            }
            if (ctx->info.pic_height_in_lcu%ctx->patch->height != 0)
            {
                if (ctx->patch->rows == 0)
                {
                    ctx->patch_row_height[ctx->patch->rows] = ctx->info.pic_height_in_lcu - ctx->patch->height*ctx->patch->rows;
                    ctx->patch->rows++;
                }
                else
                {
                    ctx->patch_row_height[ctx->patch->rows - 1] = ctx->patch_row_height[ctx->patch->rows - 1] + ctx->info.pic_height_in_lcu - ctx->patch->height*ctx->patch->rows;
                }
            }
            /*pointer to the data*/
            ctx->patch->width_in_lcu = ctx->patch_column_width;
            ctx->patch->height_in_lcu = ctx->patch_row_height;
        }
    }
#endif

    return COM_OK;
ERR:
    sequence_deinit(ctx);
    ctx->init_flag = 0;
    return ret;
}
#endif
#define NUM_ARG_OPTION   ((int)(sizeof(options)/sizeof(options[0]))-1)
static void print_usage(void)
{
    int i;
    char str[1024];
    v0print("< Usage >\n");
    for (i = 0; i<NUM_ARG_OPTION; i++)
    {
        if (com_args_get_help(options, i, str) < 0) return;
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
            //           iBytesRead = iBytesRead + 1;
            pucBuffer[iBytesRead++] = ucByte;
            iBytesRead_Temp++;
#if LIBVC_ON
            if (pucBuffer_Temp[0] == 0x01 && (pucBuffer_Temp[1] == 0xb3 ||
                                              pucBuffer_Temp[1] == 0xb6 || pucBuffer_Temp[1] == 0xb0 || pucBuffer_Temp[1] == 0xb1
                                              || pucBuffer_Temp[1] == 0x00))
            {
#else
            if (pucBuffer_Temp[0] == 0x01 && (pucBuffer_Temp[1] == 0xb3 ||
                                              pucBuffer_Temp[1] == 0xb6 || pucBuffer_Temp[1] == 0xb0
                                              || pucBuffer_Temp[1] == 0x00))
            {
#endif
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


static unsigned int  initParsingConvertPayloadToRBSP(const unsigned int uiBytesRead, unsigned char* pBuffer, unsigned char* pBuffer2)
{
    unsigned int uiZeroCount = 0;
    unsigned int uiBytesReadOffset = 0;
    unsigned int uiBitsReadOffset = 0;
    const unsigned char *pucRead = pBuffer;
    unsigned char *pucWrite = pBuffer2;

    unsigned char ucHeaderType;
    unsigned int uiWriteOffset = uiBytesReadOffset;
    ucHeaderType = pucRead[uiBytesReadOffset++]; //HeaderType



    switch (ucHeaderType)
    {
    case 0xb5:
        if ((pucRead[uiBytesReadOffset] >> 4) == 0x0d)
        {
            break;
        }
        else
        {
        }
    case 0xb0:
    case 0xb2:
        //       initParsing(uiBytesRead);
        memcpy(pBuffer2, pBuffer, uiBytesRead);

        return uiBytesRead;
    default:
        break;
    }
    pucWrite[uiWriteOffset] = ucHeaderType;

    uiWriteOffset++;

    unsigned char ucCurByte = pucRead[uiBytesReadOffset];

    for (; uiBytesReadOffset < uiBytesRead; uiBytesReadOffset++)
    {
        ucCurByte = pucRead[uiBytesReadOffset];

        if (2 <= uiZeroCount && 0x02 == pucRead[uiBytesReadOffset])
        {
            pucWrite[uiWriteOffset] = ((pucRead[uiBytesReadOffset] >> 2) << (uiBitsReadOffset + 2));
            uiBitsReadOffset += 2;
            uiZeroCount = 0;
            //printf("\n error uiBitsReadOffset \n");
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
    //    initParsing(uiWriteOffset);
    memcpy(pBuffer, pBuffer2, uiWriteOffset);


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
#if LIBVC_ON
#if BBV
        if (op_bbv_check_flag)
        {
            read_size = initParsingConvertPayloadToRBSP(bs_size, bs_buf, bs_buf2);
        }
        else
#endif
        {
            read_size = bs_size;
        }
#else
        read_size = initParsingConvertPayloadToRBSP(bs_size, bs_buf, bs_buf2);
#endif

    }
    else
    {
        v0print("Cannot seek bitstream!\n");
        return -1;
    }
    return read_size;
}

int print_stat(DEC_STAT * stat, int ret)
{
    char stype;
    int i, j;
    if (COM_SUCCEEDED(ret))
    {
        if (stat->ctype == COM_CT_PICTURE)
        {
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
            v1print("%c-slice", stype);
        }
        else if (stat->ctype == COM_CT_SQH)
        {
            v1print("Sequence header");
        }
        else
        {
            v0print("Unknown bitstream");
        }
        v1print(" (read=%d, poc=%d) ", stat->read, (int)stat->poc);
        for (i = 0; i < 2; i++)
        {
            v1print("[L%d ", i);
            for (j = 0; j < stat->refpic_num[i]; j++) v1print("%d ", stat->refpic[i][j]);
            v1print("] ");
        }
        if (ret == COM_OK)
        {
            v1print("\n");
        }
        else if (ret == COM_OK_FRM_DELAYED)
        {
            v1print("->Frame delayed\n");
        }
        else if (ret == COM_OK_DIM_CHANGED)
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
    if (op_use_pic_signature)
    {
        value = 1;
        size = 4;
        ret = dec_config(id, DEC_CFG_SET_USE_PIC_SIGNATURE, &value, &size);
        if (COM_FAILED(ret))
        {
            v0print("failed to set config for picture signature\n");
            return -1;
        }
    }
    return 0;
}

/* convert DEC into DEC_CTX with return value if assert on */
#define DEC_ID_TO_CTX_RV(id, ctx, ret) \
    com_assert_rv((id), (ret)); \
    (ctx) = (DEC_CTX *)id; \
    com_assert_rv((ctx)->magic == DEC_MAGIC_CODE, (ret));

void write_tmp_bs(COM_PIC_HEADER * pic_header, int add_dtr, unsigned char *bs_buf, FILE * merge_fp)
{
    u8 tmp_bs;
    u16 dtr = pic_header->dtr + add_dtr;
    tmp_bs = bs_buf[4] & 0xE0;
    tmp_bs |= (dtr >> 5) & 0x1F;
    fwrite(&tmp_bs, 1, 1, merge_fp);

    tmp_bs = bs_buf[5] & 0x07;
    tmp_bs |= (dtr & 0x1F) << 3;
    fwrite(&tmp_bs, 1, 1, merge_fp);
}

int main(int argc, const char **argv)
{
    unsigned char    * bs_buf = NULL;
    DEC              id = NULL;
    DEC_CDSC         cdsc;
    COM_BITB          bitb;
    /*temporal buffer for video bit depth less than 10bit */
    COM_IMGB        * imgb_t = NULL;
    int                ret;
    int                bs_size, bs_read_pos = 0;
    FILE             * fp_bs = NULL;
    FILE             * fp_bs_write = NULL;
    int                bs_num, max_bs_num;
#if !MERGE_BITSTREAM
    u8                 tmp_size[4];
#endif
    int                bs_end_pos;
#if !MERGE_BITSTREAM
    int                intra_dist[2];
#endif
    int                intra_dist_idx = 0;
    COM_BSR         * bs;
    COM_SQH         * sqh;
    COM_PIC_HEADER  * pic_header;
    COM_CNKH        * cnkh;
    DEC_CTX        * ctx;

#if MERGE_BITSTREAM
    unsigned char    * new_bs_buf = NULL;
    unsigned char    * new_bs_buf2 = NULL;
    COM_SH_EXT *shext;
    COM_BITB    new_bitb;
    COM_BSW    *new_bs;
    new_bs = (COM_BSW*)malloc(sizeof(COM_BSW));
    u8 startcodebuf[3];
    startcodebuf[0] = 0x00;
    startcodebuf[1] = 0x00;
    startcodebuf[2] = 0x01;
#endif

#if LIBVC_ON
    unsigned char      *bs_buf_lib2 = NULL;
    DEC                id_lib = NULL;
    DEC                id_seq = NULL;
    DEC_CDSC           cdsc_lib;
    DEC_CDSC           cdsc_seq;
    COM_BSR          * bs_lib;
    COM_BSR          * bs_seq;
    COM_SQH          * sqh_lib = NULL;
    COM_SQH          * sqh_seq = NULL;
    COM_PIC_HEADER           * sh_lib = NULL;
    COM_PIC_HEADER           * sh_seq = NULL;
    COM_CNKH         * cnkh_lib;
    COM_CNKH         * cnkh_seq;
    DEC_CTX          * ctx_lib;
    DEC_CTX          * ctx_seq;
    COM_BITB           bitb_lib;
    COM_BITB           bitb_seq;
    FILE             * fp_bs_lib = NULL;
    FILE             * fp_bs_seq = NULL;
    int                bs_end_pos_lib;
    int                bs_end_pos_seq;
    int                bs_read_pos_lib;
    int                bs_read_pos_seq;
    int                bs_read_pos_sqh;
    int                tmp_bs_read_pos_lib;
    unsigned char    * bs_buf_lib_sqh = NULL;
    unsigned char    * bs_buf_seq_sqh = NULL;
    unsigned char    * bs_buf_lib = NULL;
    unsigned char    * bs_buf_seq = NULL;
    u8                 tmp_size_lib_sqh[4];
    u8                 tmp_size_seq_sqh[4];
    u8                 tmp_size_lib[4];
    u8                 tmp_size_seq[4];
    int                bs_size_lib_sqh;
    int                bs_size_seq_sqh;
    int                bs_size_lib;
    int                bs_size_seq;

    LibVCData          libvc_data;
    init_libvcdata(&libvc_data);

    /* parse options */
    ret = com_args_parse_all(argc, argv, options);
    if (ret != 0)
    {
        if (ret > 0) v0print("-%c argument should be set\n", ret);
        if (ret < 0) v0print("Configuration error, please refer to the usage.\n");
        print_usage();
        return -1;
    }

    if (op_merge_libandseq_bitstream_flag)
    {
        //initialize
        int last_referenced_library_index = -1;

        setvbuf(stdout, NULL, _IOLBF, 1024);
        fp_bs_lib = fopen(op_fname_inp_lib, "rb");
        fp_bs_seq = fopen(op_fname_inp_seq, "rb");
        fp_bs_write = fopen(op_fname_out, "wb");
        if (fp_bs_lib == NULL || fp_bs_seq == NULL || fp_bs_write == NULL)
        {
            v0print("ERROR: cannot open input or output bitstream file = %s, %s, %s\n", op_fname_inp_lib, op_fname_inp_seq, op_fname_out);
            print_usage();
            return -1;
        }
        fseek(fp_bs_lib, 0, SEEK_END);
        bs_end_pos_lib = ftell(fp_bs_lib);
        fseek(fp_bs_seq, 0, SEEK_END);
        bs_end_pos_seq = ftell(fp_bs_seq);

        bs_buf_lib = malloc(MAX_BS_BUF);
        bs_buf_seq = malloc(MAX_BS_BUF);
        bs_buf_lib_sqh = malloc(MAX_BS_BUF);
        bs_buf_seq_sqh = malloc(MAX_BS_BUF);
        bs_buf_lib2 = malloc(MAX_BS_BUF);
        if (bs_buf_lib2 == NULL)
        {
            v0print("ERROR: cannot allocate bit buffer, size2=%d\n", MAX_BS_BUF);
            return -1;
        }

        if (bs_buf_lib == NULL || bs_buf_seq == NULL || bs_buf_lib_sqh == NULL || bs_buf_seq_sqh == NULL)
        {
            v0print("ERROR: cannot allocate bit buffer, size=%d\n", MAX_BS_BUF);
            return -1;
        }

        sqh_lib = malloc(sizeof(COM_SQH));
        sqh_seq = malloc(sizeof(COM_SQH));
        sh_lib = malloc(sizeof(COM_PIC_HEADER));
        sh_seq = malloc(sizeof(COM_PIC_HEADER));

        sh_lib->pic_alf_on = (int *)malloc(N_C * sizeof(int));
        sh_lib->m_alfPictureParam = (ALFParam **)malloc((N_C) * sizeof(ALFParam *));
        for (int i = 0; i < N_C; i++)
        {
            sh_lib->m_alfPictureParam[i] = NULL;
            AllocateAlfPar(&(sh_lib->m_alfPictureParam[i]), i);
        }
        sh_seq->pic_alf_on = (int *)malloc(N_C * sizeof(int));
        sh_seq->m_alfPictureParam = (ALFParam **)malloc((N_C) * sizeof(ALFParam *));
        for (int i = 0; i < N_C; i++)
        {
            sh_seq->m_alfPictureParam[i] = NULL;
            AllocateAlfPar(&(sh_seq->m_alfPictureParam[i]), i);
        }

        if (sqh_lib == NULL || sqh_seq == NULL || sh_lib == NULL || sh_seq == NULL || sh_lib->pic_alf_on == NULL || sh_seq->pic_alf_on == NULL || sh_lib->m_alfPictureParam == NULL || sh_seq->m_alfPictureParam == NULL)
        {
            v0print("ERROR: cannot allocate sqh or sh buffer\n");
            return -1;
        }

        id_lib = dec_create(&cdsc_lib, NULL);
        com_scan_tbl_delete();
        id_seq = dec_create(&cdsc_seq, NULL);
        if (id_lib == NULL || id_seq == NULL)
        {
            v0print("ERROR: cannot create decoder\n");
            return -1;
        }
        if (set_extra_config(id_lib) || set_extra_config(id_seq))
        {
            v0print("ERROR: cannot set extra configurations\n");
            return -1;
        }
        DEC_CTX      * tmp_ctx;
        tmp_ctx = (DEC_CTX *)id_lib;
        tmp_ctx->dpm.libvc_data = &libvc_data;
        tmp_ctx = NULL;
        tmp_ctx = (DEC_CTX *)id_seq;
        tmp_ctx->dpm.libvc_data = &libvc_data;
        tmp_ctx = NULL;

        //read lib sqh
        bs_read_pos_lib = 0;
        while (1)
        {
            bs_read_pos_sqh = 0;
            bs_size_lib_sqh = read_a_bs(fp_bs_lib, &bs_read_pos_sqh, bs_buf_lib_sqh, bs_buf_lib2);

            tmp_size_lib_sqh[0] = (bs_size_lib_sqh & 0xff000000) >> 24;
            tmp_size_lib_sqh[1] = (bs_size_lib_sqh & 0x00ff0000) >> 16;
            tmp_size_lib_sqh[2] = (bs_size_lib_sqh & 0x0000ff00) >> 8;
            tmp_size_lib_sqh[3] = (bs_size_lib_sqh & 0x000000ff) >> 0;
            if (bs_size_lib_sqh <= 0)
            {
                v1print("bumping process starting...\n");
                continue;
            }

            bs_read_pos_lib += (bs_size_lib_sqh);
            bitb_lib.addr = bs_buf_lib_sqh;
            bitb_lib.ssize = bs_size_lib_sqh;
            bitb_lib.bsize = MAX_BS_BUF;
            DEC_ID_TO_CTX_RV(id_lib, ctx_lib, COM_ERR_INVALID_ARGUMENT);
            bs_lib = &ctx_lib->bs;
            sqh = &ctx_lib->info.sqh;
            pic_header = &ctx_lib->info.pic_header;
            cnkh_lib = &ctx_lib->info.cnkh;
            /* set error status */
            ctx_lib->bs_err = bitb_lib.err = 0;
            COM_TRACE_SET(1);
            /* bitstream reader initialization */
            com_bsr_init(bs_lib, bitb_lib.addr, bitb_lib.ssize, NULL);
            SET_SBAC_DEC(bs_lib, &ctx_lib->sbac_dec);

            if (bs_lib->cur[3] == 0xB0)
            {
                cnkh_lib->ctype = COM_CT_SQH;
            }
            else if (bs_lib->cur[3] == 0xB3 || bs_lib->cur[3] == 0xB6)
            {
                cnkh_lib->ctype = COM_CT_PICTURE;
            }
            else if (bs_lib->cur[3] >= 0x00 && bs_lib->cur[3] <= 0x8E)
            {
                cnkh_lib->ctype = COM_CT_SLICE;
            }

            if (cnkh_lib->ctype == COM_CT_SQH)
            {
                ret = dec_eco_sqh(bs_lib, sqh_lib);
                com_assert_rv(COM_SUCCEEDED(ret), ret);

                break;

                //fwrite(tmp_size_lib_sqh, 1, 4, fp_bs_write);
                //fwrite(bs_buf_lib_sqh, 1, bs_size_lib_sqh, fp_bs_write);
            }
            else
            {
                v0print("ERROR: The start code of lib bistream is not SPS \n");
                return -1;
            }
        }

        //read seq sqh
        bs_read_pos_seq = 0;
        while (1)
        {
            bs_read_pos_sqh = 0;
            bs_size_seq_sqh = read_a_bs(fp_bs_seq, &bs_read_pos_sqh, bs_buf_seq_sqh, bs_buf_lib2);

            tmp_size_seq_sqh[0] = (bs_size_seq_sqh & 0xff000000) >> 24;
            tmp_size_seq_sqh[1] = (bs_size_seq_sqh & 0x00ff0000) >> 16;
            tmp_size_seq_sqh[2] = (bs_size_seq_sqh & 0x0000ff00) >> 8;
            tmp_size_seq_sqh[3] = (bs_size_seq_sqh & 0x000000ff) >> 0;
            if (bs_size_seq_sqh <= 0)
            {
                v1print("bumping process starting...\n");
                continue;
            }

            bs_read_pos_seq += (bs_size_seq_sqh);
            bitb_seq.addr = bs_buf_seq_sqh;
            bitb_seq.ssize = bs_size_seq_sqh;
            bitb_seq.bsize = MAX_BS_BUF;
            DEC_ID_TO_CTX_RV(id_seq, ctx_seq, COM_ERR_INVALID_ARGUMENT);
            bs_seq = &ctx_seq->bs;
            sqh = &ctx_seq->info.sqh;
            pic_header = &ctx_seq->info.pic_header;
            cnkh_seq = &ctx_seq->info.cnkh;
            /* set error status */
            ctx_seq->bs_err = bitb_seq.err = 0;
            COM_TRACE_SET(1);
            /* bitstream reader initialization */
            com_bsr_init(bs_seq, bitb_seq.addr, bitb_seq.ssize, NULL);
            SET_SBAC_DEC(bs_seq, &ctx_seq->sbac_dec);

            if (bs_seq->cur[3] == 0xB0)
            {
                cnkh_seq->ctype = COM_CT_SQH;
            }
            else if (bs_seq->cur[3] == 0xB3 || bs_seq->cur[3] == 0xB6)
            {
                cnkh_seq->ctype = COM_CT_PICTURE;
            }
            else if (bs_seq->cur[3] >= 0x00 && bs_seq->cur[3] <= 0x8E)
            {
                cnkh_seq->ctype = COM_CT_SLICE;
            }

            if (cnkh_seq->ctype == COM_CT_SQH)
            {
                ret = dec_eco_sqh(bs_seq, sqh_seq);
                com_assert_rv(COM_SUCCEEDED(ret), ret);

                break;

                //fwrite(tmp_size_seq_sqh, 1, 4, fp_bs_write);
                //fwrite(bs_buf_seq_sqh, 1, bs_size_seq_sqh, fp_bs_write);
            }
            else
            {
                v0print("ERROR: The start code of seq bistream is not SPS \n");
                return -1;
            }
        }

        //merge lib and seq stream
        while (1)
        {
            bs_size_seq = read_a_bs(fp_bs_seq, &bs_read_pos_seq, bs_buf_seq, bs_buf_lib2);

            tmp_size_seq[0] = (bs_size_seq & 0xff000000) >> 24;
            tmp_size_seq[1] = (bs_size_seq & 0x00ff0000) >> 16;
            tmp_size_seq[2] = (bs_size_seq & 0x0000ff00) >> 8;
            tmp_size_seq[3] = (bs_size_seq & 0x000000ff) >> 0;
            if (bs_size_seq <= 0)
            {
                v1print("bumping process starting...\n");
                continue;
            }
            bs_read_pos_seq += (bs_size_seq);
            bitb_seq.addr = bs_buf_seq;
            bitb_seq.ssize = bs_size_seq;
            bitb_seq.bsize = MAX_BS_BUF;
            DEC_ID_TO_CTX_RV(id_seq, ctx_seq, COM_ERR_INVALID_ARGUMENT);
            bs_seq = &ctx_seq->bs;
            sqh = &ctx_seq->info.sqh;
            pic_header = &ctx_seq->info.pic_header;
            cnkh_seq = &ctx_seq->info.cnkh;
            /* set error status */
            ctx_seq->bs_err = bitb_seq.err = 0;
            COM_TRACE_SET(1);
            /* bitstream reader initialization */
            com_bsr_init(bs_seq, bitb_seq.addr, bitb_seq.ssize, NULL);
            SET_SBAC_DEC(bs_seq, &ctx_seq->sbac_dec);

            if (bs_seq->cur[3] == 0xB0)
            {
                cnkh_seq->ctype = COM_CT_SQH;
            }
            else if (bs_seq->cur[3] == 0xB3 || bs_seq->cur[3] == 0xB6)
            {
                cnkh_seq->ctype = COM_CT_PICTURE;
            }
            else if (bs_seq->cur[3] >= 0x00 && bs_seq->cur[3] <= 0x8E)
            {
                cnkh_seq->ctype = COM_CT_SLICE;
            }
            else if (bs_seq->cur[3] == 0xB1)
            {
                cnkh_seq->ctype = COM_CT_SLICE;
            }

            if (cnkh_seq->ctype == COM_CT_SQH)
            {
                ret = dec_eco_sqh(bs_seq, sqh);
                com_assert_rv(COM_SUCCEEDED(ret), ret);
                /*
                fwrite(tmp_size, 1, 4, fp_bs_write);
                fwrite(bs_buf, 1, bs_size, fp_bs_write);
                */
            }
            else if (cnkh_seq->ctype == COM_CT_PICTURE)
            {
                /* decode slice header */
                sh_seq->low_delay = sqh_seq->low_delay;
                sh_seq->tool_alf_on = sqh_seq->adaptive_leveling_filter_enable_flag;
                int need_minus_256 = 0;//MX: useless here ? no operation to the PM.
                ret = dec_eco_pic_header(bs_seq, sh_seq, sqh_seq, &need_minus_256);
                com_assert_rv(COM_SUCCEEDED(ret), ret);

                //find the referenced library_picture_index
                if (sh_seq->is_RLpic_flag && (sh_seq->rpl_l0.ref_pic_active_num>0 || sh_seq->rpl_l1.ref_pic_active_num>0))
                {
                    int referenced_lib_index;
                    if (sh_seq->rpl_l0.ref_pic_active_num > 0)
                    {
                        referenced_lib_index = sh_seq->rpl_l0.ref_pics_ddoi[0];
                        for (int i = 1; i < sh_seq->rpl_l0.ref_pic_active_num; i++)
                        {
                            if (referenced_lib_index != sh_seq->rpl_l0.ref_pics_ddoi[i])
                            {
                                v0print("ERROR: RL_pic can only reference 1 libpic at now \n");
                                return -1;
                            }
                        }
                        for (int i = 0; i < sh_seq->rpl_l1.ref_pic_active_num; i++)
                        {
                            if (referenced_lib_index != sh_seq->rpl_l1.ref_pics_ddoi[i])
                            {
                                v0print("ERROR: RL_pic can only reference 1 libpic at now \n");
                                return -1;
                            }
                        }
                    }
                    else
                    {
                        referenced_lib_index = sh_seq->rpl_l1.ref_pics_ddoi[0];
                        for (int i = 1; i < sh_seq->rpl_l1.ref_pic_active_num; i++)
                        {
                            if (referenced_lib_index != sh_seq->rpl_l1.ref_pics_ddoi[i])
                            {
                                v0print("ERROR: RL_pic can only reference 1 libpic at now \n");
                                return -1;
                            }
                        }
                    }

                    // when it's the first RL_pic || the referenced_library_picture_index is not same to the last_referenced_library_index
                    if (last_referenced_library_index < 0 || last_referenced_library_index != referenced_lib_index)
                    {
                        //find the referenced lib bitstream
                        tmp_bs_read_pos_lib = bs_read_pos_lib;
                        while (1)
                        {
                            bs_size_lib = read_a_bs(fp_bs_lib, &tmp_bs_read_pos_lib, bs_buf_lib, bs_buf_lib2);

                            tmp_size_lib[0] = (bs_size_lib & 0xff000000) >> 24;
                            tmp_size_lib[1] = (bs_size_lib & 0x00ff0000) >> 16;
                            tmp_size_lib[2] = (bs_size_lib & 0x0000ff00) >> 8;
                            tmp_size_lib[3] = (bs_size_lib & 0x000000ff) >> 0;
                            if (bs_size_lib <= 0)
                            {
                                v1print("bumping process starting...\n");
                                continue;
                            }

                            tmp_bs_read_pos_lib += (bs_size_lib);
                            bitb_lib.addr = bs_buf_lib;
                            bitb_lib.ssize = bs_size_lib;
                            bitb_lib.bsize = MAX_BS_BUF;
                            DEC_ID_TO_CTX_RV(id_lib, ctx_lib, COM_ERR_INVALID_ARGUMENT);
                            bs_lib = &ctx_lib->bs;
                            sqh = &ctx_lib->info.sqh;
                            pic_header = &ctx_lib->info.pic_header;
                            cnkh_lib = &ctx_lib->info.cnkh;
                            /* set error status */
                            ctx_lib->bs_err = bitb_lib.err = 0;
                            COM_TRACE_SET(1);
                            /* bitstream reader initialization */
                            com_bsr_init(bs_lib, bitb_lib.addr, bitb_lib.ssize, NULL);
                            SET_SBAC_DEC(bs_lib, &ctx_lib->sbac_dec);

                            if (bs_lib->cur[3] == 0xB0)
                            {
                                cnkh_lib->ctype = COM_CT_SQH;
                            }
                            else if (bs_lib->cur[3] == 0xB3 || bs_lib->cur[3] == 0xB6)
                            {
                                cnkh_lib->ctype = COM_CT_PICTURE;
                            }
                            else if (bs_lib->cur[3] >= 0x00 && bs_lib->cur[3] <= 0x8E)
                            {
                                cnkh_lib->ctype = COM_CT_SLICE;
                            }

                            if (cnkh_lib->ctype == COM_CT_SQH)
                            {
                                ret = dec_eco_sqh(bs_lib, sqh);
                                com_assert_rv(COM_SUCCEEDED(ret), ret);
                                /*
                                fwrite(tmp_size, 1, 4, fp_bs_write);
                                fwrite(bs_buf, 1, bs_size, fp_bs_write);
                                */
                            }
                            else if (cnkh_lib->ctype == COM_CT_PICTURE)
                            {
                                /* decode slice header */
                                sh_lib->low_delay = sqh_lib->low_delay;
                                sh_lib->tool_alf_on = sqh_lib->adaptive_leveling_filter_enable_flag;
                                int need_minus_256 = 0;//MX: useless here ? no operation to the PM.
                                ret = dec_eco_pic_header(bs_lib, sh_lib, sqh_lib, &need_minus_256);
                                com_assert_rv(COM_SUCCEEDED(ret), ret);

                                if (sh_lib->library_picture_index == referenced_lib_index)
                                {
                                    //write lib sps
                                    //fwrite(tmp_size_lib_sqh, 1, 4, fp_bs_write);
                                    fwrite(bs_buf_lib_sqh, 1, bs_size_lib_sqh, fp_bs_write);

                                    //write lib sh
                                    /* chnk size    */
                                    //fwrite(tmp_size_lib, 1, 4, fp_bs_write);
                                    fwrite(bs_buf_lib, 1, bs_size_lib, fp_bs_write);

                                    //set last_referenced_library_index
                                    last_referenced_library_index = referenced_lib_index;
                                }
                            }
                            else if (cnkh_lib->ctype == COM_CT_SLICE)
                            {
                                if (sh_lib->library_picture_index == referenced_lib_index)
                                {
                                    //write lib pic
                                    fwrite(bs_buf_lib, 1, bs_size_lib, fp_bs_write);

                                    //write seq sps
                                    //fwrite(tmp_size_seq_sqh, 1, 4, fp_bs_write);
                                    fwrite(bs_buf_seq_sqh, 1, bs_size_seq_sqh, fp_bs_write);

                                    break;
                                }

                            }

                            if (tmp_bs_read_pos_lib == bs_end_pos_lib)
                            {
                                v0print("ERROR: the referenced lib pic is not in the input lib bitstream! \n");
                                return -1;

                            }
                        }
                    }
                    else
                    {
                        //write seq sps
                        //fwrite(tmp_size_seq_sqh, 1, 4, fp_bs_write);
                        fwrite(bs_buf_seq_sqh, 1, bs_size_seq_sqh, fp_bs_write);
                    }
                }
                else if (sh_seq->slice_type == SLICE_I)
                {
                    //write seq sps
                    //fwrite(tmp_size_seq_sqh, 1, 4, fp_bs_write);
                    fwrite(bs_buf_seq_sqh, 1, bs_size_seq_sqh, fp_bs_write);
                }

                /* chnk size    */
                //fwrite(tmp_size_seq, 1, 4, fp_bs_write);
                fwrite(bs_buf_seq, 1, bs_size_seq, fp_bs_write);
            }
            else if (cnkh_seq->ctype == COM_CT_SLICE)
            {
                fwrite(bs_buf_seq, 1, bs_size_seq, fp_bs_write);
            }

            if (bs_read_pos_seq == bs_end_pos_seq)
            {
                break;
            }
        }

        if (sh_lib->pic_alf_on) free(sh_lib->pic_alf_on);
        if (sh_seq->pic_alf_on) free(sh_seq->pic_alf_on);
        if (sh_lib->m_alfPictureParam)
        {
            for (int g = 0; g < (int)N_C; g++)
            {
                freeAlfPar(sh_lib->m_alfPictureParam[g], g);
            }
            free(sh_lib->m_alfPictureParam);
            sh_lib->m_alfPictureParam = NULL;
        }
        if (sh_seq->m_alfPictureParam)
        {
            for (int g = 0; g < (int)N_C; g++)
            {
                freeAlfPar(sh_seq->m_alfPictureParam[g], g);
            }
            free(sh_seq->m_alfPictureParam);
            sh_seq->m_alfPictureParam = NULL;
        }
        if (imgb_t) imgb_free(imgb_t);
        if (id_lib) dec_delete(id_lib);
        ret = com_scan_tbl_init();
        if (ret != COM_OK)
        {
            v0print("ERROR: can not init scan_tbl\n");
            return -1;
        }
        if (id_seq) dec_delete(id_seq);
        if (fp_bs_lib) fclose(fp_bs_lib);
        if (fp_bs_seq) fclose(fp_bs_seq);
        if (sqh_lib) free(sqh_lib);
        if (sqh_seq) free(sqh_seq);
        if (sh_lib) free(sh_lib);
        if (sh_seq) free(sh_seq);
        if (bs_buf_lib) free(bs_buf_lib);
        if (bs_buf_seq) free(bs_buf_seq);
        if (bs_buf_lib_sqh) free(bs_buf_lib_sqh);
        if (bs_buf_seq_sqh) free(bs_buf_seq_sqh);
        if (fp_bs_write) fclose(fp_bs_write);
        delete_libvcdata(&libvc_data);
        return 0;
    }
    else
#endif
    {
#if BBV
        if (op_bbv_check_flag)
        {
            //initialize
            static int fps_code_tbl[14] = { -1, -1, 24, 25, -1, 30, 50, -1, 60, 100, 120, 200, 240, 300 };
            unsigned int bbv_size = 0;
            double T = 0;
            double Rmax = 0;
            int    output_reorder_delay = 0;
            int    is_bbv_delay_max = 0;
            double dec_time[BBV_CHECK_FRAMES_MAX];
            double delta_time[BBV_CHECK_FRAMES_MAX];
            double bits[BBV_CHECK_FRAMES_MAX];
            double bbv_state[BBV_CHECK_FRAMES_MAX];
            double bbv_delay[BBV_CHECK_FRAMES_MAX];
            double input_time[BBV_CHECK_FRAMES_MAX];
            double input_finish_time[BBV_CHECK_FRAMES_MAX];
            double bbv_check_times[BBV_CHECK_FRAMES_MAX];
#if BBV_LIBVC
            double D = 0;
            double P = 0;
            double reference_dec_time[BBV_CHECK_FRAMES_MAX];
            double reference_delta_time[BBV_CHECK_FRAMES_MAX];
            int    is_lib_pic[BBV_CHECK_FRAMES_MAX];
#endif
            for (int i = 0; i < BBV_CHECK_FRAMES_MAX; i++)
            {
                dec_time[i] = 0;
                delta_time[i] = 0;
                bits[i] = 0;
                bbv_state[i] = 0;
                bbv_delay[i] = 0;
                input_time[i] = 0;
                input_finish_time[i] = 0;
                bbv_check_times[i] = 0;
#if BBV_LIBVC
                reference_dec_time[i] = 0;
                reference_delta_time[i] = 0;
                is_lib_pic[i] = 0;
#endif
            }



            setvbuf(stdout, NULL, _IOLBF, 1024);
            fp_bs_seq = fopen(op_fname_inp_seq, "rb");
            if (fp_bs_seq == NULL)
            {
                v0print("ERROR: cannot open input bitstream file = %s\n", op_fname_inp_seq);
                print_usage();
                return -1;
            }
            fseek(fp_bs_seq, 0, SEEK_END);
            bs_end_pos_seq = ftell(fp_bs_seq);

            bs_buf_seq = malloc(MAX_BS_BUF);
            bs_buf_seq_sqh = malloc(MAX_BS_BUF);
            bs_buf_lib2 = malloc(MAX_BS_BUF);
            if (bs_buf_lib2 == NULL)
            {
                v0print("ERROR: cannot allocate bit buffer, size2=%d\n", MAX_BS_BUF);
                return -1;
            }

            if (bs_buf_seq == NULL || bs_buf_seq_sqh == NULL)
            {
                v0print("ERROR: cannot allocate bit buffer, size=%d\n", MAX_BS_BUF);
                return -1;
            }

            sqh_seq = malloc(sizeof(COM_SQH));
            sh_seq = malloc(sizeof(COM_PIC_HEADER));

            sh_seq->pic_alf_on = (int *)malloc(N_C * sizeof(int));
            sh_seq->m_alfPictureParam = (ALFParam **)malloc((N_C)* sizeof(ALFParam *));
            for (int i = 0; i < N_C; i++)
            {
                sh_seq->m_alfPictureParam[i] = NULL;
                AllocateAlfPar(&(sh_seq->m_alfPictureParam[i]), i);
            }

            if (sqh_seq == NULL || sh_seq == NULL || sh_seq->pic_alf_on == NULL || sh_seq->m_alfPictureParam == NULL)
            {
                v0print("ERROR: cannot allocate sqh or sh buffer\n");
                return -1;
            }

            id_seq = dec_create(&cdsc_seq, NULL);
            if (id_seq == NULL)
            {
                v0print("ERROR: cannot create decoder\n");
                return -1;
            }
            if (set_extra_config(id_seq))
            {
                v0print("ERROR: cannot set extra configurations\n");
                return -1;
            }
            DEC_CTX      * tmp_ctx;
            tmp_ctx = (DEC_CTX *)id_seq;
            tmp_ctx->dpm.libvc_data = &libvc_data;
            tmp_ctx = NULL;


            //parse the bitstream
            bs_read_pos_seq = 0;
            int dec_pic_count = 0;
            while (1)
            {
                bs_size_seq = read_a_bs(fp_bs_seq, &bs_read_pos_seq, bs_buf_seq, bs_buf_lib2);

                tmp_size_seq[0] = (bs_size_seq & 0xff000000) >> 24;
                tmp_size_seq[1] = (bs_size_seq & 0x00ff0000) >> 16;
                tmp_size_seq[2] = (bs_size_seq & 0x0000ff00) >> 8;
                tmp_size_seq[3] = (bs_size_seq & 0x000000ff) >> 0;
                if (bs_size_seq <= 0)
                {
                    v1print("bumping process starting...\n");
                    continue;
                }
                bs_read_pos_seq += (bs_size_seq);
                bitb_seq.addr = bs_buf_seq;
                bitb_seq.ssize = bs_size_seq;
                bitb_seq.bsize = MAX_BS_BUF;
                DEC_ID_TO_CTX_RV(id_seq, ctx_seq, COM_ERR_INVALID_ARGUMENT);
                bs_seq = &ctx_seq->bs;
                sqh = &ctx_seq->info.sqh;
                pic_header = &ctx_seq->info.pic_header;
                cnkh_seq = &ctx_seq->info.cnkh;
                /* set error status */
                ctx_seq->bs_err = bitb_seq.err = 0;
                COM_TRACE_SET(1);
                /* bitstream reader initialization */
                com_bsr_init(bs_seq, bitb_seq.addr, bitb_seq.ssize, NULL);
                SET_SBAC_DEC(bs_seq, &ctx_seq->sbac_dec);

                if (bs_seq->cur[3] == 0xB0)
                {
                    cnkh_seq->ctype = COM_CT_SQH;
                }
                else if (bs_seq->cur[3] == 0xB3 || bs_seq->cur[3] == 0xB6)
                {
                    cnkh_seq->ctype = COM_CT_PICTURE;
                }
                else if (bs_seq->cur[3] >= 0x00 && bs_seq->cur[3] <= 0x8E)
                {
                    cnkh_seq->ctype = COM_CT_SLICE;
                }
                else if (bs_seq->cur[3] == 0xB1)
                {
                    cnkh_seq->ctype = COM_CT_SEQ_END;
                }


                if (cnkh_seq->ctype == COM_CT_SQH)
                {
                    ret = dec_eco_sqh(bs_seq, sqh_seq);
                    com_assert_rv(COM_SUCCEEDED(ret), ret);

                    bbv_size = 16 * 1024 * sqh_seq->bbv_buffer_size;
                    Rmax = 400 * ((sqh_seq->bit_rate_upper << 18) + sqh_seq->bit_rate_lower);
                    T = 1.0 / fps_code_tbl[sqh_seq->frame_rate_code];
#if BBV_LIBVC
                    D = 1.0 / (fps_code_tbl[sqh_seq->frame_rate_code] + 1);
#endif
                    output_reorder_delay = sqh_seq->output_reorder_delay;

                    //sequence header is count into the bits of following I pic
                    bits[dec_pic_count] = bs_size_seq << 3;
                }
                else if (cnkh_seq->ctype == COM_CT_PICTURE)
                {
                    /* decode slice header */
                    sh_seq->low_delay = sqh_seq->low_delay;
                    sh_seq->tool_alf_on = sqh_seq->adaptive_leveling_filter_enable_flag;
                    int need_minus_256 = 0;//MX: useless here ? no operation to the PM.
                    ret = dec_eco_pic_header(bs_seq, sh_seq, sqh_seq, &need_minus_256 );
                    com_assert_rv(COM_SUCCEEDED(ret), ret);

                    if (dec_pic_count >= BBV_CHECK_FRAMES_MAX)
                    {
                        v0print("ERROR: the num of pic in the bitstream is larger than BBV_CHECK_FRAMES_MAX! You need to set the BBV_CHECK_FRAMES_MAX larger!\n");
                        return -1;
                    }


                    //record bbv_check_times
                    bbv_check_times[dec_pic_count] = sh_seq->bbv_check_times;

                    //picture header bits of each pic
                    bits[dec_pic_count] += bs_size_seq << 3;

                    //bbv delay time of each pic
                    if (sh_seq->bbv_delay != BBV_DELAY_MAX)
                    {
                        bbv_delay[dec_pic_count] = sh_seq->bbv_delay*1.0 / (90 * 1000);
                    }
                    else
                    {
                        is_bbv_delay_max = 1;
                        bbv_delay[dec_pic_count] = BBV_DELAY_MAX;
                    }

#if BBV_LIBVC                   
                    //is_lib_pic of each_pic
                    if (sh_seq->library_picture_index >= 0)
                    {
                        is_lib_pic[dec_pic_count] = 1;
                    }
                    else
                    {
                        is_lib_pic[dec_pic_count] = 0;
                    }
#endif

                    //delta time of each pic
#if BBV_LIBVC
                    if (!op_bbv_check_merge_steam)  //single stream
                    {
                        P = T;
                    }
                    else  //merge stream
                    {
                        P = D;
                    }

                    int recode_flag = 1; //whether to record the delta_time and reference_delta_time of current frame.
                    if (op_bbv_check_merge_steam && is_lib_pic[dec_pic_count]) //the libpics in merge stream will not display, so they don't have accordingly delta_time and reference_delta_time.
                    {
                        recode_flag = 0;
                    }

                    if (recode_flag)
                    {
                        if (sqh_seq->progressive_sequence && !sqh_seq->field_coded_sequence)
                        {
                            if (!sh_seq->repeat_first_field)
                            {
                                delta_time[sh_seq->poc] = P;
                                reference_delta_time[sh_seq->poc] = T;
                            }
                            else
                            {
                                if (!sh_seq->top_field_first)
                                {
                                    delta_time[sh_seq->poc] = 2 * P;
                                    reference_delta_time[sh_seq->poc] = 2 * T;
                                }
                                else
                                {
                                    delta_time[sh_seq->poc] = 3 * P;
                                    reference_delta_time[sh_seq->poc] = 3 * T;
                                }
                            }
                        }
                        else if (!sqh_seq->progressive_sequence && !sqh_seq->field_coded_sequence)
                        {
                            if (!sh_seq->repeat_first_field)
                            {
                                delta_time[sh_seq->poc] = P;
                                reference_delta_time[sh_seq->poc] = T;
                            }
                            else
                            {
                                delta_time[sh_seq->poc] = 1.5 * P;
                                reference_delta_time[sh_seq->poc] = 1.5 * T;
                            }
                        }
                        else if (!sqh_seq->progressive_sequence && sqh_seq->field_coded_sequence)
                        {
                            delta_time[sh_seq->poc] = 0.5 * P;
                            reference_delta_time[sh_seq->poc] = 0.5 * T;
                        }
                    }
#else
                    if (sqh_seq->progressive_sequence && !sqh_seq->field_coded_sequence)
                    {
                        if (!sh_seq->repeat_first_field)
                        {
                            delta_time[sh_seq->poc] = T;
                        }
                        else
                        {
                            if (!sh_seq->top_field_first)
                            {
                                delta_time[sh_seq->poc] = 2 * T;
                            }
                            else
                            {
                                delta_time[sh_seq->poc] = 3 * T;
                            }
                        }
                    }
                    else if (!sqh_seq->progressive_sequence && !sqh_seq->field_coded_sequence)
                    {
                        if (!sh_seq->repeat_first_field)
                        {
                            delta_time[sh_seq->poc] = T;
                        }
                        else
                        {
                            delta_time[sh_seq->poc] = 1.5 * T;
                        }
                    }
                    else if (!sqh_seq->progressive_sequence && sqh_seq->field_coded_sequence)
                    {
                        delta_time[sh_seq->poc] = 0.5 * T;
                    }
#endif
                }
                else if (cnkh_seq->ctype == COM_CT_SLICE)
                {
                    //bits of each pic
                    bits[dec_pic_count] += bs_size_seq << 3;
                    dec_pic_count++;
                }

                if (bs_read_pos_seq == bs_end_pos_seq)
                {
                    break;
                }
            }

            //------------------------------------------------------------------------------------------------------------------------------

            //when random access is made in a sequence, the delta_time of the first few frames cannot be determined from the video bitstream alone. 
            //Because output reorder delay exists, these frames decoded without output frames. We assume the delta_time of these frames is "T".
            if (output_reorder_delay > 0)
            {
                for (int i = BBV_CHECK_FRAMES_MAX - 1; i >= output_reorder_delay - 1; i--)
                {
                    delta_time[i] = delta_time[i - (output_reorder_delay - 1)];
#if BBV_LIBVC
                    reference_delta_time[i] = reference_delta_time[i - (output_reorder_delay - 1)];
#endif
                }

                for (int i = 0; i < output_reorder_delay - 1; i++)
                {
#if BBV_LIBVC
                    delta_time[i] = P;
                    reference_delta_time[i] = T;
#else
                    delta_time[i] = T;
#endif
                }
            }

            //---decode time of each pic------//
            //the decode time of the first pic
            if (!is_bbv_delay_max)
            {
                dec_time[0] = bbv_delay[0];
            }
            else
            {
                dec_time[0] = bbv_size*1.0 / Rmax;
            }
#if BBV_LIBVC
            reference_dec_time[0] = dec_time[0];
#endif
            //the decode time of the following pics
            int dec_non_libpic_count = 0;
            for (int i = 1; i < dec_pic_count; i++)
            {
                if (!sqh_seq->low_delay)
                {
#if BBV_LIBVC
                    if (!op_bbv_check_merge_steam)  //single stream
                    {
                        dec_time[i] = dec_time[i - 1] + delta_time[i - 1];
                    }
                    else  //merge stream
                    {
                        if (dec_non_libpic_count <= 0)
                        {
                            dec_time[i] = dec_time[i - 1] + P;
                            reference_dec_time[i] = dec_time[i];
                            if (!is_lib_pic[i])
                            {
                                ++dec_non_libpic_count;
                            }
                        }
                        else
                        {
                            if (is_lib_pic[i])
                            {
                                dec_time[i] = dec_time[i - 1] + delta_time[dec_non_libpic_count - 1];
                                reference_dec_time[i] = reference_dec_time[i - 1];
                            }
                            else
                            {
                                reference_dec_time[i] = reference_dec_time[i - 1] + reference_delta_time[dec_non_libpic_count - 1];
                                if (dec_non_libpic_count > 1 && dec_time[i - 1] + delta_time[dec_non_libpic_count - 1] < reference_dec_time[i] - reference_delta_time[dec_non_libpic_count - 1] - reference_delta_time[dec_non_libpic_count - 2])
                                {
                                    dec_time[i] = dec_time[i - 1] + 2 * delta_time[dec_non_libpic_count - 1];
                                }
                                else
                                {
                                    dec_time[i] = dec_time[i - 1] + delta_time[dec_non_libpic_count - 1];
                                }

                                ++dec_non_libpic_count;
                            }
                        }
                    }
#else
                    dec_time[i] = dec_time[i - 1] + delta_time[i - 1];
#endif

                }
                else
                {
                    double picture_period = 0;
                    if (sqh_seq->field_coded_sequence)
                    {
                        picture_period = T * 0.5;
                    }
                    else
                    {
                        picture_period = T;
                    }

#if BBV_LIBVC
                    if (!op_bbv_check_merge_steam)  //single stream
                    {
                        dec_time[i] = dec_time[i - 1] + delta_time[i - 1] + bbv_check_times[i] * picture_period;
                    }
                    else  //merge stream
                    {
                        if (dec_non_libpic_count <= 0)
                        {
                            dec_time[i] = dec_time[i - 1] + P + bbv_check_times[i] * picture_period;;
                            reference_dec_time[i] = dec_time[i];
                            if (!is_lib_pic[i])
                            {
                                ++dec_non_libpic_count;
                            }
                        }
                        else
                        {
                            if (is_lib_pic[i])
                            {
                                dec_time[i] = dec_time[i - 1] + delta_time[dec_non_libpic_count - 1];
                                reference_dec_time[i] = reference_dec_time[i - 1];
                            }
                            else
                            {
                                reference_dec_time[i] = reference_dec_time[i - 1] + reference_delta_time[dec_non_libpic_count - 1] + bbv_check_times[i] * picture_period;
                                if (dec_non_libpic_count > 1 && (!is_lib_pic[i - 1]) && dec_time[i - 1] + delta_time[dec_non_libpic_count - 1] + bbv_check_times[i] * picture_period < reference_dec_time[i] - reference_delta_time[dec_non_libpic_count - 1] - bbv_check_times[i] * picture_period - reference_delta_time[dec_non_libpic_count - 2] - bbv_check_times[i - 1] * picture_period)
                                {
                                    dec_time[i] = dec_time[i - 1] + 2 * delta_time[dec_non_libpic_count - 1] + bbv_check_times[i] * picture_period;
                                }
                                else if (dec_non_libpic_count > 1 && is_lib_pic[i - 1] && dec_time[i - 1] + delta_time[dec_non_libpic_count - 1] + bbv_check_times[i] * picture_period < reference_dec_time[i] - reference_delta_time[dec_non_libpic_count - 1] - bbv_check_times[i] * picture_period - reference_delta_time[dec_non_libpic_count - 2] - bbv_check_times[i - 2] * picture_period)
                                {
                                    dec_time[i] = dec_time[i - 1] + 2 * delta_time[dec_non_libpic_count - 1] + bbv_check_times[i] * picture_period;
                                }
                                else
                                {
                                    dec_time[i] = dec_time[i - 1] + delta_time[dec_non_libpic_count - 1] + bbv_check_times[i] * picture_period;
                                }


                                ++dec_non_libpic_count;
                            }
                        }
                    }
#else
                    dec_time[i] = dec_time[i - 1] + delta_time[i - 1] + bbv_check_times[i] * picture_period;
#endif
                }
            }

            //---bbv check function-------//
            BBV_STATES bbv_check_flag = BBV_OK;
            int suspend_doi = -1;
            double R = 0;
            if (!is_bbv_delay_max)
            {
                //input time of each pic
                input_time[0] = 0;
                for (int i = 1; i < dec_pic_count; i++)
                {
                    input_time[i] = dec_time[i] - bbv_delay[i];
                    if (input_time[i] <= input_time[i - 1])
                    {
                        bbv_check_flag = BBV_DELAY_WRONG;
                        suspend_doi = i;
                        break;
                    }
                }

                //bbv count and check
                if (bbv_check_flag == BBV_OK)
                {
                    //input finish time of each pic
                    for (int i = 0; i < dec_pic_count - 1; i++)
                    {
                        input_finish_time[i] = min(dec_time[i], input_time[i + 1]);
                    }
                    input_finish_time[dec_pic_count - 1] = dec_time[dec_pic_count - 1];

                    //check each pic
                    double cur_bbv = 0;
                    int bbv_check_count = 0;
                    for (int idx = 0; idx < dec_pic_count; idx++)
                    {
                        //check input speed: the input speed must smaller that Rmax
                        R = bits[idx] / (input_finish_time[idx] - input_time[idx]);
                        if (R > Rmax)
                        {
                            bbv_check_flag = BBV_INPUT_SPEED_OVERFLOW;
                            suspend_doi = idx;
                            break;
                        }

                        //bbv state check
                        if (input_finish_time[idx] < dec_time[bbv_check_count])
                        {
                            cur_bbv += bits[idx];
                        }
                        else
                        {
                            cur_bbv += bits[idx] * (dec_time[bbv_check_count] - input_time[idx]) / (input_finish_time[idx] - input_time[idx]);

                            bbv_state[bbv_check_count] = cur_bbv;
                            if (cur_bbv > bbv_size)
                            {
                                bbv_check_flag = BBV_OVERFLOW;
                                suspend_doi = idx;
                                break;
                            }
                            else
                            {
                                cur_bbv -= bits[bbv_check_count];
                                ++bbv_check_count;

                                while (input_finish_time[idx] >= dec_time[bbv_check_count] && bbv_check_count < dec_pic_count)
                                {
                                    cur_bbv += bits[idx] * (dec_time[bbv_check_count] - dec_time[bbv_check_count - 1]) / (input_finish_time[idx] - input_time[idx]);

                                    bbv_state[bbv_check_count] = cur_bbv;
                                    if (cur_bbv > bbv_size)
                                    {
                                        bbv_check_flag = BBV_OVERFLOW;
                                        suspend_doi = idx;
                                        break;
                                    }
                                    else
                                    {
                                        cur_bbv -= bits[bbv_check_count];
                                        ++bbv_check_count;
                                    }
                                }
                                cur_bbv += bits[idx] * (input_finish_time[idx] - dec_time[bbv_check_count - 1]) / (input_finish_time[idx] - input_time[idx]);

                            }
                        }
                    }
                }
            }
            else
            {
                if (bbv_size < bits[0])
                {
                    bbv_check_flag = BBV_DOWNFLOW;
                    suspend_doi = 0;
                }
                else
                {
                    bbv_state[0] = bbv_size;
                    double cur_bbv = bbv_size - bits[0];
                    for (int i = 1; i < dec_pic_count; i++)
                    {
                        double last_bbv = cur_bbv;
                        cur_bbv = min(bbv_size, last_bbv + Rmax*(dec_time[i] - dec_time[i - 1]));

                        bbv_state[i] = cur_bbv;
                        if (cur_bbv < bits[i])
                        {
                            bbv_check_flag = BBV_DOWNFLOW;
                            suspend_doi = i;
                            break;
                        }
                        else
                        {
                            cur_bbv -= bits[i];
                        }
                    }
                }
            }

            if (bbv_check_flag == BBV_OK)
            {
                v1print("BBV check is OK!\n");
            }
            else if (bbv_check_flag == BBV_OVERFLOW)
            {
                v0print("ERROR: BBV will overflow at the pic whose doi is %d!\n", suspend_doi);
            }
            else if (bbv_check_flag == BBV_DOWNFLOW)
            {
                v0print("ERROR: BBV will downflow at the pic whose doi is %d!\n", suspend_doi);
            }
            else if (bbv_check_flag == BBV_DELAY_WRONG)
            {
                v0print("ERROR: The set of BBV_DELAY is wrong result in that frames can not be input in order at the pic whose doi is %d!\nYou need to reset the value of each frame's parameter 'bbv_delay'.\n", suspend_doi);
            }
            else if (bbv_check_flag == BBV_INPUT_SPEED_OVERFLOW)
            {
                v0print("ERROR: The input BBV speed of the frame whose doi is %d exceeds the Rmax, and its input speed is %f!\nYou need to reset the value of each frame's parameter 'bbv_delay'.\n", suspend_doi, R);
            }
            else
            {
                v0print("ERROR: There is something wrong in the code when setting the value of bbv_check_flag!\n");
            }

            FILE *fp;
            fp = fopen("bbv_state.txt", "w");
            fprintf(fp, "BBS: %u, T: %f (s), D: %f (s),  Rmax: %f (bit/s)\n", bbv_size, T, D, Rmax);
            fprintf(fp, "is_merge_stream: %d, low_delay: %d, is_bbv_delay_max: %d\n", op_bbv_check_merge_steam, sqh_seq->low_delay, is_bbv_delay_max);
            if (op_bbv_check_merge_steam)
            {
                fprintf(fp, "\n//cur_bbv_size    picture_bits    dec_time    ref_dec_time   bbv_delay\n");
            }
            else
            {
                fprintf(fp, "\n//cur_bbv_size    picture_bits    dec_time    bbv_delay\n");
            }
            for (int k = 0; k<dec_pic_count; k++)
            {
                if (op_bbv_check_merge_steam)
                {
                    fprintf(fp, "%f, %.0f, %f, %f, %f\n", bbv_state[k], bits[k], dec_time[k], reference_dec_time[k], bbv_delay[k]);
                }
                else
                {
                    fprintf(fp, "%f, %.0f, %f, %f\n", bbv_state[k], bits[k], dec_time[k], bbv_delay[k]);
                }
            }
            fclose(fp);


            if (sh_seq->pic_alf_on) free(sh_seq->pic_alf_on);
            if (sh_seq->m_alfPictureParam)
            {
                for (int g = 0; g < (int)N_C; g++)
                {
                    freeAlfPar(sh_seq->m_alfPictureParam[g], g);
                }
                free(sh_seq->m_alfPictureParam);
                sh_seq->m_alfPictureParam = NULL;
            }
            if (imgb_t) imgb_free(imgb_t);
            if (id_lib) dec_delete(id_lib);
            ret = com_scan_tbl_init();
            if (ret != COM_OK)
            {
                v0print("ERROR: can not init scan_tbl\n");
                return -1;
            }
            if (id_seq) dec_delete(id_seq);
            if (fp_bs_seq) fclose(fp_bs_seq);
            if (sqh_seq) free(sqh_seq);
            if (sh_seq) free(sh_seq);
            if (bs_buf_seq) free(bs_buf_seq);
            if (bs_buf_seq_sqh) free(bs_buf_seq_sqh);
            delete_libvcdata(&libvc_data);
            return 0;

        }
        else
#endif
#if MERGE_BITSTREAM
        {
            // set line buffering (_IOLBF) for stdout to prevent incomplete logs when app crashed.
            setvbuf(stdout, NULL, _IOLBF, 1024);
            max_bs_num = argc - 2;
            fp_bs_write = fopen(argv[max_bs_num + 1], "wb");

            int pictureslice = 0;			  
            int pic_cnt = 0;	             
            int bitstreamno = -1;             
            int i_period = 0;                
            int i_periodflag = 0;			
            int max_bframes = 0;             
            int gopflag = 0;                 
            for (bs_num = 1; bs_num < max_bs_num; bs_num++)
            {
                ++bitstreamno;
                ++bs_num;
                fp_bs = fopen(argv[bs_num], "rb");
                static int same_ISLICE_flag = 0; 

                if (fp_bs == NULL)
                {
                    v0print("ERROR: cannot open bitstream file = %s\n", op_fname_inp);
                    print_usage();
                    return -1;
                }
                fseek(fp_bs, 0, SEEK_END);
                bs_end_pos = ftell(fp_bs);

                bs_buf = malloc(MAX_BS_BUF);
                if (bs_buf == NULL)
                {
                    v0print("ERROR: cannot allocate bit buffer, size=%d\n", MAX_BS_BUF);
                    return -1;
                }
                id = dec_create(&cdsc, NULL);
                if (id == NULL)
                {
                    v0print("ERROR: cannot create decoder\n");
                    return -1;
                }
                if (set_extra_config(id))
                {
                    v0print("ERROR: cannot set extra configurations\n");
                    return -1;
                }
                new_bs_buf = malloc(MAX_BS_BUF);
                if (new_bs_buf == NULL)
                {
                    v0print("ERROR: cannot allocate bit buffer, size=%d\n", MAX_BS_BUF);
                    return -1;
                }
                new_bs_buf2 = malloc(MAX_BS_BUF);
                if (new_bs_buf2 == NULL)
                {
                    v0print("ERROR: cannot allocate bit buffer, size=%d\n", MAX_BS_BUF);
                    return -1;
                }
                new_bitb.addr = new_bs_buf;
                new_bitb.addr2 = new_bs_buf2;
                new_bitb.bsize = MAX_BS_BUF;
                new_bitb.err = 0;
                bs_read_pos = 0;
                while (1)
                {
                    bs_size = read_a_bs(fp_bs, &bs_read_pos, bs_buf, bs_buf_lib2);
                    if (bs_size <= 0)
                    {
                        v1print("bumping process starting...\n");
                        continue;
                    }

                    bs_read_pos += (bs_size);
                    bitb.addr = bs_buf;
                    bitb.ssize = bs_size;
                    bitb.bsize = MAX_BS_BUF;
                    ctx = (DEC_CTX *)id;
                    DEC_ID_TO_CTX_RV(id, ctx, COM_ERR_INVALID_ARGUMENT);
                    bs = &ctx->bs;
                    sqh = &ctx->info.sqh;
                    pic_header = &ctx->info.pic_header;
                    shext = &ctx->info.shext;
                    cnkh = &ctx->info.cnkh;
                    /* set error status */
                    ctx->bs_err = 0;
                    bitb.err = 0;
                    COM_TRACE_SET(1);
                    /* bitstream reader initialization */
                    com_bsr_init(bs, bitb.addr, bitb.ssize, NULL);
                    SET_SBAC_DEC(bs, &ctx->sbac_dec);

                    if (bs->cur[3] == 0xB0)
                    {
                        cnkh->ctype = COM_CT_SQH;
                    }
                    else if (bs->cur[3] == 0xB3 || bs->cur[3] == 0xB6)
                    {
                        cnkh->ctype = COM_CT_PICTURE;
                        if (bs->cur[3] == 0xB3) pictureslice = 0;
                        if (bs->cur[3] == 0xB6)	pictureslice = 1;
                    }
                    else if (bs->cur[3] >= 0x00 && bs->cur[3] <= 0x8E)
                    {
                        cnkh->ctype = COM_CT_SLICE;
                    }

                    if (cnkh->ctype == COM_CT_SQH)
                    {
                        ret = dec_eco_sqh(bs, sqh);
                        com_assert_rv(COM_SUCCEEDED(ret), ret);
                        if (!ctx->init_flag)
                        {
                            ret = sequence_init(ctx, sqh);
                            com_assert_rv(COM_SUCCEEDED(ret), ret);
                            g_DOIPrev = g_CountDOICyCleTime = 0;
                            ctx->init_flag = 1;
                        }
                        if (bitstreamno == 0)
                        {
                            fwrite(bs_buf, 1, bs_size, fp_bs_write);
                        }
                        else
                        {
                            if (same_ISLICE_flag == 2)
                            {
                                fseek(fp_bs_write, -4, SEEK_CUR);
                            }
                            else
                            {
                                fwrite(bs_buf, 1, bs_size, fp_bs_write);
                            }
                        }

                    }
                    else if (cnkh->ctype == COM_CT_PICTURE)
                    {
                        /* decode slice header */
                        if (bitstreamno == 0)
                        {
                            if (bs->cur[3] == 0xB3 && same_ISLICE_flag < 2)
                            {
                                if (i_periodflag == 0) i_periodflag++;
                                else if (i_periodflag == 1)
                                {
                                    int need_minus_256 = 0;
                                    ret = dec_eco_pic_header(bs, pic_header, sqh, &need_minus_256);
                                    com_assert_rv(COM_SUCCEEDED(ret), ret);
                                    i_period = pic_header->poc;
                                    i_periodflag++;
                                }
                                fwrite(bs_buf, 1, bs_size, fp_bs_write);
                            }
                            if (bs->cur[3] == 0xB6)
                            {
                                if (gopflag == 0)
                                {
                                    int need_minus_256 = 0;
                                    ret = dec_eco_pic_header(bs, pic_header, sqh, &need_minus_256);
                                    com_assert_rv(COM_SUCCEEDED(ret), ret);
                                    max_bframes = pic_header->poc - 1;
                                    gopflag++;
                                }
                                fwrite(bs_buf, 1, bs_size, fp_bs_write);
                            }
                        }
                        else
                        {
                            if (bs->cur[3] == 0xB3 && same_ISLICE_flag == 1)       
                            {
                                pic_header->low_delay = sqh->low_delay;							
                                int need_minus_256 = 0;
                                ret = dec_eco_pic_header(bs, pic_header, sqh, &need_minus_256);
                                com_assert_rv(COM_SUCCEEDED(ret), ret);


                                if (pic_header->rpl_l0.ref_pic_num > 0)
                                    for (int j = 0; j < pic_header->rpl_l0.ref_pic_num; j++)
                                    {
                                        if (pic_header->rpl_l0.ref_pics_ddoi[j] == pic_header->decode_order_index)
                                        {
                                            pic_header->rpl_l0.ref_pics_ddoi[j] += max_bframes;
                                            if (pic_header->ref_pic_list_sps_flag[0] == 1)
                                            {
                                                pic_header->ref_pic_list_sps_flag[0] = 0;
                                            }
                                            break;
                                        }
                                    }
                                if (pic_header->rpl_l1.ref_pic_num > 0)
                                    for (int j = 0; j < pic_header->rpl_l1.ref_pic_num; j++)
                                    {
                                        if (pic_header->rpl_l1.ref_pics_ddoi[j] == pic_header->decode_order_index)
                                        {
                                            pic_header->rpl_l1.ref_pics_ddoi[j] += max_bframes;
                                            if (pic_header->ref_pic_list_sps_flag[1] == 1)
                                            {
                                                pic_header->ref_pic_list_sps_flag[1] = 0;
                                            }
                                            break;
                                        }
                                    }

                                pic_header->poc = pic_header->poc + i_period * bitstreamno;
                                pic_header->decode_order_index = pic_cnt;
                                pic_header->picture_output_delay = pic_header->poc - pic_cnt + ctx->info.sqh.output_reorder_delay; //output_delay = POI - DOI + reoder



                                com_bsw_init(new_bs, new_bs_buf, new_bs_buf2, new_bitb.bsize, NULL);

                                ret = enc_eco_pic_header(new_bs, pic_header, sqh);
                                com_assert_rv(ret == COM_OK, ret);

                                
                                fwrite(new_bs->beg, 1, new_bs->cur - new_bs->beg, fp_bs_write);
#if EXTENSION_USER_DATA && WRITE_MD5_IN_USER_DATA
                                if( com_bsr_next( bs, 32 ) == 0x1B2 )
                                {
                                    fwrite( startcodebuf, 1, 3, fp_bs_write );
                                    int offsetsize = 3 - bs->leftbits / 8;
                                    bs->cur += offsetsize;
                                    fwrite( bs->cur, 1, (bs->end - 3) - bs->cur + 1, fp_bs_write );
                                }
#endif

                            }
                            else if (bs->cur[3] == 0xB6)                  
                            {
                                pic_header->low_delay = sqh->low_delay;
                                int need_minus_256 = 0;
                                ret = dec_eco_pic_header(bs, pic_header, sqh, &need_minus_256);
                                com_assert_rv(COM_SUCCEEDED(ret), ret);

                            
                                if (same_ISLICE_flag != 2)          
                                {
                                    if (pic_header->rpl_l0.ref_pic_num > 0)
                                        for (int j = 0; j < pic_header->rpl_l0.ref_pic_num; j++)
                                        {
                                            if (pic_header->rpl_l0.ref_pics_ddoi[j] == pic_header->decode_order_index)
                                            {
                                                pic_header->rpl_l0.ref_pics_ddoi[j] += max_bframes;
                                                if (pic_header->ref_pic_list_sps_flag[0] == 1)
                                                {
                                                    pic_header->ref_pic_list_sps_flag[0] = 0;
                                                }
                                                break;
                                            }
                                        }
                                    if (pic_header->rpl_l1.ref_pic_num > 0)
                                        for (int j = 0; j < pic_header->rpl_l1.ref_pic_num; j++)
                                        {
                                            if (pic_header->rpl_l1.ref_pics_ddoi[j] == pic_header->decode_order_index)
                                            {
                                                pic_header->rpl_l1.ref_pics_ddoi[j] += max_bframes;
                                                if (pic_header->ref_pic_list_sps_flag[1] == 1)
                                                {
                                                    pic_header->ref_pic_list_sps_flag[1] = 0;
                                                }
                                                break;
                                            }
                                        }
                                }

                            
                                pic_header->poc = pic_header->poc + i_period * bitstreamno;

                                pic_header->decode_order_index = pic_cnt;

                                pic_header->picture_output_delay = pic_header->poc - pic_cnt + ctx->info.sqh.output_reorder_delay; //output_delay = POI - DOI + reoder

                                com_bsw_init(new_bs, new_bs_buf, new_bs_buf2, new_bitb.bsize, NULL);

                                ret = enc_eco_pic_header(new_bs, pic_header, sqh);
                                com_assert_rv(ret == COM_OK, ret);

                            
                                fwrite(new_bs->beg, 1, new_bs->cur - new_bs->beg, fp_bs_write);
#if EXTENSION_USER_DATA && WRITE_MD5_IN_USER_DATA
                                if( com_bsr_next( bs, 32 ) == 0x1B2 )
                                {
                                    fwrite( startcodebuf, 1, 3, fp_bs_write );
                                    int offsetsize = 3 - bs->leftbits / 8;
                                    bs->cur += offsetsize;
                                    fwrite( bs->cur, 1, (bs->end - 3) - bs->cur + 1, fp_bs_write );
                                }
#endif

                            }
                        }
                    }
                    else if (cnkh->ctype == COM_CT_SLICE)
                    {
                        if (pictureslice == 0)
                        {
                            if (same_ISLICE_flag == 0)
                            {
                                fwrite(bs_buf, 1, bs_size, fp_bs_write);
                                same_ISLICE_flag++;
                                pic_cnt++;
                            }
                            else if (same_ISLICE_flag == 1)
                            {
                                fwrite(bs_buf, 1, bs_size, fp_bs_write);
                                same_ISLICE_flag++;
                                pic_cnt++;
                            }
                            else
                            {
                                same_ISLICE_flag--;
                            }
                        }
                        if (pictureslice == 1)
                        {
                            fwrite(bs_buf, 1, bs_size, fp_bs_write);
                            pic_cnt++;
                        }

                    }

                    if (bs_read_pos == bs_end_pos)
                    {
                        pic_cnt--;
                        break;
                    }
                }
                if (id) dec_delete(id);
                if (imgb_t) imgb_free(imgb_t);
                if (fp_bs) fclose(fp_bs);
                if (bs_buf) free(bs_buf);
        }
            if (fp_bs_write) fclose(fp_bs_write);
            return 0;
        }
#else
        {
            // set line buffering (_IOLBF) for stdout to prevent incomplete logs when app crashed.
            setvbuf(stdout, NULL, _IOLBF, 1024);
            max_bs_num = argc - 2;
            fp_bs_write = fopen(argv[max_bs_num + 1], "wb");
            for (bs_num = 0; bs_num < max_bs_num; bs_num++)
            {
                fp_bs = fopen(argv[bs_num + 1], "rb");
                if (fp_bs == NULL)
                {
                    v0print("ERROR: cannot open bitstream file = %s\n", op_fname_inp);
                    print_usage();
                    return -1;
                }
                fseek(fp_bs, 0, SEEK_END);
                bs_end_pos = ftell(fp_bs);
                intra_dist_idx = 0;
                if (bs_num) intra_dist[0] += intra_dist[1];
                bs_buf = malloc(MAX_BS_BUF);
                if (bs_buf == NULL)
                {
                    v0print("ERROR: cannot allocate bit buffer, size=%d\n", MAX_BS_BUF);
                    return -1;
                }
                id = dec_create(&cdsc, NULL);
                if (id == NULL)
                {
                    v0print("ERROR: cannot create decoder\n");
                    return -1;
                }
                if (set_extra_config(id))
                {
                    v0print("ERROR: cannot set extra configurations\n");
                    return -1;
                }
                bs_read_pos = 0;
                while (1)
                {
                    bs_size = read_a_bs(fp_bs, &bs_read_pos, bs_buf, bs_buf_lib2);

                    tmp_size[0] = (bs_size & 0xff000000) >> 24;
                    tmp_size[1] = (bs_size & 0x00ff0000) >> 16;
                    tmp_size[2] = (bs_size & 0x0000ff00) >> 8;
                    tmp_size[3] = (bs_size & 0x000000ff) >> 0;
                    if (bs_size <= 0)
                    {
                        v1print("bumping process starting...\n");
                        continue;
                    }

                    bs_read_pos += (bs_size);
                    bitb.addr = bs_buf;
                    bitb.ssize = bs_size;
                    bitb.bsize = MAX_BS_BUF;
                    DEC_ID_TO_CTX_RV(id, ctx, COM_ERR_INVALID_ARGUMENT);
                    bs = &ctx->bs;
                    sqh = &ctx->info.sqh;
                    pic_header = &ctx->info.pic_header;
                    cnkh = &ctx->info.cnkh;
                    /* set error status */
                    ctx->bs_err = bitb.err = 0;
                    COM_TRACE_SET(1);
                    /* bitstream reader initialization */
                    com_bsr_init(bs, bitb.addr, bitb.ssize, NULL);
                    SET_SBAC_DEC(bs, &ctx->sbac_dec);

                    if (bs->cur[3] == 0xB0)
                    {
                        cnkh->ctype = COM_CT_SQH;
                    }
                    else if (bs->cur[3] == 0xB3 || bs->cur[3] == 0xB6)
                    {
                        cnkh->ctype = COM_CT_PICTURE;
                    }
                    else if (bs->cur[3] >= 0x00 && bs->cur[3] <= 0x8E)
                    {
                        cnkh->ctype = COM_CT_SLICE;
                    }

                    if (cnkh->ctype == COM_CT_SQH)
                    {
                        ret = dec_eco_sqh(bs, sqh);
                        com_assert_rv(COM_SUCCEEDED(ret), ret);
                        if (!bs_num)
                        {
                            fwrite(tmp_size, 1, 4, fp_bs_write);
                            fwrite(bs_buf, 1, bs_size, fp_bs_write);
                        }
                    }
                    else if (cnkh->ctype == COM_CT_PICTURE)
                    {
                        /* decode slice header */
                        pic_header->low_delay = sqh->low_delay;
                        int need_minus_256 = 0;//MX: useless here ? no operation to the PM.
                        ret = dec_eco_pic_header(bs, pic_header, sqh, &need_minus_256);
                        com_assert_rv(COM_SUCCEEDED(ret), ret);

#if LIBVC_ON
                        if (bs_num == 0 && (pic_header->slice_type == SLICE_I || (sqh->library_picture_enable_flag && pic_header->is_RLpic_flag)))
#else
                        if (bs_num == 0 && pic_header->slice_type == SLICE_I)
#endif
                        {
                            intra_dist[intra_dist_idx] = pic_header->dtr;
                            intra_dist_idx++;
                        }
                        if (bs_num == 0)
                        {
                            fwrite(tmp_size, 1, 4, fp_bs_write);
                            fwrite(bs_buf, 1, bs_size, fp_bs_write);
                        }
                        else
                        {
#if LIBVC_ON
                            if (!intra_dist_idx && (pic_header->slice_type == SLICE_I || (sqh->library_picture_enable_flag && pic_header->is_RLpic_flag)))
#else
                            if (!intra_dist_idx && pic_header->slice_type == SLICE_I)
#endif
                            {
                                intra_dist_idx++;
                            }
                            else
                            {
                                /* chnk size    */
                                fwrite(tmp_size, 1, 4, fp_bs_write);
                                /* chnk header  */
                                fwrite(bs_buf, 1, 4, fp_bs_write);
                                /* re-write dtr */
                                write_tmp_bs(pic_header, intra_dist[0], bs_buf, fp_bs_write);
                                /* other stream */
                                fwrite(bs_buf + 6, 1, bs_size - 6, fp_bs_write);
                            }
                        }
                    }
                    else if (cnkh->ctype == COM_CT_SLICE)
                    {
                        fwrite(bs_buf, 1, bs_size, fp_bs_write);
                    }

                    if (bs_read_pos == bs_end_pos)
                    {
                        break;
                    }
                }
                if (id) dec_delete(id);
                if (imgb_t) imgb_free(imgb_t);
                if (fp_bs) fclose(fp_bs);
                if (bs_buf) free(bs_buf);
            }
            if (fp_bs_write) fclose(fp_bs_write);
            return 0;
        }
#endif
    }
}
