#ifndef VVC_H_
#define VVC_H_

#include <memory>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace VVC
{

  enum NALUnitType
  {
    NAL_TRAIL_NUT    = 0,
    NAL_STSA_NUT     = 1,
    NAL_RADL_NUT     = 2,
    NAL_RASL_NUT     = 3,
    NAL_RSV_VCL_4    = 4,
    NAL_RSV_VCL_5    = 5,
    NAL_RSV_VCL_6    = 6,
    NAL_IDR_W_RADL   = 7,
    NAL_IDR_N_LP     = 8,
    NAL_CRA_NUT      = 9,
    NAL_GDR_NUT      = 10,
    NAL_RSV_IRAP_11  = 11,
    NAL_OPI          = 12,
    NAL_DCI          = 13,
    NAL_VPS          = 14,
    NAL_SPS          = 15,
    NAL_PPS          = 16,
    NAL_PREFIX_APS   = 17,
    NAL_SUFFIX_APS   = 18,
    NAL_PH           = 19,
    NAL_AUD          = 20,
    NAL_EOS_NUT      = 21,
    NAL_EOB_NUT      = 22,
    NAL_PREFIX_SEI   = 23,
    NAL_SUFFIX_SEI   = 24,
    NAL_FD_NUT       = 25,
  };

  class NALHeader
  {
  public:
    uint8_t     forbidden_zero_bit;
    uint8_t     nuh_reserved_zero_bit;
    uint8_t     nuh_layer_id;
    NALUnitType nal_unit_type;
    uint8_t     nuh_temporal_id_plus1;
  };

  class ProfileTierLevel
  {
  public:
    uint8_t  general_profile_idc;
    uint8_t  general_tier_flag;
    uint8_t  general_level_idc;
    uint8_t  ptl_frame_only_constraint_flag;
    uint8_t  ptl_multilayer_enabled_flag;
    uint8_t  general_sub_profile_idc[32];
    uint8_t  general_sub_profile_present_flag[32];
    uint8_t  sublayer_level_present_flag[8];
    uint8_t  sublayer_level_idc[8];
    uint8_t  ptl_num_sub_profiles;

    void toDefault();
  };

  class VPS
  {
  public:
    uint8_t  vps_video_parameter_set_id;
    uint8_t  vps_max_layers_minus1;
    uint8_t  vps_max_sublayers_minus1;
    uint8_t  vps_all_layers_same_num_sublayers_flag;
    uint8_t  vps_all_independent_layers_flag;
    uint8_t  vps_layer_id[64];
    uint8_t  vps_independent_layer_flag[64];
    uint8_t  vps_max_tid_ref_present_flag[64];
    uint8_t  vps_direct_ref_layer_flag[64][64];
    uint8_t  vps_each_layer_is_an_ols_flag;
    uint8_t  vps_ols_mode_idc;
    uint8_t  vps_num_output_layer_sets_minus1;
    uint8_t  vps_num_ptls_minus1;
    uint8_t  vps_ptl_byte_alignment_zero_bit;
    std::vector<ProfileTierLevel> ptl;
    uint8_t  vps_extension_flag;

    void toDefault();
  };

  class SPS
  {
  public:
    uint8_t  sps_seq_parameter_set_id;
    uint8_t  sps_video_parameter_set_id;
    uint8_t  sps_max_sublayers_minus1;
    uint8_t  sps_chroma_format_idc;
    uint8_t  sps_log2_ctu_size_minus5;
    uint8_t  sps_ptl_dpb_hrd_params_present_flag;
    ProfileTierLevel profile_tier_level;
    uint8_t  sps_gdr_enabled_flag;
    uint8_t  sps_ref_pic_resampling_enabled_flag;
    uint8_t  sps_res_change_in_clvs_allowed_flag;
    uint32_t sps_pic_width_max_in_luma_samples;
    uint32_t sps_pic_height_max_in_luma_samples;
    uint8_t  sps_conformance_window_flag;
    uint32_t sps_conf_win_left_offset;
    uint32_t sps_conf_win_right_offset;
    uint32_t sps_conf_win_top_offset;
    uint32_t sps_conf_win_bottom_offset;
    uint8_t  sps_subpic_info_present_flag;
    uint32_t sps_num_subpics_minus1;
    uint8_t  sps_independent_subpics_flag;
    uint32_t sps_subpic_id_len_minus1;
    std::vector<uint32_t> sps_subpic_ctu_top_left_x;
    std::vector<uint32_t> sps_subpic_ctu_top_left_y;
    std::vector<uint32_t> sps_subpic_width_minus1;
    std::vector<uint32_t> sps_subpic_height_minus1;
    uint8_t  sps_subpic_treated_as_pic_flag[64];
    uint8_t  sps_loop_filter_across_subpic_enabled_flag[64];
    uint32_t sps_bitdepth_minus8;
    uint8_t  sps_entropy_coding_sync_enabled_flag;
    uint8_t  sps_entry_point_offsets_present_flag;
    uint8_t  sps_log2_max_pic_order_cnt_lsb_minus4;
    uint8_t  sps_poc_msb_cycle_flag;
    uint32_t sps_poc_msb_cycle_len_minus1;
    uint8_t  sps_num_extra_ph_bytes;
    uint8_t  sps_num_extra_sh_bytes;
    uint8_t  sps_sublayer_dpb_params_flag;
    uint8_t  sps_log2_min_luma_coding_block_size_minus2;
    uint8_t  sps_partition_constraints_override_enabled_flag;
    uint8_t  sps_log2_diff_min_qt_min_cb_intra_slice_luma;
    uint8_t  sps_max_mtt_hierarchy_depth_intra_slice_luma;
    uint8_t  sps_log2_diff_max_bt_min_qt_intra_slice_luma;
    uint8_t  sps_log2_diff_max_tt_min_qt_intra_slice_luma;
    uint8_t  sps_qtbtt_dual_tree_intra_flag;
    uint8_t  sps_log2_diff_min_qt_min_cb_intra_slice_chroma;
    uint8_t  sps_max_mtt_hierarchy_depth_intra_slice_chroma;
    uint8_t  sps_log2_diff_max_bt_min_qt_intra_slice_chroma;
    uint8_t  sps_log2_diff_max_tt_min_qt_intra_slice_chroma;
    uint8_t  sps_log2_diff_min_qt_min_cb_inter_slice;
    uint8_t  sps_max_mtt_hierarchy_depth_inter_slice;
    uint8_t  sps_log2_diff_max_bt_min_qt_inter_slice;
    uint8_t  sps_log2_diff_max_tt_min_qt_inter_slice;
    uint8_t  sps_max_luma_transform_size_64_flag;
    uint8_t  sps_transform_skip_enabled_flag;
    uint8_t  sps_log2_transform_skip_max_size_minus2;
    uint8_t  sps_bdpcm_enabled_flag;
    uint8_t  sps_mts_enabled_flag;
    uint8_t  sps_explicit_mts_intra_enabled_flag;
    uint8_t  sps_explicit_mts_inter_enabled_flag;
    uint8_t  sps_lfnst_enabled_flag;
    uint8_t  sps_joint_cbcr_enabled_flag;
    uint8_t  sps_same_qp_table_for_chroma_flag;
    int32_t  sps_qp_table_start_minus26[3];
    uint32_t sps_num_points_in_qp_table_minus1[3];
    std::vector<int32_t> sps_delta_qp_in_val_minus1[3];
    std::vector<int32_t> sps_delta_qp_diff_val[3];
    uint8_t  sps_sao_enabled_flag;
    uint8_t  sps_alf_enabled_flag;
    uint8_t  sps_ccalf_enabled_flag;
    uint8_t  sps_lmcs_enabled_flag;
    uint8_t  sps_weighted_pred_flag;
    uint8_t  sps_weighted_bipred_flag;
    uint8_t  sps_long_term_ref_pics_flag;
    uint8_t  sps_inter_layer_prediction_enabled_flag;
    uint8_t  sps_idr_rpl_present_flag;
    uint8_t  sps_rpl1_same_as_rpl0_flag;
    uint8_t  sps_num_ref_pic_lists[2];
    uint8_t  sps_ref_wraparound_enabled_flag;
    uint8_t  sps_temporal_mvp_enabled_flag;
    uint8_t  sps_sbtmvp_enabled_flag;
    uint8_t  sps_amvr_enabled_flag;
    uint8_t  sps_bdof_enabled_flag;
    uint8_t  sps_bdof_control_present_in_ph_flag;
    uint8_t  sps_smvd_enabled_flag;
    uint8_t  sps_dmvr_enabled_flag;
    uint8_t  sps_dmvr_control_present_in_ph_flag;
    uint8_t  sps_mmvd_enabled_flag;
    uint8_t  sps_mmvd_fullpel_only_enabled_flag;
    uint8_t  sps_six_minus_max_num_merge_cand;
    uint8_t  sps_sbt_enabled_flag;
    uint8_t  sps_affine_enabled_flag;
    uint8_t  sps_affine_type_flag;
    uint8_t  sps_affine_amvr_enabled_flag;
    uint8_t  sps_affine_prof_enabled_flag;
    uint8_t  sps_prof_control_present_in_ph_flag;
    uint8_t  sps_bcw_enabled_flag;
    uint8_t  sps_ciip_enabled_flag;
    uint8_t  sps_gpm_enabled_flag;
    uint32_t sps_max_num_merge_cand_minus_max_num_gpm_cand;
    uint8_t  sps_log2_parallel_merge_level_minus2;
    uint8_t  sps_isp_enabled_flag;
    uint8_t  sps_mrl_enabled_flag;
    uint8_t  sps_mip_enabled_flag;
    uint8_t  sps_cclm_enabled_flag;
    uint8_t  sps_chroma_horizontal_collocated_flag;
    uint8_t  sps_chroma_vertical_collocated_flag;
    uint8_t  sps_palette_enabled_flag;
    uint8_t  sps_act_enabled_flag;
    uint8_t  sps_min_qp_prime_ts;
    uint8_t  sps_ibc_enabled_flag;
    uint8_t  sps_ladf_enabled_flag;
    uint8_t  sps_explicit_scaling_list_enabled_flag;
    uint8_t  sps_dep_quant_enabled_flag;
    uint8_t  sps_sign_data_hiding_enabled_flag;
    uint8_t  sps_virtual_boundaries_enabled_flag;
    uint8_t  sps_virtual_boundaries_present_flag;
    uint8_t  sps_num_ver_virtual_boundaries;
    std::vector<uint32_t> sps_virtual_boundary_pos_x_minus1;
    uint8_t  sps_num_hor_virtual_boundaries;
    std::vector<uint32_t> sps_virtual_boundary_pos_y_minus1;
    uint8_t  sps_timing_hrd_params_present_flag;
    uint8_t  sps_general_hrd_params_present_flag;
    uint8_t  sps_field_seq_flag;
    uint8_t  sps_vui_parameters_present_flag;
    uint8_t  sps_vui_payload_size_minus1;
    uint8_t  sps_extension_flag;

    void toDefault();
  };

  class PPS
  {
  public:
    uint8_t  pps_pic_parameter_set_id;
    uint8_t  pps_seq_parameter_set_id;
    uint8_t  pps_mixed_nalu_types_in_pic_flag;
    uint32_t pps_pic_width_in_luma_samples;
    uint32_t pps_pic_height_in_luma_samples;
    uint8_t  pps_conformance_window_flag;
    uint32_t pps_conf_win_left_offset;
    uint32_t pps_conf_win_right_offset;
    uint32_t pps_conf_win_top_offset;
    uint32_t pps_conf_win_bottom_offset;
    uint8_t  pps_scaling_window_explicit_signalling_flag;
    uint8_t  pps_output_flag_present_flag;
    uint8_t  pps_no_pic_partition_flag;
    uint8_t  pps_subpic_id_mapping_present_flag;
    uint8_t  pps_num_subpics_minus1;
    uint8_t  pps_subpic_id_len_minus1;
    std::vector<uint32_t> pps_subpic_id;
    uint8_t  pps_log2_ctu_size_minus5;
    uint8_t  pps_num_exp_tile_columns_minus1;
    uint8_t  pps_num_exp_tile_rows_minus1;
    std::vector<uint32_t> pps_tile_column_width_minus1;
    std::vector<uint32_t> pps_tile_row_height_minus1;
    uint8_t  pps_loop_filter_across_tiles_enabled_flag;
    uint8_t  pps_rect_slice_flag;
    uint8_t  pps_single_slice_per_subpic_flag;
    uint8_t  pps_num_slices_in_pic_minus1;
    uint8_t  pps_tile_idx_delta_present_flag;
    std::vector<uint32_t> pps_slice_width_in_tiles_minus1;
    std::vector<uint32_t> pps_slice_height_in_tiles_minus1;
    uint8_t  pps_loop_filter_across_slices_enabled_flag;
    uint8_t  pps_cabac_init_present_flag;
    uint8_t  pps_num_ref_idx_default_active_minus1[2];
    uint8_t  pps_rpl1_idx_present_flag;
    uint8_t  pps_init_qp_minus26;
    uint8_t  pps_cu_qp_delta_enabled_flag;
    uint8_t  pps_chroma_tool_offsets_present_flag;
    int32_t  pps_cb_qp_offset;
    int32_t  pps_cr_qp_offset;
    uint8_t  pps_joint_cbcr_qp_offset_present_flag;
    int32_t  pps_joint_cbcr_qp_offset_value;
    uint8_t  pps_slice_chroma_qp_offsets_present_flag;
    uint8_t  pps_cu_chroma_qp_offset_list_enabled_flag;
    uint8_t  pps_chroma_qp_offset_list_len_minus1;
    std::vector<int32_t> pps_cb_qp_offset_list;
    std::vector<int32_t> pps_cr_qp_offset_list;
    std::vector<int32_t> pps_joint_cbcr_qp_offset_list;
    uint8_t  pps_weighted_pred_flag;
    uint8_t  pps_weighted_bipred_flag;
    uint8_t  pps_deblocking_filter_control_present_flag;
    uint8_t  pps_deblocking_filter_override_enabled_flag;
    uint8_t  pps_deblocking_filter_disabled_flag;
    uint8_t  pps_dbf_info_in_ph_flag;
    int32_t  pps_luma_beta_offset_div2;
    int32_t  pps_luma_tc_offset_div2;
    int32_t  pps_cb_beta_offset_div2;
    int32_t  pps_cb_tc_offset_div2;
    int32_t  pps_cr_beta_offset_div2;
    int32_t  pps_cr_tc_offset_div2;
    uint8_t  pps_rpl_info_in_ph_flag;
    uint8_t  pps_sao_info_in_ph_flag;
    uint8_t  pps_alf_info_in_ph_flag;
    uint8_t  pps_wp_info_in_ph_flag;
    uint8_t  pps_qp_delta_info_in_ph_flag;
    uint8_t  pps_picture_header_extension_present_flag;
    uint8_t  pps_slice_header_extension_present_flag;
    uint8_t  pps_extension_flag;

    void toDefault();
  };

  class PH
  {
  public:
    uint8_t  ph_gdr_or_irap_pic_flag;
    uint8_t  ph_non_ref_pic_flag;
    uint8_t  ph_gdr_pic_flag;
    uint8_t  ph_inter_slice_allowed_flag;
    uint8_t  ph_intra_slice_allowed_flag;
    uint8_t  ph_pic_parameter_set_id;
    uint32_t ph_pic_order_cnt_lsb;
    uint8_t  ph_recovery_poc_cnt;
    uint8_t  ph_extra_bit;
    uint8_t  ph_poc_msb_cycle_present_flag;
    uint32_t ph_poc_msb_cycle_val;
    uint8_t  ph_alf_enabled_flag;
    uint8_t  ph_num_alf_aps_ids_luma;
    std::vector<uint32_t> ph_alf_aps_id_luma;
    uint8_t  ph_alf_cb_flag;
    uint8_t  ph_alf_cr_flag;
    uint8_t  ph_alf_aps_id_chroma;
    uint8_t  ph_lmcs_enabled_flag;
    uint8_t  ph_lmcs_aps_id;
    uint8_t  ph_chroma_residual_scale_flag;
    uint8_t  ph_explicit_scaling_list_enabled_flag;
    uint8_t  ph_scaling_list_aps_id;
    uint8_t  ph_virtual_boundaries_present_flag;
    uint8_t  ph_num_ver_virtual_boundaries;
    std::vector<uint32_t> ph_virtual_boundary_pos_x_minus1;
    uint8_t  ph_num_hor_virtual_boundaries;
    std::vector<uint32_t> ph_virtual_boundary_pos_y_minus1;
    uint8_t  ph_pic_output_flag;
    uint8_t  ph_partition_constraints_override_flag;
    uint8_t  ph_log2_diff_min_qt_min_cb_intra_slice_luma;
    uint8_t  ph_max_mtt_hierarchy_depth_intra_slice_luma;
    uint8_t  ph_log2_diff_max_bt_min_qt_intra_slice_luma;
    uint8_t  ph_log2_diff_max_tt_min_qt_intra_slice_luma;
    uint8_t  ph_log2_diff_min_qt_min_cb_intra_slice_chroma;
    uint8_t  ph_max_mtt_hierarchy_depth_intra_slice_chroma;
    uint8_t  ph_log2_diff_max_bt_min_qt_intra_slice_chroma;
    uint8_t  ph_log2_diff_max_tt_min_qt_intra_slice_chroma;
    uint8_t  ph_log2_diff_min_qt_min_cb_inter_slice;
    uint8_t  ph_max_mtt_hierarchy_depth_inter_slice;
    uint8_t  ph_log2_diff_max_bt_min_qt_inter_slice;
    uint8_t  ph_log2_diff_max_tt_min_qt_inter_slice;
    uint8_t  ph_cu_qp_delta_subdiv_intra_slice;
    uint8_t  ph_cu_qp_delta_subdiv_inter_slice;
    uint8_t  ph_cu_chroma_qp_offset_subdiv_intra_slice;
    uint8_t  ph_cu_chroma_qp_offset_subdiv_inter_slice;
    uint8_t  ph_temporal_mvp_enabled_flag;
    uint8_t  ph_mmvd_fullpel_only_flag;
    uint8_t  ph_mvd_l1_zero_flag;
    uint8_t  ph_bdof_disabled_flag;
    uint8_t  ph_dmvr_disabled_flag;
    uint8_t  ph_prof_disabled_flag;
    int32_t  ph_joint_cbcr_sign_flag;
    int32_t  ph_slice_sao_chroma_flag;
    int32_t  ph_slice_alf_enabled_flag;

    void toDefault();
  };

  class Slice
  {
  public:
    uint8_t  picture_header_in_slice_header_flag;
    uint8_t  slice_subpic_id;
    uint32_t slice_address;
    uint32_t slice_type;
    uint8_t  slice_pic_parameter_set_id;
    uint32_t slice_pic_order_cnt_lsb;
    uint8_t  sh_no_output_of_prior_pics_flag;
    uint8_t  sh_alf_enabled_flag;
    uint8_t  sh_num_alf_aps_ids_luma;
    std::vector<uint32_t> sh_alf_aps_id_luma;
    uint8_t  sh_alf_cb_enabled_flag;
    uint8_t  sh_alf_cr_enabled_flag;
    uint8_t  sh_alf_aps_id_chroma;
    uint8_t  sh_lmcs_used_flag;
    uint8_t  sh_explicit_scaling_list_used_flag;
    uint8_t  sh_rpl_sps_flag[2];
    uint32_t sh_rpl_idx[2];
    uint8_t  sh_rpl_idx_present[2];
    uint32_t sh_num_ref_entries[2];
    uint8_t  sh_ref_pic_lists_present;
    uint8_t  sh_rpl1_present;
    uint8_t  sh_num_ref_idx_active_override_flag;
    uint32_t sh_num_ref_idx_active_minus1[2];
    uint8_t  sh_cabac_init_flag;
    uint8_t  sh_collocated_from_l0_flag;
    uint32_t sh_collocated_ref_idx;
    int32_t  slice_qp_delta;
    int32_t  slice_cb_qp_offset;
    int32_t  slice_cr_qp_offset;
    int32_t  slice_joint_cbcr_qp_offset;
    uint8_t  sh_cu_chroma_qp_offset_enabled_flag;
    uint8_t  sh_sao_luma_used_flag;
    uint8_t  sh_sao_chroma_used_flag;
    uint8_t  sh_dep_quant_used_flag;
    uint8_t  sh_sign_data_hiding_used_flag;
    uint8_t  sh_ts_residual_coding_disabled_flag;

    void toDefault();
  };

  class NALUnit
  {
  public:
    NALUnit(NALHeader header);
    virtual ~NALUnit();

    bool      m_processFailed;
    NALHeader m_nalHeader;
  };

  class VPS_NAL: public NALUnit
  {
  public:
    VPS_NAL(NALHeader header);
    VPS vps;
  };

  class SPS_NAL: public NALUnit
  {
  public:
    SPS_NAL(NALHeader header);
    SPS sps;
  };

  class PPS_NAL: public NALUnit
  {
  public:
    PPS_NAL(NALHeader header);
    PPS pps;
  };

  class PH_NAL: public NALUnit
  {
  public:
    PH_NAL(NALHeader header);
    PH ph;
  };

  class Slice_NAL: public NALUnit
  {
  public:
    Slice_NAL(NALHeader header);
    Slice slice;
    PH     ph;         // picture header（内联时）
    bool   hasPH;
  };

  class AUD
  {
  public:
    uint8_t aud_irap_or_gdr_flag;
    uint8_t aud_pic_type;
    void toDefault();
  };

  class AUD_NAL: public NALUnit
  {
  public:
    AUD_NAL(NALHeader header);
    AUD aud;
  };

  class SeiMessage
  {
  public:
    uint32_t payload_type;
    uint32_t payload_size;
    std::vector<uint8_t> payload_data;
    void toDefault();
  };

  class SEI_NAL: public NALUnit
  {
  public:
    SEI_NAL(NALHeader header);
    std::vector<SeiMessage> messages;
  };

}

#endif
