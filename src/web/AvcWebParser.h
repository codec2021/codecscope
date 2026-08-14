#ifndef WEB_AVC_WEB_PARSER_H_
#define WEB_AVC_WEB_PARSER_H_

#include <AvcParser.h>

#include <memory>
#include <map>
#include <vector>
#include <string>
#include <cstddef>

namespace web
{

  class AvcWebParser: public AVC::Parser::Consumer
  {
    public:
      AvcWebParser();
      virtual ~AvcWebParser();

      void reset();
      void setTotalSize(std::size_t size);

      void onNALUnit(std::shared_ptr<AVC::NALUnit> pNALUnit, const AVC::Parser::Info *pInfo) override;
      void onWarning(const std::string &warning, const AVC::Parser::Info *pInfo, AVC::Parser::WarningType type) override;

      std::string serializeSummary() const;
      std::string serializeNalSyntax(std::size_t index) const;

    private:
      struct NALUEntry
      {
        std::size_t                 offset;
        std::size_t                 length;
        uint32_t                    type;
        std::string                 typeName;
        std::string                 info;
        std::string                 color;
        int                         sliceType;   // -1 非 slice, 0/1/2 = P/B/I
        int                         sliceQp;
        int                         slicePoc;
        int                         frameNum;
        std::shared_ptr<AVC::NALUnit> nal;
      };

      struct WarningEntry
      {
        std::size_t position;
        std::string message;
        int         type;
      };

      std::vector<NALUEntry>    m_nalus;
      std::vector<WarningEntry> m_warnings;

      std::size_t m_totalSize;
      std::size_t m_nalusNumber;
      std::size_t m_INumber;
      std::size_t m_PNumber;
      std::size_t m_BNumber;
      std::size_t m_frameNum;
      std::size_t m_profile;
      std::size_t m_level;
      bool        m_profilePresent;
      bool        m_levelPresent;

      std::shared_ptr<AVC::SPS_NAL> m_lastSPS;

      std::map<uint32_t, std::shared_ptr<AVC::SPS_NAL> > m_spsMap;
      std::map<uint32_t, std::shared_ptr<AVC::PPS_NAL> > m_ppsMap;
  };

}

#endif
