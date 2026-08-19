#ifndef WEB_PARSER_H_
#define WEB_PARSER_H_

#include <HevcParser.h>
#include <ConvToString.h>

#include <memory>
#include <map>
#include <vector>
#include <string>
#include <cstddef>

namespace web
{

  class WebParser: public HEVC::Parser::Consumer
  {
    public:
      WebParser();
      virtual ~WebParser();

      void reset();
      void setTotalSize(std::size_t size);

      int calcSliceQp(std::shared_ptr<HEVC::Slice> pSlice);

      void onNALUnit(std::shared_ptr<HEVC::NALUnit> pNALUnit, const HEVC::Parser::Info *pInfo) override;
      void onWarning(const std::string &warning, const HEVC::Parser::Info *pInfo, HEVC::Parser::WarningType type) override;

      // 汇总 JSON（NAL 列表 + 流信息 + HDR + 警告）
      std::string serializeSummary() const;
      // 指定 NAL 的语法树 JSON
      std::string serializeNalSyntax(std::size_t index) const;

    private:
      struct NALUEntry
      {
        std::size_t                      offset;
        std::size_t                      length;
        uint32_t                         type;
        std::string                      typeName;
        std::string                      info;
        std::string                      color;
        int                              sliceType;   // -1 = 非 slice, 0/1/2 = B/P/I
        int                              sliceQp;     // -1 = 无效
        int                              slicePoc;
        int                              frameNum;
        int                              sliceAddr;
        int                              firstSlice;
        std::shared_ptr<HEVC::NALUnit>   nal;
      };

      void fillPocAndRefs(NALUEntry &e, std::shared_ptr<HEVC::Slice> pSlice);

      struct WarningEntry
      {
        std::size_t                position;
        std::string                message;
        int                        type;
      };

      std::vector<NALUEntry>        m_nalus;
      std::vector<WarningEntry>     m_warnings;

      std::size_t                   m_totalSize;
      std::size_t                   m_nalusNumber;
      std::size_t                   m_INumber;
      std::size_t                   m_PNumber;
      std::size_t                   m_BNumber;
      std::size_t                   m_profile;
      std::size_t                   m_level;
      std::size_t                   m_tier;
      std::size_t                   m_frameNum;
      bool                          m_profilePresent;

      HEVC::Slice::SliceType        m_prevSliceType;

      int                           m_pocMsb;
      int                           m_prevPicOrderCntLsb;
      bool                          m_pocInitialized;

      std::shared_ptr<HEVC::SPS>                    m_lastSPS;
      std::shared_ptr<HEVC::MasteringDisplayInfo>   m_masteringDisplayInfo;
      std::shared_ptr<HEVC::ContentLightLevelInfo>  m_cllInfo;

      std::map<uint32_t, std::shared_ptr<HEVC::VPS> > m_vpsMap;
      std::map<uint32_t, std::shared_ptr<HEVC::SPS> > m_spsMap;
      std::map<uint32_t, std::shared_ptr<HEVC::PPS> > m_ppsMap;
  };

}

#endif
