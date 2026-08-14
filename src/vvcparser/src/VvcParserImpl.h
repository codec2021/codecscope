#ifndef VVC_PARSER_IMPL_H_
#define VVC_PARSER_IMPL_H_

#include "VvcParser.h"
#include "BitstreamReader.h"

#include <map>
#include <list>
#include <memory>

namespace VVC
{
  class VvcParserImpl: public Parser
  {
    public:
      std::size_t process(const uint8_t *pdata, std::size_t size, std::size_t offset = 0) override;
      void addConsumer(Consumer *pconsumer) override;
      void releaseConsumer(Consumer *pconsumer) override;

    protected:
      void processNALUnit(const uint8_t *pdata, std::size_t size, const Parser::Info &info);
      void processNALUnitHeader(BitstreamReader &bs, NALHeader *header);
      void processVPS(std::shared_ptr<VPS_NAL> pvps, BitstreamReader &bs);
      void processSPS(std::shared_ptr<SPS_NAL> psps, BitstreamReader &bs);
      void processPPS(std::shared_ptr<PPS_NAL> ppps, BitstreamReader &bs, const Parser::Info &info);
      void processPH(std::shared_ptr<PH_NAL> pph, BitstreamReader &bs, const Parser::Info &info);
      void processSlice(std::shared_ptr<Slice_NAL> pslice, BitstreamReader &bs, const Parser::Info &info);
      void processAUD(std::shared_ptr<AUD_NAL> paud, BitstreamReader &bs);
      void processSEI(std::shared_ptr<SEI_NAL> psei, BitstreamReader &bs);

      void processProfileTierLevel(ProfileTierLevel &ptl, bool profileTierPresentFlag, uint8_t maxNumSubLayersMinus1, BitstreamReader &bs);
      void processRefPicLists(BitstreamReader &bs, const Parser::Info &info);
      void processPictureHeader(PH &ph, BitstreamReader &bs, const Parser::Info &info);

      void onWarning(const std::string &warning, const Info *pInfo, WarningType type);

      std::map<uint8_t, std::shared_ptr<VPS_NAL> >  m_vpsMap;
      std::map<uint8_t, std::shared_ptr<SPS_NAL> >  m_spsMap;
      std::map<uint8_t, std::shared_ptr<PPS_NAL> >  m_ppsMap;
      std::shared_ptr<PH_NAL>                       m_lastPH;

      std::list<Consumer *> m_consumers;
  };
}

#endif
