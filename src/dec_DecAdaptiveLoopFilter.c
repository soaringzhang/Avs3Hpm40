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

#include "dec_DecAdaptiveLoopFilter.h"

#define Clip_post(high,val) ((val > high)? high: val)

/*
*************************************************************************
* Function: ALF decoding process top function
* Input:
*             ctx  : CONTEXT used for decoding process
*     pic_alf_Rec  : picture of the the ALF input image
*     pic_alf_Dec  : picture of the the ALF reconstructed image
*************************************************************************
*/
void ALFProcess_dec(DEC_CTX* ctx, ALFParam **alfParam, COM_PIC* pic_Rec, COM_PIC* pic_alf_Dec)
{
    int bit_depth = ctx->info.bit_depth_internal;
    pel *pResY, *pDecY;
    pel *pResUV[2], *pDecUV[2];
    int  ctu, NumCUInFrame, numLCUInPicWidth, numLCUInPicHeight;
    int  ctuYPos, ctuXPos, ctuHeight, ctuWidth;
    int  compIdx, lumaStride, chromaStride, lcuHeight, lcuWidth, img_height, img_width;
    BOOL isLeftAvail, isRightAvail, isAboveAvail, isBelowAvail;
    BOOL isAboveLeftAvail, isAboveRightAvail;
    lcuHeight = 1 << ctx->info.log2_max_cuwh;
    lcuWidth  = lcuHeight;
    img_height = ctx->info.pic_height;
    img_width  = ctx->info.pic_width;
    numLCUInPicWidth  = img_width / lcuWidth;
    numLCUInPicHeight = img_height / lcuHeight;
    numLCUInPicWidth  += (img_width % lcuWidth) ? 1 : 0;
    numLCUInPicHeight += (img_height % lcuHeight) ? 1 : 0;
    NumCUInFrame = numLCUInPicHeight * numLCUInPicWidth;
    lumaStride = pic_Rec->stride_luma;
    chromaStride = pic_Rec->stride_chroma;
    pResY = pic_Rec->y;
    pResUV[0] = pic_Rec->u;
    pResUV[1] = pic_Rec->v;
    pDecY = pic_alf_Dec->y;
    pDecUV[0] = pic_alf_Dec->u;
    pDecUV[1] = pic_alf_Dec->v;
    for (ctu = 0; ctu < NumCUInFrame; ctu++)
    {
        ctuYPos = (ctu / numLCUInPicWidth) * lcuHeight;
        ctuXPos = (ctu % numLCUInPicWidth) * lcuWidth;
        ctuHeight = (ctuYPos + lcuHeight > img_height) ? (img_height - ctuYPos) : lcuHeight;
        ctuWidth = (ctuXPos + lcuWidth  > img_width) ? (img_width - ctuXPos) : lcuWidth;
        //derive CTU boundary availabilities
        deriveBoundaryAvail(ctx, numLCUInPicWidth, numLCUInPicHeight, ctu, &isLeftAvail, &isRightAvail, &isAboveAvail,
                            &isBelowAvail, &isAboveLeftAvail, &isAboveRightAvail);
        for (compIdx = 0; compIdx < N_C; compIdx++)
        {
            if (!ctx->Dec_ALF->m_AlfLCUEnabled[ctu][compIdx])
            {
                continue;
            }
            if (compIdx == Y_C)
            {
                filterOneCTB(ctx->Dec_ALF, pResY, pDecY, (compIdx == Y_C) ? (lumaStride) : (chromaStride)
                             , compIdx, bit_depth, alfParam[compIdx], ctuYPos, ctuHeight, ctuXPos, ctuWidth
                             , isAboveAvail, isBelowAvail, isLeftAvail, isRightAvail, isAboveLeftAvail, isAboveRightAvail, bit_depth);
            }
            else
            {
                filterOneCTB(ctx->Dec_ALF, pResUV[compIdx - U_C], pDecUV[compIdx - U_C], (compIdx == Y_C) ? (lumaStride) : (chromaStride)
                             , compIdx, bit_depth, alfParam[compIdx], ctuYPos, ctuHeight, ctuXPos, ctuWidth
                             , isAboveAvail, isBelowAvail, isLeftAvail, isRightAvail, isAboveLeftAvail, isAboveRightAvail, bit_depth);
            }
        }
    }
}

/*
*************************************************************************
* Function: ALF filter on CTB
*************************************************************************
*/
void filterOneCTB(DecALFVar * Dec_ALF, pel *pRest, pel *pDec, int stride, int compIdx, int bit_depth, ALFParam *alfParam
                  , int ctuYPos, int ctuHeight, int ctuXPos, int ctuWidth
                  , BOOL isAboveAvail, BOOL isBelowAvail, BOOL isLeftAvail, BOOL isRightAvail, BOOL isAboveLeftAvail,
                  BOOL isAboveRightAvail, int sample_bit_depth
                 )
{
    const int  formatShift = (compIdx == Y_C) ? 0 : 1;
    int ypos, xpos, height, width;
    //reconstruct coefficients to m_filterCoeffSym and m_varIndTab
    reconstructCoefInfo(compIdx, alfParam, Dec_ALF->m_filterCoeffSym,
                        Dec_ALF->m_varIndTab); //reconstruct ALF coefficients & related parameters
    //derive CTB start positions, width, and height. If the boundary is not available, skip boundary samples.
    ypos = (ctuYPos >> formatShift);
    height = (ctuHeight >> formatShift);
    xpos = (ctuXPos >> formatShift);
    width = (ctuWidth >> formatShift);
    filterOneCompRegion(pRest, pDec, stride, (compIdx != Y_C)
                        , ypos, height, xpos, width, Dec_ALF->m_filterCoeffSym, Dec_ALF->m_varIndTab, Dec_ALF->m_varImg, sample_bit_depth,
                        isLeftAvail, isRightAvail, isAboveAvail, isBelowAvail, isAboveLeftAvail, isAboveRightAvail);
}
void deriveLoopFilterBoundaryAvailibility(DEC_CTX *ctx, int numLCUInPicWidth, int numLCUInPicHeight, int ctu,
        BOOL *isLeftAvail, BOOL *isRightAvail, BOOL *isAboveAvail, BOOL *isBelowAvail)
{
    int numLCUsInFrame = numLCUInPicHeight * numLCUInPicWidth;
    BOOL isAboveLeftAvail;
    BOOL isAboveRightAvail;
    BOOL isBelowLeftAvail;
    BOOL isBelowRightAvail;
    int  lcuHeight = 1 << ctx->info.log2_max_cuwh;
    int  lcuWidth = lcuHeight;
    int  img_height = ctx->info.pic_height;
    int  img_width = ctx->info.pic_width;
    int  NumCUInFrame;
    int  pic_x;
    int  pic_y;
    int  mb_x;
    int  mb_y;
    int  mb_nr;
    int  smb_mb_width;
    int  smb_mb_height;
    int  pic_mb_width = img_width / MIN_CU_SIZE;
    int  pic_mb_height = img_height / MIN_CU_SIZE;
    int  cuCurrNum;
    int  cuCurr;
    int  cuLeft;
    int  cuRight;
    int  cuAbove;
    int  cuBelow;
    int  cuAboveLeft;
    int  cuAboveRight;
    int  cuBelowLeft;
    int  cuBelowRight;
    int curSliceNr, neighorSliceNr, neighorSliceNr1, neighorSliceNr2;
    NumCUInFrame = numLCUInPicHeight * numLCUInPicWidth;
    pic_x = (ctu % numLCUInPicWidth) * lcuWidth;
    pic_y = (ctu / numLCUInPicWidth) * lcuHeight;
    pic_mb_width += (img_width  % MIN_CU_SIZE) ? 1 : 0;
    pic_mb_height += (img_height % MIN_CU_SIZE) ? 1 : 0;
    mb_x = pic_x / MIN_CU_SIZE;
    mb_y = pic_y / MIN_CU_SIZE;
    mb_nr = mb_y * pic_mb_width + mb_x;
    smb_mb_width = lcuWidth >> MIN_CU_LOG2;
    smb_mb_height = lcuHeight >> MIN_CU_LOG2;
    cuCurrNum = mb_nr;
    *isLeftAvail = (ctu % numLCUInPicWidth != 0);
    *isRightAvail = (ctu % numLCUInPicWidth != numLCUInPicWidth - 1);
    *isAboveAvail = (ctu >= numLCUInPicWidth);
    *isBelowAvail = (ctu  < numLCUsInFrame - numLCUInPicWidth);
    isAboveLeftAvail = (*isAboveAvail && *isLeftAvail);
    isAboveRightAvail = (*isAboveAvail && *isRightAvail);
    isBelowLeftAvail = (*isBelowAvail && *isLeftAvail);
    isBelowRightAvail = (*isBelowAvail && *isRightAvail);
    s8 *map_patch_idx = ctx->map.map_patch_idx;
    cuCurr = cuCurrNum;
    cuLeft = *isLeftAvail ? (cuCurrNum - 1) : -1;
    cuRight = *isRightAvail ? (cuCurrNum + 1) : -1;
    cuAbove = *isAboveAvail ? (cuCurrNum - pic_mb_width) : -1;
    cuBelow = *isBelowAvail ? (cuCurrNum + pic_mb_width) : -1;
    cuAboveLeft = isAboveLeftAvail ? (cuCurrNum - pic_mb_width - 1) : -1;
    cuAboveRight = isAboveRightAvail ? (cuCurrNum - pic_mb_width + 1) : -1;
    cuBelowLeft = isBelowLeftAvail ? (cuCurrNum + pic_mb_width - 1) : -1;
    cuBelowRight = isBelowRightAvail ? (cuCurrNum + pic_mb_width + 1) : -1;
    *isLeftAvail = *isRightAvail = *isAboveAvail = *isBelowAvail = FALSE;
    curSliceNr = map_patch_idx[cuCurr];
    if (cuLeft != -1)
    {
        neighorSliceNr = map_patch_idx[cuLeft];
        if (curSliceNr == neighorSliceNr)
        {
            *isLeftAvail = TRUE;
        }
    }
    if (cuRight != -1)
    {
        neighorSliceNr = map_patch_idx[cuRight];
        if (curSliceNr == neighorSliceNr)
        {
            *isRightAvail = TRUE;
        }
    }
    if (cuAbove != -1 && cuAboveLeft != -1 && cuAboveRight != -1)
    {
        neighorSliceNr = map_patch_idx[cuAbove];
        neighorSliceNr1 = map_patch_idx[cuAboveLeft];
        neighorSliceNr2 = map_patch_idx[cuAboveRight];
        if ((curSliceNr == neighorSliceNr)
                && (neighorSliceNr == neighorSliceNr1)
                && (neighorSliceNr == neighorSliceNr2))
        {
            *isAboveAvail = TRUE;
        }
    }
    if (cuBelow != -1 && cuBelowLeft != -1 && cuBelowRight != -1)
    {
        neighorSliceNr = map_patch_idx[cuBelow];
        neighorSliceNr1 = map_patch_idx[cuBelowLeft];
        neighorSliceNr2 = map_patch_idx[cuBelowRight];
        if ((curSliceNr == neighorSliceNr)
                && (neighorSliceNr == neighorSliceNr1)
                && (neighorSliceNr == neighorSliceNr2))
        {
            *isBelowAvail = TRUE;
        }
    }
}

