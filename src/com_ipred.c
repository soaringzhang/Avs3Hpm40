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

#if DT_PARTITION
void pred_recon_intra_luma_pb(COM_PIC *pic, u32 *map_scu, s8* map_ipm, int pic_width_in_scu, int pic_height_in_scu, s16 resi[N_C][MAX_CU_DIM], pel nb[N_C][N_REF][MAX_CU_SIZE * 3], COM_MODE *mod_info_curr,
                              int pb_x, int pb_y, int pb_w, int pb_h, int pb_scup, int pb_idx, int cu_x, int cu_y, int cu_width, int cu_height, int bit_depth)
{
    pel *rec, *pred_tb;
    int num_nz_temp[MAX_NUM_TB][N_C];
    int s_rec;
    int x_scu, y_scu, tb_x, tb_y, tb_w, tb_h, tb_scup;
    int pb_part_size = mod_info_curr->pb_part;
    u16 avail_cu;
    pel* resi_tb;
    int num_luma_in_prev_pb = 0;

    for (int i = 0; i < pb_idx; i++)
        num_luma_in_prev_pb += mod_info_curr->pb_info.sub_w[i] * mod_info_curr->pb_info.sub_h[i];

    int num_tb_in_pb = get_part_num_tb_in_pb(pb_part_size, pb_idx);
    int tb_idx_offset = get_tb_idx_offset(pb_part_size, pb_idx);
    get_tb_width_height_in_pb(pb_w, pb_h, pb_part_size, pb_idx, &tb_w, &tb_h);
    for (int tb_idx = 0; tb_idx < num_tb_in_pb; tb_idx++)
    {
        get_tb_pos_in_pb(pb_x, pb_y, pb_part_size, tb_w, tb_h, tb_idx, &tb_x, &tb_y);
        x_scu = tb_x >> 2;
        y_scu = tb_y >> 2;
        tb_scup = y_scu * pic_width_in_scu + x_scu;

        if (tb_idx == 0)
            assert(tb_scup == pb_scup);

        avail_cu = com_get_avail_intra(x_scu, y_scu, pic_width_in_scu, tb_scup, map_scu);
        /* prediction */
        s_rec = pic->stride_luma;
        rec = pic->y + (tb_y * s_rec) + tb_x;
        com_get_nbr(tb_x, tb_y, tb_w, tb_h, rec, s_rec, avail_cu, nb, tb_scup, map_scu, pic_width_in_scu, pic_height_in_scu, bit_depth, Y_C);

        pred_tb = mod_info_curr->pred[Y_C]; // pred is temp memory
        com_ipred(nb[0][0] + 3, nb[0][1] + 3, pred_tb, mod_info_curr->ipm[pb_idx][0], tb_w, tb_h, bit_depth, avail_cu, mod_info_curr->ipf_flag);
#if DT_INTRA_BOUNDARY_FILTER_OFF
        if (pb_part_size != SIZE_2Nx2N)
            assert(mod_info_curr->ipf_flag == 0);
#endif

        /* reconstruction */
        //get start of tb residual
        int coef_offset_tb = get_coef_offset_tb(cu_x, cu_y, tb_x, tb_y, cu_width, cu_height, mod_info_curr->tb_part);
        resi_tb = resi[Y_C] + coef_offset_tb; //residual is stored sequentially, not in raster
        assert((tb_w * tb_h) * tb_idx + num_luma_in_prev_pb == coef_offset_tb);
        //to fit the interface of com_recon()
        for( int comp = 0; comp < N_C; comp++ )
        {
            num_nz_temp[TB0][comp] = mod_info_curr->num_nz[tb_idx + tb_idx_offset][comp];
        }

        //here we treat a tb as one non-separable region; stride for resi_tb and pred are both tb_w; nnz shall be assigned to TB0
        com_recon(SIZE_2Nx2N, resi_tb, pred_tb, num_nz_temp, Y_C, tb_w, tb_h, s_rec, rec, bit_depth);

        //update map
        update_intra_info_map_scu(map_scu, map_ipm, tb_x, tb_y, tb_w, tb_h, pic_width_in_scu, mod_info_curr->ipm[pb_idx][0]);
    }
}
#endif


