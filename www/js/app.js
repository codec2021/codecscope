(function () {
  "use strict";

  var Module = null;
  var currentData = null;      // 汇总 JSON
  var currentCodec = null;     // "hevc" / "avc" / "vvc"
  var currentWarnings = [];
  var fileBytes = null;        // 原始文件字节（用于 hex 视图）
  var currentDescription = null; // 解码器 description（hvcC/avcC 原始字节，MP4 容器时）
  var currentNalLengthSize = 4;  // description 存在时的 NAL 长度前缀字节数
  var currentContainerInfo = null; // MP4 容器级信息（General/Video/Audio/Other）
  var currentImageBlob = null;     // HEIC/HEIF 原始文件 Blob（用于原生解码预览）
  var currentHeicRawBytes = null;  // HEIC 原始字节（用于 libheif 解码）
  var selectedIndex = -1;

  var statusEl = document.getElementById("status");
  var fileInput = document.getElementById("fileInput");
  var openBtn = document.getElementById("openBtn");
  var resetBtn = document.getElementById("resetBtn");
  var dropzone = document.getElementById("dropzone");
  var fileNameEl = document.getElementById("fileName");
  var codecBadge = document.getElementById("codecBadge");
  var statsBar = document.getElementById("statsBar");
  var timelinePanel = document.getElementById("timelinePanel");
  var mainArea = document.getElementById("mainArea");
  var bottomPanels = document.getElementById("bottomPanels");

  var nalScroll = document.getElementById("nalScroll");
  var nalSpacer = document.getElementById("nalSpacer");
  var nalRows = document.getElementById("nalRows");
  var nalCount = document.getElementById("nalCount");
  var syntaxTree = document.getElementById("syntaxTree");
  var syntaxTitle = document.getElementById("syntaxTitle");
  var mediaInfoView = document.getElementById("mediaInfoView");
  var hexView = document.getElementById("hexView");
  var bitrateView = document.getElementById("bitrateView");
  var hdrInfo = document.getElementById("hdrInfo");
  var warningBody = document.getElementById("warningBody");
  var warningCount = document.getElementById("warningCount");
  var warningFilter = document.getElementById("warningFilter");
  var timeline = document.getElementById("timeline");
  var previewView = document.getElementById("previewView");
  var previewCanvas = document.getElementById("previewCanvas");
  var previewMsg = document.getElementById("previewMsg");
  var previewHint = document.getElementById("previewHint");
  var previewPlayBtn = document.getElementById("previewPlayBtn");
  var previewPrevBtn = document.getElementById("previewPrevBtn");
  var previewNextBtn = document.getElementById("previewNextBtn");

  var ROW_HEIGHT = 22;
  var timelineZoom = 1.15;  // 时间轴缩放倍数
  var setMobileView = null; // 移动端视图切换函数
  var selectedSlice = -1; // 时间轴上选中的帧
  var previewCanvasTouched = false; // 预览 canvas 是否已被帧画面覆盖

  // 时间轴布局常量
  var TL_LABEL_H = 24;        // 顶部帧号/POC 标记区高度(px)
  var TL_BAR_H = 36;          // 色块区高度(px)
  var TL_MIN_BAR_W = 0.8;     // 色块最小宽度(px)
  var TL_FRAME_GAP_RATIO = 0.8; // 帧间空隙 = barW * 该比例（至少 2px）
  var TL_INNER_GAP_RATIO = 0.12; // 帧内 slice 间隙 = barW * 该比例
  var TL_MARK_STEP = 44;      // 帧号/POC 标记至少间隔(px)
  var TL_ZOOM_STEP = 1.5;     // 缩放每步倍数
  var TL_ZOOM_MIN = 0.25;     // 最小缩放
  var TL_ZOOM_MAX = 64;       // 最大缩放
  // 播放常量
  var PLAY_DECODE_LEAD = 16;  // 播放时解码领先帧数
  var PLAY_FRAME_KEEP = 24;   // 保留最近多少已解码帧
  var PLAY_PREVIEW_TIMEOUT = 10000; // 单帧预览解码超时(ms)

  function setStatus(msg) { statusEl.textContent = msg; }
  function hex8(v) { var s = v.toString(16); while (s.length < 8) s = "0" + s; return "0x" + s; }

  // ---------- WASM 封装 ----------
  function parseBuffer(bytes) {
    var ptr = Module._malloc(bytes.length);
    Module.HEAPU8.set(bytes, ptr);

    var codecPtr = Module._detect_codec(ptr, bytes.length);
    var codec = Module.UTF8ToString(codecPtr);
    Module._hevc_free(codecPtr);

    var outPtr;
    if (codec === "avc") outPtr = Module._avc_parse(ptr, bytes.length);
    else if (codec === "vvc") outPtr = Module._vvc_parse(ptr, bytes.length);
    else outPtr = Module._hevc_parse(ptr, bytes.length);

    Module._free(ptr);
    var json = Module.UTF8ToString(outPtr);
    Module._hevc_free(outPtr);

    var data = JSON.parse(json);
    if (data.error) throw new Error(data.error);
    return { codec: codec, data: data };
  }

  function fetchNalSyntax(index) {
    var outPtr;
    if (currentCodec === "avc") outPtr = Module._avc_get_nal_syntax(index);
    else if (currentCodec === "vvc") outPtr = Module._vvc_get_nal_syntax(index);
    else outPtr = Module._hevc_get_nal_syntax(index);
    var json = Module.UTF8ToString(outPtr);
    Module._hevc_free(outPtr);
    return JSON.parse(json);
  }

  // ---------- 渲染 ----------
  function renderStats(si) {
    statsBar.innerHTML = "";
    function stat(label, value) {
      var d = document.createElement("div");
      d.className = "stat";
      d.innerHTML = '<span class="label">' + label + '</span><span class="value">' + value + "</span>";
      statsBar.appendChild(d);
    }
    stat("NALUs", si.nalus);
    stat("Slices", si.slices);
    stat("I", si.i + (si.iPct !== undefined ? " (" + si.iPct + "%)" : ""));
    stat("P", si.p + (si.pPct !== undefined ? " (" + si.pPct + "%)" : ""));
    stat("B", si.b + (si.bPct !== undefined ? " (" + si.bPct + "%)" : ""));
    var resW = si.picWidth || (currentData && currentData.hdr && currentData.hdr.picWidth);
    var resH = si.picHeight || (currentData && currentData.hdr && currentData.hdr.picHeight);
    if (resW) stat("Resolution", resW + " x " + resH);
    stat("Profile", si.profile);
    stat("Level", si.level);
    if (si.tier) stat("Tier", si.tier);
    if (si.fps && parseFloat(si.fps) > 0) stat("FPS", si.fps);
  }

  // ---------- MediaInfo 树状视图 ----------
  function chromaFormatName(idc) {
    return ["4:0:0", "4:2:0", "4:2:2", "4:4:4"][idc] || ("idc " + idc);
  }
  function codecFullName() {
    if (currentCodec === "avc") return "AVC (Advanced Video Coding) / H.264";
    if (currentCodec === "hevc") return "HEVC (High Efficiency Video Coding) / H.265";
    if (currentCodec === "vvc") return "VVC (Versatile Video Coding) / H.266";
    if (currentCodec === "av1") return "AV1 (AOMedia Video 1)";
    if (currentCodec === "vp9") return "VP9 (Google/On2)";
    if (currentCodec === "jpeg") return "JPEG (Joint Photographic Experts Group)";
    if (currentCodec === "image") return "Image (no bitstream syntax)";
    return "Unknown";
  }
  function codecShortName() {
    if (currentCodec === "avc") return "AVC";
    if (currentCodec === "hevc") return "HEVC";
    if (currentCodec === "vvc") return "VVC";
    if (currentCodec === "av1") return "AV1";
    if (currentCodec === "vp9") return "VP9";
    if (currentCodec === "jpeg") return "JPEG";
    if (currentCodec === "image") return "Image";
    return "?";
  }
  function sampleEntryName(cc) {
    if (!cc) return "";
    cc = cc.toLowerCase();
    if (cc === "hvc1" || cc === "hev1") return "HEVC (H.265)";
    if (cc === "avc1" || cc === "avc3") return "AVC (H.264)";
    if (cc === "vvc1" || cc === "vvi1") return "VVC (H.266)";
    if (cc === "dvh1" || cc === "dvhe") return "Dolby Vision HEVC";
    if (cc === "dvav" || cc === "dva1") return "Dolby Vision AVC";
    if (cc === "mp4v") return "MPEG-4 Visual";
    return "";
  }
  function fmtSize(bytes) {
    if (bytes == null) return "?";
    if (bytes >= 1048576) return (bytes / 1048576).toFixed(1) + " MiB";
    if (bytes >= 1024) return (bytes / 1024).toFixed(1) + " KiB";
    return bytes + " B";
  }
  function fmtDuration(seconds) {
    if (seconds == null) return "?";
    var ms = Math.round(seconds * 1000);
    var h = Math.floor(ms / 3600000);
    var m = Math.floor((ms % 3600000) / 60000);
    var s = Math.floor((ms % 60000) / 1000);
    var mm = ms % 1000;
    var out = "";
    if (h > 0) out += h + " 时 ";
    if (m > 0 || h > 0) out += m + " 分 ";
    out += s + " 秒 " + mm + " 毫秒";
    return out;
  }
  function fmtBitrate(bitsPerSec) {
    if (bitsPerSec == null) return "?";
    if (bitsPerSec >= 1000000) return (bitsPerSec / 1000000).toFixed(1) + " Mb/s";
    return (bitsPerSec / 1000).toFixed(0) + " kb/s";
  }
  function buildMediaInfoData() {
    var si = currentData.streamInfo;
    var hdr = currentData.hdr || {};
    var ci = currentContainerInfo;
    var srcFile = fileNameEl.textContent;
    var fname = srcFile;
    var m = /^(.*?) \(([\d.]+) MB\)$/.exec(srcFile);
    if (m) fname = m[1];

    // ---- General ----
    var generalChildren = [{ n: "Complete name: " + fname }];
    if (ci && ci.isContainer) {
      generalChildren.push({ n: "Format: " + ci.format });
      generalChildren.push({ n: "Format profile: " + ci.formatProfile });
      generalChildren.push({ n: "File size: " + fmtSize(ci.fileSize) });
      generalChildren.push({ n: "Duration: " + fmtDuration(ci.duration) });
      generalChildren.push({ n: "Overall bit rate: " + fmtBitrate(ci.overallBitrate) });
      if (ci.creationTime) generalChildren.push({ n: "Encoded date: " + ci.creationTime });
    } else {
      generalChildren.push({ n: "Format: " + codecFullName() });
      generalChildren.push({ n: "File size: " + fmtSize(currentData.totalSize) });
    }
    if (si.fps > 0) generalChildren.push({ n: "Frame rate: " + si.fps + " FPS" });
    var general = { n: "General", c: generalChildren };

    // ---- Video ----
    var vTrack = ci ? ci.tracks.find(function (t) { return t.handler === "vide"; }) : null;
    var videoChildren = [
      { n: "Format: " + codecShortName() },
      { n: "Format/Info: " + codecFullName() }
    ];
    videoChildren.push({ n: "Format profile: " + si.profile + "@L" + si.level + "@" + (si.tier || "Main") });
    if (vTrack && vTrack.codec) {
      var seName = sampleEntryName(vTrack.codec);
      videoChildren.push({ n: "Codec ID: " + vTrack.codec + (seName ? " (" + seName + ")" : "") });
    }
    if (vTrack && vTrack.codec) {
      var seCodec = null;
      var cc = (vTrack.codec || "").toLowerCase();
      if (cc === "hvc1" || cc === "hev1" || cc === "dvh1" || cc === "dvhe") seCodec = "hevc";
      else if (cc === "avc1" || cc === "avc3" || cc === "dvav" || cc === "dva1") seCodec = "avc";
      else if (cc === "vvc1" || cc === "vvi1") seCodec = "vvc";
      if (seCodec && seCodec !== currentCodec)
        videoChildren.push({ n: "Note: container declares " + sampleEntryName(vTrack.codec) + " but bitstream parsed as " + codecShortName() + " (possible transcoding on upload)" });
    }
    if (vTrack && vTrack.seconds) videoChildren.push({ n: "Duration: " + fmtDuration(vTrack.seconds) });
    if (vTrack && vTrack.bitrate) videoChildren.push({ n: "Bit rate: " + fmtBitrate(vTrack.bitrate) });
    var vw = (vTrack && vTrack.width) || hdr.picWidth;
    var vh = (vTrack && vTrack.height) || hdr.picHeight;
    if (vw) videoChildren.push({ n: "Width: " + vw + " pixels" });
    if (vh) videoChildren.push({ n: "Height: " + vh + " pixels" });
    if (vw && vh) videoChildren.push({ n: "Display aspect ratio: " + (vw / vh).toFixed(3) });
    if (si.fps > 0) videoChildren.push({ n: "Frame rate: " + si.fps + " FPS" });
    videoChildren.push({ n: "Color space: YUV" });
    if (si.chromaFormat !== undefined) videoChildren.push({ n: "Chroma subsampling: " + chromaFormatName(si.chromaFormat) });
    if (si.bitDepth !== undefined) videoChildren.push({ n: "Bit depth: " + si.bitDepth + " bits" });
    if (hdr.fullRange !== undefined) videoChildren.push({ n: "Color range: " + (hdr.fullRange ? "Full" : "Limited") });
    if (hdr.colourPrimaries) videoChildren.push({ n: "Color primaries: " + hdr.colourPrimaries });
    if (hdr.transferCharacteristics) videoChildren.push({ n: "Transfer characteristics: " + hdr.transferCharacteristics });
    if (hdr.matrixCoefficients) videoChildren.push({ n: "Matrix coefficients: " + hdr.matrixCoefficients });
    if (hdr.hasCll) {
      videoChildren.push({ n: "Maximum Content Light Level: " + hdr.maxCll + " cd/m2" });
      videoChildren.push({ n: "Maximum Frame-Average Light Level: " + hdr.avgCll + " cd/m2" });
    }
    if (vTrack && vTrack.streamSize) videoChildren.push({ n: "Stream size: " + fmtSize(vTrack.streamSize) });
    var video = { n: "Video", c: videoChildren };

    // ---- Audio ----
    var roots = [general, video];
    if (ci && ci.isContainer) {
      var aTracks = ci.tracks.filter(function (t) { return t.handler === "soun"; });
      aTracks.forEach(function (at) {
        var ac = [
          { n: "Format: PCM" },
          { n: "Format settings: Big / Signed" },
          { n: "Codec ID: " + (at.codec || "twos") }
        ];
        if (at.seconds) ac.push({ n: "Duration: " + fmtDuration(at.seconds) });
        if (at.bitrate) ac.push({ n: "Bit rate mode: Constant" });
        if (at.bitrate) ac.push({ n: "Bit rate: " + fmtBitrate(at.bitrate) });
        if (at.channels) ac.push({ n: "Channel(s): " + at.channels + " channels" });
        if (at.sampleRate) ac.push({ n: "Sampling rate: " + (at.sampleRate / 1000).toFixed(1) + " kHz" });
        if (at.bitDepth) ac.push({ n: "Bit depth: " + at.bitDepth + " bits" });
        if (at.streamSize) ac.push({ n: "Stream size: " + fmtSize(at.streamSize) });
        roots.push({ n: "Audio", c: ac });
      });
    }

    // ---- Other（meta 轨道，如 Sony rtmd）----
    if (ci && ci.isContainer) {
      var oTracks = ci.tracks.filter(function (t) { return t.handler !== "vide" && t.handler !== "soun"; });
      oTracks.forEach(function (ot) {
        var oc = [
          { n: "Type: meta" },
          { n: "Codec ID: " + (ot.codec || ot.handler) }
        ];
        if (ot.seconds) oc.push({ n: "Duration: " + fmtDuration(ot.seconds) });
        if (ot.streamSize) oc.push({ n: "Stream size: " + fmtSize(ot.streamSize) });
        roots.push({ n: "Other", c: oc });
      });
    }

    // ---- NAL 统计 ----
    var nalChildren = [
      { n: "NAL units: " + si.nalus },
      { n: "Slices: " + si.slices },
      { n: "I frames: " + si.i + (si.iPct !== undefined ? " (" + si.iPct + "%)" : "") },
      { n: "P frames: " + si.p + (si.pPct !== undefined ? " (" + si.pPct + "%)" : "") },
      { n: "B frames: " + si.b + (si.bPct !== undefined ? " (" + si.bPct + "%)" : "") }
    ];
    roots.push({ n: "NAL Statistics", c: nalChildren });

    var root = { n: "MediaInfo", c: roots };
    return root;
  }
  function renderMediaInfo() {
    mediaInfoView.innerHTML = "";
    mediaInfoView.appendChild(buildTree(buildMediaInfoData()));
  }

  // 虚拟滚动 NAL 列表
  function renderNalTable() {
    var nalus = currentData.nalus;
    nalCount.textContent = "(" + nalus.length + ")";
    nalSpacer.style.height = (nalus.length * ROW_HEIGHT) + "px";
    selectedIndex = -1;
    updateVisibleRows();
  }

  // 一次遍历完成：收集 slice、按帧分组、标注每个 NAL 的 frameIdx
  function computeFrames() {
    var nalus = currentData.nalus;
    var slices = [];
    var frames = [];
    var hasFirstSlice = false;
    for (var i = 0; i < nalus.length; i++) {
      if (nalus[i].sliceType >= 0) { hasFirstSlice = nalus[i].firstSlice !== undefined; break; }
    }
    for (var j = 0; j < nalus.length; j++) {
      var n = nalus[j];
      if (n.sliceType < 0) { n.frameIdx = -1; continue; }
      var poc = (n.slicePoc !== undefined && n.slicePoc !== null) ? n.slicePoc : frames.length;
      slices.push({ index: j, type: n.sliceType, poc: poc, firstSlice: n.firstSlice });
      var si = slices.length - 1;
      var prev = frames.length > 0 ? frames[frames.length - 1] : null;
      var sameFrame = false;
      if (hasFirstSlice) {
        sameFrame = prev !== null && n.firstSlice === 0;
      } else {
        sameFrame = prev !== null && prev.poc >= 0 && prev.poc === poc && prev.last === si - 1;
      }
      if (sameFrame) {
        prev.last = si;
        prev.slices.push(si);
      } else {
        frames.push({ first: si, last: si, slices: [si], poc: poc, frameNum: frames.length });
      }
      n.frameIdx = frames.length - 1;
    }
    return { slices: slices, frames: frames };
  }

  function makeRow(i) {
    var n = currentData.nalus[i];
    var row = document.createElement("div");
    row.className = "nal-row";
    row.dataset.index = i;

    var off = document.createElement("span");
    off.className = "c-offset";
    off.textContent = hex8(n.offset);

    var len = document.createElement("span");
    len.className = "c-length";
    len.textContent = n.length;

    var type = document.createElement("span");
    type.className = "c-type";
    type.textContent = n.typeName;

    var fr = document.createElement("span");
    fr.className = "c-frame";
    fr.textContent = (n.frameIdx !== undefined && n.frameIdx >= 0) ? String(n.frameIdx) : "";

    var info = document.createElement("span");
    info.className = "c-info";
    info.textContent = n.info;
    if (n.color) info.style.color = n.color;

    row.appendChild(off);
    row.appendChild(len);
    row.appendChild(type);
    row.appendChild(fr);
    row.appendChild(info);
    return row;
  }

  function updateVisibleRows() {
    var total = currentData.nalus.length;
    var scrollTop = nalScroll.scrollTop;
    var viewHeight = nalScroll.clientHeight;
    var start = Math.floor(scrollTop / ROW_HEIGHT);
    var visible = Math.ceil(viewHeight / ROW_HEIGHT) + 3;
    var end = Math.min(start + visible, total);

    nalRows.innerHTML = "";
    nalRows.style.transform = "translateY(" + (start * ROW_HEIGHT) + "px)";
    var frag = document.createDocumentFragment();
    for (var i = start; i < end; i++) {
      var row = makeRow(i);
      if (i === selectedIndex) row.classList.add("selected");
      frag.appendChild(row);
    }
    nalRows.appendChild(frag);
  }

  // 语法树
  function renderSyntax(node) {
    syntaxTree.innerHTML = "";
    syntaxTree.appendChild(buildTree(node));
  }

  function buildTree(node) {
    var container = document.createElement("div");
    container.className = "tree-node";

    var row = document.createElement("div");
    row.className = "tree-row";
    var hasChildren = node.c && node.c.length > 0;

    var toggle = document.createElement("span");
    toggle.className = "toggle";
    if (hasChildren) toggle.textContent = "\u25bc";
    row.appendChild(toggle);

    var label = document.createElement("span");
    label.className = "label" + (hasChildren ? " group" : "");
    label.textContent = node.n;
    row.appendChild(label);
    container.appendChild(row);

    if (hasChildren) {
      var wrap = document.createElement("div");
      wrap.className = "tree-children";
      node.c.forEach(function (child) { wrap.appendChild(buildTree(child)); });
      container.appendChild(wrap);

      var collapsed = false;
      toggle.addEventListener("click", function () {
        collapsed = !collapsed;
        wrap.style.display = collapsed ? "none" : "block";
        toggle.textContent = collapsed ? "\u25b6" : "\u25bc";
      });
    }
    return container;
  }

  // Hex 视图
  function renderHex(index) {
    var nal = currentData.nalus[index];
    if (!fileBytes || nal.length <= 0) { hexView.textContent = "No data"; return; }
    var end = Math.min(nal.offset + nal.length, fileBytes.length);
    var out = "";
    for (var i = nal.offset; i < end; i += 16) {
      var lineHex = "", lineAscii = "";
      for (var j = 0; j < 16; j++) {
        if (i + j < end) {
          var b = fileBytes[i + j];
          lineHex += (b < 16 ? "0" : "") + b.toString(16) + " ";
          lineAscii += (b >= 32 && b <= 126) ? String.fromCharCode(b) : ".";
        } else {
          lineHex += "   ";
        }
      }
      out += '<span class="hex-offset">' + hex8(i) + "</span>  " +
             '<span class="hex-byte">' + lineHex + "</span>" +
             '<span class="hex-ascii">|' + lineAscii + "|</span>\n";
    }
    hexView.innerHTML = out;
  }

  // 码率视图：每帧字节大小柱状图 + 统计
  function renderBitrate() {
    if (!currentData || !timeline._slices) { bitrateView.innerHTML = ""; return; }
    var frames = timeline._frames || [];
    if (frames.length === 0) { bitrateView.innerHTML = ""; return; }

    var sizes = new Array(frames.length);
    var total = 0, maxSize = 0, maxIdx = 0;
    for (var f = 0; f < frames.length; f++) {
      var fr = frames[f];
      var sz = 0;
      for (var k = 0; k < fr.slices.length; k++) {
        var nal = currentData.nalus[timeline._slices[fr.slices[k]].index];
        if (nal) sz += nal.length;
      }
      sizes[f] = sz;
      total += sz;
      if (sz > maxSize) { maxSize = sz; maxIdx = f; }
    }

    var fps = 0;
    if (currentData.streamInfo) fps = parseFloat(currentData.streamInfo.fps) || 0;
    var avgBytes = total / frames.length;
    var bitrate = fps > 0 ? total * 8 * fps : 0;

    var html = '<div class="bitrate-stats">';
    html += '<span>Frames: <b>' + frames.length + '</b></span>';
    html += '<span>Total: <b>' + fmtSize(total) + '</b></span>';
    html += '<span>Avg frame: <b>' + fmtSize(Math.round(avgBytes)) + '</b></span>';
    if (bitrate > 0) html += '<span>Avg bitrate: <b>' + fmtBitrate(bitrate) + '</b></span>';
    html += '<span>Peak: <b>frame #' + maxIdx + ' (' + fmtSize(maxSize) + ')</b></span>';
    html += '</div>';

    var canvas = document.createElement("canvas");
    canvas.className = "bitrate-chart";
    var w = Math.max(300, bitrateView.clientWidth - 24);
    var h = 180;
    var dpr = window.devicePixelRatio || 1;
    canvas.width = Math.max(1, Math.round(w * dpr));
    canvas.height = Math.max(1, Math.round(h * dpr));
    canvas.style.width = w + "px";
    canvas.style.height = h + "px";
    var ctx = canvas.getContext("2d");
    ctx.scale(dpr, dpr);

    var barW = w / frames.length;
    var chartH = h - 14;
    for (var i = 0; i < frames.length; i++) {
      var bh = maxSize > 0 ? Math.max(1, (sizes[i] / maxSize) * chartH) : 0;
      var x = i * barW;
      ctx.fillStyle = (i === maxIdx) ? "#e05555" : "#4d94e8";
      ctx.fillRect(x, h - bh - 8, Math.max(0.5, barW - 0.5), bh);
    }
    ctx.fillStyle = "#888";
    ctx.font = "9px monospace";
    ctx.fillText("0", 2, h - 1);
    ctx.fillText(fmtSize(maxSize), Math.max(0, w - 60), 10);

    bitrateView.innerHTML = html;
    bitrateView.appendChild(canvas);
  }

  // 帧时间轴（Slice 可视化，标记帧号 / POC）
  function renderTimeline() {
    var cf = computeFrames();
    var slices = cf.slices;
    if (slices.length === 0) return;
    var frames = cf.frames;
    timeline._frames = frames;

    // 预计算关键帧索引（用于 findKeyFrame 二分查找，避免 seek 时线性扫描）
    var keyFrames = [];
    for (var kfi = 0; kfi < frames.length; kfi++) {
      var ksl = frames[kfi].slices;
      for (var kk = 0; kk < ksl.length; kk++) {
        var knal = currentData.nalus[slices[ksl[kk]].index];
        if (knal && isKeyNal(knal.type)) { keyFrames.push(kfi); break; }
      }
    }
    timeline._keyFrames = keyFrames;

    var dpr = window.devicePixelRatio || 1;
    var labelH = TL_LABEL_H;
    var barH = TL_BAR_H;
    var wrapW = timeline.parentNode.clientWidth - 24;
    var fitBarW = wrapW / slices.length;
    var barW = Math.max(TL_MIN_BAR_W, fitBarW * timelineZoom);
    var frameGap = Math.max(2, Math.round(barW * TL_FRAME_GAP_RATIO));
    var innerGap = Math.max(0, Math.round(barW * TL_INNER_GAP_RATIO));

    // 计算每个 slice 的 x 坐标（帧之间插入物理空隙）
    var xs = new Array(slices.length);
    var cursor = 0;
    for (var fi = 0; fi < frames.length; fi++) {
      if (fi > 0) cursor += frameGap;
      var f = frames[fi];
      for (var si = f.first; si <= f.last; si++) {
        xs[si] = cursor;
        cursor += barW + innerGap;
      }
    }
    var w = cursor;
    var h = labelH + barH;
    timeline.style.width = w + "px";
    timeline.style.height = h + "px";
    timeline.width = w * dpr;
    timeline.height = h * dpr;
    var ctx = timeline.getContext("2d");
    var colors = { 2: "#E02020", 0: "#4d94e8", 1: "#00B050" };
    var barTop = labelH;
    var step = Math.max(1, Math.ceil(TL_MARK_STEP / barW));

    // 底图缓存：尺寸或 zoom 变化时重绘底图（色块+标记），否则复用离屏底图
    var baseDirty = !timeline._base || timeline._baseBarW !== barW || timeline._baseSliceCount !== slices.length;
    if (baseDirty) {
      var base = timeline._base;
      if (!base || base.width !== w * dpr || base.height !== h * dpr) {
        base = document.createElement("canvas");
        base.width = w * dpr;
        base.height = h * dpr;
        timeline._base = base;
      }
      var bctx = base.getContext("2d");
      bctx.setTransform(1, 0, 0, 1, 0, 0);
      bctx.scale(dpr, dpr);
      bctx.clearRect(0, 0, w, h);
      bctx.font = "9px monospace";
      bctx.textBaseline = "middle";
      for (var i = 0; i < slices.length; i++) {
        var s = slices[i];
        bctx.fillStyle = colors[s.type] || "#888";
        bctx.fillRect(xs[i], barTop, Math.max(1, barW - 0.6), barH);
      }
      var nextMark = 0;
      for (var m = 0; m < frames.length; m++) {
        var fm = frames[m];
        if (fm.first < nextMark) continue;
        bctx.fillStyle = "#bbb";
        bctx.fillText(String(fm.frameNum), xs[fm.first] + 1, 8);               // 帧号
        bctx.fillStyle = "#777";
        bctx.fillText(String(fm.poc), xs[fm.first] + 1, labelH - 6);           // POC
        nextMark = fm.first + step;
      }
      timeline._baseBarW = barW;
      timeline._baseSliceCount = slices.length;
    }

    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, w, h);
    ctx.drawImage(timeline._base, 0, 0, w * dpr, h * dpr, 0, 0, w, h);

    // 高亮选中帧（若为帧首 slice 则框住整帧范围）
    if (selectedSlice >= 0 && selectedSlice < slices.length) {
      var hx = xs[selectedSlice];
      ctx.strokeStyle = "#ffffff";
      ctx.lineWidth = 1.5;
      var hlW = Math.max(1, barW - 1);
      for (var hi = 0; hi < frames.length; hi++) {
        if (frames[hi].first === selectedSlice) {
          hlW = Math.max(1, (frames[hi].last - frames[hi].first + 1) * (barW + innerGap) - innerGap - 1);
          break;
        }
      }
      ctx.strokeRect(hx + 0.5, barTop + 0.5, hlW, barH - 1);
      hlFrame = { slice: selectedSlice, x: hx, w: hlW };
    } else {
      hlFrame.slice = -1;
    }
    timeline._slices = slices;
    timeline._xs = xs;
    timeline._barW = barW;
    timeline._frameGap = frameGap;
    timeline._innerGap = innerGap;
    timeline._labelH = labelH;
    var nalToSlice = {};
    for (var mi = 0; mi < slices.length; mi++) nalToSlice[slices[mi].index] = mi;
    timeline._nalToSlice = nalToSlice;

    var legend = document.getElementById("timelineLegend");
    legend.innerHTML =
      '<b style="color:#E02020">I</b> <b style="color:#4d94e8">P</b> <b style="color:#00B050">B</b>' +
      ' <span style="color:var(--text-dim)"> | Frame# / POC | Click a frame to preview | Zoom</span>' +
      ' <button class="zoom-btn" onclick="window.__tlZoom(1)">+</button>' +
      ' <button class="zoom-btn" onclick="window.__tlZoom(-1)">−</button>' +
      ' <button class="zoom-btn" title="Prev frame" onclick="window.__tlStep(-1)">|◀</button>' +
      ' <button class="zoom-btn" title="Next frame" onclick="window.__tlStep(1)">▶|</button>';
  }

  var zoomRaf = 0;
  window.__tlZoom = function (d) {
    if (d > 0) timelineZoom = timelineZoom * TL_ZOOM_STEP;
    else timelineZoom = timelineZoom / TL_ZOOM_STEP;
    timelineZoom = Math.max(TL_ZOOM_MIN, Math.min(TL_ZOOM_MAX, timelineZoom));
    if (zoomRaf) cancelAnimationFrame(zoomRaf);
    zoomRaf = requestAnimationFrame(function () {
      zoomRaf = 0;
      renderTimeline();
    });
  };

  window.__tlStep = function (delta) {
    stepFrame(delta);
  };

  function timelineTip() {
    if (!timeline._tip) {
      timeline._tip = document.createElement("div");
      timeline._tip.className = "timeline-tip";
      document.body.appendChild(timeline._tip);
    }
    return timeline._tip;
  }
  function sliceAtX(x) {
    var xs = timeline._xs, barW = timeline._barW;
    if (!xs || xs.length === 0) return -1;
    // 二分查找最后一个 xs[i] <= x 的 i（落在色块内）
    var lo = 0, hi = xs.length - 1, ans = -1;
    while (lo <= hi) {
      var mid = (lo + hi) >> 1;
      if (xs[mid] <= x) { ans = mid; lo = mid + 1; } else { hi = mid - 1; }
    }
    if (ans >= 0 && ans < xs.length) {
      // 若 x 落在帧间空隙或超过色块右边界，则不属于任何 slice
      if (x > xs[ans] + barW) return -1;
      return ans;
    }
    return -1;
  }
  function timelineMove(e) {
    if (!timeline._slices) return;
    var rect = timeline.getBoundingClientRect();
    var x = e.clientX - rect.left;
    var idx = sliceAtX(x);
    if (idx < 0 || idx >= timeline._slices.length) { timelineTip().style.display = "none"; return; }
    var s = timeline._slices[idx];
    var t = { 2: "I", 0: "P", 1: "B" }[s.type] || "?";
    var fi = frameIndexOfSlice(idx);
    var frameLabel = fi >= 0 ? fi : "?";
    timelineTip().textContent = "Frame " + frameLabel + "  [" + t + "]\nPOC " + s.poc;
    timelineTip().style.display = "block";
    timelineTip().style.left = (e.clientX + 12) + "px";
    timelineTip().style.top = (e.clientY + 12) + "px";
  }

timeline.addEventListener("click", function (e) {
     if (!timeline._slices || !timeline._frames) return;
     var rect = timeline.getBoundingClientRect();
     var x = e.clientX - rect.left;
     var idx = sliceAtX(x);
     if (idx >= 0 && idx < timeline._slices.length) {
       var fi = frameIndexOfSlice(idx);
       var si = fi >= 0 ? timeline._frames[fi].first : idx;
       syncSelectionFromNal(timeline._slices[si].index, true);
     }
   });
  timeline.addEventListener("mousemove", timelineMove);
  timeline.addEventListener("mouseleave", function () { timelineTip().style.display = "none"; });

  // ---------- 画面预览（WebCodecs 解码 H.264/H.265） ----------
  var play = {
    active: false,
    timer: null,
    decoder: null,
    params: [],
    feedFrame: -1,
    frames: {},
    curFrame: -1,
    displayOrder: false,  // true=显示顺序(POC), false=解码顺序
    order: null,          // 显示顺序的帧索引(fi)列表
    orderPos: -1          // 显示游标（order 数组下标）
  };

  function frameIndexOfSlice(sliceIdx) {
    var frames = timeline._frames;
    if (!frames) return -1;
    for (var i = 0; i < frames.length; i++) {
      if (sliceIdx >= frames[i].first && sliceIdx <= frames[i].last) return i;
    }
    return -1;
  }

  function findKeyFrame(frameIndex) {
    var frames = timeline._frames;
    if (!frames) return -1;
    var kf = timeline._keyFrames;
    if (kf && kf.length > 0) {
      // 二分查找 <= frameIndex 的最大关键帧
      var lo = 0, hi = kf.length - 1, ans = -1;
      while (lo <= hi) {
        var mid = (lo + hi) >> 1;
        if (kf[mid] <= frameIndex) { ans = kf[mid]; lo = mid + 1; }
        else hi = mid - 1;
      }
      return ans;
    }
    for (var f = frameIndex; f >= 0; f--) {
      var sl = frames[f].slices;
      for (var k = 0; k < sl.length; k++) {
        var nal = currentData.nalus[timeline._slices[sl[k]].index];
        if (nal && isKeyNal(nal.type)) return f;
      }
    }
    return -1;
  }

  function frameIsKey(fi) {
    var frames = timeline._frames;
    var sl = frames[fi].slices;
    for (var k = 0; k < sl.length; k++) {
      var nal = currentData.nalus[timeline._slices[sl[k]].index];
      if (nal && isKeyNal(nal.type)) return true;
    }
    return false;
  }

  function buildPlayOrder() {
    // 构建显示顺序（按 POC）的帧索引列表；跨 GOP 用 GOP 编号保持全局唯一
    var frames = timeline._frames;
    if (!frames || frames.length === 0) { play.order = []; return; }
    var minPoc = Infinity;
    for (var i = 0; i < frames.length; i++) if (frames[i].poc < minPoc) minPoc = frames[i].poc;
    var entries = [];
    var gop = 0;
    for (var f = 0; f < frames.length; f++) {
      if (f > 0 && frameIsKey(f)) gop++;
      // 显示键 = GOP 编号 * 大基数 + (POC - minPoc)，保证 GOP 间顺序、GOP 内按 POC
      entries.push({ fi: f, key: gop * 100000 + (frames[f].poc - minPoc) });
    }
    entries.sort(function (a, b) { return a.key - b.key; });
    play.order = entries.map(function (e) { return e.fi; });
  }

  function makeAuData(nalIdxs) {
    if (currentCodec === "vp9") {
      // VP9：chunk 数据是 VP9 frame 本身（无 start code，WebCodecs 规范）
      var vtotal = 0;
      for (var vk = 0; vk < nalIdxs.length; vk++) vtotal += currentData.nalus[nalIdxs[vk]].length;
      var vdata = new Uint8Array(vtotal);
      var vo = 0;
      for (var vk2 = 0; vk2 < nalIdxs.length; vk2++) {
        var vnal = currentData.nalus[nalIdxs[vk2]];
        if (vnal.offset < 0 || vnal.offset + vnal.length > fileBytes.length) { vo += vnal.length; continue; }
        vdata.set(fileBytes.subarray(vnal.offset, vnal.offset + vnal.length), vo);
        vo += vnal.length;
      }
      return vdata;
    }
    if (currentDescription) {
      // hevc/avc 格式：length-prefixed NAL（description 存在时解码器要求此格式）
      var total = 0;
      var bodies = [];
      for (var k = 0; k < nalIdxs.length; k++) {
        var nal = currentData.nalus[nalIdxs[k]];
        var off = nal.offset, len = nal.length;
        if (off < 0 || off + len > fileBytes.length) continue;
        var startLen = 3;
        if (fileBytes[off] === 0 && fileBytes[off + 1] === 0 && fileBytes[off + 2] === 0 && fileBytes[off + 3] === 1) startLen = 4;
        else if (fileBytes[off] === 0 && fileBytes[off + 1] === 0 && fileBytes[off + 2] === 1) startLen = 3;
        else startLen = 0;
        var body = fileBytes.subarray(off + startLen, off + len);
        bodies.push(body);
        total += currentNalLengthSize + body.length;
      }
      var data = new Uint8Array(total);
      var o = 0;
      for (var k2 = 0; k2 < bodies.length; k2++) {
        var blen = bodies[k2].length;
        for (var s = currentNalLengthSize - 1; s >= 0; s--) data[o++] = (blen >> (s * 8)) & 0xFF;
        data.set(bodies[k2], o);
        o += blen;
      }
      return data;
    }
    var len = 0;
    for (var k = 0; k < nalIdxs.length; k++) len += currentData.nalus[nalIdxs[k]].length;
    var data = new Uint8Array(len);
    var o = 0;
    for (var k2 = 0; k2 < nalIdxs.length; k2++) {
      var nal = currentData.nalus[nalIdxs[k2]];
      if (nal.offset < 0 || nal.offset + nal.length > fileBytes.length) { o += nal.length; continue; }
      data.set(fileBytes.subarray(nal.offset, nal.offset + nal.length), o);
      o += nal.length;
    }
    return data;
  }

  function feedFrames(toFrame) {
    var frames = timeline._frames;
    if (!frames || frames.length === 0) return;
    var end = Math.min(toFrame, frames.length - 1);
    while (play.feedFrame < end && play.decoder && play.decoder.state === "configured") {
      var fi = play.feedFrame + 1;
      var nalIdxs = [];
      var isKey = false;
      for (var k = 0; k < frames[fi].slices.length; k++) {
        var nalIdx = timeline._slices[frames[fi].slices[k]].index;
        nalIdxs.push(nalIdx);
        if (isKeyNal(currentData.nalus[nalIdx].type)) isKey = true;
      }
      try {
        play.decoder.decode(new EncodedVideoChunk({
          type: isKey ? "key" : "delta",
          timestamp: fi,
          data: makeAuData(isKey && !currentDescription ? play.params.concat(nalIdxs) : nalIdxs)
        }));
        play.feedFrame = fi;
      } catch (err) {
        previewMsg.textContent = "Decode error: " + err.message;
        stopPlayback();
        return;
      }
    }
  }

  var cachedCodecSupport = null; // 缓存 isConfigSupported 结果，避免每次 seek 都异步等待

  function initPlayDecoder(done, needReset) {
    var cs = codecString();
    if (!cs) { previewMsg.textContent = "Unable to determine codec string"; return; }
    var conf = { codec: cs, optimizeForLatency: true };
    if (currentDescription) conf.description = currentDescription;

    function createDecoder() {
      if (needReset || !play.decoder) {
        // 需要重新从关键帧解码：清空已解码帧
        if (needReset) {
          for (var k in play.frames) { try { play.frames[k].close(); } catch (e) {} }
          play.frames = {};
          play.feedFrame = -1;
        }
        play.params = [];
        var types = currentCodec === "avc" ? [7, 8] : [32, 33, 34];
        for (var i = 0; i < currentData.nalus.length; i++)
          if (types.indexOf(currentData.nalus[i].type) >= 0) play.params.push(i);
      }

      if (needReset && play.decoder) {
        // 往前 seek：关闭旧 decoder（避免 reset 后参考帧/参数集状态不一致）
        try { play.decoder.close(); } catch (e) {}
        play.decoder = null;
      }
      if (!play.decoder) {
        var dec = new VideoDecoder({
          output: function (frame) { play.frames[frame.timestamp] = frame; },
          error: function (err) {
            if (play.decoder !== dec) return; // 僵尸 decoder 忽略
            previewMsg.textContent = "Decode error: " + err.message;
            stopPlayback();
          }
        });
        play.decoder = dec;
        dec.configure(conf);
      }
      done();
    }

    if (cachedCodecSupport && cachedCodecSupport.cs === cs) {
      if (!cachedCodecSupport.supported) {
        previewMsg.textContent = "Browser does not support decoding " + cs;
        return;
      }
      createDecoder();
      return;
    }

    var probeCfg = { codec: cs };
    if (currentDescription) probeCfg.description = currentDescription;
    VideoDecoder.isConfigSupported(probeCfg).then(function (support) {
      cachedCodecSupport = { cs: cs, supported: support.supported };
      if (!currentData || !currentCodec) return; // 文件已切换
      if (!support.supported) {
        previewMsg.textContent = "Browser does not support decoding " + cs + (currentCodec === "hevc" ? " (H.265 may be restricted by hardware/licensing)" : "");
        return;
      }
      createDecoder();
    }).catch(function (err) {
      previewMsg.textContent = "Config error: " + err.message;
    });
  }

  var hlFrame = { slice: -1, x: 0, w: 0 };
  var playProgress = { start: -1, end: -1 };

  function redrawTimelineBase() {
    if (!timeline._base || !timeline._slices) return;
    var ctx = timeline.getContext("2d");
    var dpr = window.devicePixelRatio || 1;
    var w = timeline.width / dpr, h = timeline.height / dpr;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, w, h);
    ctx.drawImage(timeline._base, 0, 0, w * dpr, h * dpr, 0, 0, w, h);
  }

  function drawProgressRange(from, to) {
    var xs = timeline._xs;
    if (!xs) return;
    var ctx = timeline.getContext("2d");
    var dpr = window.devicePixelRatio || 1;
    var barW = timeline._barW;
    var barTop = timeline._labelH || TL_LABEL_H;
    var barH = TL_BAR_H;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.fillStyle = "rgba(255,255,255,0.35)";
    for (var i = Math.max(0, from); i <= Math.min(to, xs.length - 1); i++) {
      ctx.fillRect(xs[i], barTop, Math.max(1, barW - 0.6), barH);
    }
  }

  function resetPlayProgress(startSlice) {
    playProgress.start = startSlice;
    playProgress.end = startSlice;
    redrawTimelineBase();
    if (startSlice >= 0) drawProgressRange(startSlice, startSlice);
  }

  function updatePlayProgress(sliceIndex) {
    if (playProgress.start < 0) { resetPlayProgress(sliceIndex); return; }
    if (sliceIndex <= playProgress.end) return;
    drawProgressRange(playProgress.end + 1, sliceIndex);
    playProgress.end = sliceIndex;
  }

  // 增量高亮：只重绘高亮框，不重算 frames、不重绘底图（播放时性能关键）
  function highlightFrame(sliceIndex) {
    var xs = timeline._xs, slices = timeline._slices, base = timeline._base;
    if (!xs || !slices) return;
    if (sliceIndex === hlFrame.slice) return;
    var ctx = timeline.getContext("2d");
    var dpr = window.devicePixelRatio || 1;
    var barW = timeline._barW;
    var barTop = timeline._labelH || TL_LABEL_H;
    var barH = TL_BAR_H;

    // 清除旧高亮：用底图重绘旧区域
    if (hlFrame.slice >= 0 && hlFrame.slice < slices.length && base) {
      var ox = hlFrame.x, ow = hlFrame.w + 3;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      ctx.drawImage(base, ox * dpr, barTop * dpr, ow * dpr, barH * dpr, ox, barTop, ow, barH);
    }

    if (sliceIndex >= 0 && sliceIndex < slices.length) {
      var hx = xs[sliceIndex];
      var innerGap = timeline._innerGap || 0;
      var hlW = Math.max(1, barW - 1);
      for (var hi = 0; hi < timeline._frames.length; hi++) {
        if (timeline._frames[hi].first === sliceIndex) {
          hlW = Math.max(1, (timeline._frames[hi].last - timeline._frames[hi].first + 1) * (barW + innerGap) - innerGap - 1);
          break;
        }
      }
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      ctx.strokeStyle = "#ffffff";
      ctx.lineWidth = 1.5;
      ctx.strokeRect(hx + 0.5, barTop + 0.5, hlW, barH - 1);
      hlFrame = { slice: sliceIndex, x: hx, w: hlW };
      // 自动滚动，让当前帧保持可见
      var wrap = timeline.parentNode;
      if (wrap && wrap.scrollWidth > wrap.clientWidth) {
        var target = hx - wrap.clientWidth / 2;
        if (target < 0) target = 0;
        wrap.scrollLeft = target;
      }
    } else {
      hlFrame.slice = -1;
    }
    selectedSlice = sliceIndex;
  }

  function showPlayFrame(frameIndex) {
    var frame = play.frames[frameIndex];
    var f = timeline._frames[frameIndex];
    if (frame) {
      drawVideoFrame(frame);
      previewHint.textContent = "Frame " + (f ? f.frameNum : frameIndex) + " / POC " + (f ? f.poc : frameIndex);
    }
    if (f) {
      updatePlayProgress(f.first);
      highlightFrame(f.first);
    }
    // 基于解码游标清理：只回收很早解码且不会再显示的帧
    var threshold = play.feedFrame - PLAY_FRAME_KEEP;
    for (var k in play.frames) {
      if (parseInt(k, 10) < threshold) { try { play.frames[k].close(); } catch (e) {} delete play.frames[k]; }
    }
  }

  function stopPlayback() {
    if (play.timer) { clearInterval(play.timer); play.timer = null; }
    play.active = false;
    if (vvdecPlay.timer) { clearInterval(vvdecPlay.timer); vvdecPlay.timer = null; }
    vvdecPlay.active = false;
    if (vvdecPlay.decoder) { try { vvdecPlay.decoder.delete(); } catch (e) {} vvdecPlay.decoder = null; }
    if (av1Play.timer) { clearInterval(av1Play.timer); av1Play.timer = null; }
    av1Play.active = false;
    if (av1Play.decoder) { try { av1Play.decoder.delete(); } catch (e) {} av1Play.decoder = null; }
    previewPlayBtn.textContent = "▶ Play";
  }

  function startPlayback() {
    if (play.active || vvdecPlay.active || av1Play.active) { stopPlayback(); return; }
    if (currentData && currentData.isImage) { return; }
    if (currentCodec === "vvc") { startVvcPlayback(); return; }
    if (currentCodec === "av1") { startAv1Playback(); return; }
    var frames = timeline._frames;
    if (!frames || frames.length === 0) return;
    var fi = 0;
    if (selectedSlice >= 0 && selectedSlice < timeline._slices.length) {
      var ii = frameIndexOfSlice(selectedSlice);
      if (ii >= 0) fi = ii;
    }
    if (findKeyFrame(fi) < 0) { previewMsg.textContent = "Key frame not found"; return; }
    buildPlayOrder();
    initPlayDecoder(function () {
      play.active = true;
      play.feedFrame = -1;
      if (play.displayOrder) {
        // 显示顺序模式：从选中帧在 order 里的位置开始，按 POC 顺序显示
        play.orderPos = 0;
        for (var p = 0; p < play.order.length; p++) { if (play.order[p] === fi) { play.orderPos = p; break; } }
        play.curFrame = fi;
      } else {
        play.curFrame = fi;
      }
      previewPlayBtn.textContent = "⏸ Pause";
      previewMsg.textContent = "";
      resetPlayProgress(frames[fi].first);
      feedFrames(fi);      var iv = 40;
      if (currentData && currentData.streamInfo) {
        var fpsN = parseFloat(currentData.streamInfo.fps);
        if (fpsN > 0) iv = Math.max(1, Math.round(1000 / fpsN));
      }
      play.timer = setInterval(function () {
        if (!play.active) return;
        if (play.displayOrder) {
          if (play.orderPos >= play.order.length) { stopPlayback(); return; }
          var target = play.order[play.orderPos];
          if (play.frames[target]) { showPlayFrame(target); play.orderPos++; }
          // 解码始终按解码顺序领先：喂到 target 之后若干帧
          feedFrames(Math.min(target + PLAY_DECODE_LEAD, frames.length - 1));
        } else {
          if (play.curFrame >= frames.length - 1) { stopPlayback(); return; }
          showPlayFrame(play.curFrame);
          play.curFrame++;
          feedFrames(Math.min(play.curFrame + 10, frames.length - 1));
        }
      }, iv);
    }, true);
  }


  function nalData(nal) {
    var off = nal.offset, len = nal.length;
    var startLen = 3;
    if (fileBytes[off] === 0 && fileBytes[off + 1] === 0 && fileBytes[off + 2] === 0 && fileBytes[off + 3] === 1) startLen = 4;
    else if (fileBytes[off] === 0 && fileBytes[off + 1] === 0 && fileBytes[off + 2] === 1) startLen = 3;
    else startLen = 0;
    return fileBytes.subarray(off + startLen, off + len);
  }

  function isVclNal(type) {
    if (currentCodec === "avc") return type === 1 || type === 5;
    if (currentCodec === "hevc") return type <= 31;
    if (currentCodec === "vvc") return type >= 0 && type <= 12;
    if (currentCodec === "vp9") return type === 0 || type === 1;
    return false;
  }
  function isKeyNal(type) {
    if (currentCodec === "avc") return type === 5;
    if (currentCodec === "hevc") return type === 16 || type === 17 || type === 18 || type === 19 || type === 20 || type === 21;
    if (currentCodec === "vvc") return type === 7 || type === 8 || type === 9 || type === 10;
    if (currentCodec === "vp9") return type === 0;
    return false;
  }

  function findNalByType(type) {
    for (var i = 0; i < currentData.nalus.length; i++)
      if (currentData.nalus[i].type === type) return i;
    return -1;
  }

  function codecString() {
    function stripEP(ebsp) {
      var out = [];
      var z = 0;
      for (var i = 0; i < ebsp.length; i++) {
        var b = ebsp[i];
        if (z >= 2 && b === 3) { z = 0; continue; }
        out.push(b);
        z = (b === 0) ? z + 1 : 0;
      }
      return new Uint8Array(out);
    }
    function rev32(v) { var r = 0; for (var i = 0; i < 32; i++) { r = (r << 1) | (v & 1); v >>>= 1; } return r >>> 0; }
    if (currentCodec === "vp9") {
      var pp = ("0" + (currentVp9Profile || 0)).slice(-2);
      var dd = ("0" + (currentVp9BitDepth || 8)).slice(-2);
      return "vp09." + pp + ".10." + dd;
    }
    if (currentCodec === "avc") {
      var si = findNalByType(7);
      if (si < 0) return null;
      var sps = stripEP(nalData(currentData.nalus[si]));
      if (sps.length < 4) return null;
      function hx(v) { var s = v.toString(16); return (s.length < 2 ? "0" : "") + s; }
      return (currentDescription ? "avc1" : "avc3") + "." + hx(sps[1]) + hx(sps[2]) + hx(sps[3]);
    }
    if (currentCodec === "hevc") {
      // 有 hvcC description 时，直接用它的一般 profile 字段（不受 max_sub_layers 影响）
      if (currentDescription && currentDescription.length >= 13) {
        var dp = currentDescription[1];
        var dSpace = (dp >> 6) & 3;
        var dTier = (dp >> 5) & 1;
        var dProfileIdc = dp & 0x1F;
        var dCompat = (currentDescription[2] << 24) | (currentDescription[3] << 16) | (currentDescription[4] << 8) | currentDescription[5];
        var dLevel = currentDescription[12];
        var dSpaceChar = ["", "A", "B", "C"][dSpace];
        var dCompatHex = rev32(dCompat).toString(16).toUpperCase();
        var dTierChar = dTier ? "H" : "L";
        return "hvc1." + dSpaceChar + dProfileIdc + "." + dCompatHex + "." + dTierChar + dLevel + ".B0";
      }
      var si = findNalByType(33);
      if (si < 0) return null;
      var sps = stripEP(nalData(currentData.nalus[si]));
      if (sps.length < 15) return null;
      var b1 = sps[3];
      var space = (b1 >> 6) & 3;
      var tier = (b1 >> 5) & 1;
      var profileIdc = b1 & 0x1F;
      var compat = (sps[4] << 24) | (sps[5] << 16) | (sps[6] << 8) | sps[7];
      var level = sps[14];
      var spaceChar = ["", "A", "B", "C"][space];
      var compatHex = rev32(compat).toString(16).toUpperCase();
      var constraintHex = "";
      var cstarted = false;
      for (var cb = 47; cb >= 0; cb--) {
        var byteIdx = 8 + (cb >> 3);
        var bitIdx = cb & 7;
        var bit = (sps[byteIdx] >> bitIdx) & 1;
        if (bit || cstarted) { cstarted = true; constraintHex += bit; }
      }
      if (constraintHex === "") constraintHex = "0";
      constraintHex = parseInt(constraintHex, 2).toString(16).toUpperCase();
      var tierChar = tier ? "H" : "L";
      return "hev1." + spaceChar + profileIdc + "." + compatHex + "." + tierChar + level + "." + constraintHex;
    }
    return null;
  }

  function drawVideoFrame(frame) {
    var c = previewCanvas;
    var w = frame.displayWidth || frame.codedWidth || frame.width || 0;
    var h = frame.displayHeight || frame.codedHeight || frame.height || 0;
    if (w <= 0 || h <= 0) return;
    var maxW = previewView.clientWidth - 32;
    var maxH = 480;
    if (maxW < 160) maxW = 160;
    var scale = Math.min(1, maxW / w, maxH / h);
    c.width = Math.max(1, Math.round(w * scale));
    c.height = Math.max(1, Math.round(h * scale));
    previewCanvasTouched = true;
    var ctx = c.getContext("2d");
    ctx.drawImage(frame, 0, 0, c.width, c.height);
  }

  function presetPreviewCanvas() {
    var hdr = currentData && currentData.hdr;
    var w = hdr && hdr.picWidth, h = hdr && hdr.picHeight;
    if (!w || !h) return;
    var maxW = previewView.clientWidth - 32;
    var maxH = 480;
    if (maxW < 160) maxW = 160;
    var scale = Math.min(1, maxW / w, maxH / h);
    previewCanvas.width = Math.max(1, Math.round(w * scale));
    previewCanvas.height = Math.max(1, Math.round(h * scale));
  }

  // ---------- HEIC libheif 解码 ----------
  var heicDecoding = false;

  var libheifModule = null;

  // ---------- VVC vvdec 解码 ----------
  var vvdecModule = null;
  var vvdecLoading = false;
  var vvdecPending = [];

  function loadVvdec(cb) {
    if (vvdecModule) { cb(); return; }
    vvdecPending.push(cb);
    if (vvdecLoading) return;
    vvdecLoading = true;
    previewMsg.textContent = "Loading VVC decoder (vvdec)...";
    var s = document.createElement("script");
    s.src = "js/vvdecapp.js";
    s.onload = function () {
      if (typeof CreateVVdeC !== "function") {
        vvdecLoading = false;
        vvdecPending = [];
        previewMsg.textContent = "VVC decoder module not found";
        return;
      }
      CreateVVdeC({ locateFile: function (f) { return "js/" + f; } }).then(function (m) {
        vvdecModule = m;
        vvdecLoading = false;
        var cbs = vvdecPending;
        vvdecPending = [];
        cbs.forEach(function (f) { f(); });
      }).catch(function (e) {
        vvdecLoading = false;
        vvdecPending = [];
        previewMsg.textContent = "VVC decoder init failed: " + (e && e.message ? e.message : e);
      });
    };
    s.onerror = function () {
      vvdecLoading = false;
      vvdecPending = [];
      previewMsg.textContent = "Failed to load VVC decoder";
    };
    document.head.appendChild(s);
  }

  function makeVvcAu(nalIdx, cts) {
    var M = vvdecModule;
    var nal = currentData.nalus[nalIdx];
    var off = nal.offset, len = nal.length;
    if (off < 0 || off + len > fileBytes.length) return null;
    var body = fileBytes.subarray(off, off + len);
    var au = new M.AccessUnit();
    au.alloc_payload(body.length);
    au.payload.set(body);
    au.payloadUsedSize = body.length;
    au.cts = cts; au.ctsValid = true; au.dts = cts; au.dtsValid = true;
    return au;
  }

  var vvdecPlay = {
    active: false,
    timer: null,
    decoder: null,
    nalIdx: 0,
    cts: 0,
    curSlice: -1
  };

  function startVvcPlayback() {
    if (vvdecPlay.active) { stopPlayback(); return; }
    if (vvdecLoading) return;
    loadVvdec(function () {
      if (vvdecPlay.active) return;
      if (!currentData || !timeline._frames) return;
      var M = vvdecModule;
      var params = new M.Params();
      params.threads = 0;
      vvdecPlay.decoder = new M.Decoder(params);
      try { params.delete(); } catch (e) {}
      vvdecPlay.nalIdx = 0;
      vvdecPlay.cts = 0;
      vvdecPlay.active = true;
      previewPlayBtn.textContent = "⏸ Pause";
      previewMsg.textContent = "";
      resetPlayProgress(timeline._frames && timeline._frames.length ? timeline._frames[0].first : -1);

      var iv = 33;
      if (currentData && currentData.streamInfo) {
        var fpsN = parseFloat(currentData.streamInfo.fps);
        if (fpsN > 0) iv = Math.max(1, Math.round(1000 / fpsN));
      }

      vvdecPlay.timer = setInterval(function () {
        if (!vvdecPlay.active || !vvdecPlay.decoder) return;
        // 喂一个 AU（从当前 NAL 到下一个 AUD）
        var total = currentData.nalus.length;
        if (vvdecPlay.nalIdx >= total) { stopPlayback(); return; }
        var auStart = vvdecPlay.nalIdx;
        var auEnd = total;
        for (var i = auStart; i < total; i++) {
          if (i > auStart && currentData.nalus[i].type === 20) { auEnd = i; break; }
        }
        for (var q = auStart; q < auEnd; q++) {
          var t = currentData.nalus[q].type;
          if (t === 20) vvdecPlay.cts++;
          if (t >= 0 && t <= 12) {
            var si = timeline._nalToSlice ? timeline._nalToSlice[q] : -1;
            if (si !== undefined && si >= 0) vvdecPlay.curSlice = si;
          }
          var au = makeVvcAu(q, vvdecPlay.cts);
          if (!au) continue;
          var h = new M.FrameHandle();
          vvdecPlay.decoder.decode(au, h);
          if (h.frame) {
            drawVvcFrame(M, h.frame, vvdecPlay.curSlice);
            vvdecPlay.decoder.frame_unref(h.frame);
          }
          try { au.delete(); } catch (e) {}
          try { h.delete(); } catch (e) {}
        }
        vvdecPlay.nalIdx = auEnd;
      }, iv);
    });
  }

  function decodeVvcFrame(fi) {
    loadVvdec(function () {
      var M = vvdecModule;
      var frames = timeline._frames;
      if (!frames || frames.length === 0) return;
      var keyFi = findKeyFrame(fi);
      if (keyFi < 0) { previewMsg.textContent = "Key frame not found"; return; }

      var targetLastSlice = frames[fi].last;
      var targetLastNal = timeline._slices[targetLastSlice].index;

      var params = new M.Params();
      params.threads = 0;
      var decoder = new M.Decoder(params);
      try { params.delete(); } catch (e) {}

      var cts = 0;

      function feedNal(nalIdx) {
        var nal = currentData.nalus[nalIdx];
        var off = nal.offset, len = nal.length;
        if (off < 0 || off + len > fileBytes.length) return null;
        var body = fileBytes.subarray(off, off + len);
        var au = new M.AccessUnit();
        au.alloc_payload(body.length);
        au.payload.set(body);
        au.payloadUsedSize = body.length;
        au.cts = cts; au.ctsValid = true; au.dts = cts; au.dtsValid = true;
        return au;
      }

      try {
        // 关键帧第一个 slice 的 NAL index
        var firstSliceNal = timeline._slices[frames[keyFi].first].index;
        // 往前找关键帧 AU 的起点（AUD 或参数集）
        var startNal = firstSliceNal;
        for (var p = firstSliceNal - 1; p >= 0; p--) {
          var pt = currentData.nalus[p].type;
          if (pt === 20) { startNal = p; break; }        // AUD
          if (pt >= 0 && pt <= 12) break;                  // 上一个 slice，停止
          if (pt >= 14 && pt <= 17) startNal = p;          // 参数集
        }

        var lastFramePtr = 0;
        // 从 startNal 喂到目标帧最后一个 slice
        for (var q = startNal; q <= targetLastNal; q++) {
          var t2 = currentData.nalus[q].type;
          if (t2 === 20) cts++; // AUD 边界递增 cts
          var au2 = feedNal(q);
          if (!au2) continue;
          var h = new M.FrameHandle();
          decoder.decode(au2, h);
          if (h.frame) {
            if (lastFramePtr) decoder.frame_unref(lastFramePtr);
            lastFramePtr = h.frame;
          }
          try { au2.delete(); } catch (e) {}
          try { h.delete(); } catch (e) {}
        }

        // flush 输出剩余帧，取最后一个
        var fh = new M.FrameHandle();
        decoder.flush(fh);
        if (fh.frame) {
          if (lastFramePtr) decoder.frame_unref(lastFramePtr);
          lastFramePtr = fh.frame;
        }
        try { fh.delete(); } catch (e) {}

        if (lastFramePtr) {
          drawVvcFrame(M, lastFramePtr);
          decoder.frame_unref(lastFramePtr);
        } else {
          previewMsg.textContent = "VVC decode: no output";
        }
      } catch (e) {
        previewMsg.textContent = "VVC decode error: " + e.message;
      } finally {
        try { decoder.delete(); } catch (e2) {}
      }
    });
  }

  function drawVvcFrame(M, framePtr, sliceIndex) {
    var img = M.get_RGBA_image_JS(framePtr);
    var c = previewCanvas;
    var tmp = document.createElement("canvas");
    tmp.width = img.width; tmp.height = img.height;
    tmp.getContext("2d").putImageData(img, 0, 0);
    var maxW = previewView.clientWidth - 32, maxH = 480;
    if (maxW < 160) maxW = 160;
    var scale = Math.min(1, maxW / img.width, maxH / img.height);
    c.width = Math.max(1, Math.round(img.width * scale));
    c.height = Math.max(1, Math.round(img.height * scale));
    c.getContext("2d").drawImage(tmp, 0, 0, c.width, c.height);
    previewHint.textContent = "Frame " + img.width + " x " + img.height + " (VVC)";
    previewMsg.textContent = "";
    previewCanvasTouched = true;
    if (sliceIndex >= 0) {
      updatePlayProgress(sliceIndex);
      highlightFrame(sliceIndex);
    }
  }

  // ---------- AV1 dav1d 解码 ----------
  var dav1dModule = null;
  var dav1dLoading = false;
  var dav1dPending = [];
  var currentAv1Frames = null;
  var currentVp9Frames = null;
  var currentVp9Profile = 0;
  var currentVp9BitDepth = 8;

  function loadDav1d(cb) {
    if (dav1dModule) { cb(); return; }
    dav1dPending.push(cb);
    if (dav1dLoading) return;
    dav1dLoading = true;
    previewMsg.textContent = "Loading AV1 decoder (dav1d)...";
    var s = document.createElement("script");
    s.src = "js/dav1dapp.js";
    s.onload = function () {
      if (typeof CreateDav1dModule !== "function") {
        dav1dLoading = false; dav1dPending = [];
        previewMsg.textContent = "AV1 decoder module not found";
        return;
      }
      CreateDav1dModule({ locateFile: function (f) { return "js/" + f; } }).then(function (m) {
        dav1dModule = m; dav1dLoading = false;
        var cbs = dav1dPending; dav1dPending = [];
        cbs.forEach(function (f) { f(); });
      }).catch(function (e) {
        dav1dLoading = false; dav1dPending = [];
        previewMsg.textContent = "AV1 decoder init failed: " + (e && e.message ? e.message : e);
      });
    };
    s.onerror = function () {
      dav1dLoading = false; dav1dPending = [];
      previewMsg.textContent = "Failed to load AV1 decoder";
    };
    document.head.appendChild(s);
  }

  var av1Play = {
    active: false,
    timer: null,
    decoder: null,
    frameIdx: 0
  };

  function drawAv1Frame(img, frameIdx) {
    var c = previewCanvas;
    var tmp = document.createElement("canvas");
    tmp.width = img.width; tmp.height = img.height;
    tmp.getContext("2d").putImageData(img, 0, 0);
    var maxW = previewView.clientWidth - 32, maxH = 480;
    if (maxW < 160) maxW = 160;
    var scale = Math.min(1, maxW / img.width, maxH / img.height);
    c.width = Math.max(1, Math.round(img.width * scale));
    c.height = Math.max(1, Math.round(img.height * scale));
    c.getContext("2d").drawImage(tmp, 0, 0, c.width, c.height);
    previewHint.textContent = "Frame " + (frameIdx !== undefined ? frameIdx : "?") + " (" + img.width + " x " + img.height + ", AV1)";
    previewMsg.textContent = "";
    previewCanvasTouched = true;
    if (frameIdx !== undefined && timeline._slices && timeline._frames) {
      var f = timeline._frames[frameIdx];
      if (f) { updatePlayProgress(f.first); highlightFrame(f.first); }
    }
  }

  function startAv1Playback() {
    if (av1Play.active) { stopPlayback(); return; }
    if (dav1dLoading) return;
    if (!currentAv1Frames || currentAv1Frames.length === 0) return;
    loadDav1d(function () {
      if (av1Play.active) return;
      if (!currentData || !currentAv1Frames || !timeline._frames) return;
      var M = dav1dModule;
      av1Play.decoder = new M.Decoder(0);
      av1Play.frameIdx = 0;
      av1Play.active = true;
      previewPlayBtn.textContent = "⏸ Pause";
      previewMsg.textContent = "";
      resetPlayProgress(timeline._frames && timeline._frames.length ? timeline._frames[0].first : -1);
      var iv = 33;
      if (currentData && currentData.streamInfo) {
        var fpsN = parseFloat(currentData.streamInfo.fps);
        if (fpsN > 0) iv = Math.max(1, Math.round(1000 / fpsN));
      }
      av1Play.timer = setInterval(function () {
        if (!av1Play.active || !av1Play.decoder) return;
        var total = currentAv1Frames.length;
        if (av1Play.frameIdx >= total) { stopPlayback(); return; }
        av1Play.decoder.send(currentAv1Frames[av1Play.frameIdx].data);
        var img;
        while ((img = av1Play.decoder.get_rgba()) !== null) {
          drawAv1Frame(img, av1Play.frameIdx);
        }
        av1Play.frameIdx++;
      }, iv);
    });
  }

  function decodeAv1Frame(fi) {
    loadDav1d(function () {
      var M = dav1dModule;
      if (!currentAv1Frames || currentAv1Frames.length === 0) return;
      var decoder = new M.Decoder(0);
      var lastImg = null, lastIdx = 0;
      try {
        for (var k = 0; k <= fi && k < currentAv1Frames.length; k++) {
          decoder.send(currentAv1Frames[k].data);
          var img;
          while ((img = decoder.get_rgba()) !== null) { lastImg = img; lastIdx = k; }
        }
        if (lastImg) drawAv1Frame(lastImg, lastIdx);
        else previewMsg.textContent = "AV1 decode: no output";
      } catch (e) {
        previewMsg.textContent = "AV1 decode error: " + e.message;
      } finally {
        try { decoder.delete(); } catch (e2) {}
      }
    });
  }

  function loadAndDecodeHeic() {
    if (libheifModule) { decodeWithLibheif(); return; }
    if (typeof libheif === "function") { initLibheif(); return; }
    previewMsg.textContent = "Loading HEIC decoder...";
    var s = document.createElement("script");
    s.src = "js/libheif.min.js";
    s.onload = function () { initLibheif(); };
    s.onerror = function () { previewMsg.textContent = "Failed to load HEIC decoder library"; heicDecoding = false; };
    document.head.appendChild(s);
  }

  function initLibheif() {
    try {
      libheifModule = libheif();
      if (!libheifModule || !libheifModule.HeifDecoder) {
        previewMsg.textContent = "HEIC decoder init failed: no HeifDecoder";
        heicDecoding = false;
        return;
      }
      decodeWithLibheif();
    } catch (e) {
      console.error("libheif init error:", e);
      previewMsg.textContent = "HEIC decoder init failed: " + e.message;
      heicDecoding = false;
    }
  }

  function decodeWithLibheif() {
    try {
      var decoder = new libheifModule.HeifDecoder();
      var results = decoder.decode(currentHeicRawBytes);
      if (!results || results.length === 0) { previewMsg.textContent = "HEIC decode failed"; heicDecoding = false; return; }
      var image = null;
      for (var i = 0; i < results.length; i++) {
        var img = results[i];
        try {
          if (typeof img.is_grid === "function" && img.is_grid()) continue;
          if (typeof img.get_width === "function" && typeof img.get_height === "function" && img.get_width() > 0 && img.get_height() > 0) {
            image = img;
            break;
          }
        } catch (e) { continue; }
      }
      if (!image) image = results[0];
      var w = image.get_width(), h = image.get_height();
      var tmpCanvas = document.createElement("canvas");
      tmpCanvas.width = w; tmpCanvas.height = h;
      var tmpCtx = tmpCanvas.getContext("2d");
      var imgData = tmpCtx.createImageData(w, h);
      image.display(imgData, function (imgData) {
        tmpCtx.putImageData(imgData, 0, 0);
        var canvas = previewCanvas;
        var maxW = previewView.clientWidth - 32, maxH = 480;
        if (maxW < 160) maxW = 160;
        var scale = Math.min(1, maxW / w, maxH / h);
        canvas.width = Math.max(1, Math.round(w * scale));
        canvas.height = Math.max(1, Math.round(h * scale));
        canvas.getContext("2d").drawImage(tmpCanvas, 0, 0, canvas.width, canvas.height);
        previewHint.textContent = "Image " + w + " x " + h;
        previewMsg.textContent = "";
        previewCanvasTouched = true;
        heicDecoding = false;
      });
    } catch (e) {
      console.error("libheif error:", e);
      previewMsg.textContent = "HEIC decode error: " + e.message;
      heicDecoding = false;
    }
  }

  function previewFrame(sliceIndex) {
    if (!fileBytes || !currentData) return;
    if (currentData.isImage) {
      if (play.active || vvdecPlay.active) stopPlayback();
      if (play.decoder) { try { play.decoder.close(); } catch (e) {} play.decoder = null; }
      for (var fk in play.frames) { try { play.frames[fk].close(); } catch (e) {} }
      play.frames = {};
      if (heicDecoding) return;
      heicDecoding = true;
      previewHint.textContent = "Decoding image...";
      previewMsg.textContent = "";
      if (currentAv1Frames && currentAv1Frames.length) {
        heicDecoding = false;
        decodeAv1Frame(0);
      } else if (currentHeicRawBytes) {
        if (typeof libheif !== "undefined") {
          decodeWithLibheif();
        } else if (currentImageBlob) {
          createImageBitmap(currentImageBlob).then(function (bmp) {
            drawVideoFrame(bmp);
            previewHint.textContent = "Image " + bmp.width + " x " + bmp.height;
            previewMsg.textContent = "";
            heicDecoding = false;
          }).catch(function () { loadAndDecodeHeic(); });
        } else {
          loadAndDecodeHeic();
        }
      } else if (currentImageBlob) {
        createImageBitmap(currentImageBlob).then(function (bmp) {
          drawVideoFrame(bmp);
          previewHint.textContent = "Image " + bmp.width + " x " + bmp.height;
          previewMsg.textContent = "";
          heicDecoding = false;
        }).catch(function () {
          previewMsg.textContent = "Failed to decode image";
          heicDecoding = false;
        });
      } else {
        previewMsg.textContent = "No image data available";
        heicDecoding = false;
      }
      return;
    }
    heicDecoding = false;
    if (!timeline._slices) return;
    if (currentCodec === "av1") {
      var aframes0 = timeline._frames;
      if (!aframes0 || aframes0.length === 0) return;
      var afi0 = frameIndexOfSlice(sliceIndex);
      if (afi0 < 0) afi0 = 0;
      if (play.active || vvdecPlay.active || av1Play.active) stopPlayback();
      previewMsg.textContent = "";
      previewHint.textContent = "Decoding AV1...";
      decodeAv1Frame(afi0);
      return;
    }
    if (currentCodec === "vvc") {
      var frames0 = timeline._frames;
      if (!frames0 || frames0.length === 0) return;
      var fi0 = frameIndexOfSlice(sliceIndex);
      if (fi0 < 0) fi0 = 0;
      if (play.active || vvdecPlay.active) stopPlayback();
      previewMsg.textContent = "";
      previewHint.textContent = "Decoding VVC...";
      decodeVvcFrame(fi0);
      return;
    }
    if (!("VideoDecoder" in window)) {
      previewMsg.textContent = "Browser does not support WebCodecs";
      return;
    }
    if (play.active || vvdecPlay.active) stopPlayback();
    var frames = timeline._frames;
    if (!frames || frames.length === 0) return;
    var fi = frameIndexOfSlice(sliceIndex);
    if (fi < 0) fi = 0;

    // 已解码的帧直接显示（顺序 seek 关键优化）
    if (play.frames[fi]) {
      showPlayFrame(fi);
      return;
    }

    var keyFi = findKeyFrame(fi);
    if (keyFi < 0) { previewMsg.textContent = "Key frame not found"; return; }

    // 往前 seek（目标帧在已解码位置之前）需要 reset + 从关键帧重新解码
    var needReset = (play.feedFrame > fi);

    var pf = frames[fi];
    previewHint.textContent = "Decoding... (POC " + (pf ? pf.poc : "?") + ")";
    previewMsg.textContent = "";

    initPlayDecoder(function () {
      if (needReset) play.feedFrame = keyFi - 1;
      feedFrames(fi);
      var waited = 0;
      var check = setInterval(function () {
        var frame = play.frames[fi];
        if (frame) {
          clearInterval(check);
          showPlayFrame(fi);
          return;
        }
        waited += 10;
        if (waited > PLAY_PREVIEW_TIMEOUT) {
          clearInterval(check);
          if (!previewMsg.textContent) previewMsg.textContent = "Decode timeout";
        }
      }, 10);
    }, needReset);
  }

  // HDR
  function renderHdr(h) {
    hdrInfo.innerHTML = "";
    function row(k, v) {
      if (v === undefined || v === null || v === "") return;
      var d = document.createElement("div");
      d.className = "kv-row";
      d.innerHTML = '<span class="k">' + k + '</span><span class="v">' + v + "</span>";
      hdrInfo.appendChild(d);
    }
    row("Colour primaries", h.colourPrimaries);
    row("Transfer characteristics", h.transferCharacteristics);
    row("Matrix coefficients", h.matrixCoefficients);
    if (h.chromaLocTop !== undefined && h.chromaLocTop !== 0) {
      row("Chroma loc (top)", h.chromaLocTop);
      row("Chroma loc (bottom)", h.chromaLocBottom);
    }
    row("Full range", h.fullRange);
    if (h.hasCll) { row("Max CLL", h.maxCll); row("Avg CLL", h.avgCll); }
    if (h.hasMdi) row("Mastering display", h.masteringDisplay);
  }

  function renderWarnings() {
    var filter = parseInt(warningFilter.value, 10);
    var filtered = currentWarnings.filter(function (w) { return filter === -1 || w.type === filter; });
    warningCount.textContent = "(" + filtered.length + ")";
    warningBody.innerHTML = "";
    var frag = document.createDocumentFragment();
    filtered.forEach(function (w) {
      var tr = document.createElement("tr");
      var td0 = document.createElement("td");
      td0.className = "mono";
      td0.textContent = hex8(w.position) + " (" + w.position + ")";
      var td1 = document.createElement("td");
      td1.textContent = w.message;
      tr.appendChild(td0); tr.appendChild(td1);
      frag.appendChild(tr);
    });
    warningBody.appendChild(frag);
  }

  function selectNal(index, scrollTo) {
    if (!currentData) return;
    selectedIndex = index;
    var nal = currentData.nalus[index];
    syntaxTitle.textContent = "#" + index + "  " + nal.typeName + "  @ " + hex8(nal.offset);

    var rows = nalRows.children;
    for (var i = 0; i < rows.length; i++) {
      rows[i].classList.toggle("selected", parseInt(rows[i].dataset.index, 10) === index);
    }

    renderSyntax(nal.jpegSyntax || fetchNalSyntax(index));
    renderHex(index);

    if (scrollTo) {
      var targetTop = index * ROW_HEIGHT;
      nalScroll.scrollTop = targetTop - nalScroll.clientHeight / 2;
      updateVisibleRows();
    }
  }

  function syncSelectionFromNal(nalIndex, scrollTo) {
    if (!currentData || !timeline._slices) { selectNal(nalIndex, scrollTo); return; }
    var si = timeline._nalToSlice ? timeline._nalToSlice[nalIndex] : -1;
    if (si === undefined) si = -1;
    if (si < 0) { selectNal(nalIndex, scrollTo); return; }
    if (selectedSlice !== si) {
      selectedSlice = si;
      renderTimeline();
      if (!previewView.classList.contains("hidden")) previewFrame(si);
    }
    selectNal(nalIndex, scrollTo);
  }

  nalRows.addEventListener("click", function (e) {
    var row = e.target.closest(".nal-row");
    if (row) syncSelectionFromNal(parseInt(row.dataset.index, 10), false);
  });

  nalScroll.addEventListener("scroll", updateVisibleRows);

  warningFilter.addEventListener("change", renderWarnings);

  // tab 切换
  var tabSyntax = document.getElementById("tabSyntax");
  var tabPreview = document.getElementById("tabPreview");
  var tabHex = document.getElementById("tabHex");
  var tabBitrate = document.getElementById("tabBitrate");
  var tabMediaInfo = document.getElementById("tabMediaInfo");

  function showTab(which) {
    tabSyntax.classList.toggle("active", which === "syntax");
    tabPreview.classList.toggle("active", which === "preview");
    tabHex.classList.toggle("active", which === "hex");
    tabBitrate.classList.toggle("active", which === "bitrate");
    tabMediaInfo.classList.toggle("active", which === "mediainfo");
    syntaxTree.classList.toggle("hidden", which !== "syntax");
    previewView.classList.toggle("hidden", which !== "preview");
    hexView.classList.toggle("hidden", which !== "hex");
    bitrateView.classList.toggle("hidden", which !== "bitrate");
    mediaInfoView.classList.toggle("hidden", which !== "mediainfo");
    if (which === "preview" && currentData && !previewCanvasTouched) presetPreviewCanvas();
    if (which === "bitrate" && currentData) renderBitrate();
    if (which === "mediainfo" && currentData && !mediaInfoView.dataset.rendered) {
      renderMediaInfo();
      mediaInfoView.dataset.rendered = "1";
    }
  }

  tabSyntax.addEventListener("click", function () { showTab("syntax"); });
  tabPreview.addEventListener("click", function () { showTab("preview"); });
  tabHex.addEventListener("click", function () { showTab("hex"); });
  tabBitrate.addEventListener("click", function () { showTab("bitrate"); });
  tabMediaInfo.addEventListener("click", function () { showTab("mediainfo"); });

  previewPlayBtn.addEventListener("click", function () { startPlayback(); });
  previewPrevBtn.addEventListener("click", function () { stepFrame(-1); });
  previewNextBtn.addEventListener("click", function () { stepFrame(1); });
  var previewOrderBtn = document.getElementById("previewOrderBtn");
  previewOrderBtn.addEventListener("click", function () {
    if (play.active || vvdecPlay.active) stopPlayback();
    play.displayOrder = !play.displayOrder;
    previewOrderBtn.textContent = play.displayOrder ? "Display Order" : "Decode Order";
    previewOrderBtn.title = play.displayOrder ? "Play in display order (POC)" : "Play in decode order";
  });

  function stepFrame(delta) {
    if (play.active || vvdecPlay.active) stopPlayback();
    if (currentData && currentData.isImage) return;
    var frames = timeline._frames;
    if (!frames || frames.length === 0) return;
    var fi = 0;
    if (selectedSlice >= 0 && selectedSlice < timeline._slices.length) {
      var ii = frameIndexOfSlice(selectedSlice);
      if (ii >= 0) fi = ii;
    }
    fi += delta;
    fi = Math.max(0, Math.min(fi, frames.length - 1));
    var f = frames[fi];
    selectedSlice = f.first;
    selectNal(timeline._slices[f.first].index, true);
    renderTimeline();
    if (!previewView.classList.contains("hidden")) previewFrame(f.first);
  }

  // ---------- 文件处理 ----------
  function clearAll() {
    stopPlayback();
    if (play.decoder) { try { play.decoder.close(); } catch (e) {} play.decoder = null; }
    for (var k in play.frames) { try { play.frames[k].close(); } catch (e) {} }
    play.frames = {};
    play.feedFrame = -1;
    play.params = [];
    play.curFrame = -1;
    play.order = null;
    play.orderPos = -1;
    play.displayOrder = false;
    var previewOrderBtn = document.getElementById("previewOrderBtn");
    if (previewOrderBtn) { previewOrderBtn.textContent = "Decode Order"; }
    if (vvdecPlay.decoder) { try { vvdecPlay.decoder.delete(); } catch (e) {} vvdecPlay.decoder = null; }
    vvdecPlay.nalIdx = 0;
    vvdecPlay.cts = 0;
    if (av1Play.decoder) { try { av1Play.decoder.delete(); } catch (e) {} av1Play.decoder = null; }
    av1Play.frameIdx = 0;
    currentAv1Frames = null;
    currentVp9Frames = null;
    currentVp9Profile = 0;
    currentVp9BitDepth = 8;
    heicDecoding = false;

    currentData = null;
    currentCodec = null;
    currentWarnings = [];
    fileBytes = null;
    currentDescription = null;
    currentNalLengthSize = 4;
    currentContainerInfo = null;
    currentImageBlob = null;
    currentHeicRawBytes = null;
    selectedIndex = -1;
    selectedSlice = -1;
    hlFrame = { slice: -1, x: 0, w: 0 };
    playProgress = { start: -1, end: -1 };

    timeline._frames = null;
    timeline._slices = null;
    timeline._nalToSlice = null;
    timeline._keyFrames = null;
    timeline._xs = null;
    timeline._base = null;
    timeline._baseBarW = null;
    timeline._baseSliceCount = null;
    timeline.width = 0;
    timeline.height = 0;

    previewCanvas.width = 0;
    previewCanvas.height = 0;
    previewCanvasTouched = false;
    previewHint.textContent = "";
    previewMsg.textContent = "Click a frame on the timeline to preview";

    syntaxTree.innerHTML = "";
    syntaxTitle.textContent = "";
    mediaInfoView.innerHTML = "";
    mediaInfoView.dataset.rendered = "";
    bitrateView.innerHTML = "";
    hdrInfo.innerHTML = "";
    warningBody.innerHTML = "";
    warningCount.textContent = "";
    nalRows.innerHTML = "";
    nalSpacer.style.height = "0px";
    nalCount.textContent = "";
    codecBadge.textContent = "";
    codecBadge.classList.add("hidden");
    statsBar.innerHTML = "";
  }

  function handleFile(file) {
    if (!file) return;
    clearAll();
    setStatus("Parsing " + file.name + " ...");
    fileNameEl.textContent = file.name + " (" + (file.size / 1024 / 1024).toFixed(2) + " MB)";

    var reader = new FileReader();
    reader.onload = function (e) {
      var rawBytes = new Uint8Array(e.target.result);
      try {
        var t0 = performance.now();
        var srcNote = "";
        var isImage = false;
        var imageW = 0, imageH = 0;
        var result = null;
        var simpleImageType = H26xDemux.detectImageType(rawBytes);
        if (simpleImageType === "image/jpeg") {
          var jpeg = (typeof JpegParser !== "undefined") ? JpegParser.parseJpeg(rawBytes) : null;
          currentImageBlob = new Blob([rawBytes], { type: "image/jpeg" });
          currentHeicRawBytes = null;
          fileBytes = rawBytes;
          currentDescription = null;
          currentNalLengthSize = 4;
          currentContainerInfo = null;
          isImage = true;
          srcNote = " (JPEG image)";
          var segs = jpeg ? jpeg.segments : [];
          if (jpeg && jpeg.width) { imageW = jpeg.width; imageH = jpeg.height; }
          result = {
            codec: "jpeg",
            imageLabel: "JPEG",
            data: {
              nalus: segs.map(function (s) { return { offset: s.offset, length: s.length, type: s.type, typeName: s.typeName, info: s.info, color: s.color, sliceType: -1, jpegSyntax: s.syntax }; }),
              streamInfo: { nalus: segs.length, slices: 0, i: 0, p: 0, b: 0, profile: "JPEG", level: "-", picWidth: jpeg ? jpeg.width : 0, picHeight: jpeg ? jpeg.height : 0 },
              hdr: {},
              warnings: []
            }
          };
        } else if (simpleImageType) {
          currentImageBlob = new Blob([rawBytes], { type: simpleImageType });
          currentHeicRawBytes = null;
          fileBytes = new Uint8Array(0);
          currentDescription = null;
          currentNalLengthSize = 4;
          currentContainerInfo = null;
          isImage = true;
          srcNote = " (" + simpleImageType.split("/")[1].toUpperCase() + " image)";
          result = {
            codec: "image",
            imageLabel: simpleImageType.split("/")[1].toUpperCase(),
            data: {
              nalus: [],
              streamInfo: { nalus: 0, slices: 0, i: 0, p: 0, b: 0, profile: "N/A", level: "N/A" },
              hdr: {},
              warnings: []
            }
          };
        } else if (H26xDemux.isAvif(rawBytes)) {
          var avif = H26xDemux.parseAvif(rawBytes);
          if (!avif || !avif.frames || !avif.frames.length) {
            setStatus("AVIF parse failed");
            return;
          }
          fileBytes = rawBytes;
          currentDescription = null;
          currentNalLengthSize = 4;
          currentContainerInfo = null;
          currentAv1Frames = avif.frames;
          isImage = true;
          imageW = avif.width || 0;
          imageH = avif.height || 0;
          srcNote = " (AVIF image)";
          result = {
            codec: "av1",
            data: {
              nalus: avif.obus.map(function (o) { return { offset: o.offset, length: o.length, type: o.type, typeName: o.typeName, info: o.info, color: o.color, sliceType: (o.sliceType !== undefined ? o.sliceType : -1), frameType: o.frameType, jpegSyntax: o.syntax }; }),
              streamInfo: { nalus: avif.obus.length, slices: avif.frames.length, i: avif.frames.length, p: 0, b: 0, profile: "AV1", level: String(avif.level), picWidth: avif.width, picHeight: avif.height },
              hdr: {},
              warnings: []
            }
          };
        } else if (H26xDemux.isHeic(rawBytes)) {
          var heic = H26xDemux.parseHeic(rawBytes);
          if (!heic || !heic.annexb.length) {
            setStatus("HEIC parse failed");
            return;
          }
          fileBytes = heic.annexb;
          currentDescription = heic.description || null;
          currentNalLengthSize = heic.nalLengthSize || 4;
          currentImageBlob = new Blob([rawBytes], { type: "image/heic" });
          currentHeicRawBytes = rawBytes;
          isImage = true;
          imageW = heic.picWidth || 0;
          imageH = heic.picHeight || 0;
          srcNote = " (HEIC image)";
        } else if (H26xDemux.isMp4(rawBytes)) {
          var av1mp4 = H26xDemux.demuxAv1Mp4(rawBytes);
          if (av1mp4 && av1mp4.frames && av1mp4.frames.length) {
            fileBytes = rawBytes;
            currentDescription = null;
            currentNalLengthSize = 4;
            currentContainerInfo = null;
            currentAv1Frames = av1mp4.frames;
            srcNote = " (AV1 MP4)";
            result = {
              codec: "av1",
              data: {
                nalus: av1mp4.obus.map(function (o) { return { offset: o.offset, length: o.length, type: o.type, typeName: o.typeName, info: o.info, color: o.color, sliceType: (o.sliceType !== undefined ? o.sliceType : -1), frameType: o.frameType, jpegSyntax: o.syntax }; }),
                streamInfo: { nalus: av1mp4.obus.length, slices: av1mp4.frames.length, i: av1mp4.frames.length, p: 0, b: 0, profile: "AV1", level: String(av1mp4.level), picWidth: av1mp4.width, picHeight: av1mp4.height },
                hdr: {},
                warnings: []
              }
            };
          } else {
            var demuxed = H26xDemux.demuxMp4(rawBytes);
            if (!demuxed || !demuxed.annexb.length) {
              setStatus("MP4 demux failed: no video track found");
              return;
            }
            fileBytes = demuxed.annexb;
            currentDescription = demuxed.description || null;
            currentNalLengthSize = demuxed.nalLengthSize || 4;
            currentContainerInfo = H26xDemux.parseContainerInfo(rawBytes);
            srcNote = " (MP4 demuxed)";
          }
        } else if (H26xDemux.isIvf(rawBytes) || H26xDemux.isAv1AnnexB(rawBytes) || H26xDemux.isWebm(rawBytes)) {
          var demuxedAv = H26xDemux.isWebm(rawBytes) ? H26xDemux.demuxWebm(rawBytes) : H26xDemux.parseIvf(rawBytes);
          if (!demuxedAv || !demuxedAv.frames || !demuxedAv.frames.length) {
            setStatus("AV1/VP9 parse failed");
            return;
          }
          fileBytes = rawBytes;
          currentDescription = null;
          currentNalLengthSize = 4;
          currentContainerInfo = null;
          if (demuxedAv.codec === "vp9") {
            currentVp9Frames = demuxedAv.frames;
            currentVp9Profile = demuxedAv.profile || 0;
            currentVp9BitDepth = demuxedAv.bitDepth || 8;
            srcNote = " (VP9)";
            result = {
              codec: "vp9",
              data: {
                nalus: demuxedAv.units.map(function (u) { return { offset: u.offset, length: u.length, type: u.type, typeName: u.typeName, info: u.info, color: u.color, sliceType: (u.sliceType !== undefined ? u.sliceType : -1), frameType: u.frameType, jpegSyntax: u.syntax }; }),
                streamInfo: { nalus: demuxedAv.units.length, slices: demuxedAv.frames.length, i: 0, p: 0, b: 0, profile: "VP9", level: "-", picWidth: demuxedAv.width, picHeight: demuxedAv.height },
                hdr: {},
                warnings: []
              }
            };
          } else {
            currentAv1Frames = demuxedAv.frames;
            srcNote = " (AV1)";
            result = {
              codec: "av1",
              data: {
                nalus: demuxedAv.obus.map(function (o) { return { offset: o.offset, length: o.length, type: o.type, typeName: o.typeName, info: o.info, color: o.color, sliceType: (o.sliceType !== undefined ? o.sliceType : -1), frameType: o.frameType, jpegSyntax: o.syntax }; }),
                streamInfo: { nalus: demuxedAv.obus.length, slices: demuxedAv.frames.length, i: demuxedAv.frames.length, p: 0, b: 0, profile: "AV1", level: String(demuxedAv.level), picWidth: demuxedAv.width, picHeight: demuxedAv.height },
                hdr: {},
                warnings: []
              }
            };
          }
        } else {
          fileBytes = rawBytes;
          currentDescription = null;
          currentNalLengthSize = 4;
          currentContainerInfo = null;
        }
        if (!result) result = parseBuffer(fileBytes);
        var t1 = performance.now();
        currentCodec = result.codec;
        currentData = result.data;
        currentWarnings = result.data.warnings || [];
        if (currentCodec === "vvc") loadVvdec(function () {});
        if (isImage) {
          currentData.isImage = true;
          if (imageW && imageH) {
            currentData.hdr = currentData.hdr || {};
            currentData.hdr.picWidth = imageW;
            currentData.hdr.picHeight = imageH;
          }
          if (heic) {
            currentData.grid = heic.grid || null;
            currentData.tiles = heic.tiles || null;
          }
        }
        computeFrames();

        var si = result.data.streamInfo;
        var badgeText = result.imageLabel || currentCodec.toUpperCase();
        if (si.bitDepth !== undefined) badgeText += " · " + si.bitDepth + "-bit";
        if (si.chromaFormat !== undefined) badgeText += " · " + chromaFormatName(si.chromaFormat);
        codecBadge.textContent = badgeText;
        codecBadge.classList.remove("hidden");
        dropzone.classList.add("hidden");
        resetBtn.classList.remove("hidden");
        statsBar.classList.remove("hidden");
        timelinePanel.classList.remove("hidden");
        mainArea.classList.remove("hidden");
        bottomPanels.classList.remove("hidden");

        renderStats(currentData.streamInfo);
        renderNalTable();
        renderHdr(currentData.hdr);
        renderWarnings();
        selectedSlice = -1;
        renderTimeline();
        presetPreviewCanvas();
        syntaxTree.innerHTML = "";
        syntaxTitle.textContent = "";
        mediaInfoView.innerHTML = "";
        mediaInfoView.dataset.rendered = "";

        if (currentData.nalus.length > 0) selectNal(0, false);
        if (isImage) { showTab("preview"); if (setMobileView) setMobileView("syntax"); previewFrame(0); }
        else if (currentData.nalus.length > 0) { showTab("preview"); if (setMobileView) setMobileView("syntax"); presetPreviewCanvas(); previewFrame(0); }

        setStatus("Parsed: " + currentCodec.toUpperCase() + ", " + currentData.nalus.length + " NAL units" + srcNote + ", " + (t1 - t0).toFixed(0) + " ms");
      } catch (err) {
        clearAll();
        setStatus("Parse error: " + err.message);
        console.error(err);
      }
    };
    reader.readAsArrayBuffer(file);
  }

  openBtn.addEventListener("click", function () { fileInput.click(); });
  fileInput.addEventListener("change", function () { handleFile(fileInput.files[0]); });

  function resetAll() {
    clearAll();
    fileNameEl.textContent = "No file loaded";
    fileInput.value = "";

    dropzone.classList.remove("hidden");
    statsBar.classList.add("hidden");
    timelinePanel.classList.add("hidden");
    previewPlayBtn.textContent = "▶ Play";
    mainArea.classList.add("hidden");
    bottomPanels.classList.add("hidden");
    resetBtn.classList.add("hidden");

    hexView.innerHTML = "";
    showTab("syntax");

    setStatus("Parser ready (H.264/H.265/H.266). Open or drop a bitstream file.");
  }

  resetBtn.addEventListener("click", resetAll);
  dropzone.addEventListener("click", function () { fileInput.click(); });
  dropzone.addEventListener("dragover", function (e) { e.preventDefault(); dropzone.classList.add("dragover"); });
  dropzone.addEventListener("dragleave", function () { dropzone.classList.remove("dragover"); });
  dropzone.addEventListener("drop", function (e) {
    e.preventDefault();
    dropzone.classList.remove("dragover");
    if (e.dataTransfer.files.length) handleFile(e.dataTransfer.files[0]);
  });

  var helpBtn = document.getElementById("helpBtn");
  var helpModal = document.getElementById("helpModal");
  var helpClose = document.getElementById("helpClose");
  helpBtn.addEventListener("click", function () { helpModal.classList.remove("hidden"); });
  helpClose.addEventListener("click", function () { helpModal.classList.add("hidden"); });
  helpModal.addEventListener("click", function (e) { if (e.target === helpModal) helpModal.classList.add("hidden"); });

  window.addEventListener("resize", function () {
    if (currentData) { renderTimeline(); updateVisibleRows(); }
  });

  // ---------- 可拖拽调整大小 ----------
  function firstColWidth(grid) {
    var cs = getComputedStyle(grid);
    var cols = cs.gridTemplateColumns.split(" ");
    return parseFloat(cols[0]) || Math.round(grid.clientWidth * 0.44);
  }

  function makeSplitterCol(handleId, grid) {
    var handle = document.getElementById(handleId);
    if (!handle) return;
    handle.addEventListener("mousedown", function (e) {
      e.preventDefault();
      handle.classList.add("dragging");
      var startX = e.clientX;
      var startFirst = firstColWidth(grid);
      function move(ev) {
        var dx = ev.clientX - startX;
        var first = Math.max(160, Math.min(startFirst + dx, grid.clientWidth - 240));
        grid.style.gridTemplateColumns = first + "px 5px 1fr";
      }
      function up() {
        handle.classList.remove("dragging");
        document.removeEventListener("mousemove", move);
        document.removeEventListener("mouseup", up);
      }
      document.addEventListener("mousemove", move);
      document.addEventListener("mouseup", up);
    });
  }

  function makeSplitterRow(handleId, lower) {
    var handle = document.getElementById(handleId);
    if (!handle) return;
    handle.addEventListener("mousedown", function (e) {
      e.preventDefault();
      handle.classList.add("dragging");
      var startY = e.clientY;
      var startH = lower.offsetHeight;
      function move(ev) {
        var dy = ev.clientY - startY;
        var h = startH - dy; // 向下拖 dy>0 → 下方面板变小，上方面板变大
        h = Math.max(60, Math.min(h, window.innerHeight - 200));
        lower.style.flex = "0 0 " + h + "px";
      }
      function up() {
        handle.classList.remove("dragging");
        document.removeEventListener("mousemove", move);
        document.removeEventListener("mouseup", up);
      }
      document.addEventListener("mousemove", move);
      document.addEventListener("mouseup", up);
    });
  }

  // ---------- 启动 ----------
  function initMobileNav() {
    var nav = document.getElementById("mobileNav");
    if (!nav) return;
    function setView(name) {
      var btns = nav.querySelectorAll("button");
      for (var i = 0; i < btns.length; i++) btns[i].classList.toggle("active", btns[i].dataset.view === name);
      document.body.setAttribute("data-mobile-view", name);
    }
    nav.addEventListener("click", function (e) {
      var btn = e.target.closest("button");
      if (btn && btn.dataset.view) setView(btn.dataset.view);
    });
    setView("nal");
    return setView;
  }

  function boot() {
    makeSplitterCol("splitMainCol", mainArea);
    makeSplitterCol("splitBottomCol", bottomPanels);
    makeSplitterRow("splitMainRow", bottomPanels);
    setMobileView = initMobileNav();
    if (typeof createHevcModule !== "function") {
      setStatus("Error: WASM module (hevc.js) not found");
      return;
    }
    createHevcModule().then(function (m) {
      Module = m;
      setStatus("Parser ready (H.264/H.265/H.266). Open or drop a bitstream file.");
    }).catch(function (err) {
      setStatus("Failed to load WASM module: " + err);
      console.error(err);
    });
  }

  boot();
})();
