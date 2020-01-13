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

#include "dec_def.h"
#include "com_tbl.h"


void read_byte(COM_BSR * bs, DEC_SBAC * sbac)
{
    if (sbac->bits_Needed >= 0)
    {
        u8 new_byte = (u8)com_bsr_read(bs, 8);
        sbac->prev_bytes = ((sbac->prev_bytes << 8) | new_byte);
        sbac->value += new_byte << sbac->bits_Needed;
        sbac->bits_Needed -= 8;
    }
}

static __inline int ace_get_shift(int v)
{
#ifdef _WIN32
    unsigned long index;
    _BitScanReverse(&index, v);
    return 8 - index;
#else
    return __builtin_clz(v) - 23;
#endif
}

static __inline int ace_get_log2(int v)
{
#ifdef _WIN32
    unsigned long index;
    _BitScanReverse(&index, v);
    return index;
#else
    return 31 - __builtin_clz(v);
#endif
}

u32 dec_sbac_decode_bin(COM_BSR * bs, DEC_SBAC * sbac, SBAC_CTX_MODEL * model)
{
#if TRACE_BIN
    SBAC_CTX_MODEL prev_model = *model;
#endif
    u16 prob_lps = ((*model) & PROB_MASK) >> 1;
    u16 cmps = (*model) & 1;
    u32 bin  = cmps;
    u32 rLPS = prob_lps >> LG_PMPS_SHIFTNO;
    u32 rMPS = sbac->range - rLPS;
    int s_flag = rMPS < QUAR_HALF_PROB;
    rMPS |= 0x100;
    u32 scaled_rMPS = rMPS << (8 - s_flag);
    assert(sbac->range >= rLPS);  //! this maybe triggered, so it can be removed
    if(sbac->value < scaled_rMPS) //! MPS
    {
        sbac->range = rMPS;
        if (s_flag)
        {
            (sbac)->value = sbac->value << 1;
            sbac->bits_Needed++;
            read_byte(bs, sbac);
        }
#if CHECK_ALL_CTX
        *model = tab_cycno_lgpmps_mps[*model & 0xFFFF] | (((*model >> 15) | 1) << 16);
#else
        *model = tab_cycno_lgpmps_mps[*model];
#endif
    }
    else //! LPS
    {
        bin = 1 - bin;
        rLPS = (sbac->range << s_flag) - rMPS;
        int shift = ace_get_shift(rLPS);
        sbac->range = rLPS << shift;
        sbac->value = (sbac->value - scaled_rMPS) << (s_flag + shift);
        sbac->bits_Needed += (s_flag + shift);
        while (sbac->bits_Needed >= 0)
        {
            read_byte(bs, sbac);
        }
#if CHECK_ALL_CTX
        *model = tab_cycno_lgpmps_mps[(*model & 0xFFFF) | (1 << 13)] | (((*model >> 15) | 1) << 16);
#else
        *model = tab_cycno_lgpmps_mps[(*model) | (1 << 13)];
#endif
    }
#if TRACE_BIN
    COM_TRACE_COUNTER;
    COM_TRACE_STR("model ");
    COM_TRACE_INT(prev_model);
    COM_TRACE_STR("-->");
    COM_TRACE_INT(*model);
    COM_TRACE_STR("MPS Range ");
    COM_TRACE_INT(sbac->range);
    COM_TRACE_STR("LPS Range ");
    COM_TRACE_INT(rLPS);
    COM_TRACE_STR("\n");
#endif
    return bin;
}

u32 dec_sbac_decode_binW(COM_BSR * bs, DEC_SBAC * sbac, SBAC_CTX_MODEL * model1, SBAC_CTX_MODEL * model2)
{
#if 0//TRACE_BIN
    SBAC_CTX_MODEL prev_model = *model;
#endif
    u16 prob_lps;
    u16 prob_lps1 = ((*model1) & PROB_MASK) >> 1;
    u16 prob_lps2 = ((*model2) & PROB_MASK) >> 1;
    u16 cmps;
    u16 cmps1 = (*model1) & 1;
    u16 cmps2 = (*model2) & 1;
    u32 rLPS;
    u32 rMPS;
    int s_flag;
    u32 bin;
    u32 scaled_rMPS;

    if (cmps1 == cmps2)
    {
        cmps = cmps1;
        prob_lps = (prob_lps1 + prob_lps2) >> 1;
    }
    else
    {
        if (prob_lps1 < prob_lps2)
        {
            cmps = cmps1;
            prob_lps = (256 << LG_PMPS_SHIFTNO) - 1 - ((prob_lps2 - prob_lps1) >> 1);
        }
        else
        {
            cmps = cmps2;
            prob_lps = (256 << LG_PMPS_SHIFTNO) - 1 - ((prob_lps1 - prob_lps2) >> 1);
        }
    }

    rLPS = prob_lps >> LG_PMPS_SHIFTNO;
    rMPS = sbac->range - rLPS;
    s_flag = rMPS < QUAR_HALF_PROB;
    rMPS |= 0x100;
    bin = cmps;
    scaled_rMPS = rMPS << (8 - s_flag);
    assert(sbac->range >= rLPS);  //! this maybe triggered, so it can be removed

    if (sbac->value < scaled_rMPS) //! MPS
    {
        sbac->range = rMPS;
        if (s_flag)
        {
            (sbac)->value = sbac->value << 1;
            sbac->bits_Needed++;
            read_byte(bs, sbac);
        }
    }
    else //! LPS
    {
        bin = 1 - bin;
        rLPS = (sbac->range << s_flag) - rMPS;
        int shift = ace_get_shift(rLPS);
        sbac->range = rLPS << shift;
        sbac->value = (sbac->value - scaled_rMPS) << (s_flag + shift);
        sbac->bits_Needed += (s_flag + shift);
        while (sbac->bits_Needed >= 0)
        {
            read_byte(bs, sbac);
        }
    }
#if CHECK_ALL_CTX
    if (bin != cmps1)
    {
        *model1 = tab_cycno_lgpmps_mps[(*model1 & 0xFFFF) | (1 << 13)] | (((*model1 >> 15) | 1) << 16);
    }
    else
    {
        *model1 = tab_cycno_lgpmps_mps[*model1 & 0xFFFF] | (((*model1 >> 15) | 1) << 16);
    }
    if (bin != cmps2)
    {
        *model2 = tab_cycno_lgpmps_mps[(*model2 & 0xFFFF) | (1 << 13)] | (((*model2 >> 15) | 1) << 16);
    }
    else
    {
        *model2 = tab_cycno_lgpmps_mps[*model2 & 0xFFFF] | (((*model2 >> 15) | 1) << 16);
    }
#else
    if (bin != cmps1)
    {
        *model1 = tab_cycno_lgpmps_mps[(*model1) | (1 << 13)];
    }
    else
    {
        *model1 = tab_cycno_lgpmps_mps[*model1];
    }
    if (bin != cmps2)
    {
        *model2 = tab_cycno_lgpmps_mps[(*model2) | (1 << 13)];
    }
    else
    {
        *model2 = tab_cycno_lgpmps_mps[*model2];
    }
#endif

#if 0//TRACE_BIN
    COM_TRACE_COUNTER;
    COM_TRACE_STR("model ");
    COM_TRACE_INT(prev_model);
    COM_TRACE_STR("-->");
    COM_TRACE_INT(*model);
    COM_TRACE_STR("MPS Range ");
    COM_TRACE_INT(sbac->range);
    COM_TRACE_STR("LPS Range ");
    COM_TRACE_INT(rLPS);
    COM_TRACE_STR("\n");
#endif
    return bin;
}


static u32 sbac_decode_bin_ep(COM_BSR * bs, DEC_SBAC * sbac)
{
    u32 bin;
    u32 scaled_range;
#if TRACE_BIN
    COM_TRACE_COUNTER;
    COM_TRACE_STR("range ");
    COM_TRACE_INT(sbac->range);
    COM_TRACE_STR("\n");
#endif
    scaled_range = sbac->range << DEC_RANGE_SHIFT;
    if(sbac->value < scaled_range)
    {
        bin = 0;
    }
    else
    {
        sbac->value -= scaled_range;
        bin = 1;
    }
    (sbac)->value = sbac->value << 1;
    sbac->bits_Needed++;
    read_byte(bs, sbac);
    return bin;
}

int dec_sbac_decode_bin_trm(COM_BSR * bs, DEC_SBAC * sbac)
{
    u8 s_flag = (sbac->range - 1 < QUAR_HALF_PROB);
    u32 rMPS = (sbac->range - 1) | 0x100;
    u32 scaled_rMPS = rMPS << (8 - s_flag);
    if (sbac->value < scaled_rMPS) //! MPS
    {
        sbac->range = rMPS;
        if (s_flag)
        {
            (sbac)->value = sbac->value << 1;
            sbac->bits_Needed++;
            read_byte(bs, sbac);
        }
        return 0;
    }
    else
    {
        u32 rLPS = s_flag ? ((sbac->range << 1) - rMPS) : 1;
        int shift = ace_get_shift(rLPS);
        sbac->range = rLPS << shift;
        sbac->value = (sbac->value - scaled_rMPS) << (s_flag + shift);
        sbac->bits_Needed += (s_flag + shift);

        while (sbac->bits_Needed > 0) // the last byte is written, so read the last byte before reading the termination padding byte
        {
            read_byte(bs, sbac);
        }
        while (!COM_BSR_IS_BYTE_ALIGN(bs))
        {
            assert(com_bsr_read1(bs) == 0);
        }
        return 1; /* end of slice */
    }
}

static u32 sbac_read_bins_ep_msb(COM_BSR * bs, DEC_SBAC * sbac, int num_bin)
{
    int bin = 0;
    u32 val = 0;

    for (bin = num_bin - 1; bin >= 0; bin--)
    {
        val = (val << 1) | sbac_decode_bin_ep(bs, sbac);
    }
    return val;
}

static u32 sbac_read_unary_sym_ep(COM_BSR * bs, DEC_SBAC * sbac)
{
    u32 val = 0;
    u32 bin;

    do
    {
        bin = !sbac_decode_bin_ep(bs, sbac);
        val += bin;
    }
    while (bin);

    return val;
}

static u32 sbac_read_unary_sym(COM_BSR * bs, DEC_SBAC * sbac, SBAC_CTX_MODEL * model, u32 num_ctx)
{
    u32 val = 0;
    u32 bin;

    do
    {
        bin = !dec_sbac_decode_bin(bs, sbac, model + min(val, num_ctx - 1));
        val += bin;
    }
    while (bin);

    return val;
}

static u32 sbac_read_truncate_unary_sym(COM_BSR * bs, DEC_SBAC * sbac, SBAC_CTX_MODEL * model, u32 num_ctx, u32 max_num)
{
    u32 val = 0;
    u32 bin;

    do
    {
        bin = !dec_sbac_decode_bin(bs, sbac, model + min(val, num_ctx - 1));
        val += bin;
    }
    while (val < max_num - 1 && bin);

    return val;
}

static u32 sbac_read_truncate_unary_sym_ep(COM_BSR * bs, DEC_SBAC * sbac, u32 max_num)
{
    u32 val = 0;
    u32 bin;

    do
    {
        bin = !sbac_decode_bin_ep(bs, sbac);
        val += bin;
    }
    while (val < max_num - 1 && bin);

    return val;
}

#if CHROMA_NOT_SPLIT
static int dec_eco_cbf_uv(COM_BSR * bs, DEC_SBAC * sbac, u8(*cbf)[N_C])
{
    COM_SBAC_CTX * sbac_ctx = &sbac->ctx;

    cbf[TBUV0][Y_C] = 0;
    cbf[TBUV0][U_C] = (u8)dec_sbac_decode_bin(bs, sbac, sbac_ctx->cbf + 1);
    cbf[TBUV0][V_C] = (u8)dec_sbac_decode_bin(bs, sbac, sbac_ctx->cbf + 2);

    COM_TRACE_STR("cbf U ");
    COM_TRACE_INT(cbf[TBUV0][U_C]);
    COM_TRACE_STR("cbf V ");
    COM_TRACE_INT(cbf[TBUV0][V_C]);
    COM_TRACE_STR("\n");
    return COM_OK;
}
#endif

#if IPCM
static int dec_eco_cbf(COM_BSR * bs, DEC_SBAC * sbac, int tb_avaliable, const int pb_part_size, int* tb_part_size, u8 pred_mode, s8 ipm[MAX_NUM_PB][2], u8 (*cbf)[N_C], u8 tree_status, DEC_CTX * ctx)
#else
static int dec_eco_cbf(COM_BSR * bs, DEC_SBAC * sbac, int tb_avaliable, const int pb_part_size, int* tb_part_size, u8 pred_mode, u8 (*cbf)[N_C], u8 tree_status, DEC_CTX * ctx)
#endif
{
    COM_MODE *mod_info_curr = &ctx->core->mod_info_curr;
    COM_SBAC_CTX * sbac_ctx = &sbac->ctx;
    int i, part_num;

    /* decode allcbf */
    if(pred_mode != MODE_INTRA)
    {
        int tb_split;
        if (pred_mode != MODE_DIR)
        {
#if CHROMA_NOT_SPLIT //ctp_zero_flag
            if (tree_status == TREE_LC)
            {
#endif
#if SEP_CONTEXT
                int ctp_zero_flag = 1;
                if (mod_info_curr->cu_width_log2 > 6 || mod_info_curr->cu_height_log2 > 6)
                {
                    ctp_zero_flag = dec_sbac_decode_bin(bs, sbac, sbac_ctx->ctp_zero_flag + 1);
                    assert(ctp_zero_flag == 1);
                }
                else
                    ctp_zero_flag = dec_sbac_decode_bin(bs, sbac, sbac_ctx->ctp_zero_flag);
#else
                int ctp_zero_flag = dec_sbac_decode_bin(bs, sbac, sbac_ctx->ctp_zero_flag);
#endif
                COM_TRACE_COUNTER;
                COM_TRACE_STR("ctp zero flag ");
                COM_TRACE_INT(ctp_zero_flag);
                COM_TRACE_STR("\n");

                if (ctp_zero_flag)
                {
                    for (i = 0; i < MAX_NUM_TB; i++)
                    {
                        cbf[i][Y_C] = cbf[i][U_C] = cbf[i][V_C] = 0;
                    }
                    *tb_part_size = SIZE_2Nx2N;
                    return COM_OK;
                }
#if CHROMA_NOT_SPLIT
            }
#endif
        }

        if (tb_avaliable)
        {
            tb_split = dec_sbac_decode_bin(bs, sbac, sbac_ctx->tb_split);
            *tb_part_size = tb_split? get_tb_part_size_by_pb(pb_part_size, pred_mode) : SIZE_2Nx2N;
        }
        else
        {
            tb_split = 0;
            *tb_part_size = SIZE_2Nx2N;
        }
        COM_TRACE_COUNTER;
        COM_TRACE_STR("tb_split ");
        COM_TRACE_INT(tb_split);
        COM_TRACE_STR("\n");

        if (tree_status == TREE_LC)
        {
            cbf[TBUV0][U_C] = (u8)dec_sbac_decode_bin(bs, sbac, sbac_ctx->cbf + 1);
            cbf[TBUV0][V_C] = (u8)dec_sbac_decode_bin(bs, sbac, sbac_ctx->cbf + 2);
            COM_TRACE_STR("cbf U ");
            COM_TRACE_INT(cbf[TBUV0][U_C]);
            COM_TRACE_STR("cbf V ");
            COM_TRACE_INT(cbf[TBUV0][V_C]);
            COM_TRACE_STR("\n");
        }
        else
        {
            assert(tree_status == TREE_L);
            cbf[TBUV0][U_C] = 0;
            cbf[TBUV0][V_C] = 0;
            COM_TRACE_STR("[cbf uv at tree L]\n");
        }

        COM_TRACE_STR("cbf Y ");
#if CHROMA_NOT_SPLIT
        if(cbf[TBUV0][U_C] + cbf[TBUV0][V_C] == 0 && *tb_part_size == SIZE_2Nx2N && tree_status == TREE_LC)
#else
        if(cbf[TBUV0][U_C] + cbf[TBUV0][V_C] == 0 && *tb_part_size == SIZE_2Nx2N)
#endif
        {
            cbf[TB0][Y_C] = 1;
            COM_TRACE_INT(1);
        }
        else
        {
            part_num = get_part_num(*tb_part_size);
            for (i = 0; i < part_num; i++)
            {
                cbf[i][Y_C] = (u8)dec_sbac_decode_bin(bs, sbac, sbac_ctx->cbf);
                COM_TRACE_INT(cbf[i][Y_C]);
            }
        }
        COM_TRACE_STR("\n");
    }
    else
    {
#if IPCM
        if (!(ipm[PB0][0] == IPD_IPCM))
        {
#endif
            *tb_part_size = get_tb_part_size_by_pb(pb_part_size, pred_mode);
            part_num = get_part_num(*tb_part_size);

            COM_TRACE_STR("cbf Y ");
            for (i = 0; i < part_num; i++)
            {
                cbf[i][Y_C] = (u8)dec_sbac_decode_bin(bs, sbac, sbac_ctx->cbf);
                COM_TRACE_INT(cbf[i][Y_C]);
            }
            COM_TRACE_STR("\n");
#if IPCM
        }
#endif
        if (tree_status == TREE_LC)
        {
#if IPCM
            if (!(ipm[PB0][0] == IPD_IPCM && ipm[PB0][1] == IPD_DM_C))
            {
#endif
                cbf[TBUV0][U_C] = (u8)dec_sbac_decode_bin(bs, sbac, sbac_ctx->cbf + 1);
                cbf[TBUV0][V_C] = (u8)dec_sbac_decode_bin(bs, sbac, sbac_ctx->cbf + 2);
                COM_TRACE_STR("cbf U ");
                COM_TRACE_INT(cbf[TBUV0][U_C]);
                COM_TRACE_STR("cbf V ");
                COM_TRACE_INT(cbf[TBUV0][V_C]);
                COM_TRACE_STR("\n");
#if IPCM
            }
#endif
        }
        else
        {
            assert(tree_status == TREE_L);
            cbf[TBUV0][U_C] = 0;
            cbf[TBUV0][V_C] = 0;
            COM_TRACE_STR("[cbf uv at tree L]\n");
        }
    }

    return COM_OK;
}

static u32 dec_eco_run(COM_BSR *bs, DEC_SBAC *sbac, SBAC_CTX_MODEL *model)
{
    u32 act_sym = sbac_read_truncate_unary_sym(bs, sbac, model, 2, 17);

    if (act_sym == 16)
    {
        int golomb_order = 0;
        golomb_order = sbac_read_unary_sym_ep(bs, sbac);
        act_sym += (1 << golomb_order) - 1;
        act_sym += sbac_read_bins_ep_msb(bs, sbac, golomb_order);
    }

    COM_TRACE_COUNTER;
    COM_TRACE_STR("run:");
    COM_TRACE_INT(act_sym);
    COM_TRACE_STR("\n");
    return act_sym;
}

static u32 dec_eco_level(COM_BSR *bs, DEC_SBAC *sbac, SBAC_CTX_MODEL *model)
{
    u32 act_sym = sbac_read_truncate_unary_sym(bs, sbac, model, 2, 9);

    if (act_sym == 8)
    {
        int golomb_order = 0;
        golomb_order = sbac_read_unary_sym_ep(bs, sbac);
        act_sym += (1 << golomb_order) - 1;
        act_sym += sbac_read_bins_ep_msb(bs, sbac, golomb_order);
    }

    COM_TRACE_COUNTER;
    COM_TRACE_STR("level:");
    COM_TRACE_INT(act_sym);
    COM_TRACE_STR("\n");
    return act_sym;
}

static int dec_eco_run_length_cc(COM_BSR *bs, DEC_SBAC *sbac, s16 *coef, int log2_w, int log2_h, int ch_type)
{
    COM_SBAC_CTX  *sbac_ctx;
    int            sign, level, prev_level, run, last_flag;
    int            t0, scan_pos_offset, num_coeff, i, coef_cnt = 0;
    const u16     *scanp;
    sbac_ctx = &sbac->ctx;
    scanp = com_scan_tbl[COEF_SCAN_ZIGZAG][log2_w - 1][log2_h - 1];
    num_coeff = 1 << (log2_w + log2_h);
    scan_pos_offset = 0;
    prev_level = 6;

    do
    {
        t0 = ((COM_MIN(prev_level - 1, 5)) * 2) + (ch_type == Y_C ? 0 : 12);
        /* Run parsing */
        run = dec_eco_run(bs, sbac, sbac_ctx->run + t0);

        for (i = scan_pos_offset; i < scan_pos_offset + run; i++)
        {
            coef[scanp[i]] = 0;
        }
        scan_pos_offset += run;
        /* Level parsing */
        level = dec_eco_level(bs, sbac, sbac_ctx->level + t0);
        level++;

        /* Sign parsing */
        sign = sbac_decode_bin_ep(bs, sbac);
        coef[scanp[scan_pos_offset]] = sign ? -(s16)level : (s16)level;
        coef_cnt++;
        if (scan_pos_offset >= num_coeff - 1)
        {
            break;
        }

        /* Last flag parsing */
        last_flag = dec_sbac_decode_binW(bs, sbac,
                                         &sbac_ctx->last1[COM_MIN(prev_level - 1, 5)        + (ch_type == Y_C ? 0 : NUM_SBAC_CTX_LAST1)],
                                         &sbac_ctx->last2[ace_get_log2(scan_pos_offset + 1) + (ch_type == Y_C ? 0 : NUM_SBAC_CTX_LAST2)]);
        prev_level = level;
        scan_pos_offset++;
    }
    while (!last_flag);

#if ENC_DEC_TRACE
    COM_TRACE_STR("coef");
    COM_TRACE_INT(ch_type);
    for (scan_pos_offset = 0; scan_pos_offset < num_coeff; scan_pos_offset++)
    {
        COM_TRACE_INT(coef[scan_pos_offset]);
    }
    COM_TRACE_STR("\n");
#endif
    return COM_OK;
}

static int dec_eco_xcoef(COM_BSR *bs, DEC_SBAC *sbac, s16 *coef, int log2_w, int log2_h, int ch_type)
{
    if( (log2_w > MAX_TR_LOG2) || (log2_h > MAX_TR_LOG2) )
    {
        printf( "transform size > 64x64" );
        assert( 0 );
    }

    dec_eco_run_length_cc(bs, sbac, coef, log2_w, log2_h, (ch_type == Y_C ? 0 : 1));
    return COM_OK;
}

static int decode_refidx(COM_BSR * bs, DEC_SBAC * sbac, int num_refp)
{
    int ref_num = 0;

    if(num_refp > 1)
    {
        ref_num = sbac_read_truncate_unary_sym(bs, sbac, sbac->ctx.refi, 3, num_refp);
    }

    COM_TRACE_COUNTER;
    COM_TRACE_STR("refi ");
    COM_TRACE_INT(ref_num);
    COM_TRACE_STR(" ctx ");
    COM_TRACE_INT( 0 );
    COM_TRACE_STR("\n");
    return ref_num;
}

#if MODE_CONS
u8 dec_eco_cons_pred_mode_child(COM_BSR * bs, DEC_SBAC * sbac)
{
    u8 flag = dec_sbac_decode_bin(bs, sbac, sbac->ctx.cons_mode);
    u8 cons_mode = flag ? ONLY_INTRA : ONLY_INTER;

    COM_TRACE_COUNTER;
    COM_TRACE_STR("cons mode ");
    COM_TRACE_INT(cons_mode);
    COM_TRACE_STR("\n");

    return cons_mode;
}
#endif

static u8 decode_skip_flag(COM_BSR * bs, DEC_SBAC * sbac, DEC_CTX * ctx)
{
#if NUM_SBAC_CTX_SKIP_FLAG > 1
    COM_MODE *mod_info_curr = &ctx->core->mod_info_curr;
    u32 *map_scu = ctx->map.map_scu;
    u8   avail[2] = { 0, 0 };
    int  scun[2];
    int  ctx_inc = 0;
    u8   bSkipMode;
    scun[0] = mod_info_curr->scup - ctx->info.pic_width_in_scu;
    scun[1] = mod_info_curr->scup - 1;

    if (mod_info_curr->y_scu > 0)
    {
        avail[0] = MCU_GET_CODED_FLAG(map_scu[scun[0]]); // up
    }
    if (mod_info_curr->x_scu > 0)
    {
        avail[1] = MCU_GET_CODED_FLAG(map_scu[scun[1]]); // left
    }
    if (avail[0])
    {
        ctx_inc += MCU_GET_SF(map_scu[scun[0]]);
    }
    if (avail[1])
    {
        ctx_inc += MCU_GET_SF(map_scu[scun[1]]);
    }

#if SEP_CONTEXT
    assert(NUM_SBAC_CTX_SKIP_FLAG == 4);
    if (mod_info_curr->cu_width_log2 + mod_info_curr->cu_height_log2 < 6)
    {
        ctx_inc = 3;
    }
#endif

    bSkipMode = (u8)dec_sbac_decode_bin(bs, sbac, sbac->ctx.skip_flag + ctx_inc);
#else
    u8 bSkipMode = (u8)dec_sbac_decode_bin(bs, sbac, sbac->ctx.skip_flag);
#endif
    COM_TRACE_COUNTER;
    COM_TRACE_STR("skip flag ");
    COM_TRACE_INT(bSkipMode);
#if NUM_SBAC_CTX_SKIP_FLAG > 1
    COM_TRACE_STR(" skip ctx ");
    COM_TRACE_INT(ctx_inc);
#endif
    COM_TRACE_STR("\n");
    return bSkipMode;
}

static u8 decode_umve_flag(COM_BSR * bs, DEC_SBAC * sbac, DEC_CTX * ctx)
{
    u8 bUmveSkipMode = dec_sbac_decode_bin(bs, sbac, &sbac->ctx.umve_flag);
    COM_TRACE_COUNTER;
    COM_TRACE_STR("umve flag ");
    COM_TRACE_INT(bUmveSkipMode);
    COM_TRACE_STR("\n");
    return bUmveSkipMode;
}

static u8 decode_umve_idx(COM_BSR * bs, DEC_SBAC * sbac, DEC_CTX * ctx)
{
    int base_idx = 0, step_idx = 0, dir_idx;
    int umve_idx = 0;
    int num_cand_minus1_base = UMVE_BASE_NUM - 1;
    int num_cand_minus1_step = UMVE_REFINE_STEP - 1;
    int idx0;

    if (num_cand_minus1_base > 0)
    {
        idx0 = dec_sbac_decode_bin(bs, sbac, sbac->ctx.umve_base_idx);
        if (!idx0)
        {
            base_idx++;
            for (; base_idx < num_cand_minus1_base; base_idx++)
            {
                idx0 = sbac_decode_bin_ep(bs, sbac);
                if (idx0)
                {
                    break;
                }
            }
        }
    }
    if (num_cand_minus1_step > 0)
    {
        idx0 = dec_sbac_decode_bin(bs, sbac, sbac->ctx.umve_step_idx);
        if (!idx0)
        {
            step_idx++;
            for (; step_idx < num_cand_minus1_step; step_idx++)
            {
                idx0 = sbac_decode_bin_ep(bs, sbac);
                if (idx0)
                {
                    break;
                }
            }
        }
    }

    dir_idx = 0;
    idx0 = dec_sbac_decode_bin(bs, sbac, &sbac->ctx.umve_dir_idx[0]);
    if (idx0)
    {
        dir_idx += 2;
        idx0 = dec_sbac_decode_bin(bs, sbac, &sbac->ctx.umve_dir_idx[1]);
        if (idx0)
        {
            dir_idx += 1;
        }
        else
        {
            dir_idx += 0;
        }
    }
    else
    {
        dir_idx += 0;
        idx0 = dec_sbac_decode_bin(bs, sbac, &sbac->ctx.umve_dir_idx[1]);
        if (idx0)
        {
            dir_idx += 1;
        }
        else
        {
            dir_idx += 0;
        }
    }
    umve_idx += (base_idx * UMVE_MAX_REFINE_NUM + step_idx * 4 + dir_idx);
    COM_TRACE_COUNTER;
    COM_TRACE_STR("umve idx ");
    COM_TRACE_INT(umve_idx);
    COM_TRACE_STR("\n");
    return umve_idx;
}

static u8 decode_split_flag(DEC_CTX *c, int cu_width, int cu_height, int x, int y, COM_BSR * bs, DEC_SBAC * sbac/*, DEC_CTX * ctx*/)
{
    //split flag
    int ctx = 0;
    int x_scu = x >> MIN_CU_LOG2;
    int y_scu = y >> MIN_CU_LOG2;
    int pic_width_in_scu = c->info.pic_width >> MIN_CU_LOG2;
    u8  avail[2] = { 0, 0 };
    int scun[2];
    int scup = x_scu + y_scu * pic_width_in_scu;
    u8  bSplitFlag;

    scun[0] = scup - pic_width_in_scu;
    scun[1] = scup - 1;
    if (y_scu > 0)
    {
        avail[0] = MCU_GET_CODED_FLAG(c->map.map_scu[scun[0]]);  //up
    }
    if (x_scu > 0)
    {
        avail[1] = MCU_GET_CODED_FLAG(c->map.map_scu[scun[1]]); //left
    }

    if (avail[0])
    {
        ctx += (1 << MCU_GET_LOGW(c->map.map_cu_mode[scun[0]])) < cu_width;
    }
    if (avail[1])
    {
        ctx += (1 << MCU_GET_LOGH(c->map.map_cu_mode[scun[1]])) < cu_height;
    }
#if SEP_CONTEXT
    if (c->info.pic_header.slice_type == SLICE_I && cu_width == 128 && cu_height == 128)
    {
        ctx = 3;
    }
#endif

    bSplitFlag = (u8)dec_sbac_decode_bin(bs, sbac, sbac->ctx.split_flag + ctx);
    COM_TRACE_COUNTER;
    COM_TRACE_STR("split flag ");
    COM_TRACE_INT(bSplitFlag);
    COM_TRACE_STR("\n");
    return bSplitFlag;
}

static u8 decode_affine_flag(COM_BSR * bs, DEC_SBAC * sbac, DEC_CTX * ctx)
{
    COM_MODE *mod_info_curr = &ctx->core->mod_info_curr;
    int cu_width = 1 << mod_info_curr->cu_width_log2;
    int cu_height = 1 << mod_info_curr->cu_height_log2;
    u8  affine_flag = 0;
    if (cu_width >= AFF_SIZE && cu_height >= AFF_SIZE && ctx->info.sqh.affine_enable_flag)
    {
        affine_flag = (u8)dec_sbac_decode_bin((bs), (sbac), (sbac)->ctx.affine_flag);
    }

    COM_TRACE_COUNTER;
    COM_TRACE_STR("affine flag ");
    COM_TRACE_INT(affine_flag);
    COM_TRACE_STR("\n");
    return affine_flag;
}

static u8 decode_affine_mrg_idx( COM_BSR * bs, DEC_SBAC * sbac, DEC_CTX * ctx )
{
    u8 mrg_idx = (u8)sbac_read_truncate_unary_sym(bs, sbac, sbac->ctx.affine_mrg_idx, NUM_SBAC_CTX_AFFINE_MRG, AFF_MAX_NUM_MRG);

    COM_TRACE_COUNTER;
    COM_TRACE_STR("affine merge index ");
    COM_TRACE_INT(mrg_idx);
    COM_TRACE_STR("\n");
    return mrg_idx;
}

#if SMVD
static int decode_smvd_flag( COM_BSR * bs, DEC_SBAC * sbac, DEC_CTX * ctx )
{
    u32 smvd_flag = 0;

    smvd_flag = dec_sbac_decode_bin( bs, sbac, sbac->ctx.smvd_flag );

    COM_TRACE_COUNTER;
    COM_TRACE_STR("smvd flag ");
    COM_TRACE_INT(smvd_flag);
    COM_TRACE_STR("\n");

    return smvd_flag;
}
#endif

#if DT_SYNTAX
static int decode_part_size(COM_BSR * bs, DEC_SBAC * sbac, DEC_CTX * ctx, int cu_w, int cu_h, int pred_mode)
{
    int part_size = SIZE_2Nx2N;
    int allowDT = com_dt_allow(cu_w, cu_h, pred_mode, ctx->info.sqh.max_dt_size);
    int sym, dir, eq;

    if (!ctx->info.sqh.dt_intra_enable_flag && pred_mode == MODE_INTRA)
        return SIZE_2Nx2N;
    if (!allowDT)
        return SIZE_2Nx2N;

    sym = dec_sbac_decode_bin(bs, sbac, sbac->ctx.part_size + 0);
    if (sym == 1)
    {
        int hori_allow = (allowDT >> 0) & 0x01;
        int vert_allow = (allowDT >> 1) & 0x01;
        if (hori_allow && vert_allow)
        {
            dir = dec_sbac_decode_bin(bs, sbac, sbac->ctx.part_size + 1);
        }
        else
        {
            dir = hori_allow ? 1 : 0;
        }

        if (dir)
        {
            //hori
            eq = dec_sbac_decode_bin(bs, sbac, sbac->ctx.part_size + 2);
            if (eq)
            {
                part_size = SIZE_2NxhN;
            }
            else
            {
                sym = dec_sbac_decode_bin(bs, sbac, sbac->ctx.part_size + 3);
                part_size = sym ? SIZE_2NxnD : SIZE_2NxnU;
            }
        }
        else
        {
            //vert
            eq = dec_sbac_decode_bin(bs, sbac, sbac->ctx.part_size + 4);
            if (eq)
            {
                part_size = SIZE_hNx2N;
            }
            else
            {
                sym = dec_sbac_decode_bin(bs, sbac, sbac->ctx.part_size + 5);
                part_size = sym ? SIZE_nRx2N : SIZE_nLx2N;
            }
        }
    }
    else
    {
        part_size = SIZE_2Nx2N;
    }

    COM_TRACE_COUNTER;
    COM_TRACE_STR("part_size ");
    COM_TRACE_INT(part_size);
    COM_TRACE_STR("\n");
    return part_size;
}
#endif
static u8 decode_pred_mode(COM_BSR * bs, DEC_SBAC * sbac, DEC_CTX * ctx)
{
#if NUM_PRED_MODE_CTX > 1
    COM_MODE *mod_info_curr = &ctx->core->mod_info_curr;
    u32 *map_scu = ctx->map.map_scu;
    u8  avail[2] = { 0, 0 };
    int scun[2];
    int ctx_inc = 0;
    u8  pred_mode;
    scun[0] = mod_info_curr->scup - ctx->info.pic_width_in_scu;
    scun[1] = mod_info_curr->scup - 1;

    if (mod_info_curr->y_scu > 0)
    {
        avail[0] = MCU_GET_CODED_FLAG(map_scu[scun[0]]); // up
    }
    if (mod_info_curr->x_scu > 0)
    {
        avail[1] = MCU_GET_CODED_FLAG(map_scu[scun[1]]); // left
    }
    if (avail[0])
    {
        ctx_inc += MCU_GET_INTRA_FLAG(map_scu[scun[0]]);
    }
    if (avail[1])
    {
        ctx_inc += MCU_GET_INTRA_FLAG(map_scu[scun[1]]);
    }

    if (ctx_inc == 0)
    {
        int sample = (1 << mod_info_curr->cu_width_log2) * (1 << mod_info_curr->cu_height_log2);
        ctx_inc = (sample > 256) ? 0 : (sample > 64 ? 3 : 4);
    }

#if SEP_CONTEXT
    if (mod_info_curr->cu_width_log2 > 6 || mod_info_curr->cu_height_log2 > 6)
    {
        ctx_inc = 5;
    }
#endif
    pred_mode = dec_sbac_decode_bin(bs, sbac, sbac->ctx.pred_mode + ctx_inc) ? MODE_INTRA : MODE_INTER;
#else
    u8 pred_mode = dec_sbac_decode_bin(bs, sbac, sbac->ctx.pred_mode) ? MODE_INTRA : MODE_INTER;
#endif
    COM_TRACE_COUNTER;
    COM_TRACE_STR("pred mode ");
    COM_TRACE_INT(pred_mode);
#if NUM_PRED_MODE_CTX > 1
    COM_TRACE_STR(" pred mode ctx ");
    COM_TRACE_INT(ctx_inc);
#endif
    COM_TRACE_STR("\n");
    return pred_mode;
}

static int decode_mvr_idx(COM_BSR * bs, DEC_SBAC * sbac, BOOL is_affine)
{
#if !BD_AFFINE_AMVR
    if (is_affine)
        return 0;
#endif

#if BD_AFFINE_AMVR
    int mvr_idx;
    if (is_affine)
    {
        mvr_idx = sbac_read_truncate_unary_sym(bs, sbac, sbac->ctx.affine_mvr_idx, NUM_AFFINE_MVR_IDX_CTX, MAX_NUM_AFFINE_MVR);
    }
    else
    {
        mvr_idx = sbac_read_truncate_unary_sym(bs, sbac, sbac->ctx.mvr_idx, NUM_MVR_IDX_CTX, MAX_NUM_MVR);
    }
#else
    int mvr_idx = sbac_read_truncate_unary_sym(bs, sbac, sbac->ctx.mvr_idx, NUM_MVR_IDX_CTX, MAX_NUM_MVR);
#endif
    COM_TRACE_COUNTER;
    COM_TRACE_STR("mvr idx ");
    COM_TRACE_INT(mvr_idx);
    COM_TRACE_STR("\n");
    return mvr_idx;
}

#if EXT_AMVR_HMVP
static int decode_extend_amvr_flag(COM_BSR * bs, DEC_SBAC * sbac)
{
    u32 mvp_from_hmvp_flag = dec_sbac_decode_bin(bs, sbac, sbac->ctx.mvp_from_hmvp_flag);
    COM_TRACE_COUNTER;
    COM_TRACE_STR("extended amvr flag ");
    COM_TRACE_INT(mvp_from_hmvp_flag);
    COM_TRACE_STR("\n");
    return mvp_from_hmvp_flag;
}
#endif

static int decode_ipf_flag(COM_BSR * bs, DEC_SBAC * sbac)
{
    int ipf_flag = dec_sbac_decode_bin(bs, sbac, sbac->ctx.ipf_flag);
    COM_TRACE_COUNTER;
    COM_TRACE_STR("ipf flag ");
    COM_TRACE_INT(ipf_flag);
    COM_TRACE_STR("\n");
    return ipf_flag;
}

static u32 dec_eco_abs_mvd(COM_BSR *bs, DEC_SBAC *sbac, SBAC_CTX_MODEL *model)
{
    u32 act_sym;

    if (!dec_sbac_decode_bin(bs, sbac, model))
    {
        act_sym = 0;
    }
    else if (!dec_sbac_decode_bin(bs, sbac, model + 1))
    {
        act_sym = 1;
    }
    else if (!dec_sbac_decode_bin(bs, sbac, model + 2))
    {
        act_sym = 2;
    }
    else
    {
        int add_one = sbac_decode_bin_ep(bs, sbac);
        int golomb_order = 0;
        act_sym = 0;
        golomb_order = sbac_read_unary_sym_ep(bs, sbac);
        act_sym += (1 << golomb_order) - 1;
        act_sym += sbac_read_bins_ep_msb(bs, sbac, golomb_order);
        act_sym = 3 + act_sym * 2 + add_one;
    }

    return act_sym;
}

static int decode_mvd(COM_BSR * bs, DEC_SBAC * sbac, s16 mvd[MV_D])
{
    u32 sign;
    s16 t16;
    /* MV_X */
    t16 = (s16)dec_eco_abs_mvd(bs, sbac, sbac->ctx.mvd[0]);
    if (t16 == 0)
    {
        mvd[MV_X] = 0;
    }
    else
    {
        /* sign */
        sign = sbac_decode_bin_ep(bs, sbac);
        if(sign) mvd[MV_X] = -t16;
        else mvd[MV_X] = t16;
    }
    /* MV_Y */
    t16 = (s16)dec_eco_abs_mvd(bs, sbac, sbac->ctx.mvd[1]);
    if (t16 == 0)
    {
        mvd[MV_Y] = 0;
    }
    else
    {
        /* sign */
        sign = sbac_decode_bin_ep(bs, sbac);
        if(sign) mvd[MV_Y] = -t16;
        else mvd[MV_Y] = t16;
    }
    COM_TRACE_COUNTER;
    COM_TRACE_STR("mvd x ");
    COM_TRACE_INT(mvd[MV_X]);
    COM_TRACE_STR("mvd y ");
    COM_TRACE_INT(mvd[MV_Y]);
    COM_TRACE_STR("\n");
    return COM_OK;
}


int decode_coef(DEC_CTX * ctx, DEC_CORE * core)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    u8        cbf[MAX_NUM_TB][N_C];
    DEC_SBAC *sbac;
    COM_BSR   *bs;
#if IPCM
    int i, j, ret = 0;
#else
    int i, j, ret;
#endif
    int tb_avaliable = is_tb_avaliable(ctx->info, mod_info_curr->cu_width_log2, mod_info_curr->cu_height_log2, mod_info_curr->pb_part, mod_info_curr->cu_mode);
    int start_comp, num_comp;
    start_comp = (ctx->tree_status == TREE_L || ctx->tree_status == TREE_LC) ? Y_C : U_C;
    num_comp = ctx->tree_status == TREE_LC ? 3 : (ctx->tree_status == TREE_L ? 1 : 2);

    bs = &ctx->bs;
    sbac = GET_SBAC_DEC(bs);

    if (ctx->tree_status != TREE_C)
    {
#if IPCM
        ret = dec_eco_cbf(bs, sbac, tb_avaliable, mod_info_curr->pb_part, &mod_info_curr->tb_part, (u8)mod_info_curr->cu_mode, mod_info_curr->ipm, cbf, ctx->tree_status, ctx);
#else
        ret = dec_eco_cbf(bs, sbac, tb_avaliable, mod_info_curr->pb_part, &mod_info_curr->tb_part, (u8)mod_info_curr->cu_mode, cbf, ctx->tree_status, ctx);
#endif
    }
    else
    {
#if IPCM
        if (!(mod_info_curr->cu_mode == MODE_INTRA && mod_info_curr->ipm[PB0][0] == IPD_IPCM && mod_info_curr->ipm[PB0][1] == IPD_DM_C))
        {
#endif
            ret = dec_eco_cbf_uv(bs, sbac, cbf);
#if IPCM
        }
#endif
    }
    com_assert_rv(ret == COM_OK, ret);

    for (i = start_comp; i < start_comp + num_comp; i++)
    {
        int part_num;
        int log2_tb_w, log2_tb_h, tb_size;
        int plane_width_log2  = mod_info_curr->cu_width_log2  - (i != Y_C);
        int plane_height_log2 = mod_info_curr->cu_height_log2 - (i != Y_C);

#if IPCM
        if (mod_info_curr->cu_mode == MODE_INTRA && ((i == Y_C && mod_info_curr->ipm[PB0][0] == IPD_IPCM) || (i > Y_C && mod_info_curr->ipm[PB0][0] == IPD_IPCM && mod_info_curr->ipm[PB0][1] == IPD_DM_C)))
        {
            if (i == start_comp)
            {
                /* aec_ipcm_stuffing_bit and byte alignment (bit always = 1) inside the function */
                assert(dec_sbac_decode_bin_trm(bs, sbac) == 1);
            }
            int tb_w = plane_width_log2 > 5 ? 32 : (1 << plane_width_log2);
            int tb_h = plane_height_log2 > 5 ? 32 : (1 << plane_height_log2);
            int num_tb_w = plane_width_log2 > 5 ? 1 << (plane_width_log2 - 5) : 1;
            int num_tb_h = plane_height_log2 > 5 ? 1 << (plane_height_log2 - 5) : 1;
            for (int h = 0; h < num_tb_h; h++)
            {
                for (int w = 0; w < num_tb_w; w++)
                {
                    s16* coef_tb = mod_info_curr->coef[i] + (1 << plane_width_log2) * h * tb_h + w * tb_w;
                    decode_ipcm(bs, sbac, tb_w, tb_h, 1 << plane_width_log2, coef_tb, ctx->info.bit_depth_input, i);
                }
            }
            if ((i == Y_C && (ctx->tree_status == TREE_L || (ctx->tree_status == TREE_LC && mod_info_curr->ipm[0][1] != IPD_DM_C))) || i == V_C)
            {
                assert(COM_BSR_IS_BYTE_ALIGN(bs));
                dec_sbac_init(bs);
            }
        }
        else
        {
#endif
            part_num = get_part_num(i == Y_C ? mod_info_curr->tb_part : SIZE_2Nx2N);
            get_tb_width_height_log2(plane_width_log2, plane_height_log2, i == Y_C ? mod_info_curr->tb_part : SIZE_2Nx2N, &log2_tb_w, &log2_tb_h);
            tb_size = 1 << (log2_tb_w + log2_tb_h);

            for (j = 0; j < part_num; j++)
            {
                if (cbf[j][i])
                {
                    ret = dec_eco_xcoef(bs, sbac, mod_info_curr->coef[i] + j * tb_size, log2_tb_w, log2_tb_h, i);
                    com_assert_rv(ret == COM_OK, ret);
                    mod_info_curr->num_nz[j][i] = 1;
                }
                else
                {
                    mod_info_curr->num_nz[j][i] = 0;
                }
            }
#if IPCM
        }
#endif
    }

    return COM_OK;
}

void dec_sbac_init(COM_BSR * bs)
{
    DEC_SBAC *sbac = GET_SBAC_DEC(bs);
    sbac->range = 0x1FF;
    sbac->bits_Needed = -8;
    sbac->value = com_bsr_read(bs, READ_BITS_INIT) << 1;
    sbac->bits_Needed++;
}

int decode_intra_dir(COM_BSR * bs, DEC_SBAC * sbac, u8 mpm[2])
{
    u32 t0, t1, t2, t3, t4, t5, t6;
    int ipm = 0;
    t0 = dec_sbac_decode_bin(bs, sbac, sbac->ctx.intra_dir);
    if (t0 == 1)
    {
        t1 = dec_sbac_decode_bin(bs, sbac, sbac->ctx.intra_dir + 6);
        ipm = t1 - 2;
    }
    else
    {
        t2 = dec_sbac_decode_bin(bs, sbac, sbac->ctx.intra_dir + 1);
        t3 = dec_sbac_decode_bin(bs, sbac, sbac->ctx.intra_dir + 2);
        t4 = dec_sbac_decode_bin(bs, sbac, sbac->ctx.intra_dir + 3);
        t5 = dec_sbac_decode_bin(bs, sbac, sbac->ctx.intra_dir + 4);
        t6 = dec_sbac_decode_bin(bs, sbac, sbac->ctx.intra_dir + 5);
        ipm = (t2 << 4) + (t3 << 3) + (t4 << 2) + (t5 << 1) + t6;
    }
    ipm = (ipm < 0) ? mpm[ipm + 2] : (ipm + (ipm >= mpm[0]) + ((ipm + 1) >= mpm[1]));
    COM_TRACE_COUNTER;
    COM_TRACE_STR("ipm Y ");
    COM_TRACE_INT(ipm);
    COM_TRACE_STR(" mpm_0 ");
    COM_TRACE_INT(mpm[0]);
    COM_TRACE_STR(" mpm_1 ");
    COM_TRACE_INT(mpm[1]);
    COM_TRACE_STR("\n");
    return ipm;
}

#if TSCPM
int decode_intra_dir_c(COM_BSR * bs, DEC_SBAC * sbac, u8 ipm_l, u8 tscpm_enable_flag)
#else
int decode_intra_dir_c(COM_BSR * bs, DEC_SBAC * sbac, u8 ipm_l)
#endif
{
    u32 t0;
    int ipm = 0;
    u8 chk_bypass;

    COM_IPRED_CONV_L2C_CHK(ipm_l, chk_bypass);
    t0 = dec_sbac_decode_bin(bs, sbac, sbac->ctx.intra_dir + 7);
    if (t0 == 0)
    {
#if TSCPM
        if (tscpm_enable_flag)
        {
            int tscpm_ctx = 0;
            if (dec_sbac_decode_bin(bs, sbac, sbac->ctx.intra_dir + 9))
            {
                ipm = IPD_TSCPM_C;
                COM_TRACE_COUNTER;
                COM_TRACE_STR("ipm UV ");
                COM_TRACE_INT(ipm);
                COM_TRACE_STR("\n");
                return ipm;
            }
        }
#endif

        ipm = 1 + sbac_read_truncate_unary_sym(bs, sbac, sbac->ctx.intra_dir + 8, 1, IPD_CHROMA_CNT - 1);

        if (chk_bypass)
        {
            assert(ipm < IPD_BI_C); // if isRedundant, ipm, 1,2,3
        }

        if (chk_bypass &&  ipm >= ipm_l)
        {
            ipm++;
        }
    }
    COM_TRACE_COUNTER;
    COM_TRACE_STR("ipm UV ");
    COM_TRACE_INT(ipm);
    COM_TRACE_STR("\n");
    return ipm;
}

#if IPCM
void decode_ipcm(COM_BSR * bs, DEC_SBAC * sbac, int tb_width, int tb_height, int cu_width, s16 pcm[MAX_CU_DIM], int bit_depth, int ch_type)
{
    int i, j;
#if ENC_DEC_TRACE
    COM_TRACE_STR("pcm_");
    COM_TRACE_INT(ch_type);
    COM_TRACE_STR(":\n");
#endif
    for (i = 0; i < tb_height; i++)
    {
        for (j = 0; j < tb_width; j++)
        {
            pcm[i * cu_width + j] = com_bsr_read(bs, bit_depth);
#if ENC_DEC_TRACE
            COM_TRACE_INT(pcm[i * cu_width + j]);
#endif
        }
#if ENC_DEC_TRACE
        COM_TRACE_STR("\n");
#endif
    }
}
#endif

int decode_inter_dir(COM_BSR * bs, DEC_SBAC * sbac, int part_size, DEC_CTX * ctx)
{
    assert(ctx->info.pic_header.slice_type == SLICE_B);

    int pred_dir;
#if SEP_CONTEXT
    COM_MODE *mod_info_curr = &ctx->core->mod_info_curr;
    u32 symbol = 0;
    if (mod_info_curr->cu_width_log2 + mod_info_curr->cu_height_log2 < 6)
    {
        symbol = dec_sbac_decode_bin(bs, sbac, sbac->ctx.inter_dir + 2);
        assert(symbol == 0);
    }
    else
    {
        symbol = dec_sbac_decode_bin(bs, sbac, sbac->ctx.inter_dir);
    }
#else
    u32 symbol = dec_sbac_decode_bin(bs, sbac, sbac->ctx.inter_dir);
#endif
    if (symbol)
    {
        pred_dir = PRED_BI;
    }
    else
    {
        symbol = dec_sbac_decode_bin(bs, sbac, sbac->ctx.inter_dir + 1);
        pred_dir = symbol ? PRED_L1 : PRED_L0;
    }
    COM_TRACE_COUNTER;
    COM_TRACE_STR("pred dir ");
    COM_TRACE_INT(pred_dir);
    COM_TRACE_STR("\n");
    return pred_dir;
}

static u8 decode_skip_idx(COM_BSR * bs, DEC_SBAC * sbac, int num_hmvp_cands, DEC_CTX * ctx)
{
    int ctx_idx = 0;
    u32 skip_idx = 0;
    const u32 max_skip_num = (ctx->info.pic_header.slice_type == SLICE_P ? 2 : TRADITIONAL_SKIP_NUM) + num_hmvp_cands;

    int symbol = dec_sbac_decode_bin(bs, sbac, &sbac->ctx.skip_idx_ctx[ctx_idx]);
    while (!symbol && skip_idx < max_skip_num - 2)
    {
        ctx_idx++;
        skip_idx++;
        ctx_idx = min(ctx_idx, NUM_SBAC_CTX_SKIP_IDX - 1);
        symbol = dec_sbac_decode_bin(bs, sbac, &sbac->ctx.skip_idx_ctx[ctx_idx]);
    }
    if (!symbol)
    {
        skip_idx++;
        assert(skip_idx == max_skip_num - 1);
    }

    // for P slice, change 1, 2, ..., 11 to 3, 4, ..., 13
    if (ctx->info.pic_header.slice_type == SLICE_P && skip_idx > 0)
    {
        skip_idx += 2;
    }

    COM_TRACE_COUNTER;
    COM_TRACE_STR("skip idx ");
    COM_TRACE_INT(skip_idx);
    COM_TRACE_STR(" HmvpSpsNum ");
    COM_TRACE_INT(num_hmvp_cands);
    COM_TRACE_STR("\n");
    return (u8)skip_idx;
}

static u8 decode_direct_flag(COM_BSR * bs, DEC_SBAC * sbac, DEC_CTX * ctx)
{
#if SEP_CONTEXT
    COM_MODE *mod_info_curr = &ctx->core->mod_info_curr;
    int ctx_inc = 0;
    u32 direct_flag;
    if ((mod_info_curr->cu_width_log2 + mod_info_curr->cu_height_log2 < 6) || mod_info_curr->cu_width_log2 > 6 || mod_info_curr->cu_height_log2 > 6)
    {
        ctx_inc = 1;
    }

    direct_flag = dec_sbac_decode_bin(bs, sbac, sbac->ctx.direct_flag + ctx_inc);
#else
    u32 direct_flag = dec_sbac_decode_bin(bs, sbac, sbac->ctx.direct_flag);
#endif
    COM_TRACE_COUNTER;
    COM_TRACE_STR("direct flag ");
    COM_TRACE_INT(direct_flag);
#if SEP_CONTEXT
    COM_TRACE_STR(" direct flag ctx ");
    COM_TRACE_INT(ctx_inc);
#endif
    COM_TRACE_STR("\n");
    return (u8)direct_flag;
}

s8 dec_eco_split_mode(DEC_CTX * c, COM_BSR *bs, DEC_SBAC *sbac, int cu_width, int cu_height
                      , const int parent_split, int qt_depth, int bet_depth, int x, int y)
{
    s8 split_mode = NO_SPLIT;
    int ctx = 0;
    u32 t0;
    int split_allow[SPLIT_CHECK_NUM];
    int i, non_QT_split_mode_num;
    int boundary = 0, boundary_b = 0, boundary_r = 0;

    if(cu_width == MIN_CU_SIZE && cu_height == MIN_CU_SIZE)
    {
        split_mode = NO_SPLIT;
        return split_mode;
    }

    boundary = !(x + cu_width <= c->info.pic_width && y + cu_height <= c->info.pic_height);
    boundary_b = boundary && (y + cu_height > c->info.pic_height) && !(x + cu_width  > c->info.pic_width);
    boundary_r = boundary && (x + cu_width  > c->info.pic_width)  && !(y + cu_height > c->info.pic_height);

    com_check_split_mode(&c->info.sqh, split_allow, CONV_LOG2(cu_width), CONV_LOG2(cu_height), boundary, boundary_b, boundary_r, c->info.log2_max_cuwh, c->info.pic_header.temporal_id
                         , parent_split, qt_depth, bet_depth, c->info.pic_header.slice_type);
    non_QT_split_mode_num = 0;
    for(i = 1; i < SPLIT_QUAD; i++)
    {
        non_QT_split_mode_num += split_allow[i];
    }

    if (split_allow[SPLIT_QUAD] && !(non_QT_split_mode_num || split_allow[NO_SPLIT])) //only QT is allowed
    {
        split_mode = SPLIT_QUAD;
        return split_mode;
    }
    else if (split_allow[SPLIT_QUAD])
    {
        u8 bSplitFlag = decode_split_flag(c, cu_width, cu_height, x, y, bs, sbac);
        if (bSplitFlag)
        {
            split_mode = SPLIT_QUAD;
            return split_mode;
        }
    }

    if (non_QT_split_mode_num)
    {
        int cu_width_log2 = CONV_LOG2(cu_width);
        int cu_height_log2 = CONV_LOG2(cu_height);
        //split flag
        int x_scu = x >> MIN_CU_LOG2;
        int y_scu = y >> MIN_CU_LOG2;
        int pic_width_in_scu = c->info.pic_width >> MIN_CU_LOG2;
        u8  avail[2] = {0, 0};
        int scun[2];
        int scup = x_scu + y_scu * pic_width_in_scu;

        scun[0]  = scup - pic_width_in_scu;
        scun[1]  = scup - 1;
        if (y_scu > 0)
        {
            avail[0] = MCU_GET_CODED_FLAG(c->map.map_scu[scun[0]]);  //up
        }
        if (x_scu > 0)
        {
            avail[1] = MCU_GET_CODED_FLAG(c->map.map_scu[scun[1]]); //left
        }
        if (avail[0])
        {
            ctx += (1 << MCU_GET_LOGW(c->map.map_cu_mode[scun[0]])) < cu_width;
        }
        if (avail[1])
        {
            ctx += (1 << MCU_GET_LOGH(c->map.map_cu_mode[scun[1]])) < cu_height;
        }
#if NUM_SBAC_CTX_BT_SPLIT_FLAG == 9
        int sample = cu_width * cu_height;
        int ctx_set = (sample > 1024) ? 0 : (sample > 256 ? 1 : 2);
        int ctx_save = ctx;
        ctx += ctx_set * 3;
#endif

        if (split_allow[NO_SPLIT])
        {
            t0 = dec_sbac_decode_bin(bs, sbac, sbac->ctx.bt_split_flag + ctx);
        }
        else
        {
            t0 = 1;
        }
#if NUM_SBAC_CTX_BT_SPLIT_FLAG == 9
        ctx = ctx_save;
#endif

        if (!t0)
        {
            split_mode = NO_SPLIT;
        }
        else
        {
            int HBT = split_allow[SPLIT_BI_HOR];
            int VBT = split_allow[SPLIT_BI_VER];
            int EnableBT = HBT || VBT;
#if EQT
            int HEQT = split_allow[SPLIT_EQT_HOR];
            int VEQT = split_allow[SPLIT_EQT_VER];
            int EnableEQT = HEQT || VEQT;
#endif
            u8 ctx_dir = cu_width_log2 == cu_height_log2 ? 0 : (cu_width_log2 > cu_height_log2 ? 1 : 2);
            u8 split_dir = 0, split_typ = 0;

#if EQT
            if (EnableEQT && EnableBT)
            {
                split_typ = (u8)dec_sbac_decode_bin(bs, sbac, sbac->ctx.split_mode + ctx);
            }
            else if (EnableEQT)
            {
                split_typ = 1;
            }
#endif
            if (split_typ == 0)
            {
                if (HBT && VBT)
                {
#if SEP_CONTEXT
                    if (cu_width == 64 && cu_height == 128)
                    {
                        ctx_dir = 3;
                    }
                    if (cu_width == 128 && cu_height == 64)
                    {
                        ctx_dir = 4;
                    }
#endif
                    split_dir = (u8)dec_sbac_decode_bin(bs, sbac, sbac->ctx.split_dir + ctx_dir);
                }
                else
                {
                    split_dir = !HBT;
                }
            }
#if EQT
            if (split_typ == 1)
            {
                if (HEQT && VEQT)
                {
                    split_dir = (u8)dec_sbac_decode_bin(bs, sbac, sbac->ctx.split_dir + ctx_dir);
                }
                else
                {
                    split_dir = !HEQT;
                }
            }
#endif

#if EQT
            if (split_typ == 0) //BT
            {
#endif
                split_mode = split_dir ? SPLIT_BI_VER : SPLIT_BI_HOR;
#if EQT
            }
            else
            {
                split_mode = split_dir ? SPLIT_EQT_VER : SPLIT_EQT_HOR;
            }
#endif
        }
    }
    COM_TRACE_COUNTER;
    COM_TRACE_STR("split mode ");
    COM_TRACE_INT(split_mode);
    COM_TRACE_STR("\n");
    return split_mode;
}

#if CHROMA_NOT_SPLIT
int dec_decode_cu_chroma(DEC_CTX * ctx, DEC_CORE * core)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    COM_BSR  *bs = &ctx->bs;
    DEC_SBAC *sbac = GET_SBAC_DEC(bs);
    s8(*map_refi)[REFP_NUM];
    s16(*map_mv)[REFP_NUM][MV_D];
    u32 *map_scu;
    s8  *map_ipm;
    int cu_width = 1 << mod_info_curr->cu_width_log2, cu_height = 1 << mod_info_curr->cu_height_log2;
    int ret;
    cu_nz_cln(mod_info_curr->num_nz);
    init_pb_part(mod_info_curr);
    init_tb_part(mod_info_curr);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, mod_info_curr->pb_part, &mod_info_curr->pb_info);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, mod_info_curr->tb_part, &mod_info_curr->tb_info);

    //fetch luma pred mode
    int cu_w_scu = PEL2SCU(1 << mod_info_curr->cu_width_log2);
    int cu_h_scu = PEL2SCU(1 << mod_info_curr->cu_height_log2);
    int luma_scup = mod_info_curr->x_scu + (cu_w_scu - 1) + (mod_info_curr->y_scu + (cu_h_scu - 1)) * ctx->info.pic_width_in_scu;

    map_scu  = ctx->map.map_scu;
    map_refi = ctx->map.map_refi;
    map_mv   = ctx->map.map_mv;
    map_ipm  = ctx->map.map_ipm;
    assert(MCU_GET_CODED_FLAG(map_scu[luma_scup]));

    //prediction
    u8 luma_pred_mode = MCU_GET_INTRA_FLAG(map_scu[luma_scup])? MODE_INTRA : MODE_INTER;
    if (luma_pred_mode != MODE_INTRA)
    {
        mod_info_curr->cu_mode = MODE_INTER;
        //prepare mv to mod_info_curr
        for (int lidx = 0; lidx < REFP_NUM; lidx++)
        {
            mod_info_curr->refi[lidx] = map_refi[luma_scup][lidx];
            mod_info_curr->mv[lidx][MV_X] = map_mv[luma_scup][lidx][MV_X];
            mod_info_curr->mv[lidx][MV_Y] = map_mv[luma_scup][lidx][MV_Y];
            if (!REFI_IS_VALID(mod_info_curr->refi[lidx]))
            {
                assert(map_mv[luma_scup][lidx][MV_X] == 0);
                assert(map_mv[luma_scup][lidx][MV_Y] == 0);
            }
        }

        COM_TRACE_STR("luma pred mode INTER");
        COM_TRACE_STR("\n");
        COM_TRACE_STR("L0: Ref ");
        COM_TRACE_INT(mod_info_curr->refi[0]);
        COM_TRACE_STR("MVX ");
        COM_TRACE_INT(mod_info_curr->mv[0][MV_X]);
        COM_TRACE_STR("MVY ");
        COM_TRACE_INT(mod_info_curr->mv[0][MV_Y]);
        COM_TRACE_STR("\n");
        COM_TRACE_STR("L1: Ref ");
        COM_TRACE_INT(mod_info_curr->refi[1]);
        COM_TRACE_STR("MVX ");
        COM_TRACE_INT(mod_info_curr->mv[1][MV_X]);
        COM_TRACE_STR("MVY ");
        COM_TRACE_INT(mod_info_curr->mv[1][MV_Y]);
        COM_TRACE_STR("\n");
    }
    else
    {
        COM_TRACE_STR("luma pred mode INTRA");
        COM_TRACE_STR("\n");

        mod_info_curr->cu_mode = MODE_INTRA;
        mod_info_curr->ipm[PB0][0] = map_ipm[luma_scup];
#if TSCPM
        mod_info_curr->ipm[PB0][1] = (s8)decode_intra_dir_c(bs, sbac, mod_info_curr->ipm[PB0][0], ctx->info.sqh.tscpm_enable_flag);
#else
        mod_info_curr->ipm[PB0][1] = (s8)decode_intra_dir_c(bs, sbac, mod_info_curr->ipm[PB0][0]);
#endif
    }

    //coeff
    /* clear coefficient buffer */
    com_mset(mod_info_curr->coef[Y_C], 0, cu_width * cu_height * sizeof(s16));
    com_mset(mod_info_curr->coef[U_C], 0, (cu_width >> 1) * (cu_height >> 1) * sizeof(s16));
    com_mset(mod_info_curr->coef[V_C], 0, (cu_width >> 1) * (cu_height >> 1) * sizeof(s16));
    ret = decode_coef(ctx, core);
    com_assert_rv(ret == COM_OK, ret);
    return COM_OK;
}
#endif

int dec_decode_cu(DEC_CTX * ctx, DEC_CORE * core) // this function can be optimized to better match with the text
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    s16      mvd[MV_D];
    DEC_SBAC *sbac;
    COM_BSR  *bs;
    int      ret, cu_width, cu_height, inter_dir = 0;
    u8       bSkipMode;
    CPMV     affine_mvp[VER_NUM][MV_D];
    mod_info_curr->affine_flag = 0;
    mod_info_curr->mvr_idx = 0;
#if EXT_AMVR_HMVP
    mod_info_curr->mvp_from_hmvp_flag = 0;
#endif
    mod_info_curr->ipf_flag = 0;

    bs   = &ctx->bs;
    sbac = GET_SBAC_DEC(bs);
    cu_width  = (1 << mod_info_curr->cu_width_log2);
    cu_height = (1 << mod_info_curr->cu_height_log2);
#if TB_SPLIT_EXT
    cu_nz_cln(mod_info_curr->num_nz);
    init_pb_part(mod_info_curr);
    init_tb_part(mod_info_curr);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, mod_info_curr->pb_part, &mod_info_curr->pb_info);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, mod_info_curr->tb_part, &mod_info_curr->tb_info);
#endif

    if (ctx->info.pic_header.slice_type != SLICE_I)
    {
        if (ctx->cons_pred_mode != ONLY_INTRA)
        {
            bSkipMode = decode_skip_flag(bs, sbac, ctx);
        }
        else
        {
            bSkipMode = 0;
        }

        if (bSkipMode)
        {
            mod_info_curr->cu_mode = MODE_SKIP;
            mod_info_curr->umve_flag = 0;
            if (ctx->info.sqh.umve_enable_flag)
            {
                mod_info_curr->umve_flag = decode_umve_flag(bs, sbac, ctx);
            }

            if (mod_info_curr->umve_flag)
            {
                mod_info_curr->umve_idx = decode_umve_idx(bs, sbac, ctx);
            }
            else
            {
                mod_info_curr->affine_flag = decode_affine_flag(bs, sbac, ctx);
                if (mod_info_curr->affine_flag)
                {
                    mod_info_curr->skip_idx = decode_affine_mrg_idx(bs, sbac, ctx);
                }
                else
                {
                    mod_info_curr->skip_idx = decode_skip_idx(bs, sbac, ctx->info.sqh.num_of_hmvp_cand, ctx);
                }
            }
        }
        else
        {
            u8 direct_flag = 0;
            if (ctx->cons_pred_mode != ONLY_INTRA)
            {
                direct_flag = decode_direct_flag(bs, sbac, ctx);
            }
            if (direct_flag)
            {
                mod_info_curr->cu_mode = MODE_DIR;
                mod_info_curr->umve_flag = 0;
                if (ctx->info.sqh.umve_enable_flag)
                {
                    mod_info_curr->umve_flag = decode_umve_flag(bs, sbac, ctx);
                }

                if (mod_info_curr->umve_flag)
                {
                    mod_info_curr->umve_idx = decode_umve_idx(bs, sbac, ctx);
                }
                else
                {
                    mod_info_curr->affine_flag = decode_affine_flag(bs, sbac, ctx);
                    if (mod_info_curr->affine_flag)
                    {
                        mod_info_curr->skip_idx = decode_affine_mrg_idx(bs, sbac, ctx);

                    }
                    else
                    {
                        mod_info_curr->skip_idx = decode_skip_idx(bs, sbac, ctx->info.sqh.num_of_hmvp_cand, ctx);
                    }
                }
            }
            else
            {
                if (ctx->cons_pred_mode == NO_MODE_CONS)
                {
                    mod_info_curr->cu_mode = decode_pred_mode(bs, sbac, ctx);
                }
                else
                {
                    mod_info_curr->cu_mode = ctx->cons_pred_mode == ONLY_INTRA ? MODE_INTRA : MODE_INTER;
                }

                mod_info_curr->affine_flag = 0;
                if (mod_info_curr->cu_mode != MODE_INTRA)
                {
                    assert(mod_info_curr->cu_mode == MODE_INTER);
                    mod_info_curr->affine_flag = decode_affine_flag(bs, sbac, ctx);
                }

                if (mod_info_curr->cu_mode != MODE_INTRA && ctx->info.sqh.amvr_enable_flag)
                {
#if EXT_AMVR_HMVP
                    if (ctx->info.sqh.emvr_enable_flag && !mod_info_curr->affine_flag) // also imply ctx->info.sqh.num_of_hmvp_cand is not zero
                    {
                        mod_info_curr->mvp_from_hmvp_flag = decode_extend_amvr_flag(bs, sbac);
                    }
#endif
                    mod_info_curr->mvr_idx = (u8)decode_mvr_idx(bs, sbac, mod_info_curr->affine_flag);
                }
            }
        }
    }
    else /* SLICE_I */
    {
        mod_info_curr->cu_mode = MODE_INTRA;
    }

    /* parse prediction info */
    if (mod_info_curr->cu_mode == MODE_SKIP || mod_info_curr->cu_mode == MODE_DIR)
    {
        dec_derive_skip_direct_info(ctx, core);
        if (mod_info_curr->cu_mode == MODE_SKIP)
        {
            assert(mod_info_curr->pb_part == SIZE_2Nx2N);
        }
    }
    else if (mod_info_curr->cu_mode == MODE_INTER)
    {
        if (ctx->info.pic_header.slice_type == SLICE_P)
        {
            inter_dir = PRED_L0;
        }
        else /* if(ctx->info.pic_header.slice_type == SLICE_B) */
        {
            inter_dir = decode_inter_dir(bs, sbac, mod_info_curr->pb_part, ctx);
        }

        if (mod_info_curr->affine_flag) // affine inter motion vector
        {
            dec_eco_affine_motion_derive(ctx, core, bs, sbac, inter_dir, cu_width, cu_height, affine_mvp, mvd);
        }
        else
        {
#if SMVD
            if (ctx->info.sqh.smvd_enable_flag && inter_dir == PRED_BI
                    && (ctx->ptr - ctx->refp[0][REFP_0].ptr == ctx->refp[0][REFP_1].ptr - ctx->ptr)
#if SKIP_SMVD
                    && !mod_info_curr->mvp_from_hmvp_flag
#endif
               )
            {
                mod_info_curr->smvd_flag = (u8)decode_smvd_flag(bs, sbac, ctx);
            }
            else
            {
                mod_info_curr->smvd_flag = 0;
            }
#endif

            /* forward */
            if (inter_dir == PRED_L0 || inter_dir == PRED_BI)
            {
                s16 mvp[MV_D];
#if SMVD
                if (mod_info_curr->smvd_flag == 1)
                {
                    mod_info_curr->refi[REFP_0] = 0;
                }
                else
                {
#endif
                    mod_info_curr->refi[REFP_0] = (s8)decode_refidx(bs, sbac, ctx->dpm.num_refp[REFP_0]);
#if SMVD
                }
#endif
                com_derive_mvp(ctx->info, mod_info_curr, ctx->ptr, REFP_0, mod_info_curr->refi[REFP_0], core->cnt_hmvp_cands,
                    core->motion_cands, ctx->map, ctx->refp, mod_info_curr->mvr_idx, mvp);

                decode_mvd(bs, sbac, mvd);
                s32 mv_x = (s32)mvp[MV_X] + ((s32)mvd[MV_X] << mod_info_curr->mvr_idx);
                s32 mv_y = (s32)mvp[MV_Y] + ((s32)mvd[MV_Y] << mod_info_curr->mvr_idx);
                mod_info_curr->mv[REFP_0][MV_X] = COM_CLIP3(COM_INT16_MIN, COM_INT16_MAX, mv_x);
                mod_info_curr->mv[REFP_0][MV_Y] = COM_CLIP3(COM_INT16_MIN, COM_INT16_MAX, mv_y);

            }
            else
            {
                mod_info_curr->refi[REFP_0] = REFI_INVALID;
                mod_info_curr->mv[REFP_0][MV_X] = 0;
                mod_info_curr->mv[REFP_0][MV_Y] = 0;
            }

            /* backward */
            if (inter_dir == PRED_L1 || inter_dir == PRED_BI)
            {
                s16 mvp[MV_D];
#if SMVD
                if (mod_info_curr->smvd_flag == 1)
                {
                    mod_info_curr->refi[REFP_1] = 0;
                }
                else
                {
#endif
                    mod_info_curr->refi[REFP_1] = (s8)decode_refidx(bs, sbac, ctx->dpm.num_refp[REFP_1]);
#if SMVD
                }
#endif
                com_derive_mvp(ctx->info, mod_info_curr, ctx->ptr, REFP_1, mod_info_curr->refi[REFP_1], core->cnt_hmvp_cands,
                    core->motion_cands, ctx->map, ctx->refp, mod_info_curr->mvr_idx, mvp);

#if SMVD
                if (mod_info_curr->smvd_flag == 1)
                {
                    mvd[MV_X] = COM_CLIP3(COM_INT16_MIN, COM_INT16_MAX, -mvd[MV_X]);
                    mvd[MV_Y] = COM_CLIP3(COM_INT16_MIN, COM_INT16_MAX, -mvd[MV_Y]);
                }
                else
                {
#endif
                    decode_mvd(bs, sbac, mvd);
#if SMVD
                }
#endif
                s32 mv_x = (s32)mvp[MV_X] + ((s32)mvd[MV_X] << mod_info_curr->mvr_idx);
                s32 mv_y = (s32)mvp[MV_Y] + ((s32)mvd[MV_Y] << mod_info_curr->mvr_idx);

                mod_info_curr->mv[REFP_1][MV_X] = COM_CLIP3(COM_INT16_MIN, COM_INT16_MAX, mv_x);
                mod_info_curr->mv[REFP_1][MV_Y] = COM_CLIP3(COM_INT16_MIN, COM_INT16_MAX, mv_y);
            }
            else
            {
                mod_info_curr->refi[REFP_1] = REFI_INVALID;
                mod_info_curr->mv[REFP_1][MV_X] = 0;
                mod_info_curr->mv[REFP_1][MV_Y] = 0;
            }
        }
    }
    else if (mod_info_curr->cu_mode == MODE_INTRA)
    {
#if DT_SYNTAX
        mod_info_curr->pb_part = decode_part_size(bs, sbac, ctx, cu_width, cu_height, mod_info_curr->cu_mode);
        get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, mod_info_curr->pb_part, &mod_info_curr->pb_info);
        assert(mod_info_curr->pb_info.sub_scup[0] == mod_info_curr->scup);
        for (int part_idx = 0; part_idx < core->mod_info_curr.pb_info.num_sub_part; part_idx++)
        {
            int pb_x = mod_info_curr->pb_info.sub_x[part_idx];
            int pb_y = mod_info_curr->pb_info.sub_y[part_idx];
            int pb_width  = mod_info_curr->pb_info.sub_w[part_idx];
            int pb_height = mod_info_curr->pb_info.sub_h[part_idx];
            int pb_scup   = mod_info_curr->pb_info.sub_scup[part_idx];
#endif
            com_get_mpm(PEL2SCU(pb_x), PEL2SCU(pb_y), ctx->map.map_scu, ctx->map.map_ipm, pb_scup, ctx->info.pic_width_in_scu, mod_info_curr->mpm[part_idx]);
            mod_info_curr->ipm[part_idx][0] = (s8)decode_intra_dir(bs, sbac, mod_info_curr->mpm[part_idx]);
            SET_REFI(mod_info_curr->refi, REFI_INVALID, REFI_INVALID);
            mod_info_curr->mv[REFP_0][MV_X] = mod_info_curr->mv[REFP_0][MV_Y] = 0;
            mod_info_curr->mv[REFP_1][MV_X] = mod_info_curr->mv[REFP_1][MV_Y] = 0;
            update_intra_info_map_scu(ctx->map.map_scu, ctx->map.map_ipm, pb_x, pb_y, pb_width, pb_height, ctx->info.pic_width_in_scu, mod_info_curr->ipm[part_idx][0]);
#if DT_SYNTAX
        }
#endif
        if (ctx->tree_status != TREE_L)
        {
#if TSCPM
            mod_info_curr->ipm[PB0][1] = (s8)decode_intra_dir_c(bs, sbac, mod_info_curr->ipm[PB0][0], ctx->info.sqh.tscpm_enable_flag);
#else
            mod_info_curr->ipm[PB0][1] = (s8)decode_intra_dir_c(bs, sbac, mod_info_curr->ipm[PB0][0]);
#endif
        }

#if IPCM
        if (!((ctx->tree_status == TREE_C && mod_info_curr->ipm[PB0][0] == IPD_IPCM && mod_info_curr->ipm[PB0][1] == IPD_DM_C)
                || (ctx->tree_status != TREE_C && mod_info_curr->ipm[PB0][0] == IPD_IPCM)))
        {
#endif

#if DT_INTRA_BOUNDARY_FILTER_OFF
            if (ctx->info.sqh.ipf_enable_flag && (cu_width < MAX_CU_SIZE) && (cu_height < MAX_CU_SIZE) && core->mod_info_curr.pb_part == SIZE_2Nx2N)
#else
            if (ctx->info.sqh.ipf_enable_flag && (cu_width < MAX_CU_SIZE) && (cu_height < MAX_CU_SIZE))
#endif
            {
                mod_info_curr->ipf_flag = decode_ipf_flag(bs, sbac);
            }
#if IPCM
        }
#endif

        SET_REFI(mod_info_curr->refi, REFI_INVALID, REFI_INVALID);
        mod_info_curr->mv[REFP_0][MV_X] = mod_info_curr->mv[REFP_0][MV_Y] = 0;
        mod_info_curr->mv[REFP_1][MV_X] = mod_info_curr->mv[REFP_1][MV_Y] = 0;
    }
    else
    {
        com_assert_rv(0, COM_ERR_MALFORMED_BITSTREAM);
    }

    /* parse coefficients */
    if (mod_info_curr->cu_mode != MODE_SKIP)
    {
        /* clear coefficient buffer */
        com_mset(mod_info_curr->coef[Y_C], 0, cu_width * cu_height * sizeof(s16));
        com_mset(mod_info_curr->coef[U_C], 0, (cu_width >> 1) * (cu_height >> 1) * sizeof(s16));
        com_mset(mod_info_curr->coef[V_C], 0, (cu_width >> 1) * (cu_height >> 1) * sizeof(s16));
        ret = decode_coef(ctx, core);
        com_assert_rv(ret == COM_OK, ret);
    }
    return COM_OK;
}

int dec_eco_cnkh(COM_BSR * bs, COM_CNKH * cnkh)
{
    cnkh->ver = com_bsr_read(bs, 3);
    cnkh->ctype = com_bsr_read(bs, 4);
    cnkh->broken = com_bsr_read1(bs);
    return COM_OK;
}
#if HLS_RPL
#if LIBVC_ON
int dec_eco_rlp(COM_BSR * bs, COM_RPL * rpl, COM_SQH * sqh)
#else
int dec_eco_rlp(COM_BSR * bs, COM_RPL * rpl)
#endif
{
#if LIBVC_ON
    int ddoi_base = 0;

    rpl->reference_to_library_enable_flag = 0;
    if (sqh->library_picture_enable_flag)
    {
        rpl->reference_to_library_enable_flag = com_bsr_read1(bs);
    }
#endif

    rpl->ref_pic_num = (u32)com_bsr_read_ue(bs);
    //note, here we store DOI of each ref pic instead of delta DOI
    if (rpl->ref_pic_num > 0)
    {
#if LIBVC_ON
        rpl->library_index_flag[0] = 0;
        if (sqh->library_picture_enable_flag && rpl->reference_to_library_enable_flag)
        {
            rpl->library_index_flag[0] = com_bsr_read1(bs);
        }
        if (sqh->library_picture_enable_flag && rpl->library_index_flag[0])
        {
            rpl->ref_pics_ddoi[0] = (u32)com_bsr_read_ue(bs);
        }
        else
#endif
        {
            rpl->ref_pics_ddoi[0] = (u32)com_bsr_read_ue(bs);
            if (rpl->ref_pics_ddoi[0] != 0) rpl->ref_pics_ddoi[0] *= 1 - ((u32)com_bsr_read1(bs) << 1);
#if LIBVC_ON
            ddoi_base = rpl->ref_pics_ddoi[0];
#endif
        }

    }
    for (int i = 1; i < rpl->ref_pic_num; ++i)
    {
#if LIBVC_ON
        rpl->library_index_flag[i] = 0;
        if (sqh->library_picture_enable_flag && rpl->reference_to_library_enable_flag)
        {
            rpl->library_index_flag[i] = com_bsr_read1(bs);
        }
        if (sqh->library_picture_enable_flag && rpl->library_index_flag[i])
        {
            rpl->ref_pics_ddoi[i] = (u32)com_bsr_read_ue(bs);
        }
        else
#endif
        {
            int deltaRefPic = (u32)com_bsr_read_ue(bs);
            if (deltaRefPic != 0) deltaRefPic *= 1 - ((u32)com_bsr_read1(bs) << 1);
#if LIBVC_ON
            rpl->ref_pics_ddoi[i] = ddoi_base + deltaRefPic;
            ddoi_base = rpl->ref_pics_ddoi[i];
#else
            rpl->ref_pics_ddoi[i] = rpl->ref_pics_ddoi[i - 1] + deltaRefPic;
#endif
        }

    }
    return COM_OK;
}
#endif

void read_wq_matrix(COM_BSR *bs, u8 *m4x4, u8 *m8x8)
{
    int i;
    for (i = 0; i < 16; i++)
    {
        m4x4[i] = com_bsr_read_ue(bs);
    }
    for (i = 0; i < 64; i++)
    {
        m8x8[i] = com_bsr_read_ue(bs);
    }
}

int dec_eco_sqh(COM_BSR * bs, COM_SQH * sqh)
{
    //video_sequence_start_code
    unsigned int ret = com_bsr_read(bs, 24);
    assert(ret == 1);
    unsigned int start_code;
    start_code = com_bsr_read(bs, 8);
    assert(start_code==0xB0);

    sqh->profile_id = (u8)com_bsr_read(bs, 8);
    sqh->level_id   = (u8)com_bsr_read(bs, 8);
    sqh->progressive_sequence = (u8)com_bsr_read1(bs);
    assert(sqh->progressive_sequence == 1);
    sqh->field_coded_sequence = (u8)com_bsr_read1(bs);
#if LIBVC_ON
    sqh->library_stream_flag = (u8)com_bsr_read1(bs);
    sqh->library_picture_enable_flag = 0;
    sqh->duplicate_sequence_header_flag = 0;
    if (!sqh->library_stream_flag)
    {
        sqh->library_picture_enable_flag = (u8)com_bsr_read1(bs);
        if (sqh->library_picture_enable_flag)
        {
            sqh->duplicate_sequence_header_flag = (u8)com_bsr_read1(bs);
        }
    }
#endif
    assert(com_bsr_read1(bs) == 1);                                //marker_bit
    sqh->horizontal_size  = com_bsr_read(bs, 14);                  //horizontal_size
    assert(com_bsr_read1(bs) == 1);                                //marker_bit
    sqh->vertical_size = com_bsr_read(bs, 14);                     //vertical_size
    sqh->chroma_format    = (u8)com_bsr_read(bs, 2);
    sqh->sample_precision = (u8)com_bsr_read(bs, 3);

    if (sqh->profile_id == 0x22)
    {
        sqh->encoding_precision = (u8)com_bsr_read(bs, 3);
    }
    assert(com_bsr_read1(bs) == 1);                                //marker_bit
    sqh->aspect_ratio = (u8)com_bsr_read(bs, 4);

    sqh->frame_rate_code = (u8)com_bsr_read(bs, 4);
    assert(com_bsr_read1(bs) == 1);                                //marker_bit
    sqh->bit_rate_lower  = com_bsr_read(bs, 18);
    assert(com_bsr_read1(bs) == 1);                                //marker_bit
    sqh->bit_rate_upper = com_bsr_read(bs, 12);
    sqh->low_delay      = (u8)com_bsr_read1(bs);

    sqh->temporal_id_enable_flag = (u8)com_bsr_read1(bs);
    assert(com_bsr_read1(bs) == 1);                                //marker_bit
    sqh->bbv_buffer_size = com_bsr_read(bs, 18);
    assert(com_bsr_read1(bs) == 1);                                //marker_bit
    sqh->max_dpb_size = com_bsr_read(bs, 4) + 1;

#if HLS_RPL
    sqh->rpl1_index_exist_flag  = (u32)com_bsr_read1(bs);
    sqh->rpl1_same_as_rpl0_flag = (u32)com_bsr_read1(bs);
    assert(com_bsr_read1(bs) == 1);                                //marker_bit
    sqh->rpls_l0_num            = (u32)com_bsr_read_ue(bs);
    for (int i = 0; i < sqh->rpls_l0_num; ++i)
    {
#if LIBVC_ON
        dec_eco_rlp(bs, &sqh->rpls_l0[i], sqh);
#else
        dec_eco_rlp(bs, &sqh->rpls_l0[i]);
#endif
    }

    if (!sqh->rpl1_same_as_rpl0_flag)
    {
        sqh->rpls_l1_num = (u32)com_bsr_read_ue(bs);
        for (int i = 0; i < sqh->rpls_l1_num; ++i)
        {
#if LIBVC_ON
            dec_eco_rlp(bs, &sqh->rpls_l1[i], sqh);
#else
            dec_eco_rlp(bs, &sqh->rpls_l1[i]);
#endif
        }
    }
    else
    {
        //Basically copy everything from sqh->rpls_l0 to sqh->rpls_l1
        //MX: LIBVC is not harmonization yet.
        sqh->rpls_l1_num = sqh->rpls_l0_num;
        for (int i = 0; i < sqh->rpls_l1_num; i++)
        {
#if LIBVC_ON
            sqh->rpls_l1[i].reference_to_library_enable_flag = sqh->rpls_l0[i].reference_to_library_enable_flag;
#endif
            sqh->rpls_l1[i].ref_pic_num = sqh->rpls_l0[i].ref_pic_num;
            for (int j = 0; j < sqh->rpls_l1[i].ref_pic_num; j++)
            {
#if LIBVC_ON
                sqh->rpls_l1[i].library_index_flag[j] = sqh->rpls_l0[i].library_index_flag[j];
#endif
                sqh->rpls_l1[i].ref_pics_ddoi[j] = sqh->rpls_l0[i].ref_pics_ddoi[j];
            }
        }
    }
    sqh->num_ref_default_active_minus1[0] = (u32)com_bsr_read_ue(bs);
    sqh->num_ref_default_active_minus1[1] = (u32)com_bsr_read_ue(bs);
#endif

    sqh->log2_max_cu_width_height = (u8)(com_bsr_read(bs, 3) + 2); //log2_lcu_size_minus2
    sqh->min_cu_size    = (u8)(1 << (com_bsr_read(bs, 2) + 2));    //log2_min_cu_size_minus2
    sqh->max_part_ratio = (u8)(1 << (com_bsr_read(bs, 2) + 2));    //log2_max_part_ratio_minus2
    sqh->max_split_times = (u8)(com_bsr_read(bs,    3) + 6 );      //max_split_times_minus6
    sqh->min_qt_size  = (u8)(1 << (com_bsr_read(bs, 3) + 2));      //log2_min_qt_size_minus2
    sqh->max_bt_size  = (u8)(1 << (com_bsr_read(bs, 3) + 2));      //log2_max_bt_size_minus2
    sqh->max_eqt_size = (u8)(1 << (com_bsr_read(bs, 2) + 3));      //log2_max_eqt_size_minus3
    assert(com_bsr_read1(bs) == 1);                                //marker_bit
    //verify parameters allowed in Profile
    assert(sqh->log2_max_cu_width_height >= 5 && sqh->log2_max_cu_width_height <= 7);
    assert(sqh->min_cu_size == 4);
    assert(sqh->max_part_ratio == 8);
    assert(sqh->max_split_times == 6);
    assert(sqh->min_qt_size == 4 || sqh->min_qt_size == 8 || sqh->min_qt_size == 16 || sqh->min_qt_size == 32 || sqh->min_qt_size == 64 || sqh->min_qt_size == 128);
    assert(sqh->max_bt_size == 64 || sqh->max_bt_size == 128);
    assert(sqh->max_eqt_size == 8 || sqh->max_eqt_size == 16 || sqh->max_eqt_size == 32 || sqh->max_eqt_size == 64);

    sqh->wq_enable = (u8)com_bsr_read1(bs);                        //weight_quant_enable_flag
    if (sqh->wq_enable)
    {
        sqh->seq_wq_mode = com_bsr_read1(bs);                      //load_seq_weight_quant_data_flag
        if (sqh->seq_wq_mode)
        {
            read_wq_matrix(bs, sqh->wq_4x4_matrix, sqh->wq_8x8_matrix);  //weight_quant_matrix( )
        }
        else
        {
            memcpy(sqh->wq_4x4_matrix, tab_WqMDefault4x4, sizeof(tab_WqMDefault4x4));
            memcpy(sqh->wq_8x8_matrix, tab_WqMDefault8x8, sizeof(tab_WqMDefault8x8));
        }
    }
    sqh->secondary_transform_enable_flag = (u8)com_bsr_read1(bs);
    sqh->sample_adaptive_offset_enable_flag = (u8)com_bsr_read1(bs);
    sqh->adaptive_leveling_filter_enable_flag = (u8)com_bsr_read1(bs);
    sqh->affine_enable_flag = (u8)com_bsr_read1(bs);
#if SMVD
    sqh->smvd_enable_flag = (u8)com_bsr_read1(bs);
#endif
#if IPCM
    sqh->ipcm_enable_flag = com_bsr_read1(bs);
    assert(sqh->sample_precision == 1 || sqh->sample_precision == 2);
#endif
    sqh->amvr_enable_flag = (u8)com_bsr_read1(bs);
    sqh->num_of_hmvp_cand = (u8)com_bsr_read(bs, 4);
    sqh->umve_enable_flag = (u8)com_bsr_read1(bs);
#if EXT_AMVR_HMVP
    if (sqh->amvr_enable_flag && sqh->num_of_hmvp_cand)
    {
        sqh->emvr_enable_flag = (u8)com_bsr_read1(bs);
    }
    else
    {
        sqh->emvr_enable_flag = 0;
    }
#endif
    sqh->ipf_enable_flag = (u8)com_bsr_read1(bs); //ipf_enable_flag
#if TSCPM
    sqh->tscpm_enable_flag = (u8)com_bsr_read1(bs);
#endif
    assert(com_bsr_read1(bs) == 1);              //marker_bit
#if DT_PARTITION
    sqh->dt_intra_enable_flag = (u8)com_bsr_read1(bs);
    if (sqh->dt_intra_enable_flag)
    {
        u32 log2_max_dt_size_minus4 = com_bsr_read(bs, 2);
        assert(log2_max_dt_size_minus4 <= 2);
        sqh->max_dt_size = (u8)(1 << (log2_max_dt_size_minus4 + 4)); //log2_max_dt_size_minus4
    }
#endif
    sqh->position_based_transform_enable_flag = (u8)com_bsr_read1(bs); //pbt_enable_flag
    if (sqh->low_delay == 0)
    {
        sqh->output_reorder_delay = (u8)com_bsr_read(bs, 5);
    }
    else
    {
        sqh->output_reorder_delay = 0;
    }
#if PATCH
    sqh->cross_patch_loop_filter = (u8)com_bsr_read1(bs); //cross_patch_loopfilter_enable_flag
    sqh->patch_ref_colocated = (u8)com_bsr_read1(bs);     //ref_colocated_patch_flag
    sqh->patch_stable = (u8)com_bsr_read1(bs);            //stable_patch_flag
    if (sqh->patch_stable)
    {
        sqh->patch_uniform = (u8)com_bsr_read1(bs);       //uniform_patch_flag
        if (sqh->patch_uniform)
        {
            assert(com_bsr_read1(bs) == 1);                      //marker_bit
            sqh->patch_width_minus1 = (u8)com_bsr_read_ue(bs);   //patch_width_minus1
            sqh->patch_height_minus1 = (u8)com_bsr_read_ue(bs);  //patch_height_minus1
        }
    }
#endif
    com_bsr_read(bs, 2); //reserved_bits r(2)

    //next_start_code()
    assert(com_bsr_read1(bs)==1); // stuffing_bit '1'
    while (!COM_BSR_IS_BYTE_ALIGN(bs))
    {
        assert(com_bsr_read1(bs) == 0); // stuffing_bit '0'
    }
    while (com_bsr_next(bs, 24) != 0x1)
    {
        assert(com_bsr_read(bs, 8) == 0); // stuffing_byte '00000000'
    };

    /* check values */
    int max_cuwh = 1 << sqh->log2_max_cu_width_height;
    if(max_cuwh < MIN_CU_SIZE || max_cuwh > MAX_CU_SIZE)
    {
        return COM_ERR_MALFORMED_BITSTREAM;
    }
    return COM_OK;
}

int dec_eco_pic_header(COM_BSR * bs, COM_PIC_HEADER * pic_header, COM_SQH * sqh, int* need_minus_256)
{
    unsigned int ret = com_bsr_read(bs, 24);
    assert(ret == 1);
    unsigned int start_code = com_bsr_read(bs, 8);
    if (start_code == 0xB3)
    {
        pic_header->slice_type = SLICE_I;
    }
    assert(start_code == 0xB6 || start_code == 0xB3);
    if (start_code != 0xB3)//MX: not SLICE_I
    {
      pic_header->random_access_decodable_flag = com_bsr_read1(bs);
    }
    pic_header->bbv_delay = com_bsr_read(bs, 8) << 24 | com_bsr_read(bs, 8) << 16 | com_bsr_read(bs, 8) << 8 | com_bsr_read(bs, 8);

    if (start_code == 0xB6)
    {
        //picture_coding_type
        u8 val = (u8)com_bsr_read(bs, 2);
        assert(val == 1 || val == 2);
        pic_header->slice_type = val == 1 ? SLICE_P : SLICE_B;
    }
    if (pic_header->slice_type == SLICE_I)
    {
        pic_header->time_code_flag = com_bsr_read1(bs);
        if (pic_header->time_code_flag == 1)
        {
            pic_header->time_code = com_bsr_read(bs, 24);
        }
    }

    pic_header->decode_order_index = com_bsr_read(bs, 8);

#if LIBVC_ON
    pic_header->library_picture_index = -1;
    if (pic_header->slice_type == SLICE_I)
    {
        if (sqh->library_stream_flag)
        {
            pic_header->library_picture_index = com_bsr_read_ue(bs);
        }
    }
#endif

    if (sqh->temporal_id_enable_flag == 1)
    {
        pic_header->temporal_id = (u8)com_bsr_read(bs, 3);
    }
    if (sqh->low_delay == 0)
    {
        pic_header->picture_output_delay = com_bsr_read_ue(bs);
        pic_header->bbv_check_times = 0;
    }
    else
    {
        pic_header->picture_output_delay = 0;
        pic_header->bbv_check_times = com_bsr_read_ue(bs);
    }

    //the field information below is not used by decoder -- start
    pic_header->progressive_frame = com_bsr_read1(bs);
    assert(pic_header->progressive_frame == 1);
    if (pic_header->progressive_frame == 0)
    {
        pic_header->picture_structure = com_bsr_read1(bs);
    }
    else
    {
        pic_header->picture_structure = 1;
    }
    pic_header->top_field_first = com_bsr_read1(bs);
    pic_header->repeat_first_field = com_bsr_read1(bs);
    if (sqh->field_coded_sequence == 1)
    {
        pic_header->top_field_picture_flag = com_bsr_read1(bs);
        com_bsr_read1(bs); // reserved_bits r(1)
    }
    // -- end

#if LIBVC_ON
    if (!sqh->library_stream_flag)
    {
#endif
      if (pic_header->decode_order_index < g_DOIPrev)
      {
        *need_minus_256 = 1;
        g_CountDOICyCleTime++;                    // initialized the number .
      }
      g_DOIPrev = pic_header->decode_order_index;
      pic_header->dtr = pic_header->decode_order_index + (DOI_CYCLE_LENGTH*g_CountDOICyCleTime) + pic_header->picture_output_delay - sqh->output_reorder_delay;//MX: in the decoder, the only usage of g_CountDOICyCleTime is to derive the POC/POC. here is the dtr
#if LIBVC_ON
    }
    else
    {
        pic_header->dtr = pic_header->decode_order_index + (DOI_CYCLE_LENGTH*g_CountDOICyCleTime) + pic_header->picture_output_delay - sqh->output_reorder_delay;
    }
#endif

    //initialization
    pic_header->rpl_l0_idx = pic_header->rpl_l1_idx = -1;
    pic_header->ref_pic_list_sps_flag[0] = pic_header->ref_pic_list_sps_flag[1] = 0;
    pic_header->rpl_l0.ref_pic_num = 0;
    pic_header->rpl_l1.ref_pic_num = 0;
    pic_header->rpl_l0.ref_pic_active_num = 0;
    pic_header->rpl_l1.ref_pic_active_num = 0;
    for (int i = 0; i < MAX_NUM_REF_PICS; i++)
    {
        pic_header->rpl_l0.ref_pics[i] = 0;
        pic_header->rpl_l1.ref_pics[i] = 0;
        pic_header->rpl_l0.ref_pics_ddoi[i] = 0;
        pic_header->rpl_l1.ref_pics_ddoi[i] = 0;
        pic_header->rpl_l0.ref_pics_doi[i] = 0;
        pic_header->rpl_l1.ref_pics_doi[i] = 0;
#if LIBVC_ON
        pic_header->rpl_l0.library_index_flag[i] = 0;
        pic_header->rpl_l1.library_index_flag[i] = 0;
#endif
    }

#if HLS_RPL
    //TBD(@Chernyak) if(!IDR) condition to be added here
    pic_header->poc = pic_header->dtr;
    //pic_header->poc = com_bsr_read_ue(bs);
    // L0 candidates signaling
    pic_header->ref_pic_list_sps_flag[0] = com_bsr_read1(bs);
    if (pic_header->ref_pic_list_sps_flag[0])
    {
        if (sqh->rpls_l0_num)
        {
            if (sqh->rpls_l0_num > 1)
            {
                pic_header->rpl_l0_idx = com_bsr_read_ue(bs);
            }
            else//if sps only have 1 RPL, no need to signal the idx
            {
                pic_header->rpl_l0_idx = 0;
            }
            memcpy(&pic_header->rpl_l0, &sqh->rpls_l0[pic_header->rpl_l0_idx], sizeof(pic_header->rpl_l0));
            pic_header->rpl_l0.poc = pic_header->poc;
        }
    }
    else
    {
#if LIBVC_ON
        dec_eco_rlp(bs, &pic_header->rpl_l0, sqh);
#else
        dec_eco_rlp(bs, &pic_header->rpl_l0);
#endif
        pic_header->rpl_l0.poc = pic_header->poc;
    }

    //L1 candidates signaling
    pic_header->ref_pic_list_sps_flag[1] = sqh->rpl1_index_exist_flag ? com_bsr_read1(bs) : pic_header->ref_pic_list_sps_flag[0];
    if (pic_header->ref_pic_list_sps_flag[1])
    {
        if (sqh->rpls_l1_num > 1 && sqh->rpl1_index_exist_flag)
        {
            pic_header->rpl_l1_idx = com_bsr_read_ue(bs);
        }
        else if (!sqh->rpl1_index_exist_flag)
        {
            pic_header->rpl_l1_idx = pic_header->rpl_l0_idx;
        }
        else//if sps only have 1 RPL, no need to signal the idx
        {
            assert(sqh->rpls_l1_num == 1);
            pic_header->rpl_l1_idx = 0;
        }
        memcpy(&pic_header->rpl_l1, &sqh->rpls_l1[pic_header->rpl_l1_idx], sizeof(pic_header->rpl_l1));
        pic_header->rpl_l1.poc = pic_header->poc;
    }
    else
    {
#if LIBVC_ON
        dec_eco_rlp(bs, &pic_header->rpl_l1, sqh);
#else
        dec_eco_rlp(bs, &pic_header->rpl_l1);
#endif
        pic_header->rpl_l1.poc = pic_header->poc;
    }

    if (pic_header->slice_type != SLICE_I)
    {
        pic_header->num_ref_idx_active_override_flag = com_bsr_read1(bs);
        if (pic_header->num_ref_idx_active_override_flag)
        {
            pic_header->rpl_l0.ref_pic_active_num = (u32)com_bsr_read_ue(bs) + 1;
            if (pic_header->slice_type == SLICE_P)
            {
                pic_header->rpl_l1.ref_pic_active_num = 0;
            }
            else if (pic_header->slice_type == SLICE_B)
            {
                pic_header->rpl_l1.ref_pic_active_num = (u32)com_bsr_read_ue(bs) + 1;
            }
        }
        else
        {
            //Hendry -- @Roman: we need to signal the num_ref_idx_default_active_minus1[ i ]. This syntax element is in the PPS in the spec document
            pic_header->rpl_l0.ref_pic_active_num = sqh->num_ref_default_active_minus1[0]+1;
            if (pic_header->slice_type == SLICE_P)
            {
              pic_header->rpl_l1.ref_pic_active_num = 0;
            }
            else
            {
              pic_header->rpl_l1.ref_pic_active_num = sqh->num_ref_default_active_minus1[1] + 1;
            }
        }
    }
    if (pic_header->slice_type == SLICE_I)
    {
        pic_header->rpl_l0.ref_pic_active_num = 0;
        pic_header->rpl_l1.ref_pic_active_num = 0;
    }
#if LIBVC_ON
    pic_header->is_RLpic_flag = 0;
    if (pic_header->slice_type != SLICE_I)
    {
        int only_ref_libpic_flag = 1;
        for (int i = 0; i < pic_header->rpl_l0.ref_pic_active_num; i++)
        {
            if (!pic_header->rpl_l0.library_index_flag[i])
            {
                only_ref_libpic_flag = 0;
                break;
            }
        }
        if (only_ref_libpic_flag)
        {
            for (int i = 0; i < pic_header->rpl_l1.ref_pic_active_num; i++)
            {
                if (!pic_header->rpl_l1.library_index_flag[i])
                {
                    only_ref_libpic_flag = 0;
                    break;
                }
            }
        }
        pic_header->is_RLpic_flag = only_ref_libpic_flag;
    }
#endif
#endif
    pic_header->fixed_picture_qp_flag = com_bsr_read1(bs);
    pic_header->picture_qp = com_bsr_read(bs, 7);
    //the reserved_bits only appears in inter_picture_header, so add the non-I-slice check
    if( pic_header->slice_type != SLICE_I && !(pic_header->slice_type == SLICE_B && pic_header->picture_structure == 1) )
    {
        com_bsr_read1( bs ); // reserved_bits r(1)
    }
    dec_eco_DB_param(bs, pic_header);

    pic_header->chroma_quant_param_disable_flag = (u8)com_bsr_read1(bs);
    if (pic_header->chroma_quant_param_disable_flag == 0)
    {
        pic_header->chroma_quant_param_delta_cb = (u8)com_bsr_read_se(bs);
        pic_header->chroma_quant_param_delta_cr = (u8)com_bsr_read_se(bs);
    }
    else
    {
        pic_header->chroma_quant_param_delta_cb = pic_header->chroma_quant_param_delta_cr = 0;
    }

    if (sqh->wq_enable)
    {
        pic_header->pic_wq_enable = com_bsr_read1(bs);
        if (pic_header->pic_wq_enable)
        {
            pic_header->pic_wq_data_idx = com_bsr_read(bs, 2);
            if (pic_header->pic_wq_data_idx == 0)
            {
                memcpy(pic_header->wq_4x4_matrix, sqh->wq_4x4_matrix, sizeof(sqh->wq_4x4_matrix));
                memcpy(pic_header->wq_8x8_matrix, sqh->wq_8x8_matrix, sizeof(sqh->wq_8x8_matrix));
            }
            else if (pic_header->pic_wq_data_idx == 1)
            {
                int delta, i;
                com_bsr_read1( bs ); //reserved_bits r(1)
                pic_header->wq_param = com_bsr_read(bs, 2); //weight_quant_param_index
                pic_header->wq_model = com_bsr_read(bs, 2); //weight_quant_model
                if (pic_header->wq_param == 0)
                {
                    memcpy(pic_header->wq_param_vector, tab_wq_param_default[1], sizeof(pic_header->wq_param_vector));
                }
                else if (pic_header->wq_param == 1)
                {
                    for (i = 0; i < 6; i++)
                    {
                        delta = com_bsr_read_se(bs);
                        pic_header->wq_param_vector[i] = delta + tab_wq_param_default[0][i];
                    }
                }
                else
                {
                    for (i = 0; i < 6; i++)
                    {
                        delta = com_bsr_read_se(bs);
                        pic_header->wq_param_vector[i] = delta + tab_wq_param_default[1][i];
                    }
                }
                set_pic_wq_matrix_by_param(pic_header->wq_param_vector, pic_header->wq_model, pic_header->wq_4x4_matrix, pic_header->wq_8x8_matrix);
            }
            else
            {
                read_wq_matrix(bs, pic_header->wq_4x4_matrix, pic_header->wq_8x8_matrix);
            }
        }
        else
        {
            init_pic_wq_matrix(pic_header->wq_4x4_matrix, pic_header->wq_8x8_matrix);
        }
    }
    else
    {
        pic_header->pic_wq_enable = 0;
        init_pic_wq_matrix(pic_header->wq_4x4_matrix, pic_header->wq_8x8_matrix);
    }

    if (pic_header->tool_alf_on)
    {
        /* decode ALF flag and ALF coeff */
        dec_eco_ALF_param(bs, pic_header);
    }
    else
    {
        memset( pic_header->pic_alf_on, 0, N_C * sizeof( int ) );
    }

    if (pic_header->slice_type != SLICE_I && sqh->affine_enable_flag)
    {
        pic_header->affine_subblock_size_idx = com_bsr_read(bs, 1);
    }

    /* byte align */
    ret = com_bsr_read1(bs);
    assert(ret == 1);
    while (!COM_BSR_IS_BYTE_ALIGN(bs))
    {
        assert(com_bsr_read1(bs) == 0);
    }
    while (com_bsr_next(bs, 24) != 0x1)
    {
        assert(com_bsr_read(bs, 8) == 0);
    }
    return COM_OK;
}

int dec_eco_patch_header(COM_BSR * bs, COM_SQH *sqh, COM_PIC_HEADER * ph, COM_SH_EXT * sh,PATCH_INFO *patch)
{
    //patch_start_code_prefix & patch_index
    unsigned int ret = com_bsr_read(bs, 24);
    assert(ret == 1);
    patch->idx = com_bsr_read(bs, 8);
    printf("%d ", patch->idx);
    assert(patch->idx >= 0x00 && patch->idx <= 0x8E);

    if (!ph->fixed_picture_qp_flag)
    {
        sh->fixed_slice_qp_flag = (u8)com_bsr_read1(bs);
        sh->slice_qp = (u8)com_bsr_read(bs, 7);
    }
    else
    {
        sh->fixed_slice_qp_flag = 1;
        sh->slice_qp = (u8)ph->picture_qp;
    }
    if (sqh->sample_adaptive_offset_enable_flag)
    {
        sh->slice_sao_enable[Y_C] = (u8)com_bsr_read1(bs);
        sh->slice_sao_enable[U_C] = (u8)com_bsr_read1(bs);
        sh->slice_sao_enable[V_C] = (u8)com_bsr_read1(bs);
    }
#if PATCH_HEADER_PARAM_TEST
    //for test each patch has different sao param
    printf("SAO:%d%d%d ", sh->slice_sao_enable[Y_C], sh->slice_sao_enable[U_C], sh->slice_sao_enable[V_C]);
#endif
    /* byte align */
    while (!COM_BSR_IS_BYTE_ALIGN(bs))
    {
        assert(com_bsr_read1(bs) == 1);
    }
    return COM_OK;
}

#if PATCH
int dec_eco_send(COM_BSR * bs)
{
    while (com_bsr_next(bs, 24) != 0x1)
    {
        com_bsr_read(bs, 8);
    }
    int ret = com_bsr_read(bs, 24);
    assert(ret == 1);
    ret = com_bsr_read(bs, 8);
    assert(ret == 0x8F);
    return COM_OK;
}
#endif

int dec_eco_udata(DEC_CTX * ctx, COM_BSR * bs)
{
    int    i;
    u32 code;
    /* should be aligned before adding user data */
    com_assert_rv(COM_BSR_IS_BYTE_ALIGN(bs), COM_ERR_UNKNOWN);
    code = com_bsr_read(bs, 8);
    while (code != COM_UD_END)
    {
        switch(code)
        {
        case COM_UD_PIC_SIGNATURE:
            /* read signature (HASH) from bitstream */
            for(i = 0; i < 16; i++)
            {
                ctx->pic_sign[i] = (u8)com_bsr_read(bs, 8);
                if (i % 2 == 1)
                {
                    u8 bit = com_bsr_read1(bs); //add one bit to prevent from start code, like a marker bit in SQH
                    assert(bit == 1);
            }
            }
            ctx->pic_sign_exist = 1;
            break;
        default:
            com_assert_rv(0, COM_ERR_UNEXPECTED);
        }
        code = com_bsr_read(bs, 8);
    }
    return COM_OK;
}

void dec_eco_affine_motion_derive(DEC_CTX *ctx, DEC_CORE *core, COM_BSR *bs, DEC_SBAC *sbac, int inter_dir, int cu_width, int cu_height, CPMV affine_mvp[VER_NUM][MV_D], s16 mvd[MV_D])
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    int vertex;
    int vertex_num = mod_info_curr->affine_flag + 1;
    /* forward */
    if (inter_dir == PRED_L0 || inter_dir == PRED_BI)
    {
        mod_info_curr->refi[REFP_0] = (s8)decode_refidx(bs, sbac, ctx->dpm.num_refp[REFP_0]);
        com_get_affine_mvp_scaling(&ctx->info, mod_info_curr, ctx->refp, &ctx->map, ctx->ptr, REFP_0, affine_mvp, vertex_num
#if BD_AFFINE_AMVR
                                   , mod_info_curr->mvr_idx
#endif
                                  );

        for (vertex = 0; vertex < vertex_num; vertex++)
        {
            decode_mvd(bs, sbac, mvd);
#if BD_AFFINE_AMVR
            u8 amvr_shift = Tab_Affine_AMVR(mod_info_curr->mvr_idx);
            s32 mv_x = (s32)affine_mvp[vertex][MV_X] + ((s32)mvd[MV_X] << amvr_shift);
            s32 mv_y = (s32)affine_mvp[vertex][MV_Y] + ((s32)mvd[MV_Y] << amvr_shift);
#else
            s32 mv_x = (s32)affine_mvp[vertex][MV_X] + (s32)mvd[MV_X];
            s32 mv_y = (s32)affine_mvp[vertex][MV_Y] + (s32)mvd[MV_Y];
#endif
            mod_info_curr->affine_mv[REFP_0][vertex][MV_X] = (CPMV)COM_CLIP3(COM_CPMV_MIN, COM_CPMV_MAX, mv_x);
            mod_info_curr->affine_mv[REFP_0][vertex][MV_Y] = (CPMV)COM_CLIP3(COM_CPMV_MIN, COM_CPMV_MAX, mv_y);
        }
    }
    else
    {
        mod_info_curr->refi[REFP_0] = REFI_INVALID;
        for (vertex = 0; vertex < vertex_num; vertex++)
        {
            mod_info_curr->affine_mv[REFP_0][vertex][MV_X] = 0;
            mod_info_curr->affine_mv[REFP_0][vertex][MV_Y] = 0;
        }
        mod_info_curr->refi[REFP_0] = REFI_INVALID;
        mod_info_curr->mv[REFP_0][MV_X] = 0;
        mod_info_curr->mv[REFP_0][MV_Y] = 0;
    }

    /* backward */
    if (inter_dir == PRED_L1 || inter_dir == PRED_BI)
    {
        mod_info_curr->refi[REFP_1] = (s8)decode_refidx(bs, sbac, ctx->dpm.num_refp[REFP_1]);
        com_get_affine_mvp_scaling(&ctx->info, mod_info_curr, ctx->refp, &ctx->map, ctx->ptr, REFP_1, affine_mvp, vertex_num
#if BD_AFFINE_AMVR
                                   , mod_info_curr->mvr_idx
#endif
                                  );

        for (vertex = 0; vertex < vertex_num; vertex++)
        {
            decode_mvd(bs, sbac, mvd);
#if BD_AFFINE_AMVR
            u8 amvr_shift = Tab_Affine_AMVR(mod_info_curr->mvr_idx);
            s32 mv_x = (s32)affine_mvp[vertex][MV_X] + ((s32)mvd[MV_X] << amvr_shift);
            s32 mv_y = (s32)affine_mvp[vertex][MV_Y] + ((s32)mvd[MV_Y] << amvr_shift);
#else
            s32 mv_x = (s32)affine_mvp[vertex][MV_X] + (s32)mvd[MV_X];
            s32 mv_y = (s32)affine_mvp[vertex][MV_Y] + (s32)mvd[MV_Y];
#endif
            mod_info_curr->affine_mv[REFP_1][vertex][MV_X] = (CPMV)COM_CLIP3(COM_CPMV_MIN, COM_CPMV_MAX, mv_x);
            mod_info_curr->affine_mv[REFP_1][vertex][MV_Y] = (CPMV)COM_CLIP3(COM_CPMV_MIN, COM_CPMV_MAX, mv_y);
        }
    }
    else
    {
        mod_info_curr->refi[REFP_1] = REFI_INVALID;
        for (vertex = 0; vertex < vertex_num; vertex++)
        {
            mod_info_curr->affine_mv[REFP_1][vertex][MV_X] = 0;
            mod_info_curr->affine_mv[REFP_1][vertex][MV_Y] = 0;
        }
        mod_info_curr->refi[REFP_1] = REFI_INVALID;
        mod_info_curr->mv[REFP_1][MV_X] = 0;
        mod_info_curr->mv[REFP_1][MV_Y] = 0;
    }
}

int dec_eco_DB_param(COM_BSR *bs, COM_PIC_HEADER *pic_header)
{
    pic_header->loop_filter_disable_flag = (u8)com_bsr_read1(bs);
    if (pic_header->loop_filter_disable_flag == 0)
    {
        pic_header->loop_filter_parameter_flag = (u8)com_bsr_read(bs, 1);
        if (pic_header->loop_filter_parameter_flag)
        {
            pic_header->alpha_c_offset = com_bsr_read_se(bs);
            pic_header->beta_offset = com_bsr_read_se(bs);
        }
        else
        {
            pic_header->alpha_c_offset = 0;
            pic_header->beta_offset = 0;
        }
    }
    return  COM_OK;
}

int dec_eco_sao_mergeflag(DEC_SBAC *sbac, COM_BSR *bs, int mergeleft_avail, int mergeup_avail)
{
    int MergeLeft = 0;
    int MergeUp = 0;
    int act_ctx = mergeleft_avail + mergeup_avail;
    int act_sym = 0;

    if (act_ctx == 1)
    {
        act_sym = dec_sbac_decode_bin(bs, sbac, &sbac->ctx.sao_merge_flag[0]);
    }
    else if (act_ctx == 2)
    {
        act_sym = dec_sbac_decode_bin(bs, sbac, &sbac->ctx.sao_merge_flag[1]);
        if (act_sym != 1)
        {
            act_sym += (dec_sbac_decode_bin(bs, sbac, &sbac->ctx.sao_merge_flag[2]) << 1);
        }
    }

    /* Get SaoMergeMode by SaoMergeTypeIndex */
    if (mergeleft_avail && !mergeup_avail)
    {
        return act_sym ? 2 : 0;
    }
    if (mergeup_avail && !mergeleft_avail)
    {
        return act_sym ? 1 : 0;
    }
    return act_sym ? 3 - act_sym : 0;
}

int dec_eco_sao_mode(DEC_SBAC *sbac, COM_BSR *bs)
{
    int  act_sym = 0;

    if (!dec_sbac_decode_bin(bs, sbac, sbac->ctx.sao_mode))
    {
        act_sym = 1 + !sbac_decode_bin_ep(bs, sbac);
    }
    return act_sym;
}

int dec_eco_sao_offset(DEC_SBAC *sbac, COM_BSR *bs, SAOBlkParam *saoBlkParam, int *offset)
{
    int i;
    assert(saoBlkParam->modeIdc == SAO_MODE_NEW);
    static const int EO_OFFSET_INV__MAP[] = { 1, 0, 2, -1, 3, 4, 5, 6 };

    for (i = 0; i < 4; i++)
    {
        int act_sym;
        if (saoBlkParam->typeIdc == SAO_TYPE_BO)
        {
            act_sym = !dec_sbac_decode_bin(bs, sbac, sbac->ctx.sao_offset);
            if (act_sym)
            {
                act_sym += sbac_read_truncate_unary_sym_ep(bs, sbac, saoclip[SAO_CLASS_BO][2]);

                if (sbac_decode_bin_ep(bs, sbac))
                {
                    act_sym = -act_sym;
                }
            }
        }
        else
        {
            int value2 = (i >= 2) ? (i + 1) : i;
            int maxvalue = saoclip[value2][2];
            int cnt = sbac_read_truncate_unary_sym_ep(bs, sbac, maxvalue + 1);

            if (value2 == SAO_CLASS_EO_FULL_VALLEY)
            {
                act_sym = EO_OFFSET_INV__MAP[cnt];
            }
            else if (value2 == SAO_CLASS_EO_FULL_PEAK)
            {
                act_sym = -EO_OFFSET_INV__MAP[cnt];
            }
            else if (value2 == SAO_CLASS_EO_HALF_PEAK)
            {
                act_sym = -cnt;
            }
            else
            {
                act_sym = cnt;
            }
        }
        offset[i] = act_sym;
        offset[i] = offset[i];
    }
    return 1;
}

int dec_eco_sao_EO_type(DEC_SBAC *sbac, COM_BSR *bs, SAOBlkParam *saoBlkParam)
{
    return (sbac_decode_bin_ep(bs, sbac) << 0) + (sbac_decode_bin_ep(bs, sbac) << 1);
}

void dec_eco_sao_BO_start(DEC_SBAC *sbac, COM_BSR *bs, SAOBlkParam *saoBlkParam, int stBnd[2])
{
    int act_sym = 0;
    int start = (sbac_decode_bin_ep(bs, sbac) << 0) +
                (sbac_decode_bin_ep(bs, sbac) << 1) +
                (sbac_decode_bin_ep(bs, sbac) << 2) +
                (sbac_decode_bin_ep(bs, sbac) << 3) +
                (sbac_decode_bin_ep(bs, sbac) << 4);

    int golomb_order = 1 + sbac_read_truncate_unary_sym_ep(bs, sbac, 4);

    act_sym = (1 << golomb_order) - 2;
    golomb_order %= 4;

    while (golomb_order--)
    {
        if (sbac_decode_bin_ep(bs, sbac))
        {
            act_sym += (1 << golomb_order);
        }
    }

    stBnd[0] = start;
    stBnd[1] = (start + 2 + act_sym) % 32;
}


int dec_eco_ALF_param(COM_BSR *bs, COM_PIC_HEADER *sh)
{
    ALFParam **m_alfPictureParam = sh->m_alfPictureParam;
    int *pic_alf_on = sh->pic_alf_on;
    int compIdx;

    pic_alf_on[Y_C] = com_bsr_read(bs, 1);
    pic_alf_on[U_C] = com_bsr_read(bs, 1);
    pic_alf_on[V_C] = com_bsr_read(bs, 1);
    m_alfPictureParam[Y_C]->alf_flag = pic_alf_on[Y_C];
    m_alfPictureParam[U_C]->alf_flag = pic_alf_on[U_C];
    m_alfPictureParam[V_C]->alf_flag = pic_alf_on[V_C];
    if (pic_alf_on[Y_C] || pic_alf_on[U_C] || pic_alf_on[V_C])
    {
        for (compIdx = 0; compIdx < N_C; compIdx++)
        {
            if (pic_alf_on[compIdx])
            {
                dec_eco_AlfCoeff(bs, m_alfPictureParam[compIdx]);
            }
        }
    }
    return COM_OK;
}

int dec_eco_AlfCoeff(COM_BSR * bs, ALFParam *Alfp)
{
    int pos;
    int f = 0, symbol, pre_symbole;
    const int numCoeff = (int)ALF_MAX_NUM_COEF;
    switch (Alfp->componentID)
    {
    case U_C:
    case V_C:
        for (pos = 0; pos < numCoeff; pos++)
        {
            Alfp->coeffmulti[0][pos] = com_bsr_read_se(bs);
        }
        break;
    case Y_C:
        Alfp->filters_per_group = com_bsr_read_ue(bs);
        Alfp->filters_per_group = Alfp->filters_per_group + 1;
        memset(Alfp->filterPattern, 0, NO_VAR_BINS * sizeof(int));
        pre_symbole = 0;
        symbol = 0;
        for (f = 0; f < Alfp->filters_per_group; f++)
        {
            if (f > 0)
            {
                if (Alfp->filters_per_group != 16)
                {
                    symbol = com_bsr_read_ue(bs);
                }
                else
                {
                    symbol = 1;
                }
                Alfp->filterPattern[symbol + pre_symbole] = 1;
                pre_symbole = symbol + pre_symbole;
            }
            for (pos = 0; pos < numCoeff; pos++)
            {
                Alfp->coeffmulti[f][pos] = com_bsr_read_se(bs);
            }
        }
        break;
    default:
        printf("Not a legal component ID\n");
        assert(0);
        exit(-1);
    }
    return COM_OK;
}

u32 dec_eco_AlfLCUCtrl(COM_BSR * bs, DEC_SBAC * sbac, int compIdx, int ctx_idx)
{
    return dec_sbac_decode_bin(bs, sbac, sbac->ctx.alf_lcu_enable);
}

int dec_eco_lcu_delta_qp(COM_BSR * bs, DEC_SBAC * sbac, int last_dqp)
{
    COM_SBAC_CTX * ctx = &sbac->ctx;
    int act_ctx;
    int act_sym;
    int dquant;

    act_ctx = ((last_dqp != 0) ? 1 : 0);
    act_sym = !dec_sbac_decode_bin(bs, sbac, ctx->delta_qp + act_ctx);

    if (act_sym != 0)
    {
        u32 bin;
        act_ctx = 2;

        do
        {
            bin = dec_sbac_decode_bin(bs, sbac, ctx->delta_qp + act_ctx);
            act_ctx = min(3, act_ctx + 1);
            act_sym += !bin;
        }
        while (!bin);
    }

    dquant = (act_sym + 1) >> 1;

    // lsb is signed bit
    if ((act_sym & 0x01) == 0)
    {
        dquant = -dquant;
    }

    return dquant;
}

#if EXTENSION_USER_DATA
int user_data(DEC_CTX * ctx, COM_BSR * bs)
{
    com_bsr_read(bs, 24);
    com_bsr_read(bs, 8);                            // user_data_start_code f(32)
    while (com_bsr_next(bs, 24) != 0x000001)
    {
#if WRITE_MD5_IN_USER_DATA
        /* parse user data: MD5*/
        dec_eco_udata(ctx, bs);
#endif
    }
    return 0;
}

int sequence_display_extension(COM_BSR * bs)
{
    com_bsr_read(bs, 3);                                            // video_format             u(3)
    com_bsr_read1(bs);                                              // sample_range             u(1)
    int colour_description = com_bsr_read1(bs);                     // colour_description       u(1)
    if (colour_description)
    {
        int colour_primaries = com_bsr_read(bs, 8);                // colour_primaries         u(8)
        int transfer_characteristics = com_bsr_read(bs, 8);        // transfer_characteristics u(8)
        com_bsr_read(bs, 8);                                       // matrix_coefficients      u(8)
    }
    com_bsr_read(bs, 14);                                           // display_horizontal_size  u(14)
    com_bsr_read1(bs);                                              // marker_bit               f(1)
    com_bsr_read(bs, 14);                                           // display_vertical_size    u(14)
    char td_mode_flag = com_bsr_read1(bs);                          // td_mode_flag             u(1)
    if (td_mode_flag == 1)
    {
        com_bsr_read(bs, 8);                                        // td_packing_mode          u(8)
        com_bsr_read1(bs);                                          // view_reverse_flag        u(1)
    }

    assert(com_bsr_read1(bs) == 1);
    while (!COM_BSR_IS_BYTE_ALIGN(bs))
    {
        assert(com_bsr_read1(bs) == 0);
    }
    while (com_bsr_next(bs, 24) != 0x1)
    {
        assert(com_bsr_read(bs, 8) == 0);
    }
    return 0;
}

int temporal_scalability_extension(COM_BSR * bs)
{
    int num_of_temporal_level_minus1 = com_bsr_read(bs, 3); // num_of_temporal_level_minus1 u(3)
    for (int i = 0; i < num_of_temporal_level_minus1; i++)
    {
        com_bsr_read(bs, 4);                                // temporal_frame_rate_code[i]  u(4)
        com_bsr_read(bs, 18);                               // temporal_bit_rate_lower[i]   u(18)
        com_bsr_read1(bs);                                  // marker_bit                   f(1)
        com_bsr_read(bs, 12);                               // temporal_bit_rate_upper[i]   u(12)
    }
    return 0;
}

int copyright_extension(COM_BSR * bs)
{
    com_bsr_read1(bs);              // copyright_flag       u(1)
    com_bsr_read(bs, 8);            // copyright_id         u(8)
    com_bsr_read1(bs);              // original_or_copy     u(1)
    com_bsr_read(bs, 7);            // reserved_bits        r(7)
    com_bsr_read1(bs);              //  marker_bit          f(1)
    com_bsr_read(bs, 20);           // copyright_number_1   u(20)
    com_bsr_read1(bs);              // marker_bit           f(1)
    com_bsr_read(bs, 22);           // copyright_number_2   u(22)
    com_bsr_read1(bs);              // marker_bit           f(1)
    com_bsr_read(bs, 22);           // copyright_number_3   u(22)
    return 0;
}

int mastering_display_and_content_metadata_extension(COM_BSR * bs)
{
    for (int c = 0; c < 3; c++)
    {
        com_bsr_read(bs, 16);       // display_primaries_x[c]           u(16)
        com_bsr_read1(bs);          // marker_bit                       f(1)
        com_bsr_read(bs, 16);       // display_primaries_y[c]           u(16)
        com_bsr_read1(bs);          // marker_bit                       f(1)
    }
    com_bsr_read(bs, 16);           // white_point_x                    u(16)
    com_bsr_read1(bs);              // marker_bit                       f(1)
    com_bsr_read(bs, 16);           // white_point_y                    u(16)
    com_bsr_read1(bs);              // marker_bit                       f(1)
    com_bsr_read(bs, 16);           // max_display_mastering_luminance  u(16)
    com_bsr_read1(bs);              // marker_bit                       f(1)
    com_bsr_read(bs, 16);           // min_display_mastering_luminance  u(16)
    com_bsr_read1(bs);              // marker_bit                       f(1)
    com_bsr_read(bs, 16);           // max_content_light_level          u(16)
    com_bsr_read1(bs);              // marker_bit                       f(1)
    com_bsr_read(bs, 16);           // max_picture_average_light_level  u(16)
    com_bsr_read1(bs);              // marker_bit                       f(1)
    com_bsr_read(bs, 16);           // reserved_bits                    r(16)
    return 0;
}

int camera_parameters_extension(COM_BSR * bs)
{
    com_bsr_read1(bs);              // reserved_bits            r(1)
    com_bsr_read(bs, 7);            // camera_id                u(7)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 22);           // height_of_image_device   u(22)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 22);           // focal_length             u(22)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 22);           // f_number                 u(22)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 22);           // vertical_angle_of_view   u(22)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 16);           // camera_position_x_upper  i(16)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 16);           // camera_position_x_lower  i(16)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 16);           // camera_position_y_upper  i(16)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 16);           // camera_position_y_lower  i(16)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 16);           // camera_position_z_upper  i(16)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 16);           // camera_position_z_lower  i(16)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 22);           // camera_direction_x       i(22)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 22);           // camera_direction_y       i(22)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 22);           // camera_direction_z       i(22)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 22);           // image_plane_vertical_x   i(22)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 22);           // image_plane_vertical_y   i(22)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 22);           // image_plane_vertical_z   i(22)
    com_bsr_read1(bs);              // marker_bit               f(1)
    com_bsr_read(bs, 16);           // reserved_bits            r(16)
    return 0;
}

#if CRR_EXTENSION_DATA
int cross_random_access_point_referencing_extension(COM_BSR * bs)
{
    unsigned int crr_lib_number;
    crr_lib_number = com_bsr_read(bs, 3);          // crr_lib_number           u(3)
    com_bsr_read1(bs);                             // marker_bit               f(1)
    unsigned int i = 0;
    while (i < crr_lib_number)
    {
        com_bsr_read(bs, 9);                       // camera_id                u(9)
        i++;
        if (i % 2 == 0)
        {
            com_bsr_read1(bs);                     // marker_bit               f(1)
        }
    }

    assert(com_bsr_read1(bs) == 1);
    while (!COM_BSR_IS_BYTE_ALIGN(bs))
    {
        assert(com_bsr_read1(bs) == 0);
    }
    while (com_bsr_next(bs, 24) != 0x1)
    {
        assert(com_bsr_read(bs, 8) == 0);
    }
    return 0;
}
#endif

int roi_parameters_extension(COM_BSR * bs, int slice_type)
{
    int current_picture_roi_num = com_bsr_read(bs, 8);  // current_picture_roi_num  u(8)
    int roiIndex = 0;
    if (slice_type != SLICE_I)
    {
        int PrevPictureROINum = com_bsr_read(bs, 8);    // prev_picture_roi_num     u(8)
        for (int i = 0; i < PrevPictureROINum; i++)
        {
            int roi_skip_run = com_bsr_read_ue(bs);
            if (roi_skip_run != 0)
            {
                for (int j = 0; j < roi_skip_run; j++)
                {
                    int skip_roi_mode = com_bsr_read1(bs);
                    if (j % 22 == 0)
                        com_bsr_read1(bs);              // marker_bit               f(1)
                }
                com_bsr_read1(bs);
            }
            else
            {
                com_bsr_read_se(bs);                    // roi_axisx_delta          se(v)
                com_bsr_read1(bs);                      // marker_bit               f(1)
                com_bsr_read_se(bs);                    // roi_axisy_delta          se(v)
                com_bsr_read1(bs);                      // marker_bit               f(1)
                com_bsr_read_se(bs);                    // roi_width_delta          se(v)
                com_bsr_read1(bs);                      // marker_bit               f(1)
                com_bsr_read_se(bs);                    // roi_height_delta         se(v)
                com_bsr_read1(bs);                      // marker_bit               f(1)

            }
        }
    }
    for (int i = roiIndex; i < current_picture_roi_num; i++)
    {
        com_bsr_read(bs, 6);                            // roi_axisx                u(6)
        com_bsr_read1(bs);                              // marker_bit               f(1)
        com_bsr_read(bs, 6);                            // roi_axisy                u(6)
        com_bsr_read1(bs);                              // marker_bit               f(1)
        com_bsr_read(bs, 6);                            // roi_width                u(6)
        com_bsr_read1(bs);                              // marker_bit               f(1)
        com_bsr_read(bs, 6);                            // roi_height               u(6)
        com_bsr_read1(bs);                              // marker_bit               f(1)

    }
    return 0;
}

int picture_display_extension( COM_BSR * bs, COM_SQH* sqh, COM_PIC_HEADER* pic_header )
{
    int num_frame_center_offsets;
    if( sqh->progressive_sequence == 1 )
    {
        if( pic_header->repeat_first_field == 1 )
        {
            num_frame_center_offsets = pic_header->top_field_first == 1 ? 3 : 2;
        }
        else
            num_frame_center_offsets = 1;
    }
    else
    {
        if( pic_header->picture_structure == 0 )
        {
            num_frame_center_offsets = 1;
        }
        else
        {
            num_frame_center_offsets = pic_header->repeat_first_field == 1 ? 3 : 2;
        }
    }

    for( int i = 0; i < num_frame_center_offsets; i++ )
    {
        com_bsr_read( bs, 16 );           // picture_centre_horizontal_offset i(16)
        com_bsr_read1( bs );              // marker_bit                       f(1)
        com_bsr_read( bs, 16 );           // picture_centre_vertical_offset   i(16)
        com_bsr_read1( bs );              // marker_bit                       f(1)
    }

    return 0;
}

int extension_data(COM_BSR * bs, int i, COM_SQH* sqh, COM_PIC_HEADER* pic_header)
{
    while (com_bsr_next(bs, 32) == 0x1B5)
    {
        com_bsr_read(bs, 32);        // extension_start_code            f(32)
        if (i == 0)
        {
            int ret = com_bsr_read(bs, 4);
            if (ret == 2)
            {
                sequence_display_extension(bs);
            }
            else if (ret == 3)
            {
                temporal_scalability_extension(bs);
            }
            else if (ret == 4)
            {
                copyright_extension(bs);
            }
            else if (ret == 0xa)
            {
                mastering_display_and_content_metadata_extension(bs);
            }
            else if (ret == 0xb)
            {
                camera_parameters_extension(bs);
            }
#if CRR_EXTENSION_DATA
            else if (ret == 0xd)
            {
                cross_random_access_point_referencing_extension(bs);
            }
#endif
            else
            {
                while (com_bsr_next(bs, 24) != 1)
                {
                    com_bsr_read(bs, 8);    // reserved_extension_data_byte u(8)
                }
            }
        }
        else
        {
            int ret = com_bsr_read(bs, 4);
            if (ret == 4)
            {
                copyright_extension(bs);
            }
            else if (ret == 7)
            {
                picture_display_extension(bs, sqh, pic_header);
            }
            else if (ret == 0xb)
            {
                camera_parameters_extension(bs);
            }
            else if (ret == 0xc)
            {
                roi_parameters_extension(bs, pic_header->slice_type);
            }
            else
            {
                while (com_bsr_next(bs, 24) != 1)
                {
                    com_bsr_read(bs, 8);    // reserved_extension_data_byte u(8)
                }
            }
        }
    }
    return 0;
}

int extension_and_user_data(DEC_CTX * ctx, COM_BSR * bs, int i, COM_SQH * sqh, COM_PIC_HEADER* pic_header)
{
    while ((com_bsr_next(bs, 32) == 0x1B5) || (com_bsr_next(bs, 32) == 0x1B2))
    {
        if (com_bsr_next(bs, 32) == 0x1B5)
        {
            extension_data(bs, i, sqh, pic_header);
        }
        if (com_bsr_next(bs, 32) == 0x1B2)
        {
            user_data(ctx, bs);
        }
    }
    return 0;
}
#endif