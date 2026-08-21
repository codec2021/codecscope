# CodecScope

一个在浏览器中运行的 **H.264 / H.265 / H.266 视频码流分析工具**（源自 [HEVCESBrowser](https://github.com/virinext/hevcesbrowser) 的 Web 版）。

复用原项目 `hevcparser` 的 C++ 解析核心，并为 H.264 (AVC) 与 H.266 (VVC) 分别实现了新解析器，通过 [Emscripten](https://emscripten.org/) 编译为 WebAssembly，前端使用原生 HTML/CSS/JavaScript，可直接用 **GitHub Pages** 免费静态托管，**无需安装、无需后端**，浏览器打开即用。

> 在线体验：https://codec2021.github.io/codecscope/

## 支持的标准

| 标准 | 说明 | 解析器 |
|------|------|--------|
| H.264 (AVC) | AVC/H.264 | `h264parser`（新实现） |
| H.265 (HEVC) | HEVC/H.265 | `hevcparser`（原项目） |
| H.266 (VVC) | VVC/H.266 | `vvcparser`（新实现） |

码流类型会**自动识别**，无需手动选择。

## 功能

- **NAL 单元列表**（虚拟滚动，可流畅处理数万 NAL 的大文件）：偏移、长度、类型、所属帧号和可读信息
- **语法元素树**：点击任一 NAL，展示其完整语法元素树（SPS/PPS/VPS/Slice Header 等逐位解析）
- **Hex 视图**：查看选中 NAL 的十六进制 + ASCII 内容
- **帧结构时间轴**：I/P/B slice 类型的可视化时间轴（GOP 结构），支持缩放、逐帧步进、帧号/POC 标记与帧间物理分隔
- **码流统计**：NALUs / Slices 数量、I/P/B 分布、Profile、Level、Tier、分辨率、真实帧率（FPS）
- **视频播放**：基于 WebCodecs 的帧级预览与播放，支持**解码顺序 / 显示顺序**切换（正确处理 B 帧重排），单帧步进
- **容器解封装**：直接拖入 MP4/MOV 文件自动解封装（hvcC/avcC 提取 description 供解码）
- **HDR 信息**：色彩原色、传输特性、矩阵系数、CLL、Mastering Display 等（HEVC）
- **警告面板**：数值越界、引用结构缺失、Profile 一致性检查，支持按类型过滤
- **三视图联动**：NAL 列表 / 时间轴 / 预览画面点击互相同步高亮跳转
- **可拖拽布局**：各面板大小可拖动调整
- **移动端适配**：窄屏自动切换为底部导航的堆叠布局

## 使用

- 拖拽或选择 **裸基本流**（`.h264 / .h265 / .h266`），或 **MP4/MOV** 容器文件
- 点击 NAL 列表任意行查看语法树与 Hex；点击时间轴帧查看预览画面
- 预览 Tab 中可播放（按真实帧率），并可在「解码顺序 / 显示顺序」间切换
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
│       └── demux.js        # MP4 解封装模块
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