void CreateAlfGlobalBuffer(DEC_CTX *ctx)
{
    int lcuHeight, lcuWidth, img_height, img_width;
    int NumCUInFrame, numLCUInPicWidth, numLCUInPicHeight;
    int n, i, g;
    int regionTable[NO_VAR_BINS] = { 0, 1, 4, 5, 15, 2, 3, 6, 14, 11, 10, 7, 13, 12,  9,  8 };
    int xInterval;
    int yInterval;
    int yIndex, xIndex;
    int yIndexOffset;
    lcuHeight = 1 << (ctx->info.log2_max_cuwh);
    lcuWidth = lcuHeight;
    img_height = ctx->info.pic_height;
    img_width = ctx->info.pic_width;
    numLCUInPicWidth = img_width / lcuWidth;
    numLCUInPicHeight = img_height / lcuHeight;
    numLCUInPicWidth += (img_width % lcuWidth) ? 1 : 0;
    numLCUInPicHeight += (img_height % lcuHeight) ? 1 : 0;
    NumCUInFrame = numLCUInPicHeight * numLCUInPicWidth;
    xInterval = ((((img_width + lcuWidth - 1) / lcuWidth) + 1) / 4 * lcuWidth);
    yInterval = ((((img_height + lcuHeight - 1) / lcuHeight) + 1) / 4 * lcuHeight);
    ctx->Dec_ALF = (DecALFVar *)malloc(1 * sizeof(DecALFVar));
    ctx->Dec_ALF->m_AlfLCUEnabled = (BOOL **)malloc(NumCUInFrame * sizeof(BOOL *));
    for (n = 0; n < NumCUInFrame; n++)
    {
        ctx->Dec_ALF->m_AlfLCUEnabled[n] = (BOOL *)malloc(N_C * sizeof(BOOL));
        memset(ctx->Dec_ALF->m_AlfLCUEnabled[n], FALSE, N_C * sizeof(BOOL));
    }
    ctx->Dec_ALF->m_varImg = (pel **)malloc(img_height * sizeof(pel *));
    for (n = 0; n < img_height; n++)
    {
        ctx->Dec_ALF->m_varImg[n] = (pel *)malloc(img_width * sizeof(pel));
        memset(ctx->Dec_ALF->m_varImg[n], 0, img_width * sizeof(pel));
    }
    ctx->Dec_ALF->m_filterCoeffSym = (int **)malloc(NO_VAR_BINS * sizeof(int *));
    for (g = 0; g < (int)NO_VAR_BINS; g++)
    {
        ctx->Dec_ALF->m_filterCoeffSym[g] = (int *)malloc(ALF_MAX_NUM_COEF * sizeof(int));
        memset(ctx->Dec_ALF->m_filterCoeffSym[g], 0, ALF_MAX_NUM_COEF * sizeof(int));
    }
    for (i = 0; i < img_height; i = i + 4)
    {
        yIndex = (yInterval == 0) ? (3) : (Clip_post(3, i / yInterval));
        yIndexOffset = yIndex * 4;
        for (g = 0; g < img_width; g = g + 4)
        {
            xIndex = (xInterval == 0) ? (3) : (Clip_post(3, g / xInterval));
            ctx->Dec_ALF->m_varImg[i >> LOG2_VAR_SIZE_H][g >> LOG2_VAR_SIZE_W] = (pel)regionTable[yIndexOffset + xIndex];
        }
    }
    ctx->Dec_ALF->m_alfPictureParam = (ALFParam **)malloc((N_C) * sizeof(ALFParam *));
    for (i = 0; i < N_C; i++)
    {
        ctx->Dec_ALF->m_alfPictureParam[i] = NULL;
        AllocateAlfPar(&(ctx->Dec_ALF->m_alfPictureParam[i]), i);
    }
}

void ReleaseAlfGlobalBuffer(DEC_CTX *ctx)
{
    int lcuHeight, lcuWidth, img_height, img_width;
    int NumCUInFrame, numLCUInPicWidth, numLCUInPicHeight;
    int n, g;

    lcuHeight = 1 << (ctx->info.log2_max_cuwh);
    lcuWidth = lcuHeight;
    img_height = ctx->info.pic_height;
    img_width = ctx->info.pic_width;
    numLCUInPicWidth = img_width / lcuWidth;
    numLCUInPicHeight = img_height / lcuHeight;
    numLCUInPicWidth += (img_width % lcuWidth) ? 1 : 0;
    numLCUInPicHeight += (img_height % lcuHeight) ? 1 : 0;
    NumCUInFrame = numLCUInPicHeight * numLCUInPicWidth;
    if (ctx->Dec_ALF)
    {
        if (ctx->Dec_ALF->m_varImg)
        {
            for (n = 0; n < img_height; n++)
            {
                free(ctx->Dec_ALF->m_varImg[n]);
            }
            free(ctx->Dec_ALF->m_varImg);
            ctx->Dec_ALF->m_varImg = NULL;
        }
        if (ctx->Dec_ALF->m_AlfLCUEnabled)
        {
            for (n = 0; n < NumCUInFrame; n++)
            {
                free(ctx->Dec_ALF->m_AlfLCUEnabled[n]);
            }
            free(ctx->Dec_ALF->m_AlfLCUEnabled);
            ctx->Dec_ALF->m_AlfLCUEnabled = NULL;
        }
        if (ctx->Dec_ALF->m_filterCoeffSym)
        {
            for (g = 0; g < (int)NO_VAR_BINS; g++)
            {
                free(ctx->Dec_ALF->m_filterCoeffSym[g]);
            }
            free(ctx->Dec_ALF->m_filterCoeffSym);
            ctx->Dec_ALF->m_filterCoeffSym = NULL;
        }
        if (ctx->Dec_ALF->m_alfPictureParam)
        {
            for (g = 0; g < (int)N_C; g++)
            {
                freeAlfPar(ctx->Dec_ALF->m_alfPictureParam[g], g);
            }
            free(ctx->Dec_ALF->m_alfPictureParam);
            ctx->Dec_ALF->m_alfPictureParam = NULL;
        }
        if (ctx->Dec_ALF)
        {
            free(ctx->Dec_ALF);
            ctx->Dec_ALF = NULL;
        }
    }
}

void AllocateAlfPar(ALFParam **alf_par, int cID)
{
    *alf_par = (ALFParam *)malloc(sizeof(ALFParam));
    (*alf_par)->alf_flag = 0;
    (*alf_par)->num_coeff = ALF_MAX_NUM_COEF;
    (*alf_par)->filters_per_group = 1;
    (*alf_par)->componentID = cID;
    (*alf_par)->coeffmulti = NULL;
    (*alf_par)->filterPattern = NULL;
    switch (cID)
    {
    case Y_C:
        get_mem2Dint(&((*alf_par)->coeffmulti), NO_VAR_BINS, ALF_MAX_NUM_COEF);
        get_mem1Dint(&((*alf_par)->filterPattern), NO_VAR_BINS);
        break;
    case U_C:
    case V_C:
        get_mem2Dint(&((*alf_par)->coeffmulti), 1, ALF_MAX_NUM_COEF);
        break;
    default:
        printf("Not a legal component ID\n");
        assert(0);
        exit(-1);
    }
}

void freeAlfPar(ALFParam *alf_par, int cID)
{
    switch (cID)
    {
    case Y_C:
        free_mem2Dint(alf_par->coeffmulti);
        free_mem1Dint(alf_par->filterPattern);
        break;
    case U_C:
    case V_C:
        free_mem2Dint(alf_par->coeffmulti);
        break;
    default:
        printf("Not a legal component ID\n");
        assert(0);
        exit(-1);
    }
    free(alf_par);
    alf_par = NULL;
}
void allocateAlfAPS(ALF_APS *pAPS)
{
    int i;
    for (i = 0; i < N_C; i++)
    {
        pAPS->alf_par[i].alf_flag = 0;
        pAPS->alf_par[i].num_coeff = ALF_MAX_NUM_COEF;
        pAPS->alf_par[i].filters_per_group = 1;
        pAPS->alf_par[i].componentID = i;
        pAPS->alf_par[i].coeffmulti = NULL;
        pAPS->alf_par[i].filterPattern = NULL;
        switch (i)
        {
        case Y_C:
            get_mem2Dint(&(pAPS->alf_par[i].coeffmulti), NO_VAR_BINS, ALF_MAX_NUM_COEF);
            get_mem1Dint(&(pAPS->alf_par[i].filterPattern), NO_VAR_BINS);
            break;
        case U_C:
        case V_C:
            get_mem2Dint(&(pAPS->alf_par[i].coeffmulti), 1, ALF_MAX_NUM_COEF);
            break;
        default:
            printf("Not a legal component ID\n");
            assert(0);
            exit(-1);
        }
    }
}

void freeAlfAPS(ALF_APS *pAPS)
{
    int i;
    for (i = 0; i < N_C; i++)
    {
        switch (i)
        {
        case Y_C:
            free_mem2Dint(pAPS->alf_par[i].coeffmulti);
            free_mem1Dint(pAPS->alf_par[i].filterPattern);
            break;
        case U_C:
        case V_C:
            free_mem2Dint(pAPS->alf_par[i].coeffmulti);
            break;
        default:
            printf("Not a legal component ID\n");
            assert(0);
            exit(-1);
        }
    }
}

void deriveBoundaryAvail(DEC_CTX *ctx, int numLCUInPicWidth, int numLCUInPicHeight, int ctu, BOOL *isLeftAvail,
                         BOOL *isRightAvail, BOOL *isAboveAvail, BOOL *isBelowAvail, BOOL *isAboveLeftAvail, BOOL *isAboveRightAvail)
{
    int  numLCUsInFrame = numLCUInPicHeight * numLCUInPicWidth;
    int  lcuHeight = 1 << ctx->info.log2_max_cuwh;
    int  lcuWidth = lcuHeight;
    int  NumCUInFrame;
    int  pic_x;
    int  pic_y;
    int  mb_x;
    int  mb_y;
    int  mb_nr;
    int  pic_mb_width = ctx->info.pic_width_in_scu;
    int  cuCurrNum;
    int  cuCurr;
    int  cuLeft;
    int  cuRight;
    int  cuAbove;
    int curSliceNr, neighorSliceNr;
    int  cuAboveLeft;
    int  cuAboveRight;
    int cross_patch_flag = ctx->info.sqh.cross_patch_loop_filter;
    NumCUInFrame = numLCUInPicHeight * numLCUInPicWidth;
    pic_x = (ctu % numLCUInPicWidth) * lcuWidth;
    pic_y = (ctu / numLCUInPicWidth) * lcuHeight;

    mb_x = pic_x / MIN_CU_SIZE;
    mb_y = pic_y / MIN_CU_SIZE;
    mb_nr = mb_y * pic_mb_width + mb_x;
    cuCurrNum = mb_nr;
    *isLeftAvail = (ctu % numLCUInPicWidth != 0);
    *isRightAvail = (ctu % numLCUInPicWidth != numLCUInPicWidth - 1);
    *isAboveAvail = (ctu >= numLCUInPicWidth);
    *isBelowAvail = (ctu  < numLCUsInFrame - numLCUInPicWidth);
    *isAboveLeftAvail = *isAboveAvail && *isLeftAvail;
    *isAboveRightAvail = *isAboveAvail && *isRightAvail;
    s8* map_patch_idx = ctx->map.map_patch_idx;
    cuCurr = (cuCurrNum);
    cuLeft = *isLeftAvail ? (cuCurrNum - 1) : -1;
    cuRight = *isRightAvail ? (cuCurrNum + (lcuWidth >> MIN_CU_LOG2)) : -1;
    cuAbove = *isAboveAvail ? (cuCurrNum - pic_mb_width) : -1;
    cuAboveLeft = *isAboveLeftAvail ? (cuCurrNum - pic_mb_width - 1) : -1;
    cuAboveRight = *isAboveRightAvail ? (cuCurrNum - pic_mb_width + (lcuWidth >> MIN_CU_LOG2)) : -1;
    if (!cross_patch_flag)
    {
        *isLeftAvail = *isRightAvail = *isAboveAvail = FALSE;
        *isAboveLeftAvail = *isAboveRightAvail = FALSE;
        curSliceNr = map_patch_idx[cuCurr];
        if (cuLeft != -1)
        {
            neighorSliceNr = map_patch_idx[cuLeft];
            if (curSliceNr == neighorSliceNr)
            {
                *isLeftAvail = TRUE;
            }
        }
        if (cuRight != -1)
        {
            neighorSliceNr = map_patch_idx[cuRight];
            if (curSliceNr == neighorSliceNr)
            {
                *isRightAvail = TRUE;
            }
        }
        if (cuAbove != -1)
        {
            neighorSliceNr = map_patch_idx[cuAbove];
            if (curSliceNr == neighorSliceNr)
            {
                *isAboveAvail = TRUE;
            }
        }
        if (cuAboveLeft != -1)
        {
            neighorSliceNr = map_patch_idx[cuAboveLeft];
            if (curSliceNr == neighorSliceNr)
            {
                *isAboveLeftAvail = TRUE;
            }
        }
        if (cuAboveRight != -1)
        {
            neighorSliceNr = map_patch_idx[cuAboveRight];
            if (curSliceNr == neighorSliceNr)
            {
                *isAboveRightAvail = TRUE;
            }
        }
    }
}