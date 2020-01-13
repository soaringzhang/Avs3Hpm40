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

#ifndef ENC_ENCADAPTIVELOOPFILTER_H
#define ENC_ENCADAPTIVELOOPFILTER_H
#include "com_def.h"
#include  "enc_def.h"
#include "com_ComAdaptiveLoopFilter.h"
#include <math.h>

void copyToImage(pel *pDest, pel *pSrc, int stride_in, int img_height, int img_width, int formatShift);
void reset_alfCorr(AlfCorrData *alfCorr);
unsigned int uvlcBitrateEstimate(int val);
void AllocateAlfCorrData(AlfCorrData **dst, int cIdx);
void freeAlfCorrData(AlfCorrData *dst);
void setCurAlfParam(ENC_CTX *ctx, COM_BSW *alf_bs_temp, ALFParam **alfPictureParam, double lambda);
void ALFProcess(ENC_CTX* ctx, COM_BSW *alf_bs_temp, ALFParam **alfPictureParam, COM_PIC * pic_rec, COM_PIC * pic_alf_Org, COM_PIC * pic_alf_Rec, double lambda_mode);
void getStatistics(ENC_CTX* ctx, pel *imgY_org, pel **imgUV_org, pel *yuvExtRecY, pel **yuvExtRecUV,
                   int stride);
void deriveLoopFilterBoundaryAvailibility(ENC_CTX* ctx, int numLCUPicWidth, int numLCUPicHeight, int ctu,
        BOOL *isLeftAvail, BOOL *isRightAvail, BOOL *isAboveAvail, BOOL *isBelowAvail);

void deriveBoundaryAvail(ENC_CTX *ctx, int numLCUPicWidth, int numLCUPicHeight, int ctu, BOOL *isLeftAvail, BOOL *isRightAvail,
                         BOOL *isAboveAvail, BOOL *isBelowAvail, BOOL *isAboveLeftAvail, BOOL *isAboveRightAvail);

void createAlfGlobalBuffers(ENC_CTX* ctx);
void destroyAlfGlobalBuffers(ENC_CTX* ctx, unsigned int uiMaxSizeInbit);
void getStatisticsOneLCU(EncALFVar *Enc_ALF, int compIdx
                         , int ctuYPos, int ctuXPos, int ctuHeight, int ctuWidth
                         , BOOL isAboveAvail, BOOL isBelowAvail, BOOL isLeftAvail, BOOL isRightAvail, BOOL isAboveLeftAvail,
                         BOOL isAboveRightAvail
                         , AlfCorrData *alfCorr, pel *pPicOrg, pel *pPicSrc
                         , int stride, int formatShift);
void calcCorrOneCompRegionLuma(EncALFVar *Enc_ALF, pel *imgOrg, pel *imgPad, int stride, int yPos, int xPos, int height, int width
                               , double ***eCorr, double **yCorr, double *pixAcc, int isLeftAvail, int isRightAvail, int isAboveAvail,
                               int isBelowAvail, int isAboveLeftAvail, int isAboveRightAvail);
void calcCorrOneCompRegionChma(pel *imgOrg, pel *imgPad, int stride, int yPos, int xPos, int height, int width,
                               double **eCorr,
                               double *yCorr, int isLeftAvail, int isRightAvail, int isAboveAvail, int isBelowAvail, int isAboveLeftAvail,
                               int isAboveRightAvail);

unsigned int estimateALFBitrateInPicHeader(ALFParam **alfPicParam);
double executePicLCUOnOffDecision(ENC_CTX *ctx, COM_BSW *alf_bs_temp, ALFParam **alfPictureParam
                                  , double lambda
                                  , BOOL isRDOEstimate
                                  , AlfCorrData *** alfCorr
                                  , pel *imgY_org, pel **imgUV_org, pel *imgY_Dec, pel **imgUV_Dec, pel *imgY_Res, pel **imgUV_Res
                                  , int stride
                                 );
unsigned int ALFParamBitrateEstimate(ALFParam *alfParam);
long long estimateFilterDistortion(EncALFVar *Enc_ALF, int compIdx, AlfCorrData *alfCorr, int **coeffSet, int filterSetSize, int *mergeTable,
                                   BOOL doPixAccMerge);
void mergeFrom(AlfCorrData *dst, AlfCorrData *src, int *mergeTable, BOOL doPixAccMerge);
long long xCalcSSD(EncALFVar *Enc_ALF, pel *pOrg, pel *pCmp, int iWidth, int iHeight, int iStride);
double findFilterCoeff(EncALFVar *Enc_ALF, double ***EGlobalSeq, double **yGlobalSeq, double *pixAccGlobalSeq, int **filterCoeffSeq,
                       int **filterCoeffQuantSeq, int intervalBest[NO_VAR_BINS][2], int varIndTab[NO_VAR_BINS], int sqrFiltLength,
                       int filters_per_fr, int *weights, double errorTabForce0Coeff[NO_VAR_BINS][2]);
double xfindBestCoeffCodMethod(int **filterCoeffSymQuant, int sqrFiltLength, int filters_per_fr,
                               double errorForce0CoeffTab[NO_VAR_BINS][2], double lambda);
int getTemporalLayerNo(int curPoc, int picDistance);
void filterOneCTB(EncALFVar *Enc_ALF, pel *pRest, pel *pDec, int stride, int compIdx, int bit_depth, ALFParam *alfParam, int ctuYPos, int ctuHeight,
                  int ctuXPos, int ctuWidth
                  , BOOL isAboveAvail, BOOL isBelowAvail, BOOL isLeftAvail, BOOL isRightAvail, BOOL isAboveLeftAvail,
                  BOOL isAboveRightAvail);
void copyOneAlfBlk(pel *picDst, pel *picSrc, int stride, int ypos, int xpos, int height, int width, int isAboveAvail,
                   int isBelowAvail);
void ADD_AlfCorrData(AlfCorrData *A, AlfCorrData *B, AlfCorrData *C);
void accumulateLCUCorrelations(ENC_CTX *ctx, AlfCorrData **alfCorrAcc, AlfCorrData *** alfCorSrcLCU, BOOL useAllLCUs);
void decideAlfPictureParam(EncALFVar *Enc_ALF, ALFParam **alfPictureParam, AlfCorrData **alfCorr, double m_dLambdaLuma);
void deriveFilterInfo(EncALFVar *Enc_ALF, int compIdx, AlfCorrData *alfCorr, ALFParam *alfFiltParam, int maxNumFilters, double lambda);
void xcodeFiltCoeff(int **filterCoeff, int *varIndTab, int numFilters, ALFParam *alfParam);
void xQuantFilterCoef(double *h, int *qh);
void xFilterCoefQuickSort(double *coef_data, int *coef_num, int upper, int lower);
void xfindBestFilterVarPred(EncALFVar *Enc_ALF, double **ySym, double ***ESym, double *pixAcc, int **filterCoeffSym,
                            int *filters_per_fr_best, int varIndTab[], double lambda_val, int numMaxFilters);
double mergeFiltersGreedy(EncALFVar *Enc_ALF, double **yGlobalSeq, double ***EGlobalSeq, double *pixAccGlobalSeq,
                          int intervalBest[NO_VAR_BINS][2], int sqrFiltLength, int noIntervals);
#if ALF_LOW_LANTENCY_ENC
void storeAlfTemporalLayerInfo(ENC_CTX *ctx, ALFParam **alfPictureParam, int temporalLayer, double lambda);
#endif
double calculateErrorAbs(double **A, double *b, double y, int size);
void predictALFCoeff(int **coeff, int numCoef, int numFilters);


double QuantizeIntegerFilterPP(double *filterCoeff, int *filterCoeffQuant, double **E, double *y, int sqrFiltLength,
                               int *weights);
void roundFiltCoeff(int *FilterCoeffQuan, double *FilterCoeff, int sqrFiltLength, int factor);
double calculateErrorCoeffProvided(double **A, double *b, double *c, int size);
void add_A(double **Amerged, double ***A, int start, int stop, int size);
void add_b(double *bmerged, double **b, int start, int stop, int size);


int gnsSolveByChol(double **LHS, double *rhs, double *x, int noEq);
long long xFastFiltDistEstimation(EncALFVar *Enc_ALF, double **ppdE, double *pdy, int *piCoeff, int iFiltLength);
long long calcAlfLCUDist(ENC_CTX *ctx, BOOL isSkipLCUBoundary, int compIdx, int lcuAddr, int ctuYPos, int ctuXPos, int ctuHeight,
                         int ctuWidth, BOOL isAboveAvail, pel *picSrc, pel *picCmp, int stride, int formatShift);

void AllocateAlfPar(ALFParam **alf_par, int cID);
void freeAlfPar(ALFParam *alf_par, int cID);
void allocateAlfAPS(ALF_APS *pAPS);
void freeAlfAPS(ALF_APS *pAPS);
void ALF_BitStreamCopy(COM_BSW *dst, COM_BSW *src);

#endif
