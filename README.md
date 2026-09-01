# CodecScope

一个在浏览器中运行的 **H.264 / H.265 / H.266 / AV1 视频码流分析工具**，同时支持 **HEIC / AVIF / JPEG / PNG 图片分析**（源自 [HEVCESBrowser](https://github.com/virinext/hevcesbrowser) 的 Web 版）。

复用原项目 `hevcparser` 的 C++ 解析核心，并为 H.264 (AVC)、H.266 (VVC) 分别实现了新解析器，通过 [Emscripten](https://emscripten.org/) 编译为 WebAssembly，前端使用原生 HTML/CSS/JavaScript，可直接用 **GitHub Pages** 免费静态托管，**无需安装、无需后端**，浏览器打开即用。

> 在线体验：https://codec2021.github.io/codecscope/

## 支持的标准

| 标准 | 说明 | 语法解析 | 画面解码 |
|------|------|---------|---------|
| H.264 (AVC) | AVC/H.264 | `h264parser`（新实现） | WebCodecs |
| H.265 (HEVC) | HEVC/H.265 | `hevcparser`（原项目） | WebCodecs |
| H.266 (VVC) | VVC/H.266 | `vvcparser`（新实现） | vvdec (WASM) |
| AV1 | AOMedia Video 1 | OBU 解析（JS） | dav1d (WASM) |
| HEIC/HEIF | iPhone 照片 | `parseHeic`（JS） | libheif (WASM) |
| AVIF | AV1 图片 | `parseAvif`（JS） | dav1d (WASM) |
| JPEG/PNG/GIF/WebP/BMP | 通用图片 | JPEG 标记段解析（JS） | 浏览器原生 |

码流类型会**自动识别**，无需手动选择。

## 功能

- **NAL / OBU 单元列表**（虚拟滚动，可流畅处理数万单元的大文件）：偏移、长度、类型、所属帧号和可读信息
- **语法元素树**：点击任一 NAL/OBU，展示其完整语法元素树（SPS/PPS/VPS/Slice Header、AV1 sequence/frame header、JPEG 标记段等逐位解析）
- **Hex 视图**：查看选中单元的十六进制 + ASCII 内容
- **帧结构时间轴**：I/P/B slice 类型的可视化时间轴（GOP 结构），支持缩放、逐帧步进、帧号/POC 标记、播放进度高亮与自动滚动
- **码流统计**：单元数量、I/P/B 分布、Profile、Level、Tier、分辨率、真实帧率（FPS）
- **视频播放**：帧级预览与连续播放，支持**解码顺序 / 显示顺序**切换（正确处理 B 帧重排）、单帧步进、播放进度高亮
- **容器解封装**：直接拖入 MP4/MOV（AVC/HEVC/VVC/AV1）、WebM、IVF 文件自动解封装
- **MediaInfo 面板**：树状展示完整容器信息（General / Video / Audio / Other 轨道）——格式、时长、码率、编码日期、音频采样率/声道/位深、流大小等，类似 MediaInfo
- **HEIC/HEIF 图片**：解析 ispe 分辨率 + hvcC 配置 + iloc/iref 图像数据，支持 **libheif WASM 解码预览**，显示 IDR 帧与语法树
- **AVIF 图片**：解析 ispe + av1C + iloc，支持 **dav1d WASM 解码预览**，显示 OBU 语法树
- **JPEG 图片**：标记段列表 + 语法树（SOF/DQT/DHT/SOS + EXIF 元数据）
- **HDR 信息**：色彩原色、传输特性、矩阵系数、CLL、Mastering Display 等（HEVC）
- **警告面板**：数值越界、引用结构缺失、Profile 一致性检查，支持按类型过滤
- **三视图联动**：单元列表 / 时间轴 / 预览画面点击互相同步高亮跳转
- **可拖拽布局**：各面板大小可拖动调整
- **移动端适配**：窄屏自动切换为底部导航的堆叠布局

## 使用

- 拖拽或选择 **裸基本流**（`.h264 / .h265 / .h266`）、**MP4/MOV**、**WebM/IVF** 容器文件，或 **HEIC / AVIF / JPEG / PNG** 图片
- 点击单元列表任意行查看语法树与 Hex；点击时间轴帧查看预览画面
- 预览 Tab 中可播放，并可在「Decode Order / Display Order」间切换
- MediaInfo Tab 查看容器/码流的完整元信息
- 分隔条可拖动调整各面板大小

## 在线使用

直接访问 GitHub Pages：

```
https://codec2021.github.io/codecscope/
```

## 目录结构

```
├── src/                    # C++ 源码
│   ├── hevcparser/         # HEVC 解析核心（原项目，纯 C++）
│   ├── h264parser/         # H.264 解析器（新实现）
│   ├── vvcparser/          # H.266/VVC 解析器（新实现）
│   ├── common/             # BitstreamReader、ConvToString 等通用工具
│   ├── web/                # Web 适配层（JSON 序列化、语法树、汇总、码流检测）
│   │   ├── Json.h/.cpp             # 轻量 JSON 序列化
│   │   ├── WebSyntaxWriter.h/.cpp  # HEVC 语法树
│   │   ├── AvcSyntaxWriter.h/.cpp  # H.264 语法树
│   │   ├── VvcSyntaxWriter.h/.cpp  # VVC 语法树
│   │   ├── WebParser.h/.cpp        # HEVC 汇总
│   │   ├── AvcWebParser.h/.cpp     # H.264 汇总
│   │   ├── VvcWebParser.h/.cpp     # VVC 汇总
│   │   ├── ProfileConformanceAnalyzer.h/.cpp  # Profile 一致性检查
│   │   ├── CodecDetector.h/.cpp    # 码流类型自动识别
│   │   └── webapi.cpp              # C 接口（供 JS 调用）
│   └── native_main.cpp     # 本地命令行测试入口
├── www/                    # 前端
│   ├── index.html
│   ├── css/style.css
│   └── js/
│       ├── app.js          # 主逻辑（渲染 / 播放 / 布局）
│       ├── demux.js        # MP4/MOV/WebM/IVF 解封装 + HEIC/AVIF 解析
│       ├── jpeg.js         # JPEG 标记段 + EXIF 解析
│       ├── libheif.min.js  # HEIC 解码库（libheif WASM）
│       ├── vvdecapp.js/.wasm  # VVC 解码库（vvdec WASM）
│       └── dav1dapp.js/.wasm # AV1 解码库（dav1d WASM）
├── build.sh                # WASM 构建脚本
├── Makefile                # 本地原生编译（测试）/ wasm 构建
├── .github/workflows/deploy.yml  # 自动构建 + 部署 GitHub Pages
└── dist/                   # 构建产物（不提交）
```

## 本地构建

### 1. 原生命令行测试（无需 Emscripten）

```bash
make native
./hevcparser_native input.h264          # 输出汇总 JSON（自动检测码流类型）
./hevcparser_native input.h264 3        # 同时输出第 3 个 NAL 的语法树
```

### 2. 构建 WebAssembly

先安装 [emsdk](https://github.com/emscripten-core/emsdk)：

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk/emsdk_env.sh
```

然后构建并本地预览：

```bash
./build.sh
python3 -m http.server -d dist 8000
# 浏览器打开 http://localhost:8000
```

## 部署到 GitHub Pages

1. 在 GitHub 上新建仓库，将本项目推送上去
2. 进入仓库 **Settings → Pages → Build and deployment → Source**，选择 **GitHub Actions**
3. 之后每次 push 到 `main`/`master` 分支，Actions 会自动构建并部署

## 许可

本项目基于 [virinext/hevcesbrowser](https://github.com/virinext/hevcesbrowser)（fork 自 [codec2021/hevcesbrowser_codec](https://github.com/codec2021/hevcesbrowser_codec)），遵循 [GNU GPL v2](LICENSE) 许可。

## 作者

- **codec2021** — https://github.com/codec2021
