#include "Vvc.h"

#include <cstring>

namespace VVC
{

  void ProfileTierLevel::toDefault()
  {
    general_profile_idc = 0;
    general_tier_flag = 0;
    general_level_idc = 0;
    ptl_frame_only_constraint_flag = 0;
    ptl_multilayer_enabled_flag = 0;
    memset(general_sub_profile_idc, 0, sizeof(general_sub_profile_idc));
    memset(general_sub_profile_present_flag, 0, sizeof(general_sub_profile_present_flag));
    memset(sublayer_level_present_flag, 0, sizeof(sublayer_level_present_flag));
    memset(sublayer_level_idc, 0, sizeof(sublayer_level_idc));
    ptl_num_sub_profiles = 0;
  }

  void VPS::toDefault()
  {
    vps_video_parameter_set_id = 0;
    vps_max_layers_minus1 = 0;
    vps_max_sublayers_minus1 = 6;
    vps_all_layers_same_num_sublayers_flag = 0;
    vps_all_independent_layers_flag = 0;
    memset(vps_layer_id, 0, sizeof(vps_layer_id));
    memset(vps_independent_layer_flag, 0, sizeof(vps_independent_layer_flag));
    memset(vps_max_tid_ref_present_flag, 0, sizeof(vps_max_tid_ref_present_flag));
    memset(vps_direct_ref_layer_flag, 0, sizeof(vps_direct_ref_layer_flag));
    vps_each_layer_is_an_ols_flag = 0;
    vps_ols_mode_idc = 0;
    vps_num_output_layer_sets_minus1 = 0;
    vps_num_ptls_minus1 = 0;
    vps_ptl_byte_alignment_zero_bit = 0;
    ptl.clear();
    vps_extension_flag = 0;
  }

  void SPS::toDefault()
  {
    sps_seq_parameter_set_id = 0;
    sps_video_parameter_set_id = 0;
    sps_max_sublayers_minus1 = 0;
    sps_chroma_format_idc = 0;
    sps_log2_ctu_size_minus5 = 0;
    sps_ptl_dpb_hrd_params_present_flag = 0;
    profile_tier_level.toDefault();
    sps_gdr_enabled_flag = 0;
    sps_ref_pic_resampling_enabled_flag = 0;
    sps_res_change_in_clvs_allowed_flag = 0;
    sps_pic_width_max_in_luma_samples = 0;
    sps_pic_height_max_in_luma_samples = 0;
    sps_conformance_window_flag = 0;
    sps_conf_win_left_offset = 0;
    sps_conf_win_right_offset = 0;
    sps_conf_win_top_offset = 0;
    sps_conf_win_bottom_offset = 0;
    sps_subpic_info_present_flag = 0;
    sps_num_subpics_minus1 = 0;
    sps_independent_subpics_flag = 0;
    sps_subpic_id_len_minus1 = 0;
    sps_subpic_ctu_top_left_x.clear();
    sps_subpic_ctu_top_left_y.clear();
    sps_subpic_width_minus1.clear();
    sps_subpic_height_minus1.clear();
    memset(sps_subpic_treated_as_pic_flag, 0, sizeof(sps_subpic_treated_as_pic_flag));
    memset(sps_loop_filter_across_subpic_enabled_flag, 0, sizeof(sps_loop_filter_across_subpic_enabled_flag));
    sps_bitdepth_minus8 = 0;
    sps_entropy_coding_sync_enabled_flag = 0;
    sps_entry_point_offsets_present_flag = 0;
    sps_log2_max_pic_order_cnt_lsb_minus4 = 0;
    sps_poc_msb_cycle_flag = 0;
    sps_poc_msb_cycle_len_minus1 = 0;
    sps_num_extra_ph_bits_bytes = 0;
    sps_num_extra_sh_bits = 0;
    sps_sublayer_dpb_params_flag = 0;
    sps_log2_min_luma_coding_block_size_minus2 = 0;
    sps_partition_constraints_override_enabled_flag = 0;
    sps_log2_diff_min_qt_min_cb_intra_slice_luma = 0;
    sps_max_mtt_hierarchy_depth_intra_slice_luma = 0;
    sps_log2_diff_max_bt_min_qt_intra_slice_luma = 0;
    sps_log2_diff_max_tt_min_qt_intra_slice_luma = 0;
    sps_log2_diff_min_qt_min_cb_intra_slice_chroma = 0;
    sps_max_mtt_hierarchy_depth_intra_slice_chroma = 0;
    sps_log2_diff_max_bt_min_qt_intra_slice_chroma = 0;
    sps_log2_diff_max_tt_min_qt_intra_slice_chroma = 0;
    sps_log2_diff_min_qt_min_cb_inter_slice = 0;
    sps_max_mtt_hierarchy_depth_inter_slice = 0;
    sps_log2_diff_max_bt_min_qt_inter_slice = 0;
    sps_log2_diff_max_tt_min_qt_inter_slice = 0;
    sps_max_luma_transform_size_64_flag = 0;
    sps_transform_skip_enabled_flag = 0;
    sps_log2_transform_skip_max_size_minus2 = 0;
    sps_bdpcm_enabled_flag = 0;
    sps_mts_enabled_flag = 0;
    sps_explicit_mts_intra_enabled_flag = 0;
    sps_explicit_mts_inter_enabled_flag = 0;
    sps_lfnst_enabled_flag = 0;
    sps_joint_cbcr_enabled_flag = 0;
    sps_same_qp_table_for_chroma_flag = 0;
    memset(sps_qp_table_start_minus26, 0, sizeof(sps_qp_table_start_minus26));
    memset(sps_num_points_in_qp_table_minus1, 0, sizeof(sps_num_points_in_qp_table_minus1));
    for(int i = 0; i < 3; i++) { sps_delta_qp_in_val_minus1[i].clear(); sps_delta_qp_diff_val[i].clear(); }
    sps_sao_enabled_flag = 0;
    sps_alf_enabled_flag = 0;
    sps_ccalf_enabled_flag = 0;
    sps_lmcs_enabled_flag = 0;
    sps_weighted_pred_flag = 0;
    sps_weighted_bipred_flag = 0;
    sps_long_term_ref_pics_flag = 0;
    sps_inter_layer_prediction_enabled_flag = 0;
    sps_idr_rpl_present_flag = 0;
    sps_rpl1_same_as_rpl0_flag = 0;
    memset(sps_num_ref_pic_lists, 0, sizeof(sps_num_ref_pic_lists));
    sps_ref_wraparound_enabled_flag = 0;
    sps_temporal_mvp_enabled_flag = 0;
    sps_sbtmvp_enabled_flag = 0;
    sps_amvr_enabled_flag = 0;
    sps_bdof_enabled_flag = 0;
    sps_bdof_control_present_in_ph_flag = 0;
    sps_smvd_enabled_flag = 0;
    sps_dmvr_enabled_flag = 0;
    sps_dmvr_control_present_in_ph_flag = 0;
    sps_mmvd_enabled_flag = 0;
    sps_mmvd_fullpel_only_enabled_flag = 0;
    sps_six_minus_max_num_merge_cand = 0;
    sps_sbt_enabled_flag = 0;
    sps_affine_enabled_flag = 0;
    sps_affine_type_flag = 0;
    sps_affine_amvr_enabled_flag = 0;
    sps_affine_prof_enabled_flag = 0;
    sps_prof_control_present_in_ph_flag = 0;
    sps_bcw_enabled_flag = 0;
    sps_ciip_enabled_flag = 0;
    sps_gpm_enabled_flag = 0;
    sps_max_num_merge_cand_minus_max_num_gpm_cand = 0;
    sps_log2_parallel_merge_level_minus2 = 0;
    sps_isp_enabled_flag = 0;
    sps_mrl_enabled_flag = 0;
    sps_mip_enabled_flag = 0;
    sps_cclm_enabled_flag = 0;
    sps_chroma_horizontal_collocated_flag = 0;
    sps_chroma_vertical_collocated_flag = 0;
    sps_palette_enabled_flag = 0;
    sps_act_enabled_flag = 0;
    sps_min_qp_prime_ts = 0;
    sps_ibc_enabled_flag = 0;
    sps_ladf_enabled_flag = 0;
    sps_explicit_scaling_list_enabled_flag = 0;
    sps_dep_quant_enabled_flag = 0;
    sps_sign_data_hiding_enabled_flag = 0;
    sps_virtual_boundaries_enabled_flag = 0;
    sps_virtual_boundaries_present_flag = 0;
    sps_num_ver_virtual_boundaries = 0;
    sps_virtual_boundary_pos_x_minus1.clear();
    sps_num_hor_virtual_boundaries = 0;
    sps_virtual_boundary_pos_y_minus1.clear();
    sps_timing_hrd_params_present_flag = 0;
    sps_general_hrd_params_present_flag = 0;
    sps_field_seq_flag = 0;
    sps_vui_parameters_present_flag = 0;
    sps_vui_payload_size_minus1 = 0;
    sps_extension_flag = 0;
  }

  void PPS::toDefault()
  {
    pps_pic_parameter_set_id = 0;
    pps_seq_parameter_set_id = 0;
    pps_mixed_nalu_types_in_pic_flag = 0;
    pps_pic_width_in_luma_samples = 0;
    pps_pic_height_in_luma_samples = 0;
    pps_conformance_window_flag = 0;
    pps_conf_win_left_offset = 0;
    pps_conf_win_right_offset = 0;
    pps_conf_win_top_offset = 0;
    pps_conf_win_bottom_offset = 0;
    pps_scaling_window_explicit_signalling_flag = 0;
    pps_output_flag_present_flag = 0;
    pps_no_pic_partition_flag = 0;
    pps_subpic_id_mapping_present_flag = 0;
    pps_num_subpics_minus1 = 0;
    pps_subpic_id_len_minus1 = 0;
    pps_subpic_id.clear();
    pps_log2_ctu_size_minus5 = 0;
    pps_num_exp_tile_columns_minus1 = 0;
    pps_num_exp_tile_rows_minus1 = 0;
    pps_tile_column_width_minus1.clear();
    pps_tile_row_height_minus1.clear();
    pps_loop_filter_across_tiles_enabled_flag = 0;
    pps_rect_slice_flag = 0;
    pps_single_slice_per_subpic_flag = 0;
    pps_num_slices_in_pic_minus1 = 0;
    pps_tile_idx_delta_present_flag = 0;
    pps_slice_width_in_tiles_minus1.clear();
    pps_slice_height_in_tiles_minus1.clear();
    pps_loop_filter_across_slices_enabled_flag = 0;
    pps_cabac_init_present_flag = 0;
    pps_num_ref_idx_default_active_minus1[0] = pps_num_ref_idx_default_active_minus1[1] = 0;
    pps_rpl1_idx_present_flag = 0;
    pps_init_qp_minus26 = 0;
    pps_cu_qp_delta_enabled_flag = 0;
    pps_chroma_tool_offsets_present_flag = 0;
    pps_cb_qp_offset = 0;
    pps_cr_qp_offset = 0;
    pps_joint_cbcr_qp_offset_present_flag = 0;
    pps_joint_cbcr_qp_offset_value = 0;
    pps_slice_chroma_qp_offsets_present_flag = 0;
    pps_cu_chroma_qp_offset_list_enabled_flag = 0;
    pps_chroma_qp_offset_list_len_minus1 = 0;
    pps_cb_qp_offset_list.clear();
    pps_cr_qp_offset_list.clear();
    pps_joint_cbcr_qp_offset_list.clear();
    pps_deblocking_filter_control_present_flag = 0;
    pps_deblocking_filter_override_enabled_flag = 0;
    pps_deblocking_filter_disabled_flag = 0;
    pps_dbf_info_in_ph_flag = 0;
    pps_luma_beta_offset_div2 = 0;
    pps_luma_tc_offset_div2 = 0;
    pps_cb_beta_offset_div2 = 0;
    pps_cb_tc_offset_div2 = 0;
    pps_cr_beta_offset_div2 = 0;
    pps_cr_tc_offset_div2 = 0;
    pps_rpl_info_in_ph_flag = 0;
    pps_sao_info_in_ph_flag = 0;
    pps_alf_info_in_ph_flag = 0;
    pps_wp_info_in_ph_flag = 0;
    pps_qp_delta_info_in_ph_flag = 0;
    pps_picture_header_extension_present_flag = 0;
    pps_slice_header_extension_present_flag = 0;
    pps_extension_flag = 0;
  }

  void PH::toDefault()
  {
    ph_gdr_or_irap_pic_flag = 0;
    ph_non_ref_pic_flag = 0;
    ph_gdr_pic_flag = 0;
    ph_inter_slice_allowed_flag = 0;
    ph_intra_slice_allowed_flag = 0;
    ph_pic_parameter_set_id = 0;
    ph_pic_order_cnt_lsb = 0;
    ph_recovery_poc_cnt = 0;
    ph_extra_bit = 0;
    ph_poc_msb_cycle_present_flag = 0;
    ph_poc_msb_cycle_val = 0;
    ph_alf_enabled_flag = 0;
    ph_num_alf_aps_ids_luma = 0;
    ph_alf_aps_id_luma.clear();
    ph_alf_cb_flag = 0;
    ph_alf_cr_flag = 0;
    ph_alf_aps_id_chroma = 0;
    ph_lmcs_enabled_flag = 0;
    ph_lmcs_aps_id = 0;
    ph_chroma_residual_scale_flag = 0;
    ph_explicit_scaling_list_enabled_flag = 0;
    ph_scaling_list_aps_id = 0;
    ph_virtual_boundaries_present_flag = 0;
    ph_num_ver_virtual_boundaries = 0;
    ph_virtual_boundary_pos_x_minus1.clear();
    ph_num_hor_virtual_boundaries = 0;
    ph_virtual_boundary_pos_y_minus1.clear();
    ph_pic_output_flag = 0;
    ph_partition_constraints_override_flag = 0;
    ph_log2_diff_min_qt_min_cb_intra_slice_luma = 0;
    ph_max_mtt_hierarchy_depth_intra_slice_luma = 0;
    ph_log2_diff_max_bt_min_qt_intra_slice_luma = 0;
    ph_log2_diff_max_tt_min_qt_intra_slice_luma = 0;
    ph_log2_diff_min_qt_min_cb_intra_slice_chroma = 0;
    ph_max_mtt_hierarchy_depth_intra_slice_chroma = 0;
    ph_log2_diff_max_bt_min_qt_intra_slice_chroma = 0;
    ph_log2_diff_max_tt_min_qt_intra_slice_chroma = 0;
    ph_log2_diff_min_qt_min_cb_inter_slice = 0;
    ph_max_mtt_hierarchy_depth_inter_slice = 0;
    ph_log2_diff_max_bt_min_qt_inter_slice = 0;
    ph_log2_diff_max_tt_min_qt_inter_slice = 0;
    ph_cu_qp_delta_subdiv_intra_slice = 0;
    ph_cu_qp_delta_subdiv_inter_slice = 0;
    ph_cu_chroma_qp_offset_subdiv_intra_slice = 0;
    ph_cu_chroma_qp_offset_subdiv_inter_slice = 0;
    ph_joint_cbcr_sign_flag = 0;
    ph_slice_sao_chroma_flag = 0;
    ph_slice_alf_enabled_flag = 0;
  }

  void Slice::toDefault()
  {
    picture_header_in_slice_header_flag = 0;
    slice_subpic_id = 0;
    slice_address = 0;
    slice_type = 2;
    slice_pic_parameter_set_id = 0;
    slice_pic_order_cnt_lsb = 0;
    slice_qp_delta = 0;
    slice_cb_qp_offset = 0;
    slice_cr_qp_offset = 0;
    slice_joint_cbcr_qp_offset = 0;
  }

  NALUnit::NALUnit(NALHeader header): m_processFailed(false), m_nalHeader(header) {}
  NALUnit::~NALUnit() {}
  VPS_NAL::VPS_NAL(NALHeader h): NALUnit(h) {}
  SPS_NAL::SPS_NAL(NALHeader h): NALUnit(h) {}
  PPS_NAL::PPS_NAL(NALHeader h): NALUnit(h) {}
  PH_NAL::PH_NAL(NALHeader h): NALUnit(h) {}
  Slice_NAL::Slice_NAL(NALHeader h): NALUnit(h) {}
  AUD_NAL::AUD_NAL(NALHeader h): NALUnit(h) {}
  SEI_NAL::SEI_NAL(NALHeader h): NALUnit(h) {}

  void AUD::toDefault() { aud_irap_or_gdr_flag = 0; aud_pic_type = 0; }
  void SeiMessage::toDefault() { payload_type = 0; payload_size = 0; payload_data.clear(); }

}
