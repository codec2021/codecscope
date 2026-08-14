# H26xBrowser

一个在浏览器中运行的 **H.264 / H.265 / H.266 码流分析工具**（源自 [HEVCESBrowser](https://github.com/virinext/hevcesbrowser) 的 Web 版）。

复用原项目 `hevcparser` 的 C++ 解析核心，并为 H.264 (AVC) 与 H.266 (VVC) 分别实现了新解析器，通过 [Emscripten](https://emscripten.org/) 编译为 WebAssembly，前端使用原生 HTML/CSS/JavaScript，可直接用 **GitHub Pages** 免费静态托管。

## 支持的标准

| 标准 | 说明 | 解析器 |
|------|------|--------|
| H.264 (AVC) | AVC/H.264 | `h264parser`（新实现） |
| H.265 (HEVC) | HEVC/H.265 | `hevcparser`（原项目） |
| H.266 (VVC) | VVC/H.266 | `vvcparser`（新实现） |

码流类型会**自动识别**，无需手动选择。

## 功能

- **NAL 单元列表**（虚拟滚动，可流畅处理大文件）：偏移、长度、类型和可读信息
- **语法元素树**：点击任一 NAL，展示其完整语法元素树
- **Hex 视图**：查看选中 NAL 的十六进制 + ASCII 内容
- **帧结构时间轴**：I/P/B slice 类型的可视化时间轴（GOP 结构），可点击跳转
- **码流统计**：NALUs / Slices 数量、I/P/B 分布、Profile、Level、Tier、分辨率
- **HDR 信息**：色彩原色、传输特性、矩阵系数、CLL、Mastering Display 等（HEVC）
- **警告面板**：数值越界、引用结构缺失、Profile 一致性检查，支持按类型过滤

输入应为**裸基本流**（.h264 / .h265 / .h266）。若为 mp4/ts 等封装格式，请先用 ffmpeg 解封装：

```bash
ffmpeg -i input.mp4 -vcodec copy -an video.h265
```

## 在线使用

推送代码到 GitHub 后，GitHub Actions 会自动构建并部署到 GitHub Pages：

```
https://<你的用户名>.github.io/<仓库名>/
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
│   └── js/app.js
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
