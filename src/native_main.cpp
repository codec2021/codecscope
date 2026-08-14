#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>

extern "C"
{
  char *hevc_parse(const uint8_t *data, std::size_t size);
  char *hevc_get_nal_syntax(std::size_t index);
  void hevc_reset();
  void hevc_free(void *ptr);
}

int main(int argc, char **argv)
{
  if(argc < 2)
  {
    std::cerr << "Usage: " << argv[0] << " <input.h265> [nal_index]" << std::endl;
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

  char *summary = hevc_parse(data.data(), size);
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

  char *syntax = hevc_get_nal_syntax(nalIndex);
  std::cerr << "\n===== NAL syntax (" << nalIndex << ") =====\n";
  std::cerr << syntax << std::endl;
  hevc_free(syntax);

  hevc_reset();

  return 0;
}
