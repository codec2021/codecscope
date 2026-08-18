#ifndef WEB_VVC_WEB_PARSER_H_
#define WEB_VVC_WEB_PARSER_H_

#include <VvcParser.h>

#include <memory>
#include <map>
#include <vector>
#include <string>
#include <cstddef>

namespace web
{

  class VvcWebParser: public VVC::Parser::Consumer
  {
    public:
      VvcWebParser();
      virtual ~VvcWebParser();

      void reset();
      void setTotalSize(std::size_t size);

      void onNALUnit(std::shared_ptr<VVC::NALUnit> pNALUnit, const VVC::Parser::Info *pInfo) override;
      void onWarning(const std::string &warning, const VVC::Parser::Info *pInfo, VVC::Parser::WarningType type) override;

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
        int                         sliceType;
        int                         sliceQp;
        int                         slicePoc;
        int                         frameNum;
        std::vector<int>            refPocs;
        std::shared_ptr<VVC::NALUnit> nal;
      };

      void fillPocAndRefs(NALUEntry &e, const VVC::Slice_NAL *p);

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

      int         m_pocMsb;
      int         m_prevPicOrderCntLsb;
      bool        m_pocInitialized;

      std::shared_ptr<VVC::SPS_NAL> m_lastSPS;

      std::map<uint8_t, std::shared_ptr<VVC::SPS_NAL> > m_spsMap;
      std::map<uint8_t, std::shared_ptr<VVC::PPS_NAL> > m_ppsMap;
  };

}

#endif
