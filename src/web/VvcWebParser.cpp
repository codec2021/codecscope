#include "VvcWebParser.h"

#include "VvcSyntaxWriter.h"
#include "ColorNames.h"
#include "Json.h"

#include <sstream>

namespace web
{

  namespace
  {
    std::string vvcProfileName(uint32_t profileIdc)
    {
      switch(profileIdc)
      {
        case 1: return "Main 10";
        case 2: return "Main 10 Still Picture";
        case 33: return "Main 10 4:4:4";
        case 34: return "Main 10 4:4:4 Still Picture";
        default:
        {
          std::stringstream ss;
          ss << profileIdc << " (UNKNOWN)";
          return ss.str();
        }
      }
    }
  }

  VvcWebParser::VvcWebParser():
    m_totalSize(0)
    ,m_nalusNumber(0)
    ,m_INumber(0)
    ,m_PNumber(0)
    ,m_BNumber(0)
    ,m_frameNum(0)
    ,m_profile(0)
    ,m_level(0)
    ,m_profilePresent(false)
    ,m_levelPresent(false)
    ,m_pocMsb(0)
    ,m_prevPicOrderCntLsb(0)
    ,m_pocInitialized(false)
  {
  }

  VvcWebParser::~VvcWebParser()
  {
  }

  void VvcWebParser::reset()
  {
    m_nalus.clear();
    m_warnings.clear();
    m_totalSize = 0;
    m_nalusNumber = 0;
    m_INumber = 0;
    m_PNumber = 0;
    m_BNumber = 0;
    m_frameNum = 0;
    m_profile = 0;
    m_level = 0;
    m_profilePresent = false;
    m_levelPresent = false;
    m_pocMsb = 0;
    m_prevPicOrderCntLsb = 0;
    m_pocInitialized = false;
    m_lastSPS.reset();
    m_spsMap.clear();
    m_ppsMap.clear();
  }

  void VvcWebParser::setTotalSize(std::size_t size)
  {
    m_totalSize = size;
  }

  void VvcWebParser::fillPocAndRefs(NALUEntry &e, const VVC::Slice_NAL *p)
  {
    const VVC::Slice &sl = p->slice;

    std::shared_ptr<VVC::SPS_NAL> sps;
    auto ppsIt = m_ppsMap.find(sl.slice_pic_parameter_set_id);
    if(ppsIt != m_ppsMap.end())
    {
      auto spsIt = m_spsMap.find(ppsIt->second->pps.pps_seq_parameter_set_id);
      if(spsIt != m_spsMap.end())
        sps = spsIt->second;
    }
    if(!sps)
      sps = m_lastSPS;

    int pocLsb = (int)sl.slice_pic_order_cnt_lsb;
    int maxLsb = sps ? (1 << (int)(sps->sps.sps_log2_max_pic_order_cnt_lsb_minus4 + 4)) : 1;

    int msb = m_pocMsb;
    if(m_pocInitialized)
    {
      if(pocLsb < m_prevPicOrderCntLsb && (m_prevPicOrderCntLsb - pocLsb) >= maxLsb / 2)
        msb += maxLsb;
      else if(pocLsb > m_prevPicOrderCntLsb && (pocLsb - m_prevPicOrderCntLsb) > maxLsb / 2)
        msb -= maxLsb;
    }
    int pocFull = msb + pocLsb;
    m_prevPicOrderCntLsb = pocLsb;
    m_pocMsb = msb;
    m_pocInitialized = true;

    e.slicePoc = pocFull;
  }

  void VvcWebParser::onNALUnit(std::shared_ptr<VVC::NALUnit> pNALUnit, const VVC::Parser::Info *pInfo)
  {
    NALUEntry e;
    e.offset = pInfo ? pInfo->m_position : 0;
    e.length = 0;
    e.type = (uint32_t)pNALUnit->m_nalHeader.nal_unit_type;
    e.typeName = vvcNalTypeName(pNALUnit->m_nalHeader.nal_unit_type);
    e.sliceType = -1;
    e.sliceQp = -1;
    e.slicePoc = -1;
    e.frameNum = -1;
    e.nal = pNALUnit;

    m_nalusNumber++;

    switch(pNALUnit->m_nalHeader.nal_unit_type)
    {
      case VVC::NAL_VPS:
      {
        std::shared_ptr<VVC::VPS_NAL> p = std::dynamic_pointer_cast<VVC::VPS_NAL>(pNALUnit);
        e.info = "Video Parameter Set";
        e.color = "#FF8888";
        break;
      }

      case VVC::NAL_SPS:
      {
        std::shared_ptr<VVC::SPS_NAL> p = std::dynamic_pointer_cast<VVC::SPS_NAL>(pNALUnit);
        e.info = "Sequence Parameter Set";
        e.color = "#FF8888";
        if(p->sps.sps_ptl_dpb_hrd_params_present_flag)
        {
          m_profile = p->sps.profile_tier_level.general_profile_idc;
          m_level = p->sps.profile_tier_level.general_level_idc;
          m_profilePresent = true;
          m_levelPresent = true;
        }
        m_lastSPS = p;
        m_spsMap[p->sps.sps_seq_parameter_set_id] = p;
        break;
      }

      case VVC::NAL_PPS:
      {
        std::shared_ptr<VVC::PPS_NAL> p = std::dynamic_pointer_cast<VVC::PPS_NAL>(pNALUnit);
        e.info = "Picture Parameter Set";
        e.color = "#FF8888";
        m_ppsMap[p->pps.pps_pic_parameter_set_id] = p;
        break;
      }

      case VVC::NAL_PH:
        e.info = "Picture Header";
        e.color = "#FF8888";
        break;

      case VVC::NAL_IDR_W_RADL:
      case VVC::NAL_IDR_N_LP:
      case VVC::NAL_CRA_NUT:
      case VVC::NAL_GDR_NUT:
      {
        std::shared_ptr<VVC::Slice_NAL> p = std::dynamic_pointer_cast<VVC::Slice_NAL>(pNALUnit);
        e.info = (pNALUnit->m_nalHeader.nal_unit_type == VVC::NAL_IDR_W_RADL || pNALUnit->m_nalHeader.nal_unit_type == VVC::NAL_IDR_N_LP)
                 ? "IDR Slice #" + std::to_string(m_frameNum)
                 : "IRAP Slice #" + std::to_string(m_frameNum);
        e.color = "red";
        e.sliceType = 2;
        e.sliceQp = 26 + p->slice.slice_qp_delta;
        fillPocAndRefs(e, p.get());
        e.frameNum = m_frameNum;
        m_INumber++;
        m_frameNum++;
        break;
      }

      case VVC::NAL_TRAIL_NUT:
      case VVC::NAL_STSA_NUT:
      case VVC::NAL_RADL_NUT:
      case VVC::NAL_RASL_NUT:
      {
        std::shared_ptr<VVC::Slice_NAL> p = std::dynamic_pointer_cast<VVC::Slice_NAL>(pNALUnit);
        e.sliceQp = 26 + p->slice.slice_qp_delta;
        fillPocAndRefs(e, p.get());
        e.frameNum = m_frameNum;
        switch(p->slice.slice_type)
        {
          case 0: // B
            e.info = "B Slice #" + std::to_string(m_frameNum);
            e.color = "#FF83FA";
            e.sliceType = 1;
            m_BNumber++;
            break;
          case 1: // P
            e.info = "P Slice #" + std::to_string(m_frameNum);
            e.color = "#0000FF";
            e.sliceType = 0;
            m_PNumber++;
            break;
          case 2: // I
            e.info = "I Slice #" + std::to_string(m_frameNum);
            e.color = "#CD9B1D";
            e.sliceType = 2;
            m_INumber++;
            break;
        }
        m_frameNum++;
        break;
      }

      case VVC::NAL_AUD:
        e.info = "Access unit delimiter";
        break;

      case VVC::NAL_EOS_NUT:
        e.info = "End of sequence";
        break;

      case VVC::NAL_EOB_NUT:
        e.info = "End of bitstream";
        break;

      case VVC::NAL_PREFIX_SEI:
      case VVC::NAL_SUFFIX_SEI:
        e.info = "Supplemental Enhancement Information";
        e.color = "#BCEE68";
        break;

      case VVC::NAL_PREFIX_APS:
      case VVC::NAL_SUFFIX_APS:
        e.info = "Adaptation Parameter Set";
        e.color = "#FF8888";
        break;

      case VVC::NAL_OPI:
        e.info = "Operating Point Information";
        break;

      case VVC::NAL_DCI:
        e.info = "Decoding Capability Information";
        break;

      default:
        break;
    }

    m_nalus.push_back(e);
  }

  void VvcWebParser::onWarning(const std::string &warning, const VVC::Parser::Info *pInfo, VVC::Parser::WarningType type)
  {
    WarningEntry w;
    w.position = pInfo ? pInfo->m_position : 0;
    w.message = warning;
    w.type = (int)type;
    m_warnings.push_back(w);
  }

  std::string VvcWebParser::serializeSummary() const
  {
    std::string out;
    out.reserve(4096);

    std::vector<std::size_t> lengths(m_nalus.size(), 0);
    for(std::size_t i = 0; i < m_nalus.size(); i++)
    {
      std::size_t next = (i + 1 < m_nalus.size()) ? m_nalus[i + 1].offset : m_totalSize;
      if(next >= m_nalus[i].offset) lengths[i] = next - m_nalus[i].offset;
    }

    out += "{\"nalus\":[";
    for(std::size_t i = 0; i < m_nalus.size(); i++)
    {
      if(i) out += ",";
      out += "{\"offset\":" + std::to_string(m_nalus[i].offset);
      out += ",\"length\":" + std::to_string(lengths[i]);
      out += ",\"type\":" + std::to_string(m_nalus[i].type);
      out += ",\"typeName\":\"" + jsonEscape(m_nalus[i].typeName) + "\"";
      out += ",\"info\":\"" + jsonEscape(m_nalus[i].info) + "\"";
      out += ",\"color\":\"" + jsonEscape(m_nalus[i].color) + "\"";
      out += ",\"sliceType\":" + std::to_string(m_nalus[i].sliceType);
      out += ",\"sliceQp\":" + std::to_string(m_nalus[i].sliceQp);
      out += ",\"slicePoc\":" + std::to_string(m_nalus[i].slicePoc);
      out += ",\"frameNum\":" + std::to_string(m_nalus[i].frameNum);
      out += "}";
    }
    out += "]";

    std::size_t slicesNumber = m_INumber + m_PNumber + m_BNumber;
    out += ",\"streamInfo\":{";
    out += "\"nalus\":" + std::to_string(m_nalusNumber);
    out += ",\"slices\":" + std::to_string(slicesNumber);
    out += ",\"i\":" + std::to_string(m_INumber);
    out += ",\"p\":" + std::to_string(m_PNumber);
    out += ",\"b\":" + std::to_string(m_BNumber);
    if(slicesNumber)
    {
      std::stringstream ssI; ssI << (double)m_INumber * 100.0 / slicesNumber;
      std::stringstream ssP; ssP << (double)m_PNumber * 100.0 / slicesNumber;
      std::stringstream ssB; ssB << (double)m_BNumber * 100.0 / slicesNumber;
      out += ",\"iPct\":\"" + ssI.str() + "\"";
      out += ",\"pPct\":\"" + ssP.str() + "\"";
      out += ",\"bPct\":\"" + ssB.str() + "\"";
    }
    if(m_profilePresent)
    {
      out += ",\"profile\":\"" + jsonEscape(vvcProfileName(m_profile)) + "\"";
      out += ",\"profileIdc\":" + std::to_string(m_profile);
    }
    else
      out += ",\"profile\":\"NOT PRESENT\"";
    if(m_levelPresent)
      out += ",\"level\":\"" + std::to_string(m_level) + "\"";
    else
      out += ",\"level\":\"NOT PRESENT\"";
    out += ",\"tier\":\"\"";
    out += "}";

    out += ",\"hdr\":{";
    out += "\"colourPrimaries\":\"Unspecified\"";
    out += ",\"transferCharacteristics\":\"Unspecified\"";
    out += ",\"matrixCoefficients\":\"Unspecified\"";
    out += ",\"chromaLocTop\":0";
    out += ",\"chromaLocBottom\":0";
    out += ",\"fullRange\":0";
    out += ",\"hasCll\":false";
    out += ",\"hasMdi\":false";

    int picWidth = 0, picHeight = 0;
    if(m_lastSPS)
    {
      picWidth = m_lastSPS->sps.sps_pic_width_max_in_luma_samples;
      picHeight = m_lastSPS->sps.sps_pic_height_max_in_luma_samples;
    }
    out += ",\"picWidth\":" + std::to_string(picWidth);
    out += ",\"picHeight\":" + std::to_string(picHeight);
    out += "}";

    out += ",\"warnings\":[";
    for(std::size_t i = 0; i < m_warnings.size(); i++)
    {
      if(i) out += ",";
      out += "{\"position\":" + std::to_string(m_warnings[i].position);
      out += ",\"message\":\"" + jsonEscape(m_warnings[i].message) + "\"";
      out += ",\"type\":" + std::to_string(m_warnings[i].type) + "}";
    }
    out += "]";

    out += "}";
    return out;
  }

  std::string VvcWebParser::serializeNalSyntax(std::size_t index) const
  {
    if(index >= m_nalus.size())
      return "{\"n\":\"Invalid index\"}";

    VvcSyntaxWriter writer;
    writer.setParameterSets(m_spsMap, m_ppsMap);
    return writer.write(m_nalus[index].nal);
  }

}
