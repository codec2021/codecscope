#include "Avc.h"

#include <cstring>

namespace AVC
{

  bool NALHeader::operator == (const NALHeader &rhs) const
  {
    return forbidden_zero_bit == rhs.forbidden_zero_bit &&
           nal_ref_idc == rhs.nal_ref_idc &&
           nal_unit_type == rhs.nal_unit_type;
  }

  void ScalingMatrix::toDefault()
  {
    for(int i = 0; i < 8; i++)
    {
      seq_scaling_list_present_flag[i] = 0;
      use_default_scaling_matrix_flag[i] = 1;
      scaling_list[i].clear();
    }
  }

  void VuiParameters::toDefault()
  {
    aspect_ratio_info_present_flag = 0;
    aspect_ratio_idc = 0;
    sar_width = 0;
    sar_height = 0;
    overscan_info_present_flag = 0;
    overscan_appropriate_flag = 0;
    video_signal_type_present_flag = 0;
    video_format = 0;
    video_full_range_flag = 0;
    colour_description_present_flag = 0;
    colour_primaries = 0;
    transfer_characteristics = 0;
    matrix_coefficients = 0;
    chroma_loc_info_present_flag = 0;
    chroma_sample_loc_type_top_field = 0;
    chroma_sample_loc_type_bottom_field = 0;
    timing_info_present_flag = 0;
    num_units_in_tick = 0;
    time_scale = 0;
    fixed_frame_rate_flag = 0;
    nal_hrd_parameters_present_flag = 0;
    vcl_hrd_parameters_present_flag = 0;
    bitstream_restriction_flag = 0;
  }

  void SPS::toDefault()
  {
    profile_idc = 0;
    constraint_set0_flag = 0;
    constraint_set1_flag = 0;
    constraint_set2_flag = 0;
    constraint_set3_flag = 0;
    constraint_set4_flag = 0;
    constraint_set5_flag = 0;
    level_idc = 0;
    seq_parameter_set_id = 0;
    chroma_format_idc = 1;
    separate_colour_plane_flag = 0;
    bit_depth_luma_minus8 = 0;
    bit_depth_chroma_minus8 = 0;
    qpprime_y_zero_transform_bypass_flag = 0;
    seq_scaling_matrix_present_flag = 0;
    scaling_matrix.toDefault();
    log2_max_frame_num_minus4 = 0;
    pic_order_cnt_type = 0;
    log2_max_pic_order_cnt_lsb_minus4 = 0;
    delta_pic_order_always_zero_flag = 0;
    offset_for_non_ref_pic = 0;
    offset_for_top_to_bottom_field = 0;
    num_ref_frames_in_pic_order_cnt_cycle = 0;
    offset_for_ref_frame.clear();
    max_num_ref_frames = 0;
    gaps_in_frame_num_value_allowed_flag = 0;
    pic_width_in_mbs_minus1 = 0;
    pic_height_in_map_units_minus1 = 0;
    frame_mbs_only_flag = 1;
    mb_adaptive_frame_field_flag = 0;
    direct_8x8_inference_flag = 0;
    frame_cropping_flag = 0;
    frame_crop_left_offset = 0;
    frame_crop_right_offset = 0;
    frame_crop_top_offset = 0;
    frame_crop_bottom_offset = 0;
    vui_parameters_present_flag = 0;
    vui_parameters.toDefault();
  }

  void PPS::toDefault()
  {
    pic_parameter_set_id = 0;
    seq_parameter_set_id = 0;
    entropy_coding_mode_flag = 0;
    bottom_field_pic_order_in_frame_present_flag = 0;
    num_slice_groups_minus1 = 0;
    slice_group_map_type = 0;
    run_length_minus1 = 0;
    run_length_minus1_list.clear();
    top_left[0] = top_left[1] = top_left[2] = 0;
    bottom_right[0] = bottom_right[1] = bottom_right[2] = 0;
    slice_group_change_direction_flag = 0;
    slice_group_change_rate_minus1 = 0;
    pic_size_in_map_units_minus1 = 0;
    slice_group_id.clear();
    num_ref_idx_l0_default_active_minus1 = 0;
    num_ref_idx_l1_default_active_minus1 = 0;
    weighted_pred_flag = 0;
    weighted_bipred_idc = 0;
    pic_init_qp_minus26 = 0;
    pic_init_qs_minus26 = 0;
    chroma_qp_index_offset = 0;
    deblocking_filter_control_present_flag = 0;
    constrained_intra_pred_flag = 0;
    redundant_pic_cnt_present_flag = 0;
    transform_8x8_mode_flag = 0;
    pic_scaling_matrix_present_flag = 0;
    scaling_matrix.toDefault();
    second_chroma_qp_index_offset = 0;
  }

  void RefPicListModification::toDefault()
  {
    ref_pic_list_modification_flag_l0 = 0;
    modification_of_pic_nums_idc_l0.clear();
    abs_diff_pic_num_minus1_l0.clear();
    long_term_pic_num_l0.clear();
    ref_pic_list_modification_flag_l1 = 0;
    modification_of_pic_nums_idc_l1.clear();
    abs_diff_pic_num_minus1_l1.clear();
    long_term_pic_num_l1.clear();
  }

  void DecRefPicMarking::toDefault()
  {
    adaptive_ref_pic_marking_mode_flag = 0;
    operations.clear();
  }

  void PredWeightTable::toDefault()
  {
    luma_log2_weight_denom = 0;
    chroma_log2_weight_denom = 0;
    l0.clear();
    l1.clear();
  }

  void Slice::toDefault()
  {
    first_mb_in_slice = 0;
    slice_type = NONE_SLICE;
    pic_parameter_set_id = 0;
    colour_plane_id = 0;
    frame_num = 0;
    field_pic_flag = 0;
    bottom_field_flag = 0;
    idr_pic_id = 0;
    pic_order_cnt_lsb = 0;
    delta_pic_order_cnt_bottom = 0;
    delta_pic_order_cnt[0] = delta_pic_order_cnt[1] = 0;
    redundant_pic_cnt = 0;
    direct_spatial_mv_pred_flag = 0;
    num_ref_idx_active_override_flag = 0;
    num_ref_idx_l0_active_minus1 = 0;
    num_ref_idx_l1_active_minus1 = 0;
    ref_pic_list_modification.toDefault();
    no_output_of_prior_pics_flag = 0;
    long_term_reference_flag = 0;
    dec_ref_pic_marking.toDefault();
    cabac_init_idc = 0;
    slice_qp_delta = 0;
    sp_for_switch_flag = 0;
    slice_qs_delta = 0;
    disable_deblocking_filter_idc = 0;
    slice_alpha_c0_offset_div2 = 0;
    slice_beta_offset_div2 = 0;
    slice_group_change_cycle.clear();
  }

  NALUnit::NALUnit(NALHeader header):
    m_processFailed(false)
    ,m_nalHeader(header)
  {
  }

  NALUnit::~NALUnit()
  {
  }

  SPS_NAL::SPS_NAL(NALHeader header): NALUnit(header) {}
  PPS_NAL::PPS_NAL(NALHeader header): NALUnit(header) {}
  Slice_NAL::Slice_NAL(NALHeader header): NALUnit(header) {}
  SEI_NAL::SEI_NAL(NALHeader header): NALUnit(header) {}
  AUD_NAL::AUD_NAL(NALHeader header): NALUnit(header) {}

  void AUD::toDefault()
  {
    primary_pic_type = 0;
  }

  void SeiMessage::toDefault()
  {
    payload_type = 0;
    payload_size = 0;
    payload_data.clear();
  }

}
