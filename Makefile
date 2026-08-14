CXX ?= clang++
CXXFLAGS ?= -std=c++11 -O2 -Wall
EMCC ?= em++

INC := -Isrc/hevcparser/include -Isrc/hevcparser/src -Isrc/common -Isrc/web

PARSER_SRC := $(wildcard src/hevcparser/src/*.cpp)
COMMON_SRC := $(wildcard src/common/*.cpp)
WEB_SRC := $(wildcard src/web/*.cpp)

NATIVE_SRC := $(PARSER_SRC) $(COMMON_SRC) $(WEB_SRC) src/native_main.cpp
NATIVE_OBJ := $(NATIVE_SRC:.cpp=.o)

.PHONY: all native wasm clean

all: native

native: hevcparser_native

hevcparser_native: $(NATIVE_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INC) -c -o $@ $<

wasm:
	mkdir -p dist
	$(EMCC) $(PARSER_SRC) $(COMMON_SRC) $(WEB_SRC) \
	  $(INC) \
	  -std=c++11 -O2 \
	  -s WASM=1 \
	  -s ALLOW_MEMORY_GROWTH=1 \
	  -s EXPORTED_FUNCTIONS='["_hevc_parse","_hevc_get_nal_syntax","_hevc_reset","_hevc_free","_malloc","_free"]' \
	  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","lengthBytesUTF8","HEAPU8"]' \
	  -s MODULARIZE=1 \
	  -s EXPORT_NAME=createHevcModule \
	  -o dist/hevc.js
	cp www/index.html www/css/style.css dist/ 2>/dev/null || true
	cp -r www/js dist/ 2>/dev/null || true

clean:
	rm -f $(NATIVE_OBJ) hevcparser_native
	rm -rf dist
