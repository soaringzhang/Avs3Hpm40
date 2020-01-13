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

#ifndef COM_COMADAPTIVELOOPFILTER_H
#define COM_COMADAPTIVELOOPFILTER_H

#include "com_def.h"

extern int weightsShape1Sym[ALF_MAX_NUM_COEF + 1];

void setFilterImage(pel *pDecY, pel *pDecU, pel *pDecV, int in_stride, pel *imgY_out, pel * imgU_out, pel * imgV_out, int out_stride, int img_width,
                    int img_height);
void reconstructCoefficients(ALFParam *alfParam, int **filterCoeff);
void reconstructCoefInfo(int compIdx, ALFParam *alfParam, int **filterCoeff, int *varIndTab);
void checkFilterCoeffValue(int *filter, int filterLength);
void copyALFparam(ALFParam *dst, ALFParam *src);
void filterOneCompRegion(pel *imgRes, pel *imgPad, int stride, BOOL isChroma, int yPos, int lcuHeight, int xPos,
                         int lcuWidth, int **filterSet, int *mergeTable,
                         pel **varImg, int sample_bit_depth, int isLeftAvail, int isRightAvail, int isAboveAvail, int isBelowAvail,
                         int isAboveLeftAvail, int isAboveRightAvail);
void ExtendPicBorder(pel *img, int iHeight, int iWidth, int iMarginY, int iMarginX, pel *imgExt);
int getLCUCtrCtx_Idx(int ctu, int numLCUInPicWidth, int numLCUInPicHeight, int compIdx,
                     BOOL **AlfLCUEnabled);
int check_filtering_unit_boundary_extension(int x, int y, int lcuPosX, int lcuPosY, int startX, int startY, int endX,
        int endY, int isAboveLeftAvail, int isLeftAvail, int isAboveRightAvail, int isRightAvail);

int get_mem2Dint(int ***array2D, int rows, int columns);
int get_mem1Dint(int **array1D, int num);
void free_mem1Dint(int *array1D);
void free_mem2Dint(int **array2D);
void Copy_frame_for_ALF(COM_PIC* pic_dst, COM_PIC* pic_src);

#endif