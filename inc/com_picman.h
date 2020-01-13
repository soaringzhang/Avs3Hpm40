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

#ifndef _COM_PICMAN_H_
#define _COM_PICMAN_H_
#if HLS_RPL /*Declaration for ref pic marking and ref pic list construction functions */
int com_picman_refp_rpl_based_init(COM_PM *pm, COM_PIC_HEADER *sh, COM_REFP(*refp)[REFP_NUM]);
int com_picman_refp_rpl_based_init_decoder(COM_PM *pm, COM_PIC_HEADER *sh, COM_REFP(*refp)[REFP_NUM]);
int com_picman_refpic_marking(COM_PM *pm, COM_PIC_HEADER *sh);
int com_picman_refpic_marking_decoder(COM_PM *pm, COM_PIC_HEADER *sh);
int com_constrcut_ref_list_doi(COM_PIC_HEADER *sh);
int com_picman_dpbpic_doi_minus_cycle_length(COM_PM *pm);
int com_cleanup_useless_pic_buffer_in_pm(COM_PM *pm);
#endif
COM_PIC * com_picman_get_empty_pic(COM_PM *pm, int *err);
#if LIBVC_ON
int com_picman_put_libpic(COM_PM * pm, COM_PIC * pic, int slice_type, u32 ptr, u32 dtr, u8 temporal_id, int need_for_output, COM_REFP(*refp)[REFP_NUM], COM_PIC_HEADER * sh);
#endif
int com_picman_put_pic(COM_PM *pm, COM_PIC *pic, int slice_type, u32 ptr, u32 dtr, 
                       u32 picture_output_delay, u8 temporal_id, int need_for_output, COM_REFP (*refp)[REFP_NUM]);
#if LIBVC_ON
int com_picman_out_libpic(COM_PIC * pic, int library_picture_index, COM_PM * pm);
#endif
COM_PIC * com_picman_out_pic(COM_PM *pm, int *err, int cur_pic_doi, int state);
int com_picman_deinit(COM_PM *pm);
int com_picman_init(COM_PM *pm, int max_pb_size, int max_num_ref_pics, PICBUF_ALLOCATOR *pa);
int com_picman_check_repeat_doi(COM_PM * pm, COM_PIC_HEADER * pic_header);
#endif /* _COM_PICMAN_H_ */
