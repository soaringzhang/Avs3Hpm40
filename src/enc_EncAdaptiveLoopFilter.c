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

#include "enc_EncAdaptiveLoopFilter.h"

#define ROUND(a)  (((a) < 0)? (int)((a) - 0.5) : (int)((a) + 0.5))
#define REG              0.0001
#define REG_SQR          0.0000001

//EncALFVar *Enc_ALF;

#define Clip_post(high,val) ((val > high)? high: val)
/*
*************************************************************************
* Function: ALF encoding process top function
* Input:
*             ctx  : CONTEXT used for encoding process
*     alf_bs_temp  : bitstream structure for ALF RDO
*         pic_rec  : picture of the reconstructed image
*     pic_alf_Org  : picture of the original image
*     pic_alf_Rec  : picture of the the ALF input image
*     lambda_mode  : The lambda value in the ALF-RD decision
* Output:
*    alfPictureParam: The ALF parameter
*              apsId: The ALF parameter index in the buffer
*       isNewApsSent£ºThe New flag index
* Return:
*************************************************************************
*/
void ALFProcess(ENC_CTX* ctx, COM_BSW *alf_bs_temp, ALFParam **alfPictureParam, COM_PIC * pic_rec, COM_PIC * pic_alf_Org, COM_PIC * pic_alf_Rec, double lambda_mode)
{

    ENC_CORE *core = ctx->core;
    ENC_SBAC* alf_sbac = GET_SBAC_ENC(alf_bs_temp);
    int LumaMarginX;
    int LumaMarginY;
    int ChromaMarginX;
    int ChromaMarginY;
    int lumaStride; // Add to avoid micro trubles
    pel *pResY, *pDecY, *pOrgY;
    pel *pResUV[2], *pDecUV[2], *pOrgUV[2];
    LumaMarginX = (1 << ctx->info.log2_max_cuwh) + 16;
    LumaMarginY = (1 << ctx->info.log2_max_cuwh) + 16;
    ChromaMarginX = LumaMarginX >> 1;
    ChromaMarginY = LumaMarginY >> 1;
    pResY = pic_rec->y;
    pResUV[0] = pic_rec->u;
    pResUV[1] = pic_rec->v;
    pDecY = pic_alf_Rec->y;
    pDecUV[0] = pic_alf_Rec->u;
    pDecUV[1] = pic_alf_Rec->v;
    pOrgY = pic_alf_Org->y;
    pOrgUV[0] = pic_alf_Org->u;
    pOrgUV[1] = pic_alf_Org->v;
    SBAC_STORE((core->s_alf_initial), (*alf_sbac));
    lumaStride = pic_rec->stride_luma;
    // !!! NOTED: From There on, We use lumaStride instead of (img->width + (LumaMarginX<<1)) to avoid micro trubles
    getStatistics(ctx, pOrgY, pOrgUV, pDecY, pDecUV, lumaStride);
    setCurAlfParam(ctx, alf_bs_temp, alfPictureParam, lambda_mode);
    executePicLCUOnOffDecision(ctx, alf_bs_temp, alfPictureParam, lambda_mode, FALSE, NULL, pOrgY, pOrgUV, pDecY, pDecUV, pResY, pResUV, lumaStride);
#if ALF_LOW_LANTENCY_ENC
    if (ALF_LOW_LANTENCY_ENC)
    {
        //derive time-delayed filter for next picture
        storeAlfTemporalLayerInfo(ctx, Enc_ALF->m_temporal_alfPictureParam[ctx->info.pic_header.temporal_id], ctx->info.pic_header.temporal_id, lambda_mode);
    }
#endif
}

/*
*************************************************************************
* Function: Calculate the correlation matrix for image
* Input:
*             ctx  : CONTEXT used for encoding process
*     pic_alf_Org  : picture of the original image
*     pic_alf_Rec  : picture of the the ALF input image
*           stride : The stride of Y component of the ALF input picture
*           lambda : The lambda value in the ALF-RD decision
* Output:
* Return:
*************************************************************************
*/
void getStatistics(ENC_CTX* ctx, pel *imgY_org, pel **imgUV_org, pel *imgY_Dec, pel **imgUV_Dec, int stride)
{
    EncALFVar *Enc_ALF = ctx->Enc_ALF;
    BOOL  isLeftAvail, isRightAvail, isAboveAvail, isBelowAvail;
    BOOL  isAboveLeftAvail, isAboveRightAvail;
    int lcuHeight, lcuWidth, img_height, img_width;
    int ctu, NumCUInFrame, numLCUInPicWidth, numLCUInPicHeight;
    int ctuYPos, ctuXPos, ctuHeight, ctuWidth;
    int compIdx, formatShift;
    lcuHeight = 1 << ctx->info.log2_max_cuwh;
    lcuWidth = lcuHeight;
    img_height = ctx->info.pic_height;
    img_width = ctx->info.pic_width;
    numLCUInPicWidth = img_width / lcuWidth;
    numLCUInPicHeight = img_height / lcuHeight;
    numLCUInPicWidth += (img_width % lcuWidth) ? 1 : 0;
    numLCUInPicHeight += (img_height % lcuHeight) ? 1 : 0;
    NumCUInFrame = numLCUInPicHeight * numLCUInPicWidth;
    for (ctu = 0; ctu < NumCUInFrame; ctu++)
    {
        ctuYPos = (ctu / numLCUInPicWidth) * lcuHeight;
        ctuXPos = (ctu % numLCUInPicWidth) * lcuWidth;
        ctuHeight = (ctuYPos + lcuHeight > img_height) ? (img_height - ctuYPos) : lcuHeight;
        ctuWidth = (ctuXPos + lcuWidth  > img_width) ? (img_width - ctuXPos) : lcuWidth;
        deriveBoundaryAvail(ctx, numLCUInPicWidth, numLCUInPicHeight, ctu, &isLeftAvail, &isRightAvail, &isAboveAvail, &isBelowAvail,
                            &isAboveLeftAvail, &isAboveRightAvail);
        for (compIdx = 0; compIdx < N_C; compIdx++)
        {
            formatShift = (compIdx == Y_C) ? 0 : 1;
            reset_alfCorr(Enc_ALF->m_alfCorr[compIdx][ctu]);
            if (compIdx == Y_C)
            {
                getStatisticsOneLCU(ctx->Enc_ALF, compIdx, ctuYPos, ctuXPos, ctuHeight, ctuWidth, isAboveAvail,
                                    isBelowAvail, isLeftAvail, isRightAvail
                                    , isAboveLeftAvail, isAboveRightAvail, Enc_ALF->m_alfCorr[compIdx][ctu], imgY_org, imgY_Dec, stride, formatShift);
#if ALF_LOW_LANTENCY_ENC
                if (ALF_LOW_LANTENCY_ENC)
                {
                    reset_alfCorr(Enc_ALF->m_alfNonSkippedCorr[compIdx][ctu]);
                    getStatisticsOneLCU(ctx->Enc_ALF, compIdx, ctuYPos, ctuXPos, ctuHeight, ctuWidth, isAboveAvail, isBelowAvail, isLeftAvail,
                                        isRightAvail
                                        , isAboveLeftAvail, isAboveRightAvail, Enc_ALF->m_alfNonSkippedCorr[compIdx][ctu], imgY_org, imgY_Dec, stride,
                                        formatShift);
                }
#endif
            }
            else
            {
                getStatisticsOneLCU(ctx->Enc_ALF, compIdx, ctuYPos, ctuXPos, ctuHeight, ctuWidth, isAboveAvail,
                                    isBelowAvail, isLeftAvail, isRightAvail
                                    , isAboveLeftAvail, isAboveRightAvail, Enc_ALF->m_alfCorr[compIdx][ctu], imgUV_org[compIdx - U_C],
                                    imgUV_Dec[compIdx - U_C], (stride >> 1), formatShift);
#if ALF_LOW_LANTENCY_ENC
                if (ALF_LOW_LANTENCY_ENC)
                {
                    reset_alfCorr(Enc_ALF->m_alfNonSkippedCorr[compIdx][ctu]);
                    getStatisticsOneLCU(ctx->Enc_ALF, compIdx,ctuYPos, ctuXPos, ctuHeight, ctuWidth, isAboveAvail, isBelowAvail, isLeftAvail,
                                        isRightAvail
                                        , isAboveLeftAvail, isAboveRightAvail, Enc_ALF->m_alfNonSkippedCorr[compIdx][ctu], imgUV_org[compIdx - U_C],
                                        imgUV_Dec[compIdx - U_C], (stride >> 1), formatShift);
                }
#endif
            }
        }
    }
}