void com_get_nbr(int x, int y, int width, int height, pel *src, int s_src, u16 avail_cu, pel nb[N_C][N_REF][MAX_CU_SIZE * 3], int scup, u32 * map_scu, int pic_width_in_scu, int pic_height_in_scu, int bit_depth, int ch_type)
{
    int  i;
    int  width_in_scu  = (ch_type == Y_C) ? (width >> MIN_CU_LOG2)  : (width >> (MIN_CU_LOG2 - 1));
    int  height_in_scu = (ch_type == Y_C) ? (height >> MIN_CU_LOG2) : (height >> (MIN_CU_LOG2 - 1));
    int  unit_size = (ch_type == Y_C) ? MIN_CU_SIZE : (MIN_CU_SIZE >> 1);
    int  x_scu = PEL2SCU(ch_type == Y_C ? x : x << 1);
    int  y_scu = PEL2SCU(ch_type == Y_C ? y : y << 1);
    pel *const src_bak = src;
    pel *left = nb[ch_type][0] + 3;
    pel *up   = nb[ch_type][1] + 3;
    int pad_le = height;  //number of padding pixel in the left column
    int pad_up = width;   //number of padding pixel in the upper row
    int pad_le_in_scu = height_in_scu;
    int pad_up_in_scu = width_in_scu;

    com_mset_16b(left - 3, 1 << (bit_depth - 1), height + pad_le + 3);
    com_mset_16b(up   - 3, 1 << (bit_depth - 1), width  + pad_up + 3);
    if(IS_AVAIL(avail_cu, AVAIL_UP))
    {
        com_mcpy(up, src - s_src, width * sizeof(pel));
        for(i = 0; i < pad_up_in_scu; i++)
        {
            if(x_scu + width_in_scu + i < pic_width_in_scu && MCU_GET_CODED_FLAG(map_scu[scup - pic_width_in_scu + width_in_scu + i]))
            {
                com_mcpy(up + width + i * unit_size, src - s_src + width + i * unit_size, unit_size * sizeof(pel));
            }
            else
            {
                com_mset_16b(up + width + i * unit_size, up[width + i * unit_size - 1], unit_size);
            }
        }
    }

    if(IS_AVAIL(avail_cu, AVAIL_LE))
    {
        src--;
        for(i = 0; i < height; ++i)
        {
            left[i] = *src;
            src += s_src;
        }
        for(i = 0; i < pad_le_in_scu; i++)
        {
            if(y_scu + height_in_scu + i < pic_height_in_scu && MCU_GET_CODED_FLAG(map_scu[scup - 1 + (height_in_scu + i) *pic_width_in_scu]))
            {
                int j;
                for(j = 0; j < unit_size; ++j)
                {
                    left[height + i * unit_size + j] = *src;
                    src += s_src;
                }
            }
            else
            {
                com_mset_16b(left + height + i * unit_size, left[height + i * unit_size - 1], unit_size);
                src += (s_src * unit_size);
            }
        }
    }

    if (IS_AVAIL(avail_cu, AVAIL_UP_LE))
    {
        up[-1] = left[-1] = src_bak[-s_src - 1];
    }
    else if (IS_AVAIL(avail_cu, AVAIL_UP))
    {
        up[-1] = left[-1] = up[0];
    }
    else if (IS_AVAIL(avail_cu, AVAIL_LE))
    {
        up[-1] = left[-1] = left[0];
    }

    up[-2] = left[0];
    left[-2] = up[0];
    up[-3] = left[1];
    left[-3] = up[1];
}

void ipred_hor(pel *src_le, pel *dst, int w, int h)
{
    int i, j;
    {
        for(i = 0; i < h; i++)
        {
            for(j = 0; j < w; j++)
            {
                dst[j] = src_le[0];
            }
            dst += w;
            src_le++;
        }
    }
}

void ipred_vert(pel *src_up, pel *dst, int w, int h)
{
    int i, j;
    for(i = 0; i < h; i++)
    {
        for(j = 0; j < w; j++)
        {
            dst[j] = src_up[j];
        }
        dst += w;
    }
}

void ipred_dc(pel *src_le, pel *src_up, pel *dst, int w, int h, int bit_depth, u16 avail_cu)
{
    assert(com_tbl_log2[w] >= 2);
    assert(com_tbl_log2[h] >= 2);

    int dc = 0;
    int wh, i, j;
    if(IS_AVAIL(avail_cu, AVAIL_LE))
    {
        for (i = 0; i < h; i++)
        {
            dc += src_le[i];
        }
        if(IS_AVAIL(avail_cu, AVAIL_UP))
        {
            for (j = 0; j < w; j++)
            {
                dc += src_up[j];
            }
            dc = (dc + ((w + h) >> 1)) * (4096 / (w + h)) >> 12;
        }
        else
        {
            dc = (dc + (h >> 1)) >> com_tbl_log2[h];
        }
    }
    else if(IS_AVAIL(avail_cu, AVAIL_UP))
    {
        for (j = 0; j < w; j++)
        {
            dc += src_up[j];
        }
        dc = (dc + (w >> 1)) >> com_tbl_log2[w];
    }
    else
    {
        dc = 1 << (bit_depth - 1);
    }

    wh = w * h;
    for(i = 0; i < wh; i++)
    {
        dst[i] = (pel)dc;
    }
}

void ipred_plane(pel *src_le, pel *src_up, pel *dst, int w, int h)
{
    assert(com_tbl_log2[w] >= 2);
    assert(com_tbl_log2[h] >= 2);

    pel *rsrc;
    int  coef_h = 0, coef_v = 0;
    int  a, b, c, x, y;
    int  w2 = w >> 1;
    int  h2 = h >> 1;
    int  ib_mult[5]  = { 13, 17, 5, 11, 23 };
    int  ib_shift[5] = { 7, 10, 11, 15, 19 };
    int  idx_w = com_tbl_log2[w] - 2;
    int  idx_h = com_tbl_log2[h] - 2;
    int  im_h, is_h, im_v, is_v, temp, temp2;
    im_h = ib_mult[idx_w];
    is_h = ib_shift[idx_w];
    im_v = ib_mult[idx_h];
    is_v = ib_shift[idx_h];
    rsrc = src_up + (w2 - 1);
    for (x = 1; x < w2 + 1; x++)
    {
        coef_h += x * (rsrc[x] - rsrc[-x]);
    }
    rsrc = src_le + (h2 - 1);
    for (y = 1; y < h2 + 1; y++)
    {
        coef_v += y * (rsrc[y] - rsrc[-y]);
    }
    a = (src_le[h - 1] + src_up[w - 1]) << 4;
    b = ((coef_h << 5) * im_h + (1 << (is_h - 1))) >> is_h;
    c = ((coef_v << 5) * im_v + (1 << (is_v - 1))) >> is_v;
    temp = a - (h2 - 1) * c - (w2 - 1) * b + 16;
    for (y = 0; y < h; y++)
    {
        temp2 = temp;
        for (x = 0; x < w; x++)
        {
            dst[x] = (pel)(temp2 >> 5);
            temp2 += b;
        }
        temp += c;
        dst += w;
    }
}

void ipred_bi(pel *src_le, pel *src_up, pel *dst, int w, int h)
{
    assert(com_tbl_log2[w] >= 2);
    assert(com_tbl_log2[h] >= 2);

    int x, y;
    int ishift_x = com_tbl_log2[w];
    int ishift_y = com_tbl_log2[h];
    int ishift = COM_MIN(ishift_x, ishift_y);
    int ishift_xy = ishift_x + ishift_y + 1;
    int offset = 1 << (ishift_x + ishift_y);
    int a, b, c, wt, wxy, tmp;
    int predx;
    int ref_up[MAX_CU_SIZE], ref_le[MAX_CU_SIZE], up[MAX_CU_SIZE], le[MAX_CU_SIZE], wy[MAX_CU_SIZE];
    int wc, tbl_wc[6] = {-1, 21, 13, 7, 4, 2};
    wc = ishift_x > ishift_y ? ishift_x - ishift_y : ishift_y - ishift_x;
    com_assert(wc <= 5);

    wc = tbl_wc[wc];
    for( x = 0; x < w; x++ )
    {
        ref_up[x] = src_up[x];
    }
    for( y = 0; y < h; y++ )
    {
        ref_le[y] = src_le[y];
    }

    a = src_up[w - 1];
    b = src_le[h - 1];
    c = (w == h) ? (a + b + 1) >> 1 : (((a << ishift_x) + (b << ishift_y)) * wc + (1 << (ishift + 5))) >> (ishift + 6);
    wt = (c << 1) - a - b;
    for( x = 0; x < w; x++ )
    {
        up[x] = b - ref_up[x];
        ref_up[x] <<= ishift_y;
    }
    tmp = 0;
    for( y = 0; y < h; y++ )
    {
        le[y] = a - ref_le[y];
        ref_le[y] <<= ishift_x;
        wy[y] = tmp;
        tmp += wt;
    }
    for( y = 0; y < h; y++ )
    {
        predx = ref_le[y];
        wxy = 0;
        for( x = 0; x < w; x++ )
        {
            predx += le[y];
            ref_up[x] += up[x];
            dst[x] = (pel)(((predx << ishift_y) + (ref_up[x] << ishift_x) + wxy + offset) >> ishift_xy);
            wxy += wy[y];
        }
        dst += w;
    }
}

#if TSCPM
void downSampleCbCrPixels(int uiCWidth, int uiCHeight, int bitDept,
                          pel *pSrc, int uiSrcStride,
                          pel *pDst, int uiDstStride)
{
    int maxResult = (1 << bitDept) - 1;
    int tempValue;

    for (int j = 0; j < uiCHeight; j++)
    {
        for (int i = 0; i < uiCWidth; i++)
        {
            if (i == 0)
            {
                tempValue = (pSrc[2 * i] + pSrc[2 * i + uiSrcStride] + 1) >> 1;
            }
            else
            {
                tempValue = (pSrc[2 * i] * 2 + pSrc[2 * i + 1] + pSrc[2 * i - 1]
                             + pSrc[2 * i + uiSrcStride] * 2
                             + pSrc[2 * i + uiSrcStride + 1]
                             + pSrc[2 * i + uiSrcStride - 1]
                             + 4) >> 3;
            }

            if (tempValue > maxResult || tempValue < 0)
            {
                printf("\n TSCPM clip error");
            }
            pDst[i] = tempValue;
        }
        pDst += uiDstStride;
        pSrc += uiSrcStride * 2;
    }
}

pel xGetLumaBorderPixel(int uiIdx, int bAbovePixel, int uiCWidth, int uiCHeight, int bAboveAvail, int bLeftAvail, pel nb[N_C][N_REF][MAX_CU_SIZE * 3])
{
    pel *pSrc = NULL;
    pel dstPoint = -1;
    // Simplify Version, only copy rec luma.
    if (bAbovePixel)
    {
        if (bAboveAvail)
        {
            pSrc = nb[Y_C][1] + 3;
            int i = uiIdx;
            if (i < uiCWidth)
            {
                if (i == 0 && !bLeftAvail)
                {
                    dstPoint = (3 * pSrc[2 * i] + pSrc[2 * i + 1] + 2) >> 2;
                }
                else
                {
                    dstPoint = (2 * pSrc[2 * i] + pSrc[2 * i - 1] + pSrc[2 * i + 1] + 2) >> 2;
                }
            }
        }
    }
    else
    {
        if (bLeftAvail)
        {
            pSrc = nb[Y_C][0] + 3;
            int j = uiIdx;
            if (j < uiCHeight)
            {
                dstPoint = (pSrc[2 * j] + pSrc[2 * j  + 1] + 1) >> 1;
            }
        }
    }

    if (dstPoint < 0)
    {
        printf("\n Error get dstPoint in xGetLumaBorderPixel");
    }
    return dstPoint;
}

#define xGetSrcPixel(idx, bAbovePixel)  xGetLumaBorderPixel((idx), (bAbovePixel), uiCWidth, uiCHeight, bAboveAvaillable, bLeftAvaillable, nb)
#define xExchange(a, b, type) {type exTmp; exTmp = (a); (a) = (b); (b) = exTmp;}

void xSimpleGetTSCPMParameters(int compID, int *a, int *b, int *iShift, int bAbove, int bLeft, int uiCWidth, int uiCHeight, int bitDept, pel nb[N_C][N_REF][MAX_CU_SIZE * 3])
{
    int bAboveAvaillable = bAbove;
    int bLeftAvaillable = bLeft;

    pel *pCur = NULL;
    pel *pCurLeft = NULL;
    pel *pCurAbove = NULL;
    pCurLeft  = nb[compID][0] + 3;
    pCurAbove = nb[compID][1] + 3;

    unsigned uiInternalBitDepth = bitDept;

    int minDim = bLeftAvaillable && bAboveAvaillable ? min(uiCHeight, uiCWidth) : (bLeftAvaillable ? uiCHeight : uiCWidth);
    int numSteps = minDim;
    int yMax = 0;
    int xMax = -MAX_INT;
    int yMin = 0;
    int xMin = MAX_INT;

    // four points start
    int iRefPointLuma[4] = { -1, -1, -1, -1 };
    int iRefPointChroma[4] = { -1, -1, -1, -1 };
    int yMax_sec = 0;
    int xMax_sec = -MAX_INT;
    int yMin_sec = 0;
    int xMin_sec = MAX_INT;
    if (bAboveAvaillable)
    {
        pCur = pCurAbove;
        int idx = ((numSteps - 1) * uiCWidth) / minDim;
        iRefPointLuma[0] = xGetSrcPixel(0, 1); // pSrc[0];
        iRefPointChroma[0] = pCur[0];
        iRefPointLuma[1] = xGetSrcPixel(idx, 1); // pSrc[idx];
        iRefPointChroma[1] = pCur[idx];
        // using 4 points when only one border
        if (!bLeftAvaillable && uiCWidth >= 4)
        {
            int uiStep = uiCWidth >> 2;
            for (int i = 0; i < 4; i++)
            {
                iRefPointLuma[i] = xGetSrcPixel(i * uiStep, 1); // pSrc[i * uiStep];
                iRefPointChroma[i] = pCur[i * uiStep];
            }
        }
    }
    if (bLeftAvaillable)
    {
        pCur = pCurLeft;
        int idx = ((numSteps - 1) * uiCHeight) / minDim;
        iRefPointLuma[2] = xGetSrcPixel(0, 0); // pSrc[0];
        iRefPointChroma[2] = pCur[0];
        iRefPointLuma[3] = xGetSrcPixel(idx, 0); // pSrc[idx * iSrcStride];
        iRefPointChroma[3] = pCur[idx];
        // using 4 points when only one border
        if (!bAboveAvaillable && uiCHeight >= 4)
        {
            int uiStep = uiCHeight >> 2;
            for (int i = 0; i < 4; i++)
            {
                iRefPointLuma[i] = xGetSrcPixel(i * uiStep, 0); // pSrc[i * uiStep * iSrcStride];
                iRefPointChroma[i] = pCur[i * uiStep];
            }
        }
    }

    if ((bAboveAvaillable &&  bLeftAvaillable)
            || (bAboveAvaillable && !bLeftAvaillable  && uiCWidth >= 4)
            || (bLeftAvaillable && !bAboveAvaillable && uiCHeight >= 4) )
    {
        int minGrpIdx[2] = { 0, 2 };
        int maxGrpIdx[2] = { 1, 3 };
        int *tmpMinGrp = minGrpIdx;
        int *tmpMaxGrp = maxGrpIdx;
        if (iRefPointLuma[tmpMinGrp[0]] > iRefPointLuma[tmpMinGrp[1]]) xExchange(tmpMinGrp[0], tmpMinGrp[1], int);
        if (iRefPointLuma[tmpMaxGrp[0]] > iRefPointLuma[tmpMaxGrp[1]]) xExchange(tmpMaxGrp[0], tmpMaxGrp[1], int);
        if (iRefPointLuma[tmpMinGrp[0]] > iRefPointLuma[tmpMaxGrp[1]]) xExchange(tmpMinGrp,    tmpMaxGrp,    int *);
        if (iRefPointLuma[tmpMinGrp[1]] > iRefPointLuma[tmpMaxGrp[0]]) xExchange(tmpMinGrp[1], tmpMaxGrp[0], int);

        assert(iRefPointLuma[tmpMaxGrp[0]] >= iRefPointLuma[tmpMinGrp[0]]);
        assert(iRefPointLuma[tmpMaxGrp[0]] >= iRefPointLuma[tmpMinGrp[1]]);
        assert(iRefPointLuma[tmpMaxGrp[1]] >= iRefPointLuma[tmpMinGrp[0]]);
        assert(iRefPointLuma[tmpMaxGrp[1]] >= iRefPointLuma[tmpMinGrp[1]]);

        xMin = (iRefPointLuma  [tmpMinGrp[0]] + iRefPointLuma  [tmpMinGrp[1]] + 1 )>> 1;
        yMin = (iRefPointChroma[tmpMinGrp[0]] + iRefPointChroma[tmpMinGrp[1]] + 1) >> 1;
        
        xMax = (iRefPointLuma  [tmpMaxGrp[0]] + iRefPointLuma  [tmpMaxGrp[1]] + 1 )>> 1;
        yMax = (iRefPointChroma[tmpMaxGrp[0]] + iRefPointChroma[tmpMaxGrp[1]] + 1) >> 1;
    }
    else if (bAboveAvaillable)
    {
        for (int k = 0; k < 2; k++)
        {
            if (iRefPointLuma[k] > xMax)
            {
                xMax = iRefPointLuma[k];
                yMax = iRefPointChroma[k];
            }
            if (iRefPointLuma[k] < xMin)
            {
                xMin = iRefPointLuma[k];
                yMin = iRefPointChroma[k];
            }
        }
    }
    else if (bLeftAvaillable)
    {
        for (int k = 2; k < 4; k++)
        {
            if (iRefPointLuma[k] > xMax)
            {
                xMax = iRefPointLuma[k];
                yMax = iRefPointChroma[k];
            }
            if (iRefPointLuma[k] < xMin)
            {
                xMin = iRefPointLuma[k];
                yMin = iRefPointChroma[k];
            }
        }
    }
    // four points end

    if (bLeftAvaillable || bAboveAvaillable)
    {
        *a = 0;
        *iShift = 16;
        int diff = xMax - xMin;
        int add = 0;
        int shift = 0;
        if (diff > 64)
        {
            shift = (uiInternalBitDepth > 8) ? uiInternalBitDepth - 6 : 2;
            add = shift ? 1 << (shift - 1) : 0;
            diff = (diff + add) >> shift;

            if (uiInternalBitDepth == 10)
            {
                assert(shift == 4 && add == 8); // for default 10bit
            }
        }

        if (diff > 0)
        {
            *a = ((yMax - yMin) * g_aiTscpmDivTable64[diff - 1] + add) >> shift;
        }
        *b = yMin - (((s64)(*a) * xMin) >> (*iShift));
    }
    if (!bLeftAvaillable && !bAboveAvaillable)
    {
        *a = 0;
        *b = 1 << (uiInternalBitDepth - 1);
        *iShift = 0;
        return;
    }
}

void LinearTransformTSCPM(pel *pSrc, int iSrcStride, pel *pDst, int iDstStride, int a, int iShift, int b, int uiWidth, int uiHeight, int bitDept)
{
    int maxResult = (1 << bitDept) - 1;

    for (int j = 0; j < uiHeight; j++)
    {
        for (int i = 0; i < uiWidth; i++)
        {
            int tempValue = (((s64)a * pSrc[i]) >> (iShift >= 0 ? iShift : 0)) + b;
            pDst[i] = COM_CLIP3(0, maxResult, tempValue);
        }
        pDst += iDstStride;
        pSrc += iSrcStride;
    }
}

void ipred_tscpm(int compID, pel *piPred, pel *piRecoY, int uiStrideY,int uiWidth, int uiHeight, int bAbove, int bLeft, int bitDept, pel nb[N_C][N_REF][MAX_CU_SIZE * 3])
{
    int a, b, shift;   // parameters of Linear Model : a, b, shift
    xSimpleGetTSCPMParameters(compID, &a, &b, &shift, bAbove, bLeft, uiWidth, uiHeight, bitDept, nb);

    pel *lumaRec = piRecoY;
    int lumaRecStride = uiStrideY;
    pel upCbCrRec[MAX_CU_SIZE * MAX_CU_SIZE];
    int upCbCrStride = MAX_CU_SIZE;
    LinearTransformTSCPM(lumaRec, lumaRecStride, upCbCrRec, upCbCrStride,
                         a, shift, b, (uiWidth << 1), (uiHeight << 1), bitDept);

    int uiStride = uiWidth;
    downSampleCbCrPixels(uiWidth, uiHeight, bitDept, upCbCrRec, upCbCrStride, piPred, uiStride);
}
#endif

#define GET_REF_POS(mt,d_in,d_out,offset) \
    (d_out) = ((d_in) * (mt)) >> 10;\
    (offset) = ((((d_in) * (mt)) << 5) >> 10) - ((d_out) << 5);

#define ADI_4T_FILTER_BITS                 7
#define ADI_4T_FILTER_OFFSET              (1<<(ADI_4T_FILTER_BITS-1))


__inline
pel ipred_ang_val(pel * src_up, pel * src_le, int ipm, int i, int j, int w, int h)
{
    int offset, offset_x, offset_y;
    int t_dx, t_dy, xx, xy, yx, yy;
    int x, y, xn, yn, xn_n1, yn_n1, xn_p2, yn_p2;
    const int dxy = (ipm > IPD_HOR || ipm < IPD_VER) ? -1 : 1;
    const s16 * filter;
    const s16(*tbl_filt)[4];
    int filter_offset, filter_bits;
    const int * mt = com_tbl_ipred_dxdy[ipm];
    pel * src_ch = NULL;
    int num_selections = 0;
    int use_x = -1;
    int p, pn, pn_n1, pn_p2;
    int pos_max = w + h - 1;
    pel temp_pel = 0;
    x = INT_MAX;
    xn = INT_MAX;
    xn_n1 = INT_MAX;
    xn_p2 = INT_MAX;
    y = INT_MAX;
    yn = INT_MAX;
    yn_n1 = INT_MAX;
    yn_p2 = INT_MAX;
    xx = INT_MAX;
    yy = INT_MAX;
    filter_offset = ADI_4T_FILTER_OFFSET;
    filter_bits   = ADI_4T_FILTER_BITS;
    tbl_filt      = com_tbl_ipred_adi;
    com_assert(ipm < IPD_CNT);

    if (ipm == IPD_VER)
    {
        return src_up[i];
    }
    else if (ipm == IPD_HOR)
    {
        return src_le[j];
    }

    if (ipm < IPD_VER)
    {
        /* case x-line */
        GET_REF_POS(mt[0], j + 1, t_dx, offset);
        x = i + t_dx;
        y = -1;
    }
    else if (ipm > IPD_HOR)
    {
        GET_REF_POS(mt[1], i + 1, t_dy, offset);
        x = -1;
        y = j + t_dy;
    }
    else
    {
        GET_REF_POS(mt[0], j + 1, t_dx, offset_x);
        t_dx = -t_dx;
        xx = i + t_dx;
        yx = -1;
        GET_REF_POS(mt[1], i + 1, t_dy, offset_y);
        t_dy = -t_dy;
        xy = -1;
        yy = j + t_dy;
        if (yy <= -1)
        {
            x = xx;
            y = yx;
            offset = offset_x;
        }
        else
        {
            x = xy;
            y = yy;
            offset = offset_y;
        }
    }
    com_assert(x != INT_MAX);
    com_assert(y != INT_MAX);
    if (y == -1)
    {
        if (dxy < 0)
        {
            xn_n1 = x - 1;
            xn    = x + 1;
            xn_p2 = x + 2;
        }
        else
        {
            xn_n1 = x + 1;
            xn    = x - 1;
            xn_p2 = x - 2;
        }
        use_x = 1;
        ++num_selections;
        src_ch = src_up;
        pos_max = w * 2 - 1;
    }
    else if (x == -1)
    {
        if (dxy < 0)
        {
            yn_n1 = y - 1;
            yn    = y + 1;
            yn_p2 = y + 2;
        }
        else
        {
            yn_n1 = y + 1;
            yn    = y - 1;
            yn_p2 = y - 2;
        }
        use_x = 0;
        ++num_selections;
        src_ch = src_le;
        pos_max = h * 2 - 1;
    }
    com_assert(num_selections == 1);
    com_assert(src_ch != NULL);
    if (use_x == 1)
    {
        pn_n1 = xn_n1;
        p     = x;
        pn    = xn;
        pn_p2 = xn_p2;
    }
    else
    {
        pn_n1 = yn_n1;
        p     = y;
        pn    = yn;
        pn_p2 = yn_p2;
    }

    pn_n1 = COM_MIN(pn_n1, pos_max);
    p     = COM_MIN(p,     pos_max);
    pn    = COM_MIN(pn,    pos_max);
    pn_p2 = COM_MIN(pn_p2, pos_max);
    filter = (tbl_filt + offset)[0];
    temp_pel = (pel)((src_ch[pn_n1] * filter[0] + src_ch[p] * filter[1] + src_ch[pn] * filter[2] + src_ch[pn_p2] * filter[3] + filter_offset) >> filter_bits);
    return temp_pel;
}

void ipred_ang(pel *src_le, pel *src_up, pel *dst, int w, int h, int ipm)
{
    int i, j;
    for(j = 0; j < h; j++)
    {
        for(i = 0; i < w; i++)
        {
            dst[i] = ipred_ang_val(src_up, src_le, ipm, i, j, w, h);
        }
        dst += w;
    }
}

static const s32 g_ipf_pred_param[5][10] =
{
    { 24,  6,  2,  0,  0,  0,  0,  0,  0,  0 }, //4x4, 24, 0.5
    { 44, 25, 14,  8,  4,  2,  1,  1,  0,  0 }, //8x8, 44-1.2
    { 40, 27, 19, 13,  9,  6,  4,  3,  2,  1 }, //16x16, 40-1.8
    { 36, 27, 21, 16, 12,  9,  7,  5,  4,  3 }, //32x32, 36-2.5
    { 52, 44, 37, 31, 26, 22, 18, 15, 13, 11 }, //64x64
};

void ipf_core(pel *src_le, pel *src_up, pel *dst, int ipm, int w, int h)
{
    com_assert((MIN_CU_SIZE <= w) && (MIN_CU_SIZE <= h));
    com_assert(ipm < IPD_CNT);
    assert(com_tbl_log2[w] >= 2);
    assert(com_tbl_log2[h] >= 2);

    s32 filter_idx_hor = (s32)com_tbl_log2[w] - 2; //Block Size
    s32 filter_idx_ver = (s32)com_tbl_log2[h] - 2; //Block Size
    const s32 filter_range = 10;
    s32 ver_filter_range = filter_range;
    s32 hor_filter_range = filter_range;

    // TODO: g_ipf_pred_param doesn't support 128
    if (filter_idx_hor > 4)
    {
        filter_idx_hor = 4;
        hor_filter_range = 0; // don't use IPF at horizontal direction
    }
    if (filter_idx_ver > 4)
    {
        filter_idx_ver = 4;
        ver_filter_range = 0; // don't use IPF at vertical direction
    }

    const s32 *filter_hori_param = g_ipf_pred_param[filter_idx_hor];
    const s32 *filter_vert_param = g_ipf_pred_param[filter_idx_ver];
    const s32 par_shift = 6; //normalization factor
    const s32 par_scale = 1 << par_shift;
    const s32 par_offset = 1 << (par_shift - 1);

    if (IPD_DIA_L <= ipm && ipm <= IPD_DIA_R)
    {
        // vertical mode use left reference pixels, don't use top reference
        ver_filter_range = 0;
    }
    if (IPD_DIA_R < ipm)
    {
        // horizontal mode use top reference pixels, don't use left reference
        hor_filter_range = 0;
    }

    s32 p_ref_lenth = w + h;
    s32 *p_ref_vector = com_malloc(p_ref_lenth * sizeof(s32));
    com_mset(p_ref_vector, 0, (w + h) * sizeof(s32));
    s32 *p_ref_vector_h = p_ref_vector + h;
    for( s32 i = 0; i < w; ++i )
    {
        p_ref_vector_h[i] = src_up[i];
    }
    for( s32 i = 1; i <= h; ++i )
    {
        p_ref_vector_h[-i] = src_le[i - 1];
    }

    for (s32 row = 0; row < h; ++row)
    {
        s32 pos = row * w;
        s32 coeff_top = (row < ver_filter_range) ? filter_vert_param[row] : 0;
        for (s32 col = 0; col < w; col++, pos++)
        {
            s32 coeff_left = (col < hor_filter_range) ? filter_hori_param[col] : 0;
            s32 coeff_cur = par_scale - coeff_left - coeff_top;
            s32 sample_val = (coeff_left* p_ref_vector_h[-row - 1] + coeff_top * p_ref_vector_h[col] + coeff_cur * dst[pos] + par_offset) >> par_shift;
            dst[pos] = sample_val;
        }
    }

    // Release memory
    com_mfree(p_ref_vector);
}

void clip_pred(pel *dst, const int w, const int h, int bit_depth)
{
    com_assert(NULL != dst);
    for( int i = 0; i < h; i++ )
    {
        for( int j = 0; j < w; j++ )
        {
            dst[i * w + j] = COM_CLIP3(0, ((1 << bit_depth) - 1), dst[i * w + j]);
        }
    }
}

void com_ipred(pel *src_le, pel *src_up, pel *dst, int ipm, int w, int h, int bit_depth, u16 avail_cu, u8 ipf_flag)
{
    assert(w <= 64 && h <= 64);

    switch(ipm)
    {
    case IPD_VER:
        ipred_vert(src_up, dst, w, h);
        break;
    case IPD_HOR:
        ipred_hor(src_le, dst, w, h);
        break;
    case IPD_DC:
        ipred_dc(src_le, src_up, dst, w, h, bit_depth, avail_cu);
        break;
    case IPD_PLN:
        ipred_plane(src_le, src_up, dst, w, h);
        break;
    case IPD_BI:
        ipred_bi(src_le, src_up, dst, w, h);
        break;
    default:
        ipred_ang(src_le, src_up, dst, w, h, ipm);
        break;
    }

    if( ipf_flag )
    {
        assert((w < MAX_CU_SIZE) && (h < MAX_CU_SIZE));
        ipf_core(src_le, src_up, dst, ipm, w, h);
    }

    // Clip predicted value
    if( ipf_flag || (ipm == IPD_BI) || (ipm == IPD_PLN) )
    {
        clip_pred(dst, w, h, bit_depth);
    }
}

void com_ipred_uv(pel *src_le, pel *src_up, pel *dst, int ipm_c, int ipm, int w, int h, int bit_depth, u16 avail_cu
#if TSCPM
                  ,int chType,  pel *piRecoY, int uiStrideY, pel nb[N_C][N_REF][MAX_CU_SIZE * 3]
#endif
                 )
{
    assert(w <= 64 && h <= 64);
#if CHROMA_NOT_SPLIT
    assert(w >= 4 && h >= 4);
#endif
#if TSCPM
    int bAbove = IS_AVAIL(avail_cu, AVAIL_UP);
    int bLeft = IS_AVAIL(avail_cu, AVAIL_LE);
#endif
    if(ipm_c == IPD_DM_C && COM_IPRED_CHK_CONV(ipm))
    {
        ipm_c = COM_IPRED_CONV_L2C(ipm);
    }
    switch(ipm_c)
    {
    case IPD_DM_C:
        switch(ipm)
        {
        case IPD_PLN:
            ipred_plane(src_le, src_up, dst, w, h);
            break;
        default:
            ipred_ang(src_le, src_up, dst, w, h, ipm);
            break;
        }
        // Clip
        clip_pred(dst, w, h, bit_depth);
        break;
    case IPD_DC_C:
        ipred_dc(src_le, src_up, dst, w, h, bit_depth, avail_cu);
        break;
    case IPD_HOR_C:
        ipred_hor(src_le, dst, w, h);
        break;
    case IPD_VER_C:
        ipred_vert(src_up, dst, w, h);
        break;
    case IPD_BI_C:
        ipred_bi(src_le, src_up, dst, w, h);
        // Clip
        clip_pred(dst, w, h, bit_depth);
        break;
#if TSCPM
    case IPD_TSCPM_C:
        ipred_tscpm(chType, dst, piRecoY, uiStrideY, w,  h, bAbove, bLeft, bit_depth, nb);
        break;
#endif
    default:
        printf("\n illegal chroma intra prediction mode\n");
        break;
    }
}

void com_get_mpm(int x_scu, int y_scu, u32 *map_scu, s8 *map_ipm, int scup, int pic_width_in_scu, u8 mpm[2])
{
    u8 ipm_l = IPD_DC, ipm_u = IPD_DC;
    int valid_l = 0, valid_u = 0;

    if(x_scu > 0 && MCU_GET_INTRA_FLAG(map_scu[scup - 1]) && MCU_GET_CODED_FLAG(map_scu[scup - 1]))
    {
        ipm_l = map_ipm[scup - 1];
        valid_l = 1;
    }

    if(y_scu > 0 && MCU_GET_INTRA_FLAG(map_scu[scup - pic_width_in_scu]) && MCU_GET_CODED_FLAG(map_scu[scup - pic_width_in_scu]))
    {
        ipm_u = map_ipm[scup - pic_width_in_scu];
        valid_u = 1;
    }
    mpm[0] = COM_MIN(ipm_l, ipm_u);
    mpm[1] = COM_MAX(ipm_l, ipm_u);
    if(mpm[0] == mpm[1])
    {
        mpm[0] = IPD_DC;
        mpm[1] = (mpm[1] == IPD_DC) ? IPD_BI : mpm[1];
    }
}