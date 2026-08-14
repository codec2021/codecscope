#ifndef WEB_CODEC_DETECTOR_H_
#define WEB_CODEC_DETECTOR_H_

#include <cstdint>
#include <cstddef>
#include <string>

namespace web
{
  // 返回 "hevc" / "avc" / "vvc" / "unknown"
  std::string detectCodec(const uint8_t *data, std::size_t size);
}

#endif
