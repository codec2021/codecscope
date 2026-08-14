# HEVCESBrowser Web

[HEVCESBrowser](https://github.com/virinext/hevcesbrowser) 的 Web 版本 —— 一个在浏览器中运行的 **HEVC (H.265) 码流分析工具**。

它复用原项目 `hevcparser` 的 C++ 解析核心，通过 [Emscripten](https://emscripten.org/) 编译为 WebAssembly，前端使用原生 HTML/CSS/JavaScript，可直接用 **GitHub Pages** 免费静态托管。

## 功能

- **NAL 单元列表**：显示每个 NAL 的偏移、长度、类型和可读信息（I/P/B Slice、VPS/SPS/PPS、SEI 等）
- **语法元素树**：点击任一 NAL，展示其完整语法元素树（完整复刻桌面版 GUI 的 SyntaxViewer）
- **码流统计**：NALUs / Slices 数量、I/P/B 分布、Profile、Level、Tier
- **HDR 信息**：色彩原色、传输特性、矩阵系数、CLL、Mastering Display 等
- **警告面板**：数值越界、引用结构缺失、Profile 一致性检查，支持按类型过滤

输入应为**裸 HEVC 基本流**（.h265 / .hevc）。若为 mp4/ts 等封装格式，请先用 ffmpeg 解封装：

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
│   ├── hevcparser/         # 原 HEVC 解析核心（纯 C++，无 Qt 依赖）
│   │   ├── include/
│   │   └── src/
│   ├── common/             # ConvToString（NAL 类型名转换）
│   ├── web/                # Web 适配层
│   │   ├── Json.h/.cpp             # 轻量 JSON 序列化
│   │   ├── WebSyntaxWriter.h/.cpp  # 语法树生成（复刻 GUI 的 SyntaxViewer）
│   │   ├── WebParser.h/.cpp        # NAL 列表 / 流信息 / HDR / 警告汇总
│   │   ├── ProfileConformanceAnalyzer.h/.cpp  # Profile 一致性检查（去 Qt）
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

用任意 C++ 编译器把解析核心编译成本地可执行文件，验证解析结果：

```bash
make native
./hevcparser_native input.h265          # 输出汇总 JSON
./hevcparser_native input.h265 3        # 同时输出第 3 个 NAL 的语法树
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
