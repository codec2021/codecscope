#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>

extern "C"
{
  char *detect_codec(const uint8_t *data, std::size_t size);
  char *hevc_parse(const uint8_t *data, std::size_t size);
  char *hevc_get_nal_syntax(std::size_t index);
  void hevc_reset();
  char *avc_parse(const uint8_t *data, std::size_t size);
  char *avc_get_nal_syntax(std::size_t index);
  void avc_reset();
  char *vvc_parse(const uint8_t *data, std::size_t size);
  char *vvc_get_nal_syntax(std::size_t index);
  void vvc_reset();
  void hevc_free(void *ptr);
}

int main(int argc, char **argv)
{
  if(argc < 2)
  {
    std::cerr << "Usage: " << argv[0] << " <input> [nal_index]" << std::endl;
    return 1;
  }

  std::ifstream in(argv[1], std::ios_base::binary);
  if(!in.good())
  {
    std::cerr << "Cannot open file: " << argv[1] << std::endl;
    return 2;
  }

  in.seekg(0, std::ios::end);
  std::size_t size = (std::size_t)in.tellg();
  in.seekg(0, std::ios::beg);

  std::vector<uint8_t> data(size);
  in.read((char *)data.data(), size);

  char *codec = detect_codec(data.data(), size);
  std::string cc = codec ? codec : "unknown";
  hevc_free(codec);
  std::cerr << "检测到码流类型: " << cc << std::endl;

  char *summary = nullptr;
  if(cc == "avc")
    summary = avc_parse(data.data(), size);
  else if(cc == "vvc")
    summary = vvc_parse(data.data(), size);
  else
    summary = hevc_parse(data.data(), size);

  if(!summary)
  {
    std::cerr << "parse failed" << std::endl;
    return 3;
  }
  std::cout << summary << std::endl;
  hevc_free(summary);

  std::size_t nalIndex = 0;
  if(argc >= 3)
    nalIndex = (std::size_t)std::strtoul(argv[2], nullptr, 10);

  char *syntax = nullptr;
  if(cc == "avc")
    syntax = avc_get_nal_syntax(nalIndex);
  else if(cc == "vvc")
    syntax = vvc_get_nal_syntax(nalIndex);
  else
    syntax = hevc_get_nal_syntax(nalIndex);
  std::cerr << "\n===== NAL syntax (" << nalIndex << ") =====\n";
  std::cerr << syntax << std::endl;
  hevc_free(syntax);

  if(cc == "avc")
    avc_reset();
  else if(cc == "vvc")
    vvc_reset();
  else
    hevc_reset();

  return 0;
}
