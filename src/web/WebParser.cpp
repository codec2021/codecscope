#include "WebParser.h"

#include "WebSyntaxWriter.h"
#include "Json.h"

#include <limits>
#include <sstream>

namespace web
{

  namespace
  {
    std::string colourPrimariesToString(uint32_t value)
    {
      switch(value)
      {
        case 1: return "bt709";
        case 2: return "Unspecified";
        case 4: return "bt470m";
        case 5: return "bt470bg";
        case 6: return "smpte170m";
        case 7: return "smpte240m";
        case 8: return "film";
        case 9: return "bt2020";
        default: return "Reserved";
      }
    }

    std::string transferCharacteristicsToString(uint32_t value)
    {
      switch(value)
      {
        case 1: return "bt709";
        case 2: return "Unspecified";
        case 4: return "bt470m";
        case 5: return "bt470bg";
        case 6: return "smpte170m";
        case 7: return "smpte240m";
        case 8: return "linear";
        case 9: return "log100";
        case 10: return "log316";
        case 11: return "iec61966-2-4";
        case 12: return "bt1361e";
        case 13: return "iec61966-2-1";
        case 14: return "bt2020-10";
        case 15: return "bt2020-12";
        case 16: return "smpte-st-2084";
        case 17: return "smpte-st-428";
        case 18: return "arib-std-b67";
        default: return "Reserved";
      }
    }

    std::string matrixCoefficientsToString(uint32_t value)
    {
      switch(value)
      {
        case 0: return "GBR";
        case 1: return "bt709";
        case 2: return "Unspecified";
        case 4: return "fcc";
        case 5: return "bt470bg";
        case 6: return "smpte170m";
        case 7: return "smpte240m";
        case 8: return "YCgCo";
        case 9: return "bt2020nc";
        case 10: return "bt2020c";
        default: return "Reserved";
      }
    }

    std::string profileName(std::size_t profile)
    {
      switch(profile)
      {
        case 1: return "Main";
        case 2: return "Main 10";
        case 3: return "Main Still Picture";
        default:
        {
          std::stringstream ss;
          ss << profile << " (UNKNOWN)";
          return ss.str();
        }
      }
    }

    std::string tierName(std::size_t tier)
    {
      switch(tier)
      {
        case 0: return "Main";
        case 1: return "High";
        default: return "NOT PRESENT";
      }
    }

    std::string formatDouble(double d)
    {
      std::stringstream ss;
      ss << d;
      return ss.str();
    }
  }

  WebParser::WebParser():
    m_totalSize(0)
    ,m_nalusNumber(0)
    ,m_INumber(0)
    ,m_PNumber(0)
    ,m_BNumber(0)
    ,m_profile(std::numeric_limits<std::size_t>::max())
    ,m_level(std::numeric_limits<std::size_t>::max())
    ,m_tier(std::numeric_limits<std::size_t>::max())
    ,m_frameNum(0)
    ,m_profilePresent(false)
    ,m_prevSliceType(HEVC::Slice::NONE_SLICE)
  {
  }

  WebParser::~WebParser()
  {
  }

  void WebParser::reset()
  {
    m_nalus.clear();
    m_warnings.clear();
    m_totalSize = 0;
    m_nalusNumber = 0;
    m_INumber = 0;
    m_PNumber = 0;
    m_BNumber = 0;
    m_profile = std::numeric_limits<std::size_t>::max();
    m_level = std::numeric_limits<std::size_t>::max();
    m_tier = std::numeric_limits<std::size_t>::max();
    m_frameNum = 0;
    m_profilePresent = false;
    m_prevSliceType = HEVC::Slice::NONE_SLICE;
    m_lastSPS.reset();
    m_masteringDisplayInfo.reset();
    m_cllInfo.reset();
    m_vpsMap.clear();
    m_spsMap.clear();
    m_ppsMap.clear();
  }

  void WebParser::setTotalSize(std::size_t size)
  {
    m_totalSize = size;
  }

  int WebParser::calcSliceQp(std::shared_ptr<HEVC::Slice> pSlice)
  {
    if(!pSlice)
      return -1;
    int qp = 26 + pSlice -> slice_qp_delta;
    auto it = m_ppsMap.find(pSlice -> slice_pic_parameter_set_id);
    if(it != m_ppsMap.end() && it->second)
      qp += it->second -> init_qp_minus26;
    return qp;
  }

  void WebParser::onNALUnit(std::shared_ptr<HEVC::NALUnit> pNALUnit, const HEVC::Parser::Info *pInfo)
  {
    NALUEntry e;
    e.offset = pInfo -> m_position;
    e.length = 0;
    e.type = (uint32_t)pNALUnit -> m_nalHeader.type;
    e.typeName = ConvToString::NALUnitType(pNALUnit -> m_nalHeader.type);
    e.sliceType = -1;
    e.sliceQp = -1;
    e.slicePoc = -1;
    e.sliceAddr = -1;
    e.firstSlice = -1;
    e.nal = pNALUnit;

    m_nalusNumber++;

    using namespace HEVC;

    switch(pNALUnit -> m_nalHeader.type)
    {
      case NAL_VPS:
      {
        std::shared_ptr<VPS> pVPS = std::dynamic_pointer_cast<VPS>(pNALUnit);
        e.info = "Video Parameter Set";
        e.color = "#FF8888";
        m_profile = pVPS -> profile_tier_level.general_profile_idc;
        m_tier = pVPS -> profile_tier_level.general_tier_flag;
        m_level = pVPS -> profile_tier_level.general_level_idc;
        m_profilePresent = true;
        m_vpsMap[pVPS -> vps_video_parameter_set_id] = pVPS;
        break;
      }

      case NAL_SPS:
      {
        std::shared_ptr<SPS> pSPS = std::dynamic_pointer_cast<SPS>(pNALUnit);
        e.info = "Sequence Parameter Set";
        e.color = "#FF8888";
        m_profile = pSPS -> profile_tier_level.general_profile_idc;
        m_tier = pSPS -> profile_tier_level.general_tier_flag;
        m_level = pSPS -> profile_tier_level.general_level_idc;
        m_profilePresent = true;
        m_lastSPS = pSPS;
        m_spsMap[pSPS -> sps_seq_parameter_set_id] = pSPS;
        break;
      }

      case NAL_PPS:
      {
        std::shared_ptr<PPS> pPPS = std::dynamic_pointer_cast<PPS>(pNALUnit);
        e.info = "Picture Parameter Set";
        e.color = "#FF8888";
        m_ppsMap[pPPS -> pps_pic_parameter_set_id] = pPPS;
        break;
      }

      case NAL_IDR_W_RADL:
      case NAL_IDR_N_LP:
      {
        std::shared_ptr<Slice> pSlice = std::dynamic_pointer_cast<Slice>(pNALUnit);
        e.info = "IDR Slice #" + std::to_string(m_frameNum);
        e.color = "red";
        e.sliceType = 2; // I
        e.slicePoc = pSlice -> slice_pic_order_cnt_lsb;
        e.sliceQp = calcSliceQp(pSlice);
        e.sliceAddr = pSlice -> slice_segment_address;
        e.firstSlice = pSlice -> first_slice_segment_in_pic_flag;
        m_frameNum++;
        m_INumber++;
        m_prevSliceType = HEVC::Slice::I_SLICE;
        break;
      }

      case NAL_TRAIL_R:
      case NAL_TSA_R:
      case NAL_STSA_R:
      case NAL_RADL_R:
      case NAL_RASL_R:
      case NAL_TRAIL_N:
      case NAL_TSA_N:
      case NAL_STSA_N:
      case NAL_RADL_N:
      case NAL_RASL_N:
      case NAL_BLA_W_LP:
      case NAL_BLA_W_RADL:
      case NAL_BLA_N_LP:
      case NAL_CRA_NUT:
      {
        std::shared_ptr<Slice> pSlice = std::dynamic_pointer_cast<Slice>(pNALUnit);
        e.slicePoc = pSlice -> slice_pic_order_cnt_lsb;
        e.sliceQp = calcSliceQp(pSlice);
        e.sliceAddr = pSlice -> slice_segment_address;
        e.firstSlice = pSlice -> first_slice_segment_in_pic_flag;

        if(pSlice -> dependent_slice_segment_flag)
        {
          e.info = "Dependent Slice";
          e.sliceType = (int)m_prevSliceType; // 复用前一个 slice 类型
          switch(m_prevSliceType)
          {
            case HEVC::Slice::B_SLICE: m_BNumber++; break;
            case HEVC::Slice::P_SLICE: m_PNumber++; break;
            case HEVC::Slice::I_SLICE: m_INumber++; break;
            case HEVC::Slice::NONE_SLICE: break;
          }
        }
        else
        {
          switch(pSlice -> slice_type)
          {
            case HEVC::Slice::B_SLICE:
            {
              e.info = "B Slice #" + std::to_string(m_frameNum);
              if(pNALUnit -> m_nalHeader.type == NAL_TSA_N)
                e.color = "#FF6EB4";
              else if(pNALUnit -> m_nalHeader.type == NAL_TSA_R)
                e.color = "#FF83FA";
              else if(pNALUnit -> m_nalHeader.type == NAL_TRAIL_N)
                e.color = "#E066FF";
              else if(pNALUnit -> m_nalHeader.type == NAL_TRAIL_R)
                e.color = "#EE30A7";
              else
                e.color = "#FFB90F";
              e.sliceType = 1; // B
              m_BNumber++;
              break;
            }
            case HEVC::Slice::P_SLICE:
              e.info = "P Slice #" + std::to_string(m_frameNum);
              e.color = "#0000FF";
              e.sliceType = 0; // P
              m_PNumber++;
              break;
            case HEVC::Slice::I_SLICE:
              e.info = "I Slice #" + std::to_string(m_frameNum);
              e.color = "#CD9B1D";
              e.sliceType = 2; // I
              m_INumber++;
              break;
            default:
              break;
          }
          m_prevSliceType = (HEVC::Slice::SliceType)pSlice -> slice_type;
        }
        m_frameNum++;
        break;
      }

      case NAL_AUD:
        e.info = "Access unit delimiter";
        break;

      case NAL_EOS_NUT:
        e.info = "End of sequence";
        break;

      case NAL_EOB_NUT:
        e.info = "End of bitstream";
        break;

      case NAL_FD_NUT:
        e.info = "Filler data";
        break;

      case NAL_SEI_PREFIX:
      case NAL_SEI_SUFFIX:
      {
        e.info = "Supplemental Enhancement Information";
        e.color = "#BCEE68";

        std::shared_ptr<SEI> pSEI = std::dynamic_pointer_cast<SEI>(pNALUnit);
        for(std::size_t i = 0; i < pSEI -> sei_message.size(); i++)
        {
          std::size_t payloadType = 0;
          for(std::size_t k = 0; k < pSEI -> sei_message[i].num_payload_type_ff_bytes; k++)
            payloadType += 255;
          payloadType += pSEI -> sei_message[i].last_payload_type_byte;

          if(payloadType == HEVC::SeiMessage::MASTERING_DISPLAY_INFO)
            m_masteringDisplayInfo = std::dynamic_pointer_cast<HEVC::MasteringDisplayInfo>(pSEI -> sei_message[i].sei_payload);
          else if(payloadType == HEVC::SeiMessage::CONTENT_LIGHT_LEVEL_INFO)
            m_cllInfo = std::dynamic_pointer_cast<HEVC::ContentLightLevelInfo>(pSEI -> sei_message[i].sei_payload);
        }
        break;
      }

      default:
        break;
    }

    m_nalus.push_back(e);
  }

  void WebParser::onWarning(const std::string &warning, const HEVC::Parser::Info *pInfo, HEVC::Parser::WarningType type)
  {
    WarningEntry w;
    w.position = pInfo ? pInfo -> m_position : 0;
    w.message = warning;
    w.type = (int)type;
    m_warnings.push_back(w);
  }

  std::string WebParser::serializeSummary() const
  {
    std::string out;
    out.reserve(4096);

    // length 计算
    std::vector<std::size_t> lengths(m_nalus.size(), 0);
    for(std::size_t i = 0; i < m_nalus.size(); i++)
    {
      std::size_t next = (i + 1 < m_nalus.size()) ? m_nalus[i + 1].offset : m_totalSize;
      if(next >= m_nalus[i].offset)
        lengths[i] = next - m_nalus[i].offset;
    }

    out += "{\"nalus\":[";
    for(std::size_t i = 0; i < m_nalus.size(); i++)
    {
      if(i)
        out += ",";
      out += "{\"offset\":";
      out += std::to_string(m_nalus[i].offset);
      out += ",\"length\":";
      out += std::to_string(lengths[i]);
      out += ",\"type\":";
      out += std::to_string(m_nalus[i].type);
      out += ",\"typeName\":\"";
      out += jsonEscape(m_nalus[i].typeName);
      out += "\",\"info\":\"";
      out += jsonEscape(m_nalus[i].info);
      out += "\",\"color\":\"";
      out += jsonEscape(m_nalus[i].color);
      out += "\",\"sliceType\":";
      out += std::to_string(m_nalus[i].sliceType);
      out += ",\"sliceQp\":";
      out += std::to_string(m_nalus[i].sliceQp);
      out += ",\"slicePoc\":";
      out += std::to_string(m_nalus[i].slicePoc);
      out += "}";
    }
    out += "]";

    // streamInfo
    std::size_t slicesNumber = m_INumber + m_PNumber + m_BNumber;
    out += ",\"streamInfo\":{";
    out += "\"nalus\":" + std::to_string(m_nalusNumber);
    out += ",\"slices\":" + std::to_string(slicesNumber);
    out += ",\"i\":" + std::to_string(m_INumber);
    out += ",\"p\":" + std::to_string(m_PNumber);
    out += ",\"b\":" + std::to_string(m_BNumber);
    if(slicesNumber)
    {
      out += ",\"iPct\":" + formatDouble((double)m_INumber * 100.0 / slicesNumber);
      out += ",\"pPct\":" + formatDouble((double)m_PNumber * 100.0 / slicesNumber);
      out += ",\"bPct\":" + formatDouble((double)m_BNumber * 100.0 / slicesNumber);
    }
    if(m_profilePresent)
    {
      out += ",\"profile\":\"" + jsonEscape(profileName(m_profile)) + "\"";
      out += ",\"profileIdc\":" + std::to_string(m_profile);
    }
    else
      out += ",\"profile\":\"NOT PRESENT\"";

    if(m_level != std::numeric_limits<std::size_t>::max())
      out += ",\"level\":\"" + formatDouble((double)m_level / 30.0) + "\"";
    else
      out += ",\"level\":\"NOT PRESENT\"";

    out += ",\"tier\":\"" + jsonEscape(tierName(m_tier)) + "\"";
    out += "}";

    // HDR info
    out += ",\"hdr\":{";
    int colour_primaries = 2;
    int transfer_characteristics = 2;
    int matrix_coeffs = 2;
    int chroma_loc_top = 0;
    int chroma_loc_bottom = 0;
    int video_full_range_flag = 0;

    if(m_lastSPS && m_lastSPS -> vui_parameters_present_flag &&
       m_lastSPS -> vui_parameters.video_signal_type_present_flag &&
       m_lastSPS -> vui_parameters.colour_description_present_flag)
    {
      colour_primaries = m_lastSPS -> vui_parameters.colour_primaries;
      transfer_characteristics = m_lastSPS -> vui_parameters.transfer_characteristics;
      matrix_coeffs = m_lastSPS -> vui_parameters.matrix_coeffs;
    }

    if(m_lastSPS && m_lastSPS -> vui_parameters_present_flag &&
       m_lastSPS -> vui_parameters.chroma_loc_info_present_flag)
    {
      chroma_loc_top = m_lastSPS -> vui_parameters.chroma_sample_loc_type_top_field;
      chroma_loc_bottom = m_lastSPS -> vui_parameters.chroma_sample_loc_type_bottom_field;
    }

    if(m_lastSPS && m_lastSPS -> vui_parameters_present_flag &&
       m_lastSPS -> vui_parameters.video_signal_type_present_flag)
    {
      video_full_range_flag = m_lastSPS -> vui_parameters.video_full_range_flag;
    }

    out += "\"colourPrimaries\":\"" + jsonEscape(colourPrimariesToString(colour_primaries)) + "\"";
    out += ",\"transferCharacteristics\":\"" + jsonEscape(transferCharacteristicsToString(transfer_characteristics)) + "\"";
    out += ",\"matrixCoefficients\":\"" + jsonEscape(matrixCoefficientsToString(matrix_coeffs)) + "\"";
    out += ",\"chromaLocTop\":" + std::to_string(chroma_loc_top);
    out += ",\"chromaLocBottom\":" + std::to_string(chroma_loc_bottom);
    out += ",\"fullRange\":" + std::to_string(video_full_range_flag);

    if(m_cllInfo)
    {
      out += ",\"hasCll\":true";
      out += ",\"maxCll\":" + std::to_string(m_cllInfo -> max_content_light_level);
      out += ",\"avgCll\":" + std::to_string(m_cllInfo -> max_pic_average_light_level);
    }
    else
      out += ",\"hasCll\":false";

    if(m_masteringDisplayInfo)
    {
      out += ",\"hasMdi\":true";
      std::stringstream ss;
      ss << "G(" << m_masteringDisplayInfo->display_primary_x[0] << "," << m_masteringDisplayInfo->display_primary_y[0] << "), "
         << "B(" << m_masteringDisplayInfo->display_primary_x[1] << "," << m_masteringDisplayInfo->display_primary_y[1] << "), "
         << "R(" << m_masteringDisplayInfo->display_primary_x[2] << "," << m_masteringDisplayInfo->display_primary_y[2] << "), "
         << "WP(" << m_masteringDisplayInfo->white_point_x << "," << m_masteringDisplayInfo->white_point_y << "), "
         << "L(" << m_masteringDisplayInfo->max_display_mastering_luminance << "," << m_masteringDisplayInfo->min_display_mastering_luminance << ")";
      out += ",\"masteringDisplay\":\"" + jsonEscape(ss.str()) + "\"";
    }
    else
      out += ",\"hasMdi\":false";

    out += "}";

    // warnings
    out += ",\"warnings\":[";
    for(std::size_t i = 0; i < m_warnings.size(); i++)
    {
      if(i)
        out += ",";
      out += "{\"position\":";
      out += std::to_string(m_warnings[i].position);
      out += ",\"message\":\"";
      out += jsonEscape(m_warnings[i].message);
      out += "\",\"type\":";
      out += std::to_string(m_warnings[i].type);
      out += "}";
    }
    out += "]";

    out += "}";
    return out;
  }

  std::string WebParser::serializeNalSyntax(std::size_t index) const
  {
    if(index >= m_nalus.size())
      return "{\"n\":\"Invalid index\"}";

    SyntaxWriter writer;
    writer.setParameterSets(m_vpsMap, m_spsMap, m_ppsMap);
    return writer.write(m_nalus[index].nal);
  }

}