void deriveLoopFilterBoundaryAvailibility(ENC_CTX* ctx, int numLCUInPicWidth, int numLCUInPicHeight, int ctu,
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
    s8 *map_patch_idx = ctx->map.map_patch_idx;
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
    cuCurr = (cuCurrNum);
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
void createAlfGlobalBuffers(ENC_CTX* ctx)
{
    int lcuHeight, lcuWidth, img_height, img_width;
    int NumCUInFrame, numLCUInPicWidth, numLCUInPicHeight;
    int compIdx, n, i, g;
    int numCoef = (int)ALF_MAX_NUM_COEF;
    int regionTable[NO_VAR_BINS] = { 0, 1, 4, 5, 15, 2, 3, 6, 14, 11, 10, 7, 13, 12,  9,  8 };
    int xInterval;
    int yInterval;
    int yIndex, xIndex;
    int yIndexOffset;
    lcuHeight = 1 << ctx->info.log2_max_cuwh;
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
#if ALF_LOW_LANTENCY_ENC
    int maxNumTemporalLayer = (int)(log10((float)(ctx->param.gop_size)) / log10(2.0) + 1) + 1;
#endif
    ctx->Enc_ALF = (EncALFVar *)malloc(1 * sizeof(EncALFVar));
    ctx->Enc_ALF->m_uiBitIncrement = 0;
    memset(ctx->Enc_ALF->m_y_temp, 0, ALF_MAX_NUM_COEF * sizeof(double));
    memset(ctx->Enc_ALF->m_pixAcc_merged, 0, NO_VAR_BINS * sizeof(double));
    memset(ctx->Enc_ALF->m_varIndTab, 0, NO_VAR_BINS * sizeof(int));
    for (compIdx = 0; compIdx < N_C; compIdx++)
    {
        ctx->Enc_ALF->m_alfCorr[compIdx] = (AlfCorrData **)malloc(NumCUInFrame * sizeof(AlfCorrData *));
        ctx->Enc_ALF->m_alfNonSkippedCorr[compIdx] = (AlfCorrData **)malloc(NumCUInFrame * sizeof(AlfCorrData *));
        for (n = 0; n < NumCUInFrame; n++)
        {
            AllocateAlfCorrData(&(ctx->Enc_ALF->m_alfCorr[compIdx][n]), compIdx);
            AllocateAlfCorrData(&(ctx->Enc_ALF->m_alfNonSkippedCorr[compIdx][n]), compIdx);
        }
        AllocateAlfCorrData(&(ctx->Enc_ALF->m_alfCorrMerged[compIdx]), compIdx);
    }
#if ALF_LOW_LANTENCY_ENC
    ctx->Enc_ALF->m_alfPrevCorr = (AlfCorrData ** **)malloc(maxNumTemporalLayer * sizeof(AlfCorrData ** *));
    for (n = 0; n < maxNumTemporalLayer; n++)
    {
        ctx->Enc_ALF->m_alfPrevCorr[n] = (AlfCorrData ** *)malloc(N_C * sizeof(AlfCorrData **));
        for (compIdx = 0; compIdx < N_C; compIdx++)
        {
            ctx->Enc_ALF->m_alfPrevCorr[n][compIdx] = (AlfCorrData **)malloc(NumCUInFrame * sizeof(AlfCorrData *));
            for (i = 0; i < NumCUInFrame; i++)
            {
                AllocateAlfCorrData(&(ctx->Enc_ALF->m_alfPrevCorr[n][compIdx][i]), compIdx);
            }
        }
    }
#endif
    for (n = 0; n < NO_VAR_BINS; n++)
    {
        ctx->Enc_ALF->m_y_merged[n] = (double *)malloc(numCoef * sizeof(double));
        ctx->Enc_ALF->m_E_merged[n] = (double **)malloc(numCoef * sizeof(double *));
        for (i = 0; i < numCoef; i++)
        {
            ctx->Enc_ALF->m_E_merged[n][i] = (double *)malloc(numCoef * sizeof(double));
            memset(ctx->Enc_ALF->m_E_merged[n][i], 0, numCoef * sizeof(double));
        }
    }
    ctx->Enc_ALF->m_E_temp = (double **)malloc(numCoef * sizeof(double *));
    for (n = 0; n < numCoef; n++)
    {
        ctx->Enc_ALF->m_E_temp[n] = (double *)malloc(numCoef * sizeof(double));
        memset(ctx->Enc_ALF->m_E_temp[n], 0, numCoef * sizeof(double));
    }
#if ALF_LOW_LANTENCY_ENC
    ctx->Enc_ALF->m_temporal_alfPictureParam = (ALFParam ** *)malloc(maxNumTemporalLayer * sizeof(ALFParam **));
    for (n = 0; n < maxNumTemporalLayer; n++)
    {
        ctx->Enc_ALF->m_temporal_alfPictureParam[n] = (ALFParam **)malloc(N_C * sizeof(ALFParam *));
        for (compIdx = 0; compIdx < N_C; compIdx++)
        {
            AllocateAlfPar(&(ctx->Enc_ALF->m_temporal_alfPictureParam[n][compIdx]), compIdx);
        }
    }
#endif
    ctx->Enc_ALF->m_alfPictureParam = (ALFParam **)malloc(N_C * sizeof(ALFParam *));
    for (compIdx = 0; compIdx < N_C; compIdx++)
    {
        AllocateAlfPar(&(ctx->Enc_ALF->m_alfPictureParam[compIdx]), compIdx);
    }
    for (n = 0; n < NO_VAR_BINS; n++)
    {
        ctx->Enc_ALF->m_coeffNoFilter[n] = (int *)malloc(numCoef * sizeof(int));
        memset(&(ctx->Enc_ALF->m_coeffNoFilter[n][0]), 0, sizeof(int)*numCoef);
        ctx->Enc_ALF->m_coeffNoFilter[n][numCoef - 1] = (1 << ALF_NUM_BIT_SHIFT);
    }
    ctx->Enc_ALF->m_filterCoeffSym = (int **)malloc(NO_VAR_BINS * sizeof(int *));
    for (g = 0; g < (int)NO_VAR_BINS; g++)
    {
        ctx->Enc_ALF->m_filterCoeffSym[g] = (int *)malloc(ALF_MAX_NUM_COEF * sizeof(int));
        memset(ctx->Enc_ALF->m_filterCoeffSym[g], 0, ALF_MAX_NUM_COEF * sizeof(int));
    }
    ctx->Enc_ALF->m_numSlicesDataInOneLCU = (int *)malloc(NumCUInFrame * sizeof(int));
    memset(ctx->Enc_ALF->m_numSlicesDataInOneLCU, 0, NumCUInFrame * sizeof(int));
    ctx->Enc_ALF->m_varImg = (pel **)malloc(img_height * sizeof(pel *));
    for (n = 0; n < img_height; n++)
    {
        ctx->Enc_ALF->m_varImg[n] = (pel *)malloc(img_width * sizeof(pel));
        memset(ctx->Enc_ALF->m_varImg[n], 0, img_width * sizeof(pel));
    }
    ctx->Enc_ALF->m_AlfLCUEnabled = (BOOL **)malloc(NumCUInFrame * sizeof(BOOL *));
    for (n = 0; n < NumCUInFrame; n++)
    {
        ctx->Enc_ALF->m_AlfLCUEnabled[n] = (BOOL *)malloc(N_C * sizeof(BOOL));
        memset(ctx->Enc_ALF->m_AlfLCUEnabled[n], 0, N_C * sizeof(BOOL));
    }
    for (i = 0; i < img_height; i = i + 4)
    {
        yIndex = (yInterval == 0) ? (3) : (Clip_post(3, i / yInterval));
        yIndexOffset = yIndex * 4;
        for (g = 0; g < img_width; g = g + 4)
        {
            xIndex = (xInterval == 0) ? (3) : (Clip_post(3, g / xInterval));
            ctx->Enc_ALF->m_varImg[i >> LOG2_VAR_SIZE_H][g >> LOG2_VAR_SIZE_W] = (pel)regionTable[yIndexOffset + xIndex];
        }
    }
}
void destroyAlfGlobalBuffers(ENC_CTX* ctx, unsigned int uiMaxSizeInbit)
{
    int lcuHeight, lcuWidth, img_height, img_width;
    int NumCUInFrame, numLCUInPicWidth, numLCUInPicHeight;
    int compIdx, n, i, g;
    int numCoef = (int)ALF_MAX_NUM_COEF;
#if ALF_LOW_LANTENCY_ENC
    int maxNumTemporalLayer = (int)(log10f((float)(ctx->param.gop_size)) / log10f(2.0) + 1) + 1;
#endif
    lcuHeight = 1 << uiMaxSizeInbit;
    lcuWidth = lcuHeight;
    img_height = ctx->info.pic_height;
    img_width = ctx->info.pic_width;
    numLCUInPicWidth = img_width / lcuWidth;
    numLCUInPicHeight = img_height / lcuHeight;
    numLCUInPicWidth += (img_width % lcuWidth) ? 1 : 0;
    numLCUInPicHeight += (img_height % lcuHeight) ? 1 : 0;
    NumCUInFrame = numLCUInPicHeight * numLCUInPicWidth;
    for (compIdx = 0; compIdx < N_C; compIdx++)
    {
        for (n = 0; n < NumCUInFrame; n++)
        {
            freeAlfCorrData(ctx->Enc_ALF->m_alfCorr[compIdx][n]);
            freeAlfCorrData(ctx->Enc_ALF->m_alfNonSkippedCorr[compIdx][n]);
        }
        com_mfree(ctx->Enc_ALF->m_alfCorr[compIdx]);
        ctx->Enc_ALF->m_alfCorr[compIdx] = NULL;
        com_mfree(ctx->Enc_ALF->m_alfNonSkippedCorr[compIdx]);
        ctx->Enc_ALF->m_alfNonSkippedCorr[compIdx] = NULL;
        freeAlfCorrData(ctx->Enc_ALF->m_alfCorrMerged[compIdx]);
        com_mfree(ctx->Enc_ALF->m_alfCorrMerged[compIdx]);
    }
    for (n = 0; n < NO_VAR_BINS; n++)
    {
        com_mfree(ctx->Enc_ALF->m_y_merged[n]);
        ctx->Enc_ALF->m_y_merged[n] = NULL;
        for (i = 0; i < numCoef; i++)
        {
            com_mfree(ctx->Enc_ALF->m_E_merged[n][i]);
        }
        com_mfree(ctx->Enc_ALF->m_E_merged[n]);
        ctx->Enc_ALF->m_E_merged[n] = NULL;
    }
    for (i = 0; i < numCoef; i++)
    {
        com_mfree(ctx->Enc_ALF->m_E_temp[i]);
    }
    com_mfree(ctx->Enc_ALF->m_E_temp);
    ctx->Enc_ALF->m_E_temp = NULL;
    for (compIdx = 0; compIdx < N_C; compIdx++)
    {
        freeAlfPar(ctx->Enc_ALF->m_alfPictureParam[compIdx], compIdx);
    }
    com_mfree(ctx->Enc_ALF->m_alfPictureParam);
#if ALF_LOW_LANTENCY_ENC
    for (n = 0; n < maxNumTemporalLayer; n++)
    {
        for (compIdx = 0; compIdx < N_C; compIdx++)
        {
            for (i = 0; i < NumCUInFrame; i++)
            {
                freeAlfCorrData(ctx->Enc_ALF->m_alfPrevCorr[n][compIdx][i]);
                com_mfree(ctx->Enc_ALF->m_alfPrevCorr[n][compIdx][i]);
            }
            com_mfree(ctx->Enc_ALF->m_alfPrevCorr[n][compIdx]);
            freeAlfPar(ctx->Enc_ALF->m_temporal_alfPictureParam[n][compIdx], compIdx);
        }
        com_mfree(ctx->Enc_ALF->m_alfPrevCorr[n]);
        com_mfree(ctx->Enc_ALF->m_temporal_alfPictureParam[n]);
    }
    com_mfree(ctx->Enc_ALF->m_alfPrevCorr);
    ctx->Enc_ALF->m_alfPrevCorr = NULL;
    com_mfree(ctx->Enc_ALF->m_temporal_alfPictureParam);
    ctx->Enc_ALF->m_temporal_alfPictureParam = NULL;
#endif
    for (n = 0; n < NO_VAR_BINS; n++)
    {
        com_mfree(ctx->Enc_ALF->m_coeffNoFilter[n]);
    }
    com_mfree(ctx->Enc_ALF->m_numSlicesDataInOneLCU);
    for (n = 0; n < img_height; n++)
    {
        com_mfree(ctx->Enc_ALF->m_varImg[n]);
    }
    com_mfree(ctx->Enc_ALF->m_varImg);
    ctx->Enc_ALF->m_varImg = NULL;
    for (n = 0; n < NumCUInFrame; n++)
    {
        com_mfree(ctx->Enc_ALF->m_AlfLCUEnabled[n]);
    }
    com_mfree(ctx->Enc_ALF->m_AlfLCUEnabled);
    ctx->Enc_ALF->m_AlfLCUEnabled = NULL;
    for (g = 0; g < (int)NO_VAR_BINS; g++)
    {
        com_mfree(ctx->Enc_ALF->m_filterCoeffSym[g]);
    }
    com_mfree(ctx->Enc_ALF->m_filterCoeffSym);
    ctx->Enc_ALF->m_filterCoeffSym = NULL;
    com_mfree(ctx->Enc_ALF);
    ctx->Enc_ALF = NULL;
}

void AllocateAlfCorrData(AlfCorrData **dst, int cIdx)
{
    const int numCoef = ALF_MAX_NUM_COEF;
    const int maxNumGroups = NO_VAR_BINS;
    int numGroups = (cIdx == Y_C) ? (maxNumGroups) : (1);
    int i, j, g;
    (*dst) = (AlfCorrData *)malloc(sizeof(AlfCorrData));
    (*dst)->componentID = cIdx;
    (*dst)->ECorr = (double ** *)malloc(numGroups * sizeof(double **));
    (*dst)->yCorr = (double **)malloc(numGroups * sizeof(double *));
    (*dst)->pixAcc = (double *)malloc(numGroups * sizeof(double));
    for (g = 0; g < numGroups; g++)
    {
        (*dst)->yCorr[g] = (double *)malloc(numCoef * sizeof(double));
        for (j = 0; j < numCoef; j++)
        {
            (*dst)->yCorr[g][j] = 0;
        }
        (*dst)->ECorr[g] = (double **)malloc(numCoef * sizeof(double *));
        for (i = 0; i < numCoef; i++)
        {
            (*dst)->ECorr[g][i] = (double *)malloc(numCoef * sizeof(double));
            for (j = 0; j < numCoef; j++)
            {
                (*dst)->ECorr[g][i][j] = 0;
            }
        }
        (*dst)->pixAcc[g] = 0;
    }
}
void freeAlfCorrData(AlfCorrData *dst)
{
    const int numCoef = ALF_MAX_NUM_COEF;
    const int maxNumGroups = NO_VAR_BINS;
    int numGroups, i, g;
    if (dst->componentID >= 0)
    {
        numGroups = (dst->componentID == Y_C) ? (maxNumGroups) : (1);
        for (g = 0; g < numGroups; g++)
        {
            for (i = 0; i < numCoef; i++)
            {
                com_mfree(dst->ECorr[g][i]);
            }
            com_mfree(dst->ECorr[g]);
            com_mfree(dst->yCorr[g]);
        }
        com_mfree(dst->ECorr);
        com_mfree(dst->yCorr);
        com_mfree(dst->pixAcc);
    }
}
void reset_alfCorr(AlfCorrData *alfCorr)
{
    if (alfCorr->componentID >= 0)
    {
        int numCoef = ALF_MAX_NUM_COEF;
        int maxNumGroups = NO_VAR_BINS;
        int g, j, i;
        int numGroups = (alfCorr->componentID == Y_C) ? (maxNumGroups) : (1);
        for (g = 0; g < numGroups; g++)
        {
            alfCorr->pixAcc[g] = 0;
            for (j = 0; j < numCoef; j++)
            {
                alfCorr->yCorr[g][j] = 0;
                for (i = 0; i < numCoef; i++)
                {
                    alfCorr->ECorr[g][j][i] = 0;
                }
            }
        }
    }
}
/*
*************************************************************************
* Function: Calculate the correlation matrix for each LCU
* Input:
*  skipCUBoundaries  : Boundary skip flag
*           compIdx  : Image component index
*            lcuAddr : The address of current LCU
* (ctuXPos,ctuYPos)  : The LCU position
*          isXXAvail : The Available of neighboring LCU
*            pPicOrg : The original image buffer
*            pPicSrc : The distortion image buffer
*           stride : The width of image buffer
* Output:
* Return:
*************************************************************************
*/
void getStatisticsOneLCU(EncALFVar *Enc_ALF, int compIdx
                         , int ctuYPos, int ctuXPos, int ctuHeight, int ctuWidth
                         , BOOL isAboveAvail, BOOL isBelowAvail, BOOL isLeftAvail, BOOL isRightAvail
                         , BOOL isAboveLeftAvail, BOOL isAboveRightAvail, AlfCorrData *alfCorr, pel *pPicOrg, pel *pPicSrc
                         , int stride, int formatShift)
{
    int ypos, xpos, height, width;
    switch (compIdx)
    {
    case U_C:
    case V_C:
    {
        ypos = (ctuYPos >> formatShift);
        xpos = (ctuXPos >> formatShift);
        height = (ctuHeight >> formatShift);
        width = (ctuWidth >> formatShift);
        calcCorrOneCompRegionChma(pPicOrg, pPicSrc, stride, ypos, xpos, height, width, alfCorr->ECorr[0], alfCorr->yCorr[0],
                                  isLeftAvail, isRightAvail, isAboveAvail, isBelowAvail, isAboveLeftAvail, isAboveRightAvail);
    }
    break;
    case Y_C:
    {
        ypos = (ctuYPos >> formatShift);
        xpos = (ctuXPos >> formatShift);
        height = (ctuHeight >> formatShift);
        width = (ctuWidth >> formatShift);
        calcCorrOneCompRegionLuma(Enc_ALF, pPicOrg, pPicSrc, stride, ypos, xpos, height, width, alfCorr->ECorr, alfCorr->yCorr,
                                  alfCorr->pixAcc, isLeftAvail, isRightAvail, isAboveAvail, isBelowAvail, isAboveLeftAvail, isAboveRightAvail);
    }
    break;
    default:
    {
        printf("Not a legal component index for ALF\n");
        assert(0);
        exit(-1);
    }
    }
}

/*
*************************************************************************
* Function: Calculate the correlation matrix for Luma
*************************************************************************
*/
void calcCorrOneCompRegionLuma(EncALFVar *Enc_ALF, pel *imgOrg, pel *imgPad, int stride, int yPos, int xPos, int height, int width
                               , double ***eCorr, double **yCorr, double *pixAcc, int isLeftAvail, int isRightAvail, int isAboveAvail,
                               int isBelowAvail, int isAboveLeftAvail, int isAboveRightAvail)
{
    int xPosEnd = xPos + width;
    int N = ALF_MAX_NUM_COEF;
    int startPosLuma = isAboveAvail ? (yPos - 4) : yPos;
    int endPosLuma = isBelowAvail ? (yPos + height - 4) : (yPos + height);
    int xOffSetLeft = isLeftAvail ? -3 : 0;
    int xOffSetRight = isRightAvail ? 3 : 0;
    int yUp, yBottom;
    int xLeft, xRight;
    int ELocal[ALF_MAX_NUM_COEF];
    pel *imgPad1, *imgPad2, *imgPad3, *imgPad4, *imgPad5, *imgPad6;
    int i, j, k, l, yLocal, varInd;
    double **E;
    double *yy;
    imgPad += startPosLuma * stride;
    imgOrg += startPosLuma * stride;
    varInd = Enc_ALF->m_varImg[yPos >> LOG2_VAR_SIZE_H][xPos >> LOG2_VAR_SIZE_W];
    //loop region height
    for (i = startPosLuma; i < endPosLuma; i++)
    {
        yUp = COM_CLIP3(startPosLuma, endPosLuma - 1, i - 1);
        yBottom = COM_CLIP3(startPosLuma, endPosLuma - 1, i + 1);
        imgPad1 = imgPad + (yBottom - i) * stride;
        imgPad2 = imgPad + (yUp - i) * stride;
        yUp = COM_CLIP3(startPosLuma, endPosLuma - 1, i - 2);
        yBottom = COM_CLIP3(startPosLuma, endPosLuma - 1, i + 2);
        imgPad3 = imgPad + (yBottom - i) * stride;
        imgPad4 = imgPad + (yUp - i) * stride;
        yUp = COM_CLIP3(startPosLuma, endPosLuma - 1, i - 3);
        yBottom = COM_CLIP3(startPosLuma, endPosLuma - 1, i + 3);
        imgPad5 = imgPad + (yBottom - i) * stride;
        imgPad6 = imgPad + (yUp - i) * stride;
        //loop current ctu width
        for (j = xPos; j < xPosEnd; j++)
        {
            memset(ELocal, 0, N * sizeof(int));
            ELocal[0] = (imgPad5[j] + imgPad6[j]);
            ELocal[1] = (imgPad3[j] + imgPad4[j]);
            ELocal[3] = (imgPad1[j] + imgPad2[j]);
            // upper left c2
            xLeft = check_filtering_unit_boundary_extension(j - 1, i - 1, xPos, yPos, xPos, startPosLuma, xPosEnd - 1,
                    endPosLuma - 1, isAboveLeftAvail, isLeftAvail, isAboveRightAvail, isRightAvail);
            ELocal[2] = imgPad2[xLeft];
            // upper right c4
            xRight = check_filtering_unit_boundary_extension(j + 1, i - 1, xPos, yPos, xPos, startPosLuma, xPosEnd - 1,
                     endPosLuma - 1, isAboveLeftAvail, isLeftAvail, isAboveRightAvail, isRightAvail);
            ELocal[4] = imgPad2[xRight];
            // lower left c4
            xLeft = check_filtering_unit_boundary_extension(j - 1, i + 1, xPos, yPos, xPos, startPosLuma, xPosEnd - 1,
                    endPosLuma - 1, isAboveLeftAvail, isLeftAvail, isAboveRightAvail, isRightAvail);
            ELocal[4] += imgPad1[xLeft];
            // lower right c2
            xRight = check_filtering_unit_boundary_extension(j + 1, i + 1, xPos, yPos, xPos, startPosLuma, xPosEnd - 1,
                     endPosLuma - 1, isAboveLeftAvail, isLeftAvail, isAboveRightAvail, isRightAvail);
            ELocal[2] += imgPad1[xRight];
            xLeft = COM_CLIP3(xPos + xOffSetLeft, xPosEnd - 1 + xOffSetRight, j - 1);
            xRight = COM_CLIP3(xPos + xOffSetLeft, xPosEnd - 1 + xOffSetRight, j + 1);
            ELocal[7] = (imgPad[xRight] + imgPad[xLeft]);
            xLeft = COM_CLIP3(xPos + xOffSetLeft, xPosEnd - 1 + xOffSetRight, j - 2);
            xRight = COM_CLIP3(xPos + xOffSetLeft, xPosEnd - 1 + xOffSetRight, j + 2);
            ELocal[6] = (imgPad[xRight] + imgPad[xLeft]);
            xLeft = COM_CLIP3(xPos + xOffSetLeft, xPosEnd - 1 + xOffSetRight, j - 3);
            xRight = COM_CLIP3(xPos + xOffSetLeft, xPosEnd - 1 + xOffSetRight, j + 3);
            ELocal[5] = (imgPad[xRight] + imgPad[xLeft]);
            ELocal[8] = (imgPad[j]);
            yLocal = imgOrg[j];
            pixAcc[varInd] += (yLocal * yLocal);
            E = eCorr[varInd];
            yy = yCorr[varInd];
            for (k = 0; k < N; k++)
            {
                for (l = k; l < N; l++)
                {
                    E[k][l] += (double)(ELocal[k] * ELocal[l]);
                }
                yy[k] += (double)(ELocal[k] * yLocal);
            }
        }
        imgPad += stride;
        imgOrg += stride;
    }
    for (varInd = 0; varInd < NO_VAR_BINS; varInd++)
    {
        E = eCorr[varInd];
        for (k = 1; k < N; k++)
        {
            for (l = 0; l < k; l++)
            {
                E[k][l] = E[l][k];
            }
        }
    }
}


/*
*************************************************************************
* Function: Calculate the correlation matrix for Chroma
*************************************************************************
*/
void calcCorrOneCompRegionChma(pel *imgOrg, pel *imgPad, int stride, int yPos, int xPos, int height, int width,
                               double **eCorr, double *yCorr, int isLeftAvail, int isRightAvail, int isAboveAvail, int isBelowAvail,
                               int isAboveLeftAvail, int isAboveRightAvail)
{
    int xPosEnd = xPos + width;
    int N = ALF_MAX_NUM_COEF;
    int startPosChroma = isAboveAvail ? (yPos - 4) : yPos;
    int endPosChroma = isBelowAvail ? (yPos + height - 4) : (yPos + height);
    int xOffSetLeft = isLeftAvail ? -3 : 0;
    int xOffSetRight = isRightAvail ? 3 : 0;
    int yUp, yBottom;
    int xLeft, xRight;
    int ELocal[ALF_MAX_NUM_COEF];
    pel *imgPad1, *imgPad2, *imgPad3, *imgPad4, *imgPad5, *imgPad6;
    int i, j, k, l, yLocal;
    imgPad += startPosChroma * stride;
    imgOrg += startPosChroma * stride;
    for (i = startPosChroma; i < endPosChroma; i++)
    {
        yUp = COM_CLIP3(startPosChroma, endPosChroma - 1, i - 1);
        yBottom = COM_CLIP3(startPosChroma, endPosChroma - 1, i + 1);
        imgPad1 = imgPad + (yBottom - i) * stride;
        imgPad2 = imgPad + (yUp - i) * stride;
        yUp = COM_CLIP3(startPosChroma, endPosChroma - 1, i - 2);
        yBottom = COM_CLIP3(startPosChroma, endPosChroma - 1, i + 2);
        imgPad3 = imgPad + (yBottom - i) * stride;
        imgPad4 = imgPad + (yUp - i) * stride;
        yUp = COM_CLIP3(startPosChroma, endPosChroma - 1, i - 3);
        yBottom = COM_CLIP3(startPosChroma, endPosChroma - 1, i + 3);
        imgPad5 = imgPad + (yBottom - i) * stride;
        imgPad6 = imgPad + (yUp - i) * stride;
        for (j = xPos; j < xPosEnd; j++)
        {
            memset(ELocal, 0, N * sizeof(int));
            ELocal[0] = (imgPad5[j] + imgPad6[j]);
            ELocal[1] = (imgPad3[j] + imgPad4[j]);
            ELocal[3] = (imgPad1[j] + imgPad2[j]);
            // upper left c2
            xLeft = check_filtering_unit_boundary_extension(j - 1, i - 1, xPos, yPos, xPos, startPosChroma, xPosEnd - 1,
                    endPosChroma - 1, isAboveLeftAvail, isLeftAvail, isAboveRightAvail, isRightAvail);
            ELocal[2] = imgPad2[xLeft];
            // upper right c4
            xRight = check_filtering_unit_boundary_extension(j + 1, i - 1, xPos, yPos, xPos, startPosChroma, xPosEnd - 1,
                     endPosChroma - 1, isAboveLeftAvail, isLeftAvail, isAboveRightAvail, isRightAvail);
            ELocal[4] = imgPad2[xRight];
            // lower left c4
            xLeft = check_filtering_unit_boundary_extension(j - 1, i + 1, xPos, yPos, xPos, startPosChroma, xPosEnd - 1,
                    endPosChroma - 1, isAboveLeftAvail, isLeftAvail, isAboveRightAvail, isRightAvail);
            ELocal[4] += imgPad1[xLeft];
            // lower right c2
            xRight = check_filtering_unit_boundary_extension(j + 1, i + 1, xPos, yPos, xPos, startPosChroma, xPosEnd - 1,
                     endPosChroma - 1, isAboveLeftAvail, isLeftAvail, isAboveRightAvail, isRightAvail);
            ELocal[2] += imgPad1[xRight];
            xLeft = COM_CLIP3(xPos + xOffSetLeft, xPosEnd - 1 + xOffSetRight, j - 1);
            xRight = COM_CLIP3(xPos + xOffSetLeft, xPosEnd - 1 + xOffSetRight, j + 1);
            ELocal[7] = (imgPad[xRight] + imgPad[xLeft]);
            xLeft = COM_CLIP3(xPos + xOffSetLeft, xPosEnd - 1 + xOffSetRight, j - 2);
            xRight = COM_CLIP3(xPos + xOffSetLeft, xPosEnd - 1 + xOffSetRight, j + 2);
            ELocal[6] = (imgPad[xRight] + imgPad[xLeft]);
            xLeft = COM_CLIP3(xPos + xOffSetLeft, xPosEnd - 1 + xOffSetRight, j - 3);
            xRight = COM_CLIP3(xPos + xOffSetLeft, xPosEnd - 1 + xOffSetRight, j + 3);
            ELocal[5] = (imgPad[xRight] + imgPad[xLeft]);
            ELocal[8] = (imgPad[j]);
            yLocal = (int)imgOrg[j];
            for (k = 0; k < N; k++)
            {
                eCorr[k][k] += ELocal[k] * ELocal[k];
                for (l = k + 1; l < N; l++)
                {
                    eCorr[k][l] += ELocal[k] * ELocal[l];
                }
                yCorr[k] += yLocal * ELocal[k];
            }
        }
        imgPad += stride;
        imgOrg += stride;
    }
    for (j = 0; j < N - 1; j++)
    {
        for (i = j + 1; i < N; i++)
        {
            eCorr[i][j] = eCorr[j][i];
        }
    }
}


/*
*************************************************************************
* Function: ALF parameter selection
* Input:
*    alfPictureParam: The ALF parameter
*              apsId: The ALF parameter index in the buffer
*       isNewApsSent£ºThe New flag index
*       lambda      : The lambda value in the ALF-RD decision
* Return:
*************************************************************************
*/
void setCurAlfParam(ENC_CTX *ctx, COM_BSW* alf_bs_temp, ALFParam **alfPictureParam, double lambda)
{
    EncALFVar *Enc_ALF = ctx->Enc_ALF;
    AlfCorrData *** alfLcuCorr = NULL;
    int compIdx, i;
    AlfCorrData *alfPicCorr[N_C];
    double costMin, cost;
    ALFParam *tempAlfParam[N_C];
    int picHeaderBitrate = 0;
    for (compIdx = 0; compIdx < N_C; compIdx++)
    {
        AllocateAlfPar(&(tempAlfParam[compIdx]), compIdx);
    }
#if ALF_LOW_LANTENCY_ENC
    if (ALF_LOW_LANTENCY_ENC)  // LowLatencyEncoding part
    {
        for (compIdx = 0; compIdx < 3; compIdx++)
        {
            copyALFparam(alfPictureParam[compIdx], Enc_ALF->m_temporal_alfPictureParam[temporalLayer][compIdx]);
        }
        alfLcuCorr = Enc_ALF->m_alfPrevCorr[temporalLayer];
        costMin = executePicLCUOnOffDecision(ctx, alf_bs_temp, alfPictureParam, lambda, TRUE, alfLcuCorr, NULL, NULL, NULL, NULL, NULL, NULL, 0);
        picHeaderBitrate = estimateALFBitrateInPicHeader(alfPictureParam);
        costMin += (double)picHeaderBitrate * lambda;
    }
    else
#endif
    {
        // normal part
        costMin = 1.7e+308; // max double
        for (compIdx = 0; compIdx < N_C; compIdx++)
        {
            AllocateAlfCorrData(&(alfPicCorr[compIdx]), compIdx);
        }
        accumulateLCUCorrelations(ctx, alfPicCorr, Enc_ALF->m_alfCorr, TRUE);
        decideAlfPictureParam(Enc_ALF, tempAlfParam, alfPicCorr, lambda);
        for (i = 0; i < ALF_REDESIGN_ITERATION; i++)
        {
            if (i != 0)
            {
                //redesign filter according to the last on off results
                accumulateLCUCorrelations(ctx, alfPicCorr, Enc_ALF->m_alfCorr, FALSE);
                decideAlfPictureParam(Enc_ALF, tempAlfParam, alfPicCorr, lambda);
            }
            //estimate cost
            cost = executePicLCUOnOffDecision(ctx, alf_bs_temp, tempAlfParam, lambda, TRUE, Enc_ALF->m_alfCorr, NULL, NULL, NULL, NULL, NULL, NULL,
                                              0);
            picHeaderBitrate = estimateALFBitrateInPicHeader(tempAlfParam);
            cost += (double)picHeaderBitrate * lambda;
            if (cost < costMin)
            {
                costMin = cost;
                for (compIdx = 0; compIdx < N_C; compIdx++)
                {
                    copyALFparam(alfPictureParam[compIdx], tempAlfParam[compIdx]);
                }
            }
        }
        for (compIdx = 0; compIdx < N_C; compIdx++)
        {
            freeAlfCorrData(alfPicCorr[compIdx]);
            free(alfPicCorr[compIdx]);
        }
        alfLcuCorr = Enc_ALF->m_alfCorr;
    }
    for (compIdx = 0; compIdx < N_C; compIdx++)
    {
        freeAlfPar(tempAlfParam[compIdx], compIdx);
    }
}

unsigned int uvlcBitrateEstimate(int val)
{
    unsigned int length = 1;
    val++;
    assert(val);
    while (1 != val)
    {
        val >>= 1;
        length += 2;
    }
    return ((length >> 1) + ((length + 1) >> 1));
}
unsigned int svlcBitrateEsitmate(int val)
{
    return uvlcBitrateEstimate((val <= 0) ? (-val << 1) : ((val << 1) - 1));
}

unsigned int filterCoeffBitrateEstimate(int *coeff)
{
    unsigned int  bitrate = 0;
    int i;
    for (i = 0; i < (int)ALF_MAX_NUM_COEF; i++)
    {
        bitrate += (svlcBitrateEsitmate(coeff[i]));
    }
    return bitrate;
}
unsigned int ALFParamBitrateEstimate(ALFParam *alfParam)
{
    unsigned int  bitrate = 0; //alf enabled flag
    int noFilters, g;
    if (alfParam->alf_flag == 1)
    {
        if (alfParam->componentID == Y_C)
        {
            noFilters = alfParam->filters_per_group - 1;
            bitrate += uvlcBitrateEstimate(noFilters);
            bitrate += (4 * noFilters);
        }
        for (g = 0; g < alfParam->filters_per_group; g++)
        {
            bitrate += filterCoeffBitrateEstimate(alfParam->coeffmulti[g]);
        }
    }
    return bitrate;
}

unsigned int estimateALFBitrateInPicHeader(ALFParam **alfPicParam)
{
    //CXCTBD please help to check if the implementation is consistent with syntax coding
    int compIdx;
    unsigned int bitrate = 3; // pic_alf_enabled_flag[0,1,2]
    if (alfPicParam[0]->alf_flag == 1 || alfPicParam[1]->alf_flag == 1 || alfPicParam[2]->alf_flag == 1)
    {
        for (compIdx = 0; compIdx < N_C; compIdx++)
        {
            bitrate += ALFParamBitrateEstimate(alfPicParam[compIdx]);
        }
    }
    return bitrate;
}
long long estimateFilterDistortion(EncALFVar *Enc_ALF, int compIdx, AlfCorrData *alfCorr, int **coeffSet, int filterSetSize, int *mergeTable,
                                   BOOL doPixAccMerge)
{
    int numCoeff = (int)ALF_MAX_NUM_COEF;
    AlfCorrData *alfMerged = Enc_ALF->m_alfCorrMerged[compIdx];
    int f;
    int     **coeff = (coeffSet == NULL) ? (Enc_ALF->m_coeffNoFilter) : (coeffSet);
    long long      iDist = 0;
    mergeFrom(alfMerged, alfCorr, mergeTable, doPixAccMerge);
    for (f = 0; f < filterSetSize; f++)
    {
        iDist += xFastFiltDistEstimation(Enc_ALF, alfMerged->ECorr[f], alfMerged->yCorr[f], coeff[f], numCoeff);
    }
    return iDist;
}

long long xFastFiltDistEstimation(EncALFVar *Enc_ALF, double **ppdE, double *pdy, int *piCoeff, int iFiltLength)
{
    //static memory
    double pdcoeff[ALF_MAX_NUM_COEF];
    //variable
    int    i, j;
    long long  iDist;
    double dDist, dsum;
    unsigned int uiShift;
    for (i = 0; i < iFiltLength; i++)
    {
        pdcoeff[i] = (double)piCoeff[i] / (double)(1 << ((int)ALF_NUM_BIT_SHIFT));
    }
    dDist = 0;
    for (i = 0; i < iFiltLength; i++)
    {
        dsum = ((double)ppdE[i][i]) * pdcoeff[i];
        for (j = i + 1; j < iFiltLength; j++)
        {
            dsum += (double)(2 * ppdE[i][j]) * pdcoeff[j];
        }
        dDist += ((dsum - 2.0 * pdy[i]) * pdcoeff[i]);
    }
    uiShift = Enc_ALF->m_uiBitIncrement << 1;
    if (dDist < 0)
    {
        iDist = -(((long long)(-dDist + 0.5)) >> uiShift);
    }
    else   //dDist >=0
    {
        iDist = ((long long)(dDist + 0.5)) >> uiShift;
    }
    return iDist;
}

/*
*************************************************************************
* Function: correlation matrix merge
* Input:
*                src: input correlation matrix
*         mergeTable: merge table
* Output:
*                dst: output correlation matrix
* Return:
*************************************************************************
*/
void mergeFrom(AlfCorrData *dst, AlfCorrData *src, int *mergeTable, BOOL doPixAccMerge)
{
    int numCoef = ALF_MAX_NUM_COEF;
    double **srcE, **dstE;
    double *srcy, *dsty;
    int maxFilterSetSize, j, i, varInd, filtIdx;
    assert(dst->componentID == src->componentID);
    reset_alfCorr(dst);
    switch (dst->componentID)
    {
    case U_C:
    case V_C:
    {
        srcE = src->ECorr[0];
        dstE = dst->ECorr[0];
        srcy = src->yCorr[0];
        dsty = dst->yCorr[0];
        for (j = 0; j < numCoef; j++)
        {
            for (i = 0; i < numCoef; i++)
            {
                dstE[j][i] += srcE[j][i];
            }
            dsty[j] += srcy[j];
        }
        if (doPixAccMerge)
        {
            dst->pixAcc[0] = src->pixAcc[0];
        }
    }
    break;
    case Y_C:
    {
        maxFilterSetSize = (int)NO_VAR_BINS;
        for (varInd = 0; varInd < maxFilterSetSize; varInd++)
        {
            filtIdx = (mergeTable == NULL) ? (0) : (mergeTable[varInd]);
            srcE = src->ECorr[varInd];
            dstE = dst->ECorr[filtIdx];
            srcy = src->yCorr[varInd];
            dsty = dst->yCorr[filtIdx];
            for (j = 0; j < numCoef; j++)
            {
                for (i = 0; i < numCoef; i++)
                {
                    dstE[j][i] += srcE[j][i];
                }
                dsty[j] += srcy[j];
            }
            if (doPixAccMerge)
            {
                dst->pixAcc[filtIdx] += src->pixAcc[varInd];
            }
        }
    }
    break;
    default:
    {
        printf("not a legal component ID\n");
        assert(0);
        exit(-1);
    }
    }
}

int getTemporalLayerNo(int curPoc, int picDistance)
{
    int layer = 0;
    while (picDistance > 0)
    {
        if (curPoc % picDistance == 0)
        {
            break;
        }
        picDistance = (int)(picDistance / 2);
        layer++;
    }
    return layer;
}


/*
*************************************************************************
* Function: ALF On/Off decision for LCU
*************************************************************************
*/
double executePicLCUOnOffDecision(ENC_CTX *ctx, COM_BSW *alf_bs_temp, ALFParam **alfPictureParam
                                  , double lambda
                                  , BOOL isRDOEstimate
                                  , AlfCorrData *** alfCorr
                                  , pel *imgY_org, pel **imgUV_org, pel *imgY_Dec, pel **imgUV_Dec, pel *imgY_Res, pel **imgUV_Res
                                  , int stride
                                 )
{
    EncALFVar *Enc_ALF = ctx->Enc_ALF;
    ENC_CORE *core = ctx->core;
    ENC_SBAC *alf_sbac = GET_SBAC_ENC(alf_bs_temp);
    int bit_depth = ctx->info.bit_depth_internal;
    long long  distEnc, distOff;
    double  rateEnc, rateOff, costEnc, costOff, costAlfOn, costAlfOff;
    BOOL isLeftAvail, isRightAvail, isAboveAvail, isBelowAvail;
    BOOL isAboveLeftAvail, isAboveRightAvail;
    long long  distBestPic[N_C];
    double rateBestPic[N_C];
    int compIdx, ctu, ctuYPos, ctuXPos, ctuHeight, ctuWidth, formatShift, n, stride_in, ctx_idx;
    pel *pOrg = NULL;
    pel *pDec = NULL;
    pel *pRest = NULL;
    double lambda_luma, lambda_chroma;
    /////
    int lcuHeight, lcuWidth, img_height, img_width;
    int NumCUsInFrame, numLCUInPicWidth, numLCUInPicHeight;
    double bestCost = 0;
    SBAC_LOAD((*alf_sbac), (core->s_alf_initial));
    SBAC_STORE((core->s_alf_cu_ctr), (*alf_sbac));
    lcuHeight = 1 << ctx->info.log2_max_cuwh;
    lcuWidth = lcuHeight;
    img_height = ctx->info.pic_height;
    img_width = ctx->info.pic_width;
    numLCUInPicWidth = img_width / lcuWidth;
    numLCUInPicHeight = img_height / lcuHeight;
    numLCUInPicWidth += (img_width % lcuWidth) ? 1 : 0;
    numLCUInPicHeight += (img_height % lcuHeight) ? 1 : 0;
    NumCUsInFrame = numLCUInPicHeight * numLCUInPicWidth;
    lambda_luma = lambda; //VKTBD lambda is not correct
    lambda_chroma = lambda_luma;
    for (compIdx = 0; compIdx < N_C; compIdx++)
    {
        distBestPic[compIdx] = 0;
        rateBestPic[compIdx] = 0;
    }
    for (ctu = 0; ctu < NumCUsInFrame; ctu++)
    {
        //derive CTU width and height
        ctuYPos = (ctu / numLCUInPicWidth) * lcuHeight;
        ctuXPos = (ctu % numLCUInPicWidth) * lcuWidth;
        ctuHeight = (ctuYPos + lcuHeight > img_height) ? (img_height - ctuYPos) : lcuHeight;
        ctuWidth = (ctuXPos + lcuWidth  > img_width) ? (img_width - ctuXPos) : lcuWidth;
        //if the current CTU is the starting CTU at the slice, reset cabac
        if (ctu > 0)
        {
            int prev_ctuYPos = ((ctu - 1) / numLCUInPicWidth) * lcuHeight;
            int prev_ctuXPos = ((ctu - 1) % numLCUInPicWidth) * lcuWidth;
            s8 *map_patch_idx = ctx->map.map_patch_idx;
            int curr_mb_nr = (ctuYPos >> MIN_CU_LOG2) * (img_width >> MIN_CU_LOG2) + (ctuXPos >> MIN_CU_LOG2);
            int prev_mb_nr = (prev_ctuYPos >> MIN_CU_LOG2) * (img_width >> MIN_CU_LOG2) + (prev_ctuXPos >> MIN_CU_LOG2);
            int ctuCurr_sn = map_patch_idx[curr_mb_nr];
            int ctuPrev_sn = map_patch_idx[prev_mb_nr];
            if (ctuCurr_sn != ctuPrev_sn)
                enc_sbac_init(alf_bs_temp); // init sbac for alf rdo
        }
        //derive CTU boundary availabilities
        deriveBoundaryAvail(ctx, numLCUInPicWidth, numLCUInPicHeight, ctu, &isLeftAvail, &isRightAvail, &isAboveAvail,
                            &isBelowAvail, &isAboveLeftAvail, &isAboveRightAvail);
        for (compIdx = 0; compIdx < N_C; compIdx++)
        {
            //if slice-level enabled flag is 0, set CTB-level enabled flag 0
            if (alfPictureParam[compIdx]->alf_flag == 0)
            {
                Enc_ALF->m_AlfLCUEnabled[ctu][compIdx] = FALSE;
                continue;
            }
            if (!isRDOEstimate)
            {
                formatShift = (compIdx == Y_C) ? 0 : 1;
                pOrg = (compIdx == Y_C) ? imgY_org : imgUV_org[compIdx - U_C];
                pDec = (compIdx == Y_C) ? imgY_Dec : imgUV_Dec[compIdx - U_C];
                pRest = (compIdx == Y_C) ? imgY_Res : imgUV_Res[compIdx - U_C];
                stride_in = (compIdx == Y_C) ? (stride) : (stride >> 1);
            }
            else
            {
                formatShift = 0;
                stride_in = 0;
                pOrg = NULL;
                pDec = NULL;
                pRest = NULL;
            }

            //ALF on
            if (isRDOEstimate)
            {
                reconstructCoefInfo(compIdx, alfPictureParam[compIdx], Enc_ALF->m_filterCoeffSym, Enc_ALF->m_varIndTab);
                //distEnc is the estimated distortion reduction compared with filter-off case
                distEnc = estimateFilterDistortion(Enc_ALF, compIdx, alfCorr[compIdx][ctu], Enc_ALF->m_filterCoeffSym,
                                                   alfPictureParam[compIdx]->filters_per_group, Enc_ALF->m_varIndTab, FALSE)
                          - estimateFilterDistortion(Enc_ALF, compIdx, alfCorr[compIdx][ctu], NULL, 1, NULL, FALSE);
            }
            else
            {
                filterOneCTB(Enc_ALF, pRest, pDec, stride_in, compIdx, bit_depth, alfPictureParam[compIdx]
                             , ctuYPos, ctuHeight, ctuXPos, ctuWidth, isAboveAvail, isBelowAvail, isLeftAvail, isRightAvail, isAboveLeftAvail,
                             isAboveRightAvail);
                distEnc = calcAlfLCUDist(ctx, ALF_LOW_LANTENCY_ENC, compIdx
                                         , ctu, ctuYPos, ctuXPos, ctuHeight, ctuWidth, isAboveAvail, pOrg, pRest, stride_in, formatShift
                                        );
                distEnc -= calcAlfLCUDist(ctx, ALF_LOW_LANTENCY_ENC, compIdx
                                          , ctu, ctuYPos, ctuXPos, ctuHeight, ctuWidth, isAboveAvail,pOrg, pDec, stride_in, formatShift);
            }
            SBAC_LOAD((*alf_sbac), (core->s_alf_cu_ctr));
            ctx_idx = 0;
            rateEnc = enc_get_bit_number(alf_sbac);
            enc_eco_AlfLCUCtrl(alf_sbac, alf_bs_temp, 1);
            rateEnc = enc_get_bit_number(alf_sbac) - rateEnc;
            costEnc = (double)distEnc + (compIdx == 0 ? lambda_luma : lambda_chroma) * rateEnc;
            //ALF off
            distOff = 0;
            //rateOff = 1;
            SBAC_LOAD((*alf_sbac), (core->s_alf_cu_ctr));
            ctx_idx = 0;
            rateOff = enc_get_bit_number(alf_sbac);
            enc_eco_AlfLCUCtrl(alf_sbac, alf_bs_temp, 0);
            rateOff = enc_get_bit_number(alf_sbac) - rateOff;
            costOff = (double)distOff + (compIdx == 0 ? lambda_luma : lambda_chroma) * rateOff;
            //set CTB-level on/off flag
            Enc_ALF->m_AlfLCUEnabled[ctu][compIdx] = (costEnc < costOff) ? TRUE : FALSE;
            if (!isRDOEstimate && !Enc_ALF->m_AlfLCUEnabled[ctu][compIdx])
            {
                copyOneAlfBlk(pRest, pDec, stride_in, ctuYPos >> formatShift, ctuXPos >> formatShift, ctuHeight >> formatShift,
                              ctuWidth >> formatShift, isAboveAvail, isBelowAvail);
            }
            SBAC_LOAD((*alf_sbac), (core->s_alf_cu_ctr));
            ctx_idx = 0;
            rateOff = enc_get_bit_number(alf_sbac);
            enc_eco_AlfLCUCtrl(alf_sbac, alf_bs_temp, (Enc_ALF->m_AlfLCUEnabled[ctu][compIdx] ? 1 : 0));
            rateOff = enc_get_bit_number(alf_sbac) - rateOff;
            SBAC_STORE((core->s_alf_cu_ctr), (*alf_sbac));
            rateBestPic[compIdx] += (Enc_ALF->m_AlfLCUEnabled[ctu][compIdx] ? rateEnc : rateOff);
            distBestPic[compIdx] += (Enc_ALF->m_AlfLCUEnabled[ctu][compIdx] ? distEnc : distOff);
        } //CTB
    } //CTU
    if (isRDOEstimate || !ALF_LOW_LANTENCY_ENC)
    {
        for (compIdx = 0; compIdx < N_C; compIdx++)
        {
            if (alfPictureParam[compIdx]->alf_flag == 1)
            {
                costAlfOn = (double)distBestPic[compIdx] + (compIdx == 0 ? lambda_luma : lambda_chroma) * (rateBestPic[compIdx] +
                            (double)(ALFParamBitrateEstimate(alfPictureParam[compIdx])));
                costAlfOff = 0;
                if (costAlfOn >= costAlfOff)
                {
                    alfPictureParam[compIdx]->alf_flag = 0;
                    for (n = 0; n < NumCUsInFrame; n++)
                    {
                        Enc_ALF->m_AlfLCUEnabled[n][compIdx] = FALSE;
                    }
                    if (!isRDOEstimate)
                    {
                        formatShift = (compIdx == Y_C) ? 0 : 1;
                        pOrg = (compIdx == Y_C) ? imgY_org : imgUV_org[compIdx - U_C];
                        pDec = (compIdx == Y_C) ? imgY_Dec : imgUV_Dec[compIdx - U_C];
                        pRest = (compIdx == Y_C) ? imgY_Res : imgUV_Res[compIdx - U_C];
                        stride_in = (compIdx == Y_C) ? (stride) : (stride >> 1);
                        copyToImage(pRest, pDec, stride_in, img_height, img_width, formatShift);
                    }
                }
            }
        }
    }
    bestCost = 0;
    for (compIdx = 0; compIdx < N_C; compIdx++)
    {
        if (alfPictureParam[compIdx]->alf_flag == 1)
        {
            bestCost += (double)distBestPic[compIdx] + (compIdx == 0 ? lambda_luma : lambda_chroma) * (rateBestPic[compIdx]);
        }
    }
    //return the block-level RD cost
    return bestCost;
}

/*
*************************************************************************
* Function: ALF filter on CTB
*************************************************************************
*/
void filterOneCTB(EncALFVar *Enc_ALF, pel *pRest, pel *pDec, int stride, int compIdx, int bit_depth, ALFParam *alfParam, int ctuYPos, int ctuHeight,
                  int ctuXPos, int ctuWidth
                  , BOOL isAboveAvail, BOOL isBelowAvail, BOOL isLeftAvail, BOOL isRightAvail, BOOL isAboveLeftAvail,
                  BOOL isAboveRightAvail)
{
    //half size of 7x7cross+ 3x3square
    int  formatShift = (compIdx == Y_C) ? 0 : 1;
    int ypos, height, xpos, width;
    //reconstruct coefficients to m_filterCoeffSym and m_varIndTab
    reconstructCoefInfo(compIdx, alfParam, Enc_ALF->m_filterCoeffSym,
                        Enc_ALF->m_varIndTab); //reconstruct ALF coefficients & related parameters
    //derive CTB start positions, width, and height. If the boundary is not available, skip boundary samples.
    ypos = (ctuYPos >> formatShift);
    height = (ctuHeight >> formatShift);
    xpos = (ctuXPos >> formatShift);
    width = (ctuWidth >> formatShift);
    filterOneCompRegion(pRest, pDec, stride, (compIdx != Y_C), ypos, height, xpos, width, Enc_ALF->m_filterCoeffSym,
                        Enc_ALF->m_varIndTab, Enc_ALF->m_varImg, bit_depth, isLeftAvail, isRightAvail, isAboveAvail, isBelowAvail, isAboveLeftAvail, isAboveRightAvail);
}

long long calcAlfLCUDist(ENC_CTX *ctx, BOOL isSkipLCUBoundary, int compIdx, int lcuAddr, int ctuYPos, int ctuXPos, int ctuHeight,
                         int ctuWidth, BOOL isAboveAvail, pel *picSrc, pel *picCmp, int stride, int formatShift)
{
    EncALFVar *Enc_ALF = ctx->Enc_ALF;
    long long dist = 0;
    int  posOffset, ypos, xpos, height, width;
    pel *pelCmp;
    pel *pelSrc;
    BOOL notSkipLinesRightVB;
    BOOL notSkipLinesBelowVB = TRUE;
    int lcuHeight, lcuWidth, img_height, img_width;
    int NumCUsInFrame, numLCUInPicWidth, numLCUInPicHeight;
    lcuHeight = 1 << ctx->info.log2_max_cuwh;
    lcuWidth = lcuHeight;
    img_height = ctx->info.pic_height;
    img_width = ctx->info.pic_width;
    numLCUInPicWidth = img_width / lcuWidth;
    numLCUInPicHeight = img_height / lcuHeight;
    numLCUInPicWidth += (img_width % lcuWidth) ? 1 : 0;
    numLCUInPicHeight += (img_height % lcuHeight) ? 1 : 0;
    NumCUsInFrame = numLCUInPicHeight * numLCUInPicWidth;
    if (isSkipLCUBoundary)
    {
        if (lcuAddr + numLCUInPicWidth < NumCUsInFrame)
        {
            notSkipLinesBelowVB = FALSE;
        }
    }
    notSkipLinesRightVB = TRUE;
    if (isSkipLCUBoundary)
    {
        if ((lcuAddr + 1) % numLCUInPicWidth != 0)
        {
            notSkipLinesRightVB = FALSE;
        }
    }
    switch (compIdx)
    {
    case U_C:
    case V_C:
    {
        ypos = (ctuYPos >> formatShift);
        xpos = (ctuXPos >> formatShift);
        height = (ctuHeight >> formatShift);
        width = (ctuWidth >> formatShift);
        if (!notSkipLinesBelowVB)
        {
            height = height - (int)(DF_CHANGED_SIZE / 2) - (int)(ALF_FOOTPRINT_SIZE / 2);
        }
        if (!notSkipLinesRightVB)
        {
            width = width - (int)(DF_CHANGED_SIZE / 2) - (int)(ALF_FOOTPRINT_SIZE / 2);
        }
        if (isAboveAvail)
        {
            posOffset = ((ypos - 4) * stride) + xpos;
        }
        else
        {
            posOffset = (ypos * stride) + xpos;
        }
        pelCmp = picCmp + posOffset;
        pelSrc = picSrc + posOffset;
        dist += xCalcSSD(Enc_ALF, pelSrc, pelCmp, width, height, stride);
    }
    break;
    case Y_C:
    {
        ypos = (ctuYPos >> formatShift);
        xpos = (ctuXPos >> formatShift);
        height = (ctuHeight >> formatShift);
        width = (ctuWidth >> formatShift);
        if (!notSkipLinesBelowVB)
        {
            height = height - (int)(DF_CHANGED_SIZE)-(int)(ALF_FOOTPRINT_SIZE / 2);
        }
        if (!notSkipLinesRightVB)
        {
            width = width - (int)(DF_CHANGED_SIZE)-(int)(ALF_FOOTPRINT_SIZE / 2);
        }
        posOffset = (ypos * stride) + xpos;
        pelCmp = picCmp + posOffset;
        pelSrc = picSrc + posOffset;
        dist += xCalcSSD(Enc_ALF, pelSrc, pelCmp, width, height, stride);
    }
    break;
    default:
    {
        printf("not a legal component ID for ALF \n");
        assert(0);
        exit(-1);
    }
    }
    return dist;
}

void copyOneAlfBlk(pel *picDst, pel *picSrc, int stride, int ypos, int xpos, int height, int width, int isAboveAvail,
                   int isBelowAvail)
{
    int posOffset = (ypos * stride) + xpos;
    pel *pelDst;
    pel *pelSrc;
    int j;
    int startPos = isAboveAvail ? (ypos - 4) : ypos;
    int endPos = isBelowAvail ? (ypos + height - 4) : ypos + height;
    posOffset = (startPos * stride) + xpos;
    pelDst = picDst + posOffset;
    pelSrc = picSrc + posOffset;
    for (j = startPos; j < endPos; j++)
    {
        memcpy(pelDst, pelSrc, sizeof(pel)*width);
        pelDst += stride;
        pelSrc += stride;
    }
}

#if ALF_LOW_LANTENCY_ENC
void storeAlfTemporalLayerInfo(ENC_CTX *ctx, ALFParam **alfPictureParam, int temporalLayer, double lambda)
{
    EncALFVar *Enc_ALF = ctx->Enc_ALF;
    int compIdx, i;
    //store ALF parameters
    AlfCorrData *alfPicCorr[N_C];
    AlfCorrData *** alfPreCorr;
    AlfCorrData *alfPreCorrLCU;
    int lcuHeight, lcuWidth, img_height, img_width;
    int NumCUsInFrame, numLCUInPicWidth, numLCUInPicHeight;
    lcuHeight = 1 << ctx->info.log2_max_cuwh;
    lcuWidth = lcuHeight;
    img_height = ctx->info.pic_height;
    img_width = ctx->info.pic_width;
    numLCUInPicWidth = img_width / lcuWidth;
    numLCUInPicHeight = img_height / lcuHeight;
    numLCUInPicWidth += (img_width % lcuWidth) ? 1 : 0;
    numLCUInPicHeight += (img_height % lcuHeight) ? 1 : 0;
    NumCUsInFrame = numLCUInPicHeight * numLCUInPicWidth;
    assert(ALF_LOW_LANTENCY_ENC);
    for (compIdx = 0; compIdx < N_C; compIdx++)
    {
        AllocateAlfCorrData(&(alfPicCorr[compIdx]), compIdx);
    }
    accumulateLCUCorrelations(ctx, alfPicCorr, Enc_ALF->m_alfNonSkippedCorr, FALSE);
    decideAlfPictureParam(Enc_ALF, alfPictureParam, alfPicCorr, lambda);
    for (compIdx = 0; compIdx < N_C; compIdx++)
    {
        freeAlfCorrData(alfPicCorr[compIdx]);
    }
    //store correlations
    alfPreCorr = Enc_ALF->m_alfPrevCorr[temporalLayer];
    for (compIdx = 0; compIdx < N_C; compIdx++)
    {
        for (i = 0; i < NumCUsInFrame; i++)
        {
            alfPreCorrLCU = alfPreCorr[compIdx][i];
            reset_alfCorr(alfPreCorrLCU);
            //*alfPreCorrLCU += *(alfCorr[compIdx][i]);
            ADD_AlfCorrData(Enc_ALF->m_alfCorr[compIdx][i], alfPreCorrLCU, alfPreCorrLCU);
        }
    }
}
#endif

void ADD_AlfCorrData(AlfCorrData *A, AlfCorrData *B, AlfCorrData *C)
{
    int numCoef = ALF_MAX_NUM_COEF;
    int maxNumGroups = NO_VAR_BINS;
    int numGroups;
    int g, j, i;
    if (A->componentID >= 0)
    {
        numGroups = (A->componentID == Y_C) ? (maxNumGroups) : (1);
        for (g = 0; g < numGroups; g++)
        {
            C->pixAcc[g] = A->pixAcc[g] + B->pixAcc[g];
            for (j = 0; j < numCoef; j++)
            {
                C->yCorr[g][j] = A->yCorr[g][j] + B->yCorr[g][j];
                for (i = 0; i < numCoef; i++)
                {
                    C->ECorr[g][j][i] = A->ECorr[g][j][i] + B->ECorr[g][j][i];
                }
            }
        }
    }
}
void accumulateLCUCorrelations(ENC_CTX *ctx, AlfCorrData **alfCorrAcc, AlfCorrData *** alfCorSrcLCU, BOOL useAllLCUs)
{
    EncALFVar *Enc_ALF = ctx->Enc_ALF;
    int compIdx, numStatLCU, addr;
    AlfCorrData *alfCorrAccComp;
    int lcuHeight, lcuWidth, img_height, img_width;
    int NumCUsInFrame, numLCUInPicWidth, numLCUInPicHeight;
    lcuHeight = 1 << ctx->info.log2_max_cuwh;
    lcuWidth = lcuHeight;
    img_height = ctx->info.pic_height;
    img_width = ctx->info.pic_width;
    numLCUInPicWidth = img_width / lcuWidth;
    numLCUInPicHeight = img_height / lcuHeight;
    numLCUInPicWidth += (img_width % lcuWidth) ? 1 : 0;
    numLCUInPicHeight += (img_height % lcuHeight) ? 1 : 0;
    NumCUsInFrame = numLCUInPicHeight * numLCUInPicWidth;
    for (compIdx = 0; compIdx < N_C; compIdx++)
    {
        alfCorrAccComp = alfCorrAcc[compIdx];
        reset_alfCorr(alfCorrAccComp);
        if (!useAllLCUs)
        {
            numStatLCU = 0;
            for (addr = 0; addr < NumCUsInFrame; addr++)
            {
                if (Enc_ALF->m_AlfLCUEnabled[addr][compIdx])
                {
                    numStatLCU++;
                    break;
                }
            }
            if (numStatLCU == 0)
            {
                useAllLCUs = TRUE;
            }
        }
        for (addr = 0; addr < (int)NumCUsInFrame; addr++)
        {
            if (useAllLCUs || Enc_ALF->m_AlfLCUEnabled[addr][compIdx])
            {
                ADD_AlfCorrData(alfCorSrcLCU[compIdx][addr], alfCorrAccComp, alfCorrAccComp);
            }
        }
    }
}

void decideAlfPictureParam(EncALFVar *Enc_ALF, ALFParam **alfPictureParam, AlfCorrData **alfCorr, double lambdaLuma)
{
    double lambdaWeighting = (ALF_LOW_LANTENCY_ENC) ? (1.5) : (1.0);
    int compIdx;
    double lambda;
    ALFParam *alfParam;
    AlfCorrData *picCorr;
    for (compIdx = 0; compIdx < N_C; compIdx++)
    {
        //VKTBD chroma need different lambdas? lambdaWeighting needed?
        lambda = lambdaLuma * lambdaWeighting;
        alfParam = alfPictureParam[compIdx];
        picCorr = alfCorr[compIdx];
        alfParam->alf_flag = 1;
        deriveFilterInfo(Enc_ALF, compIdx, picCorr, alfParam, NO_VAR_BINS, lambda);
    }
}

void deriveFilterInfo(EncALFVar *Enc_ALF, int compIdx, AlfCorrData *alfCorr, ALFParam *alfFiltParam, int maxNumFilters, double lambda)
{
    int numCoeff = ALF_MAX_NUM_COEF;
    double coef[ALF_MAX_NUM_COEF];
    int lambdaForMerge, numFilters;
    switch (compIdx)
    {
    case Y_C:
    {
        lambdaForMerge = ((int)lambda) * (1 << (2 * Enc_ALF->m_uiBitIncrement));
        memset(Enc_ALF->m_varIndTab, 0, sizeof(int)*NO_VAR_BINS);
        xfindBestFilterVarPred(Enc_ALF, alfCorr->yCorr, alfCorr->ECorr, alfCorr->pixAcc, Enc_ALF->m_filterCoeffSym, &numFilters,
                               Enc_ALF->m_varIndTab, lambdaForMerge, maxNumFilters);
        xcodeFiltCoeff(Enc_ALF->m_filterCoeffSym, Enc_ALF->m_varIndTab, numFilters, alfFiltParam);
    }
    break;
    case U_C:
    case V_C:
    {
        alfFiltParam->filters_per_group = 1;
        gnsSolveByChol(alfCorr->ECorr[0], alfCorr->yCorr[0], coef, numCoeff);
        xQuantFilterCoef(coef, Enc_ALF->m_filterCoeffSym[0]);
        memcpy(alfFiltParam->coeffmulti[0], Enc_ALF->m_filterCoeffSym[0], sizeof(int)*numCoeff);
        predictALFCoeff(alfFiltParam->coeffmulti, numCoeff, alfFiltParam->filters_per_group);
    }
    break;
    default:
    {
        printf("Not a legal component ID\n");
        assert(0);
        exit(-1);
    }
    }
}

void xcodeFiltCoeff(int **filterCoeff, int *varIndTab, int numFilters, ALFParam *alfParam)
{
    int filterPattern[NO_VAR_BINS], startSecondFilter = 0, i, g;
    memset(filterPattern, 0, NO_VAR_BINS * sizeof(int));
    alfParam->num_coeff = (int)ALF_MAX_NUM_COEF;
    alfParam->filters_per_group = numFilters;
    //merge table assignment
    if (alfParam->filters_per_group > 1)
    {
        for (i = 1; i < NO_VAR_BINS; ++i)
        {
            if (varIndTab[i] != varIndTab[i - 1])
            {
                filterPattern[i] = 1;
                startSecondFilter = i;
            }
        }
    }
    memcpy(alfParam->filterPattern, filterPattern, NO_VAR_BINS * sizeof(int));
    //coefficient prediction
    for (g = 0; g < alfParam->filters_per_group; g++)
    {
        for (i = 0; i < alfParam->num_coeff; i++)
        {
            alfParam->coeffmulti[g][i] = filterCoeff[g][i];
        }
    }
    predictALFCoeff(alfParam->coeffmulti, alfParam->num_coeff, alfParam->filters_per_group);
}

void xQuantFilterCoef(double *h, int *qh)
{
    int i, N;
    int max_value, min_value;
    double dbl_total_gain;
    int total_gain, q_total_gain;
    int upper, lower;
    double *dh;
    int  *nc;
    const int    *pFiltMag;
    N = (int)ALF_MAX_NUM_COEF;
    pFiltMag = weightsShape1Sym;
    dh = (double *)malloc(N * sizeof(double));
    nc = (int *)malloc(N * sizeof(int));
    max_value = (1 << (1 + ALF_NUM_BIT_SHIFT)) - 1;
    min_value = 0 - (1 << (1 + ALF_NUM_BIT_SHIFT));
    dbl_total_gain = 0.0;
    q_total_gain = 0;
    for (i = 0; i < N; i++)
    {
        if (h[i] >= 0.0)
        {
            qh[i] = (int)(h[i] * (1 << ALF_NUM_BIT_SHIFT) + 0.5);
        }
        else
        {
            qh[i] = -(int)(-h[i] * (1 << ALF_NUM_BIT_SHIFT) + 0.5);
        }
        dh[i] = (double)qh[i] / (double)(1 << ALF_NUM_BIT_SHIFT) - h[i];
        dh[i] *= pFiltMag[i];
        dbl_total_gain += h[i] * pFiltMag[i];
        q_total_gain += qh[i] * pFiltMag[i];
        nc[i] = i;
    }
    // modification of quantized filter coefficients
    total_gain = (int)(dbl_total_gain * (1 << ALF_NUM_BIT_SHIFT) + 0.5);
    if (q_total_gain != total_gain)
    {
        xFilterCoefQuickSort(dh, nc, 0, N - 1);
        if (q_total_gain > total_gain)
        {
            upper = N - 1;
            while (q_total_gain > total_gain + 1)
            {
                i = nc[upper % N];
                qh[i]--;
                q_total_gain -= pFiltMag[i];
                upper--;
            }
            if (q_total_gain == total_gain + 1)
            {
                if (dh[N - 1] > 0)
                {
                    qh[N - 1]--;
                }
                else
                {
                    i = nc[upper % N];
                    qh[i]--;
                    qh[N - 1]++;
                }
            }
        }
        else if (q_total_gain < total_gain)
        {
            lower = 0;
            while (q_total_gain < total_gain - 1)
            {
                i = nc[lower % N];
                qh[i]++;
                q_total_gain += pFiltMag[i];
                lower++;
            }
            if (q_total_gain == total_gain - 1)
            {
                if (dh[N - 1] < 0)
                {
                    qh[N - 1]++;
                }
                else
                {
                    i = nc[lower % N];
                    qh[i]++;
                    qh[N - 1]--;
                }
            }
        }
    }
    // set of filter coefficients
    for (i = 0; i < N; i++)
    {
        qh[i] = max(min_value, min(max_value, qh[i]));
    }
    checkFilterCoeffValue(qh, N);
    free(dh);
    dh = NULL;
    free(nc);
    nc = NULL;
}
void xFilterCoefQuickSort(double *coef_data, int *coef_num, int upper, int lower)
{
    double mid, tmp_data;
    int i, j, tmp_num;
    i = upper;
    j = lower;
    mid = coef_data[(lower + upper) >> 1];
    do
    {
        while (coef_data[i] < mid)
        {
            i++;
        }
        while (mid < coef_data[j])
        {
            j--;
        }
        if (i <= j)
        {
            tmp_data = coef_data[i];
            tmp_num = coef_num[i];
            coef_data[i] = coef_data[j];
            coef_num[i] = coef_num[j];
            coef_data[j] = tmp_data;
            coef_num[j] = tmp_num;
            i++;
            j--;
        }
    }
    while (i <= j);
    if (upper < j)
    {
        xFilterCoefQuickSort(coef_data, coef_num, upper, j);
    }
    if (i < lower)
    {
        xFilterCoefQuickSort(coef_data, coef_num, i, lower);
    }
}
void xfindBestFilterVarPred(EncALFVar *Enc_ALF, double **ySym, double ***ESym, double *pixAcc, int **filterCoeffSym,
                            int *filters_per_fr_best, int varIndTab[], double lambda_val, int numMaxFilters)
{
    static BOOL isFirst = TRUE;
    static int *filterCoeffSymQuant[NO_VAR_BINS];
    int filters_per_fr, firstFilt, interval[NO_VAR_BINS][2], intervalBest[NO_VAR_BINS][2];
    int i, g;
    double  lagrangian, lagrangianMin;
    int sqrFiltLength;
    int *weights;
    double errorForce0CoeffTab[NO_VAR_BINS][2];
    {
        for (g = 0; g < NO_VAR_BINS; g++)
        {
            filterCoeffSymQuant[g] = (int *)malloc(ALF_MAX_NUM_COEF * sizeof(int));
        }
        isFirst = FALSE;
    }
    sqrFiltLength = (int)ALF_MAX_NUM_COEF;
    weights = weightsShape1Sym;
    // zero all variables
    memset(varIndTab, 0, sizeof(int)*NO_VAR_BINS);
    for (i = 0; i < NO_VAR_BINS; i++)
    {
        memset(filterCoeffSym[i], 0, sizeof(int)*ALF_MAX_NUM_COEF);
        memset(filterCoeffSymQuant[i], 0, sizeof(int)*ALF_MAX_NUM_COEF);
    }
    firstFilt = 1;
    lagrangianMin = 0;
    filters_per_fr = NO_VAR_BINS;
    while (filters_per_fr >= 1)
    {
        mergeFiltersGreedy(Enc_ALF, ySym, ESym, pixAcc, interval, sqrFiltLength, filters_per_fr);
        findFilterCoeff(Enc_ALF, ESym, ySym, pixAcc, filterCoeffSym, filterCoeffSymQuant, interval,
                        varIndTab, sqrFiltLength, filters_per_fr, weights, errorForce0CoeffTab);
        lagrangian = xfindBestCoeffCodMethod(filterCoeffSymQuant, sqrFiltLength, filters_per_fr,
                                             errorForce0CoeffTab, lambda_val);
        if (lagrangian < lagrangianMin || firstFilt == 1 || filters_per_fr == numMaxFilters)
        {
            firstFilt = 0;
            lagrangianMin = lagrangian;
            (*filters_per_fr_best) = filters_per_fr;
            memcpy(intervalBest, interval, NO_VAR_BINS * 2 * sizeof(int));
        }
        filters_per_fr--;
    }
    findFilterCoeff(Enc_ALF, ESym, ySym, pixAcc, filterCoeffSym, filterCoeffSymQuant, intervalBest,
                    varIndTab, sqrFiltLength, (*filters_per_fr_best), weights, errorForce0CoeffTab);
    if (*filters_per_fr_best == 1)
    {
        memset(varIndTab, 0, sizeof(int)*NO_VAR_BINS);
    }
    for (g = 0; g < NO_VAR_BINS; g++)
    {
        free(filterCoeffSymQuant[g]);// = (int*)malloc(ALF_MAX_NUM_COEF * sizeof(int));
    }
}

double mergeFiltersGreedy(EncALFVar *Enc_ALF, double **yGlobalSeq, double ***EGlobalSeq, double *pixAccGlobalSeq,
                          int intervalBest[NO_VAR_BINS][2], int sqrFiltLength, int noIntervals)
{
    int first, ind, ind1, ind2, i, j, bestToMerge;
    double error, error1, error2, errorMin;
    static double pixAcc_temp, error_tab[NO_VAR_BINS], error_comb_tab[NO_VAR_BINS];
    static int indexList[NO_VAR_BINS], available[NO_VAR_BINS], noRemaining;
    if (noIntervals == NO_VAR_BINS)
    {
        noRemaining = NO_VAR_BINS;
        for (ind = 0; ind < NO_VAR_BINS; ind++)
        {
            indexList[ind] = ind;
            available[ind] = 1;
            Enc_ALF->m_pixAcc_merged[ind] = pixAccGlobalSeq[ind];
            memcpy(Enc_ALF->m_y_merged[ind], yGlobalSeq[ind], sizeof(double)*sqrFiltLength);
            for (i = 0; i < sqrFiltLength; i++)
            {
                memcpy(Enc_ALF->m_E_merged[ind][i], EGlobalSeq[ind][i], sizeof(double)*sqrFiltLength);
            }
        }
    }
    // Try merging different matrices
    if (noIntervals == NO_VAR_BINS)
    {
        for (ind = 0; ind < NO_VAR_BINS; ind++)
        {
            error_tab[ind] = calculateErrorAbs(Enc_ALF->m_E_merged[ind], Enc_ALF->m_y_merged[ind], Enc_ALF->m_pixAcc_merged[ind],
                                               sqrFiltLength);
        }
        for (ind = 0; ind < NO_VAR_BINS - 1; ind++)
        {
            ind1 = indexList[ind];
            ind2 = indexList[ind + 1];
            error1 = error_tab[ind1];
            error2 = error_tab[ind2];
            pixAcc_temp = Enc_ALF->m_pixAcc_merged[ind1] + Enc_ALF->m_pixAcc_merged[ind2];
            for (i = 0; i < sqrFiltLength; i++)
            {
                Enc_ALF->m_y_temp[i] = Enc_ALF->m_y_merged[ind1][i] + Enc_ALF->m_y_merged[ind2][i];
                for (j = 0; j < sqrFiltLength; j++)
                {
                    Enc_ALF->m_E_temp[i][j] = Enc_ALF->m_E_merged[ind1][i][j] + Enc_ALF->m_E_merged[ind2][i][j];
                }
            }
            error_comb_tab[ind1] = calculateErrorAbs(Enc_ALF->m_E_temp, Enc_ALF->m_y_temp, pixAcc_temp,
                                   sqrFiltLength) - error1 - error2;
        }
    }
    while (noRemaining > noIntervals)
    {
        errorMin = 0;
        first = 1;
        bestToMerge = 0;
        for (ind = 0; ind < noRemaining - 1; ind++)
        {
            error = error_comb_tab[indexList[ind]];
            if ((error < errorMin || first == 1))
            {
                errorMin = error;
                bestToMerge = ind;
                first = 0;
            }
        }
        ind1 = indexList[bestToMerge];
        ind2 = indexList[bestToMerge + 1];
        Enc_ALF->m_pixAcc_merged[ind1] += Enc_ALF->m_pixAcc_merged[ind2];
        for (i = 0; i < sqrFiltLength; i++)
        {
            Enc_ALF->m_y_merged[ind1][i] += Enc_ALF->m_y_merged[ind2][i];
            for (j = 0; j < sqrFiltLength; j++)
            {
                Enc_ALF->m_E_merged[ind1][i][j] += Enc_ALF->m_E_merged[ind2][i][j];
            }
        }
        available[ind2] = 0;
        //update error tables
        error_tab[ind1] = error_comb_tab[ind1] + error_tab[ind1] + error_tab[ind2];
        if (indexList[bestToMerge] > 0)
        {
            ind1 = indexList[bestToMerge - 1];
            ind2 = indexList[bestToMerge];
            error1 = error_tab[ind1];
            error2 = error_tab[ind2];
            pixAcc_temp = Enc_ALF->m_pixAcc_merged[ind1] + Enc_ALF->m_pixAcc_merged[ind2];
            for (i = 0; i < sqrFiltLength; i++)
            {
                Enc_ALF->m_y_temp[i] = Enc_ALF->m_y_merged[ind1][i] + Enc_ALF->m_y_merged[ind2][i];
                for (j = 0; j < sqrFiltLength; j++)
                {
                    Enc_ALF->m_E_temp[i][j] = Enc_ALF->m_E_merged[ind1][i][j] + Enc_ALF->m_E_merged[ind2][i][j];
                }
            }
            error_comb_tab[ind1] = calculateErrorAbs(Enc_ALF->m_E_temp, Enc_ALF->m_y_temp, pixAcc_temp,
                                   sqrFiltLength) - error1 - error2;
        }
        if (indexList[bestToMerge + 1] < NO_VAR_BINS - 1)
        {
            ind1 = indexList[bestToMerge];
            ind2 = indexList[bestToMerge + 2];
            error1 = error_tab[ind1];
            error2 = error_tab[ind2];
            pixAcc_temp = Enc_ALF->m_pixAcc_merged[ind1] + Enc_ALF->m_pixAcc_merged[ind2];
            for (i = 0; i < sqrFiltLength; i++)
            {
                Enc_ALF->m_y_temp[i] = Enc_ALF->m_y_merged[ind1][i] + Enc_ALF->m_y_merged[ind2][i];
                for (j = 0; j < sqrFiltLength; j++)
                {
                    Enc_ALF->m_E_temp[i][j] = Enc_ALF->m_E_merged[ind1][i][j] + Enc_ALF->m_E_merged[ind2][i][j];
                }
            }
            error_comb_tab[ind1] = calculateErrorAbs(Enc_ALF->m_E_temp, Enc_ALF->m_y_temp, pixAcc_temp,
                                   sqrFiltLength) - error1 - error2;
        }
        ind = 0;
        for (i = 0; i < NO_VAR_BINS; i++)
        {
            if (available[i] == 1)
            {
                indexList[ind] = i;
                ind++;
            }
        }
        noRemaining--;
    }
    errorMin = 0;
    for (ind = 0; ind < noIntervals; ind++)
    {
        errorMin += error_tab[indexList[ind]];
    }
    for (ind = 0; ind < noIntervals - 1; ind++)
    {
        intervalBest[ind][0] = indexList[ind];
        intervalBest[ind][1] = indexList[ind + 1] - 1;
    }
    intervalBest[noIntervals - 1][0] = indexList[noIntervals - 1];
    intervalBest[noIntervals - 1][1] = NO_VAR_BINS - 1;
    return (errorMin);
}

double calculateErrorAbs(double **A, double *b, double y, int size)
{
    int i;
    double error, sum;
    double c[ALF_MAX_NUM_COEF];
    gnsSolveByChol(A, b, c, size);
    sum = 0;
    for (i = 0; i < size; i++)
    {
        sum += c[i] * b[i];
    }
    error = y - sum;
    return error;
}
int gnsCholeskyDec(double **inpMatr, double outMatr[ALF_MAX_NUM_COEF][ALF_MAX_NUM_COEF], int noEq)
{
    int i, j, k;     /* Looping Variables */
    double scale;       /* scaling factor for each row */
    double invDiag[ALF_MAX_NUM_COEF];  /* Vector of the inverse of diagonal entries of outMatr */
    //  Cholesky decomposition starts
    for (i = 0; i < noEq; i++)
    {
        for (j = i; j < noEq; j++)
        {
            /* Compute the scaling factor */
            scale = inpMatr[i][j];
            if (i > 0)
            {
                for (k = i - 1; k >= 0; k--)
                {
                    scale -= outMatr[k][j] * outMatr[k][i];
                }
            }
            /* Compute i'th row of outMatr */
            if (i == j)
            {
                if (scale <= REG_SQR)    // if(scale <= 0 )  /* If inpMatr is singular */
                {
                    return 0;
                }
                else
                {
                    /* Normal operation */
                    invDiag[i] = 1.0 / (outMatr[i][i] = sqrt(scale));
                }
            }
            else
            {
                outMatr[i][j] = scale * invDiag[i]; /* Upper triangular part          */
                outMatr[j][i] = 0.0;              /* Lower triangular part set to 0 */
            }
        }
    }
    return 1; /* Signal that Cholesky factorization is successfully performed */
}

void gnsTransposeBacksubstitution(double U[ALF_MAX_NUM_COEF][ALF_MAX_NUM_COEF], double rhs[], double x[], int order)
{
    int i, j;             /* Looping variables */
    double sum;              /* Holds backsubstitution from already handled rows */
    /* Backsubstitution starts */
    x[0] = rhs[0] / U[0][0];               /* First row of U'                   */
    for (i = 1; i < order; i++)
    {
        /* For the rows 1..order-1           */
        for (j = 0, sum = 0.0; j < i; j++)   /* Backsubst already solved unknowns */
        {
            sum += x[j] * U[j][i];
        }
        x[i] = (rhs[i] - sum) / U[i][i];       /* i'th component of solution vect.  */
    }
}
void gnsBacksubstitution(double R[ALF_MAX_NUM_COEF][ALF_MAX_NUM_COEF], double z[ALF_MAX_NUM_COEF], int R_size,
                         double A[ALF_MAX_NUM_COEF])
{
    int i, j;
    double sum;
    R_size--;
    A[R_size] = z[R_size] / R[R_size][R_size];
    for (i = R_size - 1; i >= 0; i--)
    {
        for (j = i + 1, sum = 0.0; j <= R_size; j++)
        {
            sum += R[i][j] * A[j];
        }
        A[i] = (z[i] - sum) / R[i][i];
    }
}
int gnsSolveByChol(double **LHS, double *rhs, double *x, int noEq)
{
    double aux[ALF_MAX_NUM_COEF];     /* Auxiliary vector */
    double U[ALF_MAX_NUM_COEF][ALF_MAX_NUM_COEF];    /* Upper triangular Cholesky factor of LHS */
    int  i, singular;          /* Looping variable */
    assert(noEq > 0);
    /* The equation to be solved is LHSx = rhs */
    /* Compute upper triangular U such that U'*U = LHS */
    if (gnsCholeskyDec(LHS, U, noEq))   /* If Cholesky decomposition has been successful */
    {
        singular = 1;
        /* Now, the equation is  U'*U*x = rhs, where U is upper triangular
        * Solve U'*aux = rhs for aux
        */
        gnsTransposeBacksubstitution(U, rhs, aux, noEq);
        /* The equation is now U*x = aux, solve it for x (new motion coefficients) */
        gnsBacksubstitution(U, aux, noEq, x);
    }
    else   /* LHS was singular */
    {
        singular = 0;
        /* Regularize LHS */
        for (i = 0; i < noEq; i++)
        {
            LHS[i][i] += REG;
        }
        /* Compute upper triangular U such that U'*U = regularized LHS */
        singular = gnsCholeskyDec(LHS, U, noEq);
        if (singular == 1)
        {
            /* Solve  U'*aux = rhs for aux */
            gnsTransposeBacksubstitution(U, rhs, aux, noEq);
            /* Solve U*x = aux for x */
            gnsBacksubstitution(U, aux, noEq, x);
        }
        else
        {
            x[0] = 1.0;
            for (i = 1; i < noEq; i++)
            {
                x[i] = 0.0;
            }
        }
    }
    return singular;
}



double findFilterCoeff(EncALFVar *Enc_ALF, double ***EGlobalSeq, double **yGlobalSeq, double *pixAccGlobalSeq, int **filterCoeffSeq,
                       int **filterCoeffQuantSeq, int intervalBest[NO_VAR_BINS][2], int varIndTab[NO_VAR_BINS], int sqrFiltLength,
                       int filters_per_fr, int *weights, double errorTabForce0Coeff[NO_VAR_BINS][2])
{
    static double pixAcc_temp;
    static BOOL isFirst = TRUE;
    static int *filterCoeffQuant = NULL;
    static double *filterCoeff = NULL;
    double error;
    int k, filtNo;
    {
        get_mem1Dint(&filterCoeffQuant, ALF_MAX_NUM_COEF);
        filterCoeff = (double *)malloc(ALF_MAX_NUM_COEF * sizeof(double));
        isFirst = FALSE;
    }
    error = 0;
    for (filtNo = 0; filtNo < filters_per_fr; filtNo++)
    {
        add_A(Enc_ALF->m_E_temp, EGlobalSeq, intervalBest[filtNo][0], intervalBest[filtNo][1], sqrFiltLength);
        add_b(Enc_ALF->m_y_temp, yGlobalSeq, intervalBest[filtNo][0], intervalBest[filtNo][1], sqrFiltLength);
        pixAcc_temp = 0;
        for (k = intervalBest[filtNo][0]; k <= intervalBest[filtNo][1]; k++)
        {
            pixAcc_temp += pixAccGlobalSeq[k];
        }
        // Find coeffcients
        errorTabForce0Coeff[filtNo][1] = pixAcc_temp + QuantizeIntegerFilterPP(filterCoeff, filterCoeffQuant, Enc_ALF->m_E_temp,
                                         Enc_ALF->m_y_temp, sqrFiltLength, weights);
        errorTabForce0Coeff[filtNo][0] = pixAcc_temp;
        error += errorTabForce0Coeff[filtNo][1];
        for (k = 0; k < sqrFiltLength; k++)
        {
            filterCoeffSeq[filtNo][k] = filterCoeffQuant[k];
            filterCoeffQuantSeq[filtNo][k] = filterCoeffQuant[k];
        }
    }
    for (filtNo = 0; filtNo < filters_per_fr; filtNo++)
    {
        for (k = intervalBest[filtNo][0]; k <= intervalBest[filtNo][1]; k++)
        {
            varIndTab[k] = filtNo;
        }
    }
    free_mem1Dint(filterCoeffQuant);
    free(filterCoeff);
    return (error);
}
void add_A(double **Amerged, double ***A, int start, int stop, int size)
{
    int i, j, ind;          /* Looping variable */
    for (i = 0; i < size; i++)
    {
        for (j = 0; j < size; j++)
        {
            Amerged[i][j] = 0;
            for (ind = start; ind <= stop; ind++)
            {
                Amerged[i][j] += A[ind][i][j];
            }
        }
    }
}

void add_b(double *bmerged, double **b, int start, int stop, int size)
{
    int i, ind;          /* Looping variable */
    for (i = 0; i < size; i++)
    {
        bmerged[i] = 0;
        for (ind = start; ind <= stop; ind++)
        {
            bmerged[i] += b[ind][i];
        }
    }
}
void roundFiltCoeff(int *FilterCoeffQuan, double *FilterCoeff, int sqrFiltLength, int factor)
{
    int i;
    double diff;
    int diffInt, sign;
    for (i = 0; i < sqrFiltLength; i++)
    {
        sign = (FilterCoeff[i] > 0) ? 1 : -1;
        diff = FilterCoeff[i] * sign;
        diffInt = (int)(diff * (double)factor + 0.5);
        FilterCoeffQuan[i] = diffInt * sign;
    }
}
double calculateErrorCoeffProvided(double **A, double *b, double *c, int size)
{
    int i, j;
    double error, sum = 0;
    error = 0;
    for (i = 0; i < size; i++)   //diagonal
    {
        sum = 0;
        for (j = i + 1; j < size; j++)
        {
            sum += (A[j][i] + A[i][j]) * c[j];
        }
        error += (A[i][i] * c[i] + sum - 2 * b[i]) * c[i];
    }
    return error;
}
double QuantizeIntegerFilterPP(double *filterCoeff, int *filterCoeffQuant, double **E, double *y, int sqrFiltLength,
                               int *weights)
{
    double error;
    static BOOL isFirst = TRUE;
    static int *filterCoeffQuantMod = NULL;
    int factor = (1 << ((int)ALF_NUM_BIT_SHIFT));
    int i;
    int quantCoeffSum, minInd, targetCoeffSumInt, k, diff;
    double targetCoeffSum, errMin;
    {
        //filterCoeffQuantMod = new int[ALF_MAX_NUM_COEF];
        get_mem1Dint(&filterCoeffQuantMod, ALF_MAX_NUM_COEF);
        isFirst = FALSE;
    }
    gnsSolveByChol(E, y, filterCoeff, sqrFiltLength);
    targetCoeffSum = 0;
    for (i = 0; i < sqrFiltLength; i++)
    {
        targetCoeffSum += (weights[i] * filterCoeff[i] * factor);
    }
    targetCoeffSumInt = ROUND(targetCoeffSum);
    roundFiltCoeff(filterCoeffQuant, filterCoeff, sqrFiltLength, factor);
    quantCoeffSum = 0;
    for (i = 0; i < sqrFiltLength; i++)
    {
        quantCoeffSum += weights[i] * filterCoeffQuant[i];
    }
    while (quantCoeffSum != targetCoeffSumInt)
    {
        if (quantCoeffSum > targetCoeffSumInt)
        {
            diff = quantCoeffSum - targetCoeffSumInt;
            errMin = 0;
            minInd = -1;
            for (k = 0; k < sqrFiltLength; k++)
            {
                if (weights[k] <= diff)
                {
                    for (i = 0; i < sqrFiltLength; i++)
                    {
                        filterCoeffQuantMod[i] = filterCoeffQuant[i];
                    }
                    filterCoeffQuantMod[k]--;
                    for (i = 0; i < sqrFiltLength; i++)
                    {
                        filterCoeff[i] = (double)filterCoeffQuantMod[i] / (double)factor;
                    }
                    error = calculateErrorCoeffProvided(E, y, filterCoeff, sqrFiltLength);
                    if (error < errMin || minInd == -1)
                    {
                        errMin = error;
                        minInd = k;
                    }
                } // if (weights(k)<=diff)
            } // for (k=0; k<sqrFiltLength; k++)
            filterCoeffQuant[minInd]--;
        }
        else
        {
            diff = targetCoeffSumInt - quantCoeffSum;
            errMin = 0;
            minInd = -1;
            for (k = 0; k < sqrFiltLength; k++)
            {
                if (weights[k] <= diff)
                {
                    for (i = 0; i < sqrFiltLength; i++)
                    {
                        filterCoeffQuantMod[i] = filterCoeffQuant[i];
                    }
                    filterCoeffQuantMod[k]++;
                    for (i = 0; i < sqrFiltLength; i++)
                    {
                        filterCoeff[i] = (double)filterCoeffQuantMod[i] / (double)factor;
                    }
                    error = calculateErrorCoeffProvided(E, y, filterCoeff, sqrFiltLength);
                    if (error < errMin || minInd == -1)
                    {
                        errMin = error;
                        minInd = k;
                    }
                } // if (weights(k)<=diff)
            } // for (k=0; k<sqrFiltLength; k++)
            filterCoeffQuant[minInd]++;
        }
        quantCoeffSum = 0;
        for (i = 0; i < sqrFiltLength; i++)
        {
            quantCoeffSum += weights[i] * filterCoeffQuant[i];
        }
    }
    checkFilterCoeffValue(filterCoeffQuant, sqrFiltLength);
    for (i = 0; i < sqrFiltLength; i++)
    {
        filterCoeff[i] = (double)filterCoeffQuant[i] / (double)factor;
    }
    error = calculateErrorCoeffProvided(E, y, filterCoeff, sqrFiltLength);
    free_mem1Dint(filterCoeffQuantMod);
    return (error);
}

double xfindBestCoeffCodMethod(int **filterCoeffSymQuant, int sqrFiltLength, int filters_per_fr,
                               double errorForce0CoeffTab[NO_VAR_BINS][2], double lambda)
{
    int coeffBits, i;
    double error = 0, lagrangian;
    static BOOL  isFirst = TRUE;
    static int **coeffmulti = NULL;
    int g;
    {
        get_mem2Dint(&coeffmulti, NO_VAR_BINS, ALF_MAX_NUM_COEF);
        isFirst = FALSE;
    }
    for (g = 0; g < filters_per_fr; g++)
    {
        for (i = 0; i < sqrFiltLength; i++)
        {
            coeffmulti[g][i] = filterCoeffSymQuant[g][i];
        }
    }
    predictALFCoeff(coeffmulti, sqrFiltLength, filters_per_fr);
    coeffBits = 0;
    for (g = 0; g < filters_per_fr; g++)
    {
        coeffBits += filterCoeffBitrateEstimate(coeffmulti[g]);
    }
    for (i = 0; i < filters_per_fr; i++)
    {
        error += errorForce0CoeffTab[i][1];
    }
    lagrangian = error + lambda * coeffBits;
    free_mem2Dint(coeffmulti);
    return (lagrangian);
}

void predictALFCoeff(int **coeff, int numCoef, int numFilters)
{
    int g, pred, sum, i;
    for (g = 0; g < numFilters; g++)
    {
        sum = 0;
        for (i = 0; i < numCoef - 1; i++)
        {
            sum += (2 * coeff[g][i]);
        }
        pred = (1 << ALF_NUM_BIT_SHIFT) - (sum);
        coeff[g][numCoef - 1] = coeff[g][numCoef - 1] - pred;
    }
}

long long xCalcSSD(EncALFVar *Enc_ALF, pel *pOrg, pel *pCmp, int iWidth, int iHeight, int iStride)
{
    long long uiSSD = 0;
    int x, y;
    unsigned int  uiShift = Enc_ALF->m_uiBitIncrement << 1;
    int iTemp;
    for (y = 0; y < iHeight; y++)
    {
        for (x = 0; x < iWidth; x++)
        {
            iTemp = pOrg[x] - pCmp[x];
            uiSSD += (iTemp * iTemp) >> uiShift;
        }
        pOrg += iStride;
        pCmp += iStride;
    }
    return uiSSD;;
}

void copyToImage(pel *pDest, pel *pSrc, int stride_in, int img_height, int img_width, int formatShift)
{
    int j, width, height;
    pel *pdst;
    pel *psrc;
    height = img_height >> formatShift;
    width = img_width >> formatShift;
    psrc = pSrc;
    pdst = pDest;
    for (j = 0; j < height; j++)
    {
        memcpy(pdst, psrc, width * sizeof(pel));
        pdst = pdst + stride_in;
        psrc = psrc + stride_in;
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
    {
        printf("Not a legal component ID\n");
        assert(0);
        exit(-1);
    }
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
    {
        printf("Not a legal component ID\n");
        assert(0);
        exit(-1);
    }
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
        {
            printf("Not a legal component ID\n");
            assert(0);
            exit(-1);
        }
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
        {
            printf("Not a legal component ID\n");
            assert(0);
            exit(-1);
        }
        }
    }
}


void deriveBoundaryAvail(ENC_CTX *ctx, int numLCUInPicWidth, int numLCUInPicHeight, int ctu, BOOL *isLeftAvail,
                         BOOL *isRightAvail, BOOL *isAboveAvail, BOOL *isBelowAvail, BOOL *isAboveLeftAvail,
                         BOOL *isAboveRightAvail)
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
    int  cuAboveLeft;
    int  cuAboveRight;
    int curSliceNr, neighorSliceNr;
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

void ALF_BitStreamCopy(COM_BSW *dst, COM_BSW *src)
{
    dst->code = src->code;
    dst->leftbits = src->leftbits;
    dst->cur = dst->beg + (src->cur - src->beg);
    dst->fn_flush = src->fn_flush;
    for (int i = 0; i<4; i++)
    {
        dst->ndata[i] = src->ndata[i];
        dst->pdata[i] = src->pdata[i];
    }
    assert(dst->size >= src->size);
    memcpy(dst->beg, src->beg, src->size * sizeof(u8));
}