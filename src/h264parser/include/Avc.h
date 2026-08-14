#ifndef AVC_H_
#define AVC_H_

#include <memory>
#include <vector>
#include <array>
#include <cstdint>
#include <cstddef>

namespace AVC
{

  enum NALUnitType
  {
    NAL_UNSPEC_0    = 0,
    NAL_SLICE       = 1,
    NAL_DPA         = 2,
    NAL_DPB         = 3,
    NAL_DPC         = 4,
    NAL_IDR_SLICE   = 5,
    NAL_SEI         = 6,
    NAL_SPS         = 7,
    NAL_PPS         = 8,
    NAL_AUD         = 9,
    NAL_END_SEQUENCE = 10,
    NAL_END_STREAM  = 11,
    NAL_FILLER      = 12,
    NAL_SPS_EXT     = 13,
    NAL_PREFIX      = 14,
    NAL_SUBSET_SPS  = 15,
    NAL_AUX_SLICE   = 19,
    NAL_SLICE_EXT   = 20,
  };

  class NALHeader
  {
  public:
    uint8_t     forbidden_zero_bit;
    uint8_t     nal_ref_idc;
    NALUnitType nal_unit_type;

    bool operator == (const NALHeader &) const;
  };

  class ScalingMatrix
  {
  public:
    uint8_t                  seq_scaling_list_present_flag[8];
    uint8_t                  use_default_scaling_matrix_flag[8];
    std::vector<uint8_t>     scaling_list[8];

    void toDefault();
  };

  class VuiParameters
  {
  public:
    uint8_t   aspect_ratio_info_present_flag;
    uint8_t   aspect_ratio_idc;
    uint16_t  sar_width;
    uint16_t  sar_height;
    uint8_t   overscan_info_present_flag;
    uint8_t   overscan_appropriate_flag;
    uint8_t   video_signal_type_present_flag;
    uint8_t   video_format;
    uint8_t   video_full_range_flag;
    uint8_t   colour_description_present_flag;
    uint8_t   colour_primaries;
    uint8_t   transfer_characteristics;
    uint8_t   matrix_coefficients;
    uint8_t   chroma_loc_info_present_flag;
    uint32_t  chroma_sample_loc_type_top_field;
    uint32_t  chroma_sample_loc_type_bottom_field;
    uint8_t   timing_info_present_flag;
    uint32_t  num_units_in_tick;
    uint32_t  time_scale;
    uint8_t   fixed_frame_rate_flag;
    uint8_t   nal_hrd_parameters_present_flag;
    uint8_t   vcl_hrd_parameters_present_flag;
    uint8_t   bitstream_restriction_flag;

    void toDefault();
  };

  class SPS
  {
  public:
    uint8_t       profile_idc;
    uint8_t       constraint_set0_flag;
    uint8_t       constraint_set1_flag;
    uint8_t       constraint_set2_flag;
    uint8_t       constraint_set3_flag;
    uint8_t       constraint_set4_flag;
    uint8_t       constraint_set5_flag;
    uint8_t       level_idc;
    uint32_t      seq_parameter_set_id;
    uint32_t      chroma_format_idc;
    uint8_t       separate_colour_plane_flag;
    uint32_t      bit_depth_luma_minus8;
    uint32_t      bit_depth_chroma_minus8;
    uint8_t       qpprime_y_zero_transform_bypass_flag;
    uint8_t       seq_scaling_matrix_present_flag;
    ScalingMatrix scaling_matrix;
    uint32_t      log2_max_frame_num_minus4;
    uint32_t      pic_order_cnt_type;
    uint32_t      log2_max_pic_order_cnt_lsb_minus4;
    uint8_t       delta_pic_order_always_zero_flag;
    int32_t       offset_for_non_ref_pic;
    int32_t       offset_for_top_to_bottom_field;
    uint32_t      num_ref_frames_in_pic_order_cnt_cycle;
    std::vector<int32_t> offset_for_ref_frame;
    uint32_t      max_num_ref_frames;
    uint8_t       gaps_in_frame_num_value_allowed_flag;
    uint32_t      pic_width_in_mbs_minus1;
    uint32_t      pic_height_in_map_units_minus1;
    uint8_t       frame_mbs_only_flag;
    uint8_t       mb_adaptive_frame_field_flag;
    uint8_t       direct_8x8_inference_flag;
    uint8_t       frame_cropping_flag;
    uint32_t      frame_crop_left_offset;
    uint32_t      frame_crop_right_offset;
    uint32_t      frame_crop_top_offset;
    uint32_t      frame_crop_bottom_offset;
    uint8_t       vui_parameters_present_flag;
    VuiParameters vui_parameters;

    void toDefault();
  };

  class PPS
  {
  public:
    uint32_t      pic_parameter_set_id;
    uint32_t      seq_parameter_set_id;
    uint8_t       entropy_coding_mode_flag;
    uint8_t       bottom_field_pic_order_in_frame_present_flag;
    uint32_t      num_slice_groups_minus1;
    uint32_t      slice_group_map_type;
    uint32_t      run_length_minus1;         // map_type 0
    std::vector<uint32_t> run_length_minus1_list;
    uint32_t      top_left[3];              // map_type 2
    uint32_t      bottom_right[3];
    uint8_t       slice_group_change_direction_flag; // map_type 3,4,5
    uint32_t      slice_group_change_rate_minus1;
    uint32_t      pic_size_in_map_units_minus1;
    std::vector<uint32_t> slice_group_id;
    uint32_t      num_ref_idx_l0_default_active_minus1;
    uint32_t      num_ref_idx_l1_default_active_minus1;
    uint8_t       weighted_pred_flag;
    uint32_t      weighted_bipred_idc;
    int32_t       pic_init_qp_minus26;
    int32_t       pic_init_qs_minus26;
    int32_t       chroma_qp_index_offset;
    uint8_t       deblocking_filter_control_present_flag;
    uint8_t       constrained_intra_pred_flag;
    uint8_t       redundant_pic_cnt_present_flag;
    uint8_t       transform_8x8_mode_flag;
    uint8_t       pic_scaling_matrix_present_flag;
    ScalingMatrix scaling_matrix;
    int32_t       second_chroma_qp_index_offset;

    void toDefault();
  };

  class RefPicListModification
  {
  public:
    uint8_t   ref_pic_list_modification_flag_l0;
    std::vector<uint32_t> modification_of_pic_nums_idc_l0;
    std::vector<uint32_t> abs_diff_pic_num_minus1_l0;
    std::vector<uint32_t> long_term_pic_num_l0;
    uint8_t   ref_pic_list_modification_flag_l1;
    std::vector<uint32_t> modification_of_pic_nums_idc_l1;
    std::vector<uint32_t> abs_diff_pic_num_minus1_l1;
    std::vector<uint32_t> long_term_pic_num_l1;

    void toDefault();
  };

  class DecRefPicMarking
  {
  public:
    uint8_t   adaptive_ref_pic_marking_mode_flag;
    struct Op
    {
      uint32_t memory_management_control_operation;
      uint32_t difference_of_pic_nums_minus1;
      uint32_t long_term_pic_num;
      uint32_t long_term_frame_idx;
      uint32_t max_long_term_frame_idx_plus1;
    };
    std::vector<Op> operations;

    void toDefault();
  };

  class PredWeightTable
  {
  public:
    uint32_t luma_log2_weight_denom;
    int32_t  chroma_log2_weight_denom;
    struct W
    {
      uint8_t luma_weight_flag;
      uint8_t chroma_weight_flag;
      int32_t luma_weight;
      int32_t luma_offset;
      int32_t chroma_weight[2];
      int32_t chroma_offset[2];
    };
    std::vector<W> l0;
    std::vector<W> l1;

    void toDefault();
  };

  class Slice
  {
  public:
    enum SliceType
    {
      P_SLICE = 0,
      B_SLICE = 1,
      I_SLICE = 2,
      SP_SLICE = 3,
      SI_SLICE = 4,
      NONE_SLICE = 9
    };

    uint32_t     first_mb_in_slice;
    uint32_t     slice_type;
    uint32_t     pic_parameter_set_id;
    uint32_t     colour_plane_id;
    uint32_t     frame_num;
    uint8_t      field_pic_flag;
    uint8_t      bottom_field_flag;
    uint32_t     idr_pic_id;
    uint32_t     pic_order_cnt_lsb;
    int32_t      delta_pic_order_cnt_bottom;
    int32_t      delta_pic_order_cnt[2];
    uint32_t     redundant_pic_cnt;
    uint8_t      direct_spatial_mv_pred_flag;
    uint8_t      num_ref_idx_active_override_flag;
    uint32_t     num_ref_idx_l0_active_minus1;
    uint32_t     num_ref_idx_l1_active_minus1;
    RefPicListModification ref_pic_list_modification;
    uint8_t      no_output_of_prior_pics_flag;
    uint8_t      long_term_reference_flag;
    DecRefPicMarking dec_ref_pic_marking;
    PredWeightTable pred_weight_table;
    uint8_t      cabac_init_idc;
    int32_t      slice_qp_delta;
    uint8_t      sp_for_switch_flag;
    int32_t      slice_qs_delta;
    uint8_t      disable_deblocking_filter_idc;
    int32_t      slice_alpha_c0_offset_div2;
    int32_t      slice_beta_offset_div2;
    std::vector<uint8_t> slice_group_change_cycle;

    void toDefault();
  };

  class NALUnit
  {
  public:
    NALUnit(NALHeader header);
    virtual ~NALUnit();

    bool        m_processFailed;
    NALHeader   m_nalHeader;
  };

  class SPS_NAL: public NALUnit
  {
  public:
    SPS_NAL(NALHeader header);
    SPS         sps;
  };

  class PPS_NAL: public NALUnit
  {
  public:
    PPS_NAL(NALHeader header);
    PPS         pps;
  };

  class Slice_NAL: public NALUnit
  {
  public:
    Slice_NAL(NALHeader header);
    Slice       slice;
  };

  class AUD
  {
  public:
    uint8_t primary_pic_type;
    void toDefault();
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

  class AUD_NAL: public NALUnit
  {
  public:
    AUD_NAL(NALHeader header);
    AUD aud;
  };

}

#endif
