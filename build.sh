#!/bin/bash
# HEVCESBrowser Web 构建脚本
# 使用 Emscripten 将 C++ 解析器编译为 WebAssembly，并打包前端静态资源到 dist/
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if ! command -v emcc >/dev/null 2>&1; then
  echo "错误: 未找到 emcc (Emscripten)。请先安装 emsdk："
  echo ""
  echo "  git clone https://github.com/emscripten-core/emsdk.git"
  echo "  cd emsdk"
  echo "  ./emsdk install latest"
  echo "  ./emsdk activate latest"
  echo "  source ./emsdk/emsdk_env.sh"
  echo ""
  echo "安装并激活后重新运行本脚本。"
  exit 1
fi

echo "编译 WASM ..."
mkdir -p dist

em++ src/hevcparser/src/*.cpp src/h264parser/src/*.cpp src/vvcparser/src/*.cpp src/common/*.cpp src/web/*.cpp \
  -Isrc/hevcparser/include -Isrc/hevcparser/src -Isrc/h264parser/include -Isrc/h264parser/src -Isrc/vvcparser/include -Isrc/vvcparser/src -Isrc/common -Isrc/web \
  -std=c++11 -O2 \
  -s WASM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORTED_FUNCTIONS='["_hevc_parse","_hevc_get_nal_syntax","_hevc_reset","_avc_parse","_avc_get_nal_syntax","_avc_reset","_vvc_parse","_vvc_get_nal_syntax","_vvc_reset","_detect_codec","_hevc_free","_malloc","_free"]' \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","lengthBytesUTF8","HEAPU8"]' \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=createHevcModule \
  -o dist/hevc.js

echo "打包前端资源 ..."
cp -r www/index.html www/css www/js dist/

echo ""
echo "构建完成，产物位于 dist/ 目录。"
echo "本地预览： python3 -m http.server -d dist 8000"
echo "然后访问 http://localhost:8000"
