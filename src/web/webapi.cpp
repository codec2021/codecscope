#include "WebParser.h"
#include "ProfileConformanceAnalyzer.h"

#include <HevcParser.h>

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

  // 解析原始 HEVC 基本流，返回汇总 JSON（NAL 列表 + 流信息 + HDR + 警告）
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

  // 返回第 index 个 NAL 单元的语法树 JSON
  HEVC_KEEPALIVE char *hevc_get_nal_syntax(std::size_t index)
  {
    if(!g_webParser)
      return dupString("{\"n\":\"No data parsed\"}");
    return dupString(g_webParser -> serializeNalSyntax(index));
  }

  // 释放上一次解析占用的资源
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

  // 释放由上面接口返回的字符串内存
  HEVC_KEEPALIVE void hevc_free(void *ptr)
  {
    free(ptr);
  }

}
