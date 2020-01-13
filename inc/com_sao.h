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

#ifndef _COM_SAO_H_
#define _COM_SAO_H_
#include "com_def.h"


void init_StateDate(SAOStatData *statsDate);

long long int get_distortion(int compIdx, int type, SAOStatData **saostatData, SAOBlkParam *sao_cur_param);
long long int  distortion_cal(long long int count, int offset, long long int diff);

void off_sao(SAOBlkParam *saoblkparam);
void copySAOParam_for_blk(SAOBlkParam *saopara_dst, SAOBlkParam *saopara_src);
void copySAOParam_for_blk_onecomponent(SAOBlkParam *saopara_dst, SAOBlkParam *saopara_src);
void Copy_LCU_for_SAO(COM_PIC * pic_dst, COM_PIC * pic_src, int pix_y, int pix_x, int smb_pix_height, int smb_pix_width);
void Copy_frame_for_SAO(COM_PIC * pic_dst, COM_PIC * pic_src);

BOOL is_same_patch(s8* map_patch_idx, int mb_nr1, int mb_nr2);

void getSaoMergeNeighbor(COM_INFO *info, s8* map_patch_idx, int pic_width_scu, int pic_width_lcu, int smb_pos, int mb_y, int mb_x,
                         SAOBlkParam **rec_saoBlkParam, int *MergeAvail,
                         SAOBlkParam sao_merge_param[][N_C]);

void checkBoundaryPara(COM_INFO *info, COM_MAP *map,int mb_y, int mb_x, int smb_pix_height, int smb_pix_width,
                       int *smb_process_left, int *smb_process_right, int *smb_process_up, int *smb_process_down, int *smb_process_upleft,
                       int *smb_process_upright, int *smb_process_leftdown, int *smb_process_rightdwon, int filter_on);

void checkBoundaryProc(COM_INFO *info, COM_MAP *map, int pix_y, int pix_x, int smb_pix_height, int smb_pix_width, int comp,
                       int *smb_process_left, int *smb_process_right, int *smb_process_up, int *smb_process_down,
                       int *smb_process_upleft, int *smb_process_upright, int *smb_process_leftdown, int *smb_process_rightdwon, int filter_on);

void SAO_on_block(COM_INFO *info, COM_MAP *map, COM_PIC  *pic_rec, COM_PIC  *pic_sao, SAOBlkParam *saoBlkParam, int compIdx, int pix_y, int pix_x, int smb_pix_height,
                  int smb_pix_width, int smb_available_left, int smb_available_right, int smb_available_up, int smb_available_down,
                  int smb_available_upleft, int smb_available_upright, int smb_available_leftdown, int smb_available_rightdwon,
                  int sample_bit_depth);

void SAO_on_smb(COM_INFO *info, COM_MAP *map, COM_PIC  *pic_rec, COM_PIC  *pic_sao, int pix_y, int pix_x, int smb_pix_width, int smb_pix_height,
                SAOBlkParam *saoBlkParam, int sample_bit_depth);

void SAOFrame(COM_INFO *info, COM_MAP *map, COM_PIC  *pic_rec, COM_PIC  *pic_sao, SAOBlkParam **rec_saoBlkParam);

int com_malloc_3d_SAOstatdate(SAOStatData ** **array3D, int num_SMB, int num_comp, int num_class);
int com_malloc_2d_SAOParameter(SAOBlkParam *** array2D, int num_SMB, int num_comp);
void com_free_3d_SAOstatdate(SAOStatData *** array3D, int num_SMB, int num_comp);
void com_free_2d_SAOParameter(SAOBlkParam **array2D, int num_SMB);
#endif