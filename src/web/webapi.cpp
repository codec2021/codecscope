#include "WebParser.h"
#include "ProfileConformanceAnalyzer.h"
#include "AvcWebParser.h"
#include "VvcWebParser.h"
#include "CodecDetector.h"

#include <HevcParser.h>
#include <AvcParser.h>
#include <VvcParser.h>

#include <cstring>
#include <cstdlib>
#include <cstdint>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define HEVC_KEEPALIVE EMSCRIPTEN_KEEPALIVE
#else
#define HEVC_KEEPALIVE
#endif

namespace
{
  web::WebParser                  *g_webParser = nullptr;
  web::ProfileConformanceAnalyzer *g_profileAnalyzer = nullptr;
  HEVC::Parser                    *g_parser = nullptr;

  web::AvcWebParser               *g_avcWebParser = nullptr;
  AVC::Parser                     *g_avcParser = nullptr;

  web::VvcWebParser               *g_vvcWebParser = nullptr;
  VVC::Parser                     *g_vvcParser = nullptr;

  char *dupString(const std::string &s)
  {
    char *out = (char *)malloc(s.size() + 1);
    if(out)
    {
      memcpy(out, s.c_str(), s.size() + 1);
    }
    return out;
  }
}

extern "C"
{

  HEVC_KEEPALIVE void hevc_reset();
  HEVC_KEEPALIVE void avc_reset();
  HEVC_KEEPALIVE void vvc_reset();

  // 检测码流类型：返回 "hevc" / "avc" / "vvc" / "unknown"
  HEVC_KEEPALIVE char *detect_codec(const uint8_t *data, std::size_t size)
  {
    return dupString(web::detectCodec(data, size));
  }

  // ---------- HEVC ----------
  HEVC_KEEPALIVE char *hevc_parse(const uint8_t *data, std::size_t size)
  {
    hevc_reset();

    g_parser = HEVC::Parser::create();
    g_webParser = new web::WebParser();
    g_profileAnalyzer = new web::ProfileConformanceAnalyzer();

    g_profileAnalyzer -> m_pconsumer = g_webParser;
    g_webParser -> setTotalSize(size);

    g_parser -> addConsumer(g_webParser);
    g_parser -> addConsumer(g_profileAnalyzer);

    g_parser -> process(data, size);

    return dupString(g_webParser -> serializeSummary());
  }

  HEVC_KEEPALIVE char *hevc_get_nal_syntax(std::size_t index)
  {
    if(!g_webParser)
      return dupString("{\"n\":\"No data parsed\"}");
    return dupString(g_webParser -> serializeNalSyntax(index));
  }

  HEVC_KEEPALIVE void hevc_reset()
  {
    if(g_parser)
    {
      HEVC::Parser::release(g_parser);
      g_parser = nullptr;
    }
    delete g_webParser;
    g_webParser = nullptr;
    delete g_profileAnalyzer;
    g_profileAnalyzer = nullptr;
  }

  // ---------- H.264/AVC ----------
  HEVC_KEEPALIVE char *avc_parse(const uint8_t *data, std::size_t size)
  {
    avc_reset();

    g_avcParser = AVC::Parser::create();
    g_avcWebParser = new web::AvcWebParser();
    g_avcWebParser -> setTotalSize(size);

    g_avcParser -> addConsumer(g_avcWebParser);
    g_avcParser -> process(data, size);

    return dupString(g_avcWebParser -> serializeSummary());
  }

  HEVC_KEEPALIVE char *avc_get_nal_syntax(std::size_t index)
  {
    if(!g_avcWebParser)
      return dupString("{\"n\":\"No data parsed\"}");
    return dupString(g_avcWebParser -> serializeNalSyntax(index));
  }

  HEVC_KEEPALIVE void avc_reset()
  {
    if(g_avcParser)
    {
      AVC::Parser::release(g_avcParser);
      g_avcParser = nullptr;
    }
    delete g_avcWebParser;
    g_avcWebParser = nullptr;
  }

  // ---------- H.266/VVC ----------
  HEVC_KEEPALIVE char *vvc_parse(const uint8_t *data, std::size_t size)
  {
    vvc_reset();

    g_vvcParser = VVC::Parser::create();
    g_vvcWebParser = new web::VvcWebParser();
    g_vvcWebParser -> setTotalSize(size);

    g_vvcParser -> addConsumer(g_vvcWebParser);
    g_vvcParser -> process(data, size);

    return dupString(g_vvcWebParser -> serializeSummary());
  }

  HEVC_KEEPALIVE char *vvc_get_nal_syntax(std::size_t index)
  {
    if(!g_vvcWebParser)
      return dupString("{\"n\":\"No data parsed\"}");
    return dupString(g_vvcWebParser -> serializeNalSyntax(index));
  }

  HEVC_KEEPALIVE void vvc_reset()
  {
    if(g_vvcParser)
    {
      VVC::Parser::release(g_vvcParser);
      g_vvcParser = nullptr;
    }
    delete g_vvcWebParser;
    g_vvcWebParser = nullptr;
  }

  // ---------- 通用 ----------
  HEVC_KEEPALIVE void hevc_free(void *ptr)
  {
    free(ptr);
  }

}
