(function () {
  "use strict";

  var Module = null;
  var currentData = null;      // 汇总 JSON
  var currentCodec = null;     // "hevc" / "avc" / "vvc"
  var currentWarnings = [];
  var fileBytes = null;        // 原始文件字节（用于 hex 视图）
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
  var hexView = document.getElementById("hexView");
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
  var timelineZoom = 1;   // 时间轴缩放倍数
  var selectedSlice = -1; // 时间轴上选中的帧

  function setStatus(msg) { statusEl.textContent = msg; }
  function hex8(v) { var s = v.toString(16); while (s.length < 8) s = "0" + s; return "0x" + s; }

  // ---------- MP4 解封装 ----------
  function readU32(d, o) { return (d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]; }
  function readU16(d, o) { return (d[o] << 8) | d[o + 1]; }
  function fourCC(d, o) { return String.fromCharCode(d[o], d[o + 1], d[o + 2], d[o + 3]); }
  function isMp4(d) { return d.length >= 12 && fourCC(d, 4) === "ftyp"; }
  function boxSize(d, o) {
    var size = readU32(d, o);
    if (size === 1) return readU32(d, o + 8) * 4294967296 + readU32(d, o + 12); // 64-bit largesize
    return size;
  }

  function findBox(d, start, end, type) {
    var i = start;
    while (i + 8 <= end) {
      var size = boxSize(d, i);
      if (size < 8 || i + size > end) break;
      if (fourCC(d, i + 4) === type) return { offset: i, size: size };
      i += size;
    }
    return null;
  }

  function findFourCC(d, start, end, cc) {
    for (var i = start; i + 4 <= end; i++) {
      if (fourCC(d, i) === cc) return { offset: i - 4, size: readU32(d, i - 4) };
    }
    return null;
  }

  function nalToAnnexB(nals) {
    // nals: array of Uint8Array（已是 EBSP，含 emulation prevention）
    var out = [];
    for (var j = 0; j < nals.length; j++) {
      out.push(0, 0, 0, 1);
      for (var k = 0; k < nals[j].length; k++) out.push(nals[j][k]);
    }
    return new Uint8Array(out);
  }

  function demuxMp4(d) {
    var moov = null;
    var i = 0;
    while (i + 8 <= d.length) {
      var size = boxSize(d, i);
      if (size < 8 || i + size > d.length) break;
      if (fourCC(d, i + 4) === "moov") { moov = { offset: i + 8, end: i + size }; break; }
      i += size;
    }
    if (!moov) return null;

    // 找视频轨道
    var trakStart = moov.offset;
    var result = null;
    while (trakStart + 8 <= moov.end && !result) {
      var trakSize = boxSize(d, trakStart);
      if (trakSize < 8) break;
      if (fourCC(d, trakStart + 4) === "trak") {
        var trakEnd = trakStart + trakSize;
        var mdia = findBox(d, trakStart + 8, trakEnd, "mdia");
        if (mdia) {
          var hdlr = findBox(d, mdia.offset + 8, mdia.offset + mdia.size, "hdlr");
          if (hdlr && fourCC(d, hdlr.offset + 16) === "vide") {
            var minf = findBox(d, mdia.offset + 8, mdia.offset + mdia.size, "minf");
            if (minf) {
              var stbl = findBox(d, minf.offset + 8, minf.offset + minf.size, "stbl");
              if (stbl) result = parseStbl(d, stbl.offset + 8, stbl.offset + stbl.size);
            }
          }
        }
      }
      trakStart += trakSize;
    }
    return result;
  }

  function parseStbl(d, start, end) {
    var stsd = findBox(d, start, end, "stsd");
    var stsz = findBox(d, start, end, "stsz");
    var stsc = findBox(d, start, end, "stsc");
    var stco = findBox(d, start, end, "stco") || findBox(d, start, end, "co64");

    if (!stsd || !stsz || !stco) return null;

    // 从 stsd 提取 codec + SPS/PPS
    var codec = null;
    var paramNals = []; // 参数集 NAL（RBSP，含 VPS/SPS/PPS）
    var sdEnd = stsd.offset + stsd.size;
    var p = stsd.offset + 16; // 跳过 version+flags(4) + entry_count(4)，到第一个 sample entry
    if (p + 8 <= sdEnd) {
      var fmt = fourCC(d, p + 4);
      if (fmt === "avc1" || fmt === "avc3") codec = "avc";
      if (fmt === "hev1" || fmt === "hvc1") codec = "hevc";
      if (fmt === "vvc1" || fmt === "vvi1") codec = "vvc";
      // 在 stsd 里搜 avcC/hvcC/vvcC box（fourcc 字节搜索）
      var cfg = findFourCC(d, stsd.offset + 8, sdEnd, "avcC") || findFourCC(d, stsd.offset + 8, sdEnd, "hvcC") || findFourCC(d, stsd.offset + 8, sdEnd, "vvcC");
      if (cfg) {
        var c = cfg.offset + 8; // 配置内容
        var cEnd = cfg.offset + cfg.size;
        if (codec === "avc") {
          // avcC: version(1)+profile(1)+compat(1)+level(1)+0xFF+0xE1+spslen(2)+sps+numPPS(1)+ppslen(2)+pps
          var spsLen = readU16(d, c + 6);
          paramNals.push(d.subarray(c + 8, c + 8 + spsLen));
          var np = c + 8 + spsLen;
          var numPps = d[np];
          np += 1;
          for (var q = 0; q < numPps; q++) {
            var ppsLen = readU16(d, np);
            paramNals.push(d.subarray(np + 2, np + 2 + ppsLen));
            np += 2 + ppsLen;
          }
        } else if (codec === "hevc" || codec === "vvc") {
          // hvcC/vvcC: 23-byte header（numOfArrays 在 offset 22），然后 arrays
          var na = d[c + 22];
          var np2 = c + 23;
          for (var q2 = 0; q2 < na; q2++) {
            var ntype = d[np2] & 0x3F;
            var numNalus = readU16(d, np2 + 1);
            np2 += 3;
            for (var q3 = 0; q3 < numNalus; q3++) {
              var nlen = readU16(d, np2);
              // 只保留 VPS/SPS/PPS（32/33/34），跳过 SEI(39) 等
              if (ntype === 32 || ntype === 33 || ntype === 34)
                paramNals.push(d.subarray(np2 + 2, np2 + 2 + nlen));
              np2 += 2 + nlen;
            }
          }
        }
      }
    }
    if (!codec) return null;

    // stsz：样本大小
    var sampleSizes = [];
    var uniformSize = readU32(d, stsz.offset + 12);   // sample_size
    var szCount = readU32(d, stsz.offset + 16);        // sample_count
    if (uniformSize > 0) {
      for (var s1 = 0; s1 < szCount; s1++) sampleSizes.push(uniformSize);
    } else {
      for (var s2 = 0; s2 < szCount; s2++) sampleSizes.push(readU32(d, stsz.offset + 20 + s2 * 4));
    }

    // stco：chunk offset
    var chunkOffsets = [];
    var coCount = readU32(d, stco.offset + 12);         // entry_count
    var co64 = fourCC(d, stco.offset + 4) === "co64";
    for (var c1 = 0; c1 < coCount; c1++) {
      if (co64) {
        var hi = readU32(d, stco.offset + 16 + c1 * 8);
        var lo = readU32(d, stco.offset + 20 + c1 * 8);
        chunkOffsets.push(hi * 4294967296 + lo);
      } else {
        chunkOffsets.push(readU32(d, stco.offset + 16 + c1 * 4));
      }
    }

    // stsc：sample to chunk
    var stscEntries = [];
    if (stsc) {
      var scCount = readU32(d, stsc.offset + 12);       // entry_count
      for (var sc = 0; sc < scCount; sc++) {
        stscEntries.push({
          firstChunk: readU32(d, stsc.offset + 16 + sc * 12),
          samplesPerChunk: readU32(d, stsc.offset + 20 + sc * 12)
        });
      }
    } else {
      stscEntries.push({ firstChunk: 1, samplesPerChunk: 1 });
    }

    // 构建 chunk → 样本数 映射
    var sampleOffsets = [];
    var chunkSampleCount = [];
    var sampleIndex = 0;
    var chunkIndex = 0;
    var dataOffset = 0;
    while (sampleIndex < sampleSizes.length && chunkIndex < chunkOffsets.length) {
      // 当前 chunk 的 samplesPerChunk
      var spc = 1;
      for (var e = 0; e < stscEntries.length; e++) {
        if (stscEntries[e].firstChunk <= chunkIndex + 1) spc = stscEntries[e].samplesPerChunk;
      }
      for (var k = 0; k < spc && sampleIndex < sampleSizes.length; k++) {
        sampleOffsets.push(chunkOffsets[chunkIndex] + dataOffset);
        dataOffset += sampleSizes[sampleIndex];
        sampleIndex++;
      }
      chunkIndex++;
      dataOffset = 0;
    }

    // 组装 Annex B 裸流：先收集 NAL 列表（subarray 引用，不复制），再一次性分配
    var nalList = [];
    for (var pn = 0; pn < paramNals.length; pn++) nalList.push(paramNals[pn]);
    for (var si2 = 0; si2 < sampleOffsets.length; si2++) {
      var sample = d.subarray(sampleOffsets[si2], sampleOffsets[si2] + sampleSizes[si2]);
      var p2 = 0;
      while (p2 + 4 <= sample.length) {
        var nlen = (sample[p2] << 24) | (sample[p2 + 1] << 16) | (sample[p2 + 2] << 8) | sample[p2 + 3];
        p2 += 4;
        if (nlen < 0 || p2 + nlen > sample.length) break;
        nalList.push(sample.subarray(p2, p2 + nlen));
        p2 += nlen;
      }
    }
    var totalSize = 0;
    for (var q = 0; q < nalList.length; q++) totalSize += nalList[q].length + 4;
    var out = new Uint8Array(totalSize);
    var w = 0;
    for (var q2 = 0; q2 < nalList.length; q2++) {
      out[w] = 0; out[w + 1] = 0; out[w + 2] = 0; out[w + 3] = 1;
      w += 4;
      out.set(nalList[q2], w);
      w += nalList[q2].length;
    }
    return { codec: codec, annexb: out };
  }

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

    return { codec: codec, data: JSON.parse(json) };
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
    if (si.picWidth) stat("Resolution", si.picWidth + " x " + si.picHeight);
    stat("Profile", si.profile);
    stat("Level", si.level);
    if (si.tier) stat("Tier", si.tier);
  }

  // 虚拟滚动 NAL 列表
  function renderNalTable() {
    var nalus = currentData.nalus;
    nalCount.textContent = "(" + nalus.length + ")";
    nalSpacer.style.height = (nalus.length * ROW_HEIGHT) + "px";
    selectedIndex = -1;
    updateVisibleRows();
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

    var info = document.createElement("span");
    info.className = "c-info";
    info.textContent = n.info;
    if (n.color) info.style.color = n.color;

    row.appendChild(off);
    row.appendChild(len);
    row.appendChild(type);
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

  // 帧时间轴（Slice 可视化，标记帧号 / POC）
  function renderTimeline() {
    var slices = [];
    currentData.nalus.forEach(function (n, i) {
      if (n.sliceType >= 0) {
        slices.push({ index: i, type: n.sliceType, poc: n.slicePoc, frame: n.frameNum });
      }
    });
    if (slices.length === 0) return;

    var dpr = window.devicePixelRatio || 1;
    var minBarW = 20 * timelineZoom;   // 每帧最小宽度（可缩放）
    var labelH = 24;       // 顶部帧号/POC 标记区
    var barH = 36;         // 色块区
    var wrapW = timeline.parentNode.clientWidth - 24;
    var w = Math.max(wrapW, slices.length * minBarW);
    var h = labelH + barH;
    timeline.style.width = w + "px";
    timeline.style.height = h + "px";
    timeline.width = w * dpr;
    timeline.height = h * dpr;
    var ctx = timeline.getContext("2d");
    ctx.scale(dpr, dpr);
    ctx.clearRect(0, 0, w, h);

    var barW = w / slices.length;
    var colors = { 2: "#E02020", 0: "#0066ff", 1: "#00B050" };
    var barTop = labelH;

    // 色块 + 帧号 / POC 标记
    var step = Math.max(1, Math.ceil(44 / barW)); // 每 ~44px 至少一个标记，避免重叠
    ctx.font = "9px monospace";
    ctx.textBaseline = "middle";
    for (var i = 0; i < slices.length; i++) {
      var s = slices[i];
      var x = i * barW;
      ctx.fillStyle = colors[s.type] || "#888";
      ctx.fillRect(x, barTop, Math.max(1, barW - 0.5), barH);
      if (i % step === 0) {
        ctx.fillStyle = "#bbb";
        ctx.fillText(String(s.frame), x + 1, 8);         // 帧号
        ctx.fillStyle = "#777";
        ctx.fillText(String(s.poc), x + 1, labelH - 6);  // POC
      }
    }
    // 高亮选中帧
    if (selectedSlice >= 0 && selectedSlice < slices.length) {
      var hx = selectedSlice * barW;
      ctx.strokeStyle = "#ffffff";
      ctx.lineWidth = 1.5;
      ctx.strokeRect(hx + 0.5, barTop + 0.5, Math.max(1, barW - 1), barH - 1);
    }
    timeline._slices = slices;
    timeline._barW = barW;
    timeline._labelH = labelH;

    var legend = document.getElementById("timelineLegend");
    legend.innerHTML =
      '<b style="color:#E02020">I</b> <b style="color:#0066ff">P</b> <b style="color:#00B050">B</b>' +
      ' <span style="color:var(--text-dim)"> | Frame# / POC | Click a frame to preview | Zoom</span>' +
      ' <button class="zoom-btn" onclick="window.__tlZoom(1)">+</button>' +
      ' <button class="zoom-btn" onclick="window.__tlZoom(-1)">−</button>';
  }

  window.__tlZoom = function (d) {
    timelineZoom = Math.max(0.5, Math.min(6, timelineZoom + d * 0.5));
    renderTimeline();
  };

  function timelineTip() {
    if (!timeline._tip) {
      timeline._tip = document.createElement("div");
      timeline._tip.className = "timeline-tip";
      document.body.appendChild(timeline._tip);
    }
    return timeline._tip;
  }
  function timelineMove(e) {
    if (!timeline._slices) return;
    var rect = timeline.getBoundingClientRect();
    var x = e.clientX - rect.left;
    var idx = Math.floor(x / timeline._barW);
    if (idx < 0 || idx >= timeline._slices.length) { timelineTip().style.display = "none"; return; }
    var s = timeline._slices[idx];
    var t = { 2: "I", 0: "P", 1: "B" }[s.type] || "?";
    timelineTip().textContent = "Frame " + s.frame + "  [" + t + "]\nPOC " + s.poc;
    timelineTip().style.display = "block";
    timelineTip().style.left = (e.clientX + 12) + "px";
    timelineTip().style.top = (e.clientY + 12) + "px";
  }

   timeline.addEventListener("click", function (e) {
     if (!timeline._slices) return;
     var rect = timeline.getBoundingClientRect();
     var x = e.clientX - rect.left;
     var idx = Math.floor(x / timeline._barW);
     if (idx >= 0 && idx < timeline._slices.length) {
       selectedSlice = idx;
       selectNal(timeline._slices[idx].index, true);
       renderTimeline();
       previewFrame(idx);
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
    feedSlice: -1,
    frames: {},
    curSlice: -1
  };

  function findKeySlice(sliceIndex) {
    for (var s = sliceIndex; s >= 0; s--) {
      var nalIdx = timeline._slices[s].index;
      if (isKeyNal(currentData.nalus[nalIdx].type)) return s;
    }
    return -1;
  }

  function makeAuData(nalIdxs) {
    var len = 0;
    for (var k = 0; k < nalIdxs.length; k++) len += currentData.nalus[nalIdxs[k]].length;
    var data = new Uint8Array(len);
    var o = 0;
    for (var k2 = 0; k2 < nalIdxs.length; k2++) {
      var nal = currentData.nalus[nalIdxs[k2]];
      data.set(fileBytes.subarray(nal.offset, nal.offset + nal.length), o);
      o += nal.length;
    }
    return data;
  }

  function feedSlices(toSlice) {
    var slices = timeline._slices;
    var end = Math.min(toSlice, slices.length - 1);
    while (play.feedSlice < end && play.decoder && play.decoder.state === "configured") {
      var si = play.feedSlice + 1;
      var nalIdx = slices[si].index;
      var isKey = isKeyNal(currentData.nalus[nalIdx].type);
      var nals = isKey ? play.params.concat([nalIdx]) : [nalIdx];
      play.decoder.decode(new EncodedVideoChunk({
        type: isKey ? "key" : "delta",
        timestamp: si,
        data: makeAuData(nals)
      }));
      play.feedSlice = si;
    }
  }

  function initPlayDecoder(done) {
    var cs = codecString();
    if (!cs) { previewMsg.textContent = "Unable to determine codec string"; return; }
    VideoDecoder.isConfigSupported({ codec: cs }).then(function (support) {
      if (!support.supported) {
        previewMsg.textContent = "Browser does not support decoding " + cs + (currentCodec === "hevc" ? " (H.265 may be restricted by hardware/licensing)" : "");
        return;
      }
      if (play.decoder) { try { play.decoder.close(); } catch (e) {} }
      for (var k in play.frames) { try { play.frames[k].close(); } catch (e) {} }
      play.frames = {};
      play.feedSlice = -1;
      play.params = [];
      var types = currentCodec === "avc" ? [7, 8] : [32, 33, 34];
      for (var i = 0; i < currentData.nalus.length; i++)
        if (types.indexOf(currentData.nalus[i].type) >= 0) play.params.push(i);
      play.decoder = new VideoDecoder({
        output: function (frame) { play.frames[frame.timestamp] = frame; },
        error: function (err) {
          previewMsg.textContent = "Decode error: " + err.message;
          stopPlayback();
        }
      });
      play.decoder.configure({ codec: cs, optimizeForLatency: true });
      done();
    }).catch(function (err) {
      previewMsg.textContent = "Config error: " + err.message;
    });
  }

  function showPlayFrame(sliceIdx) {
    var frame = play.frames[sliceIdx];
    if (frame) {
      drawVideoFrame(frame);
      previewHint.textContent = "Frame " + timeline._slices[sliceIdx].frame + " / POC " + timeline._slices[sliceIdx].poc;
    }
    for (var k in play.frames) {
      if (parseInt(k, 10) < sliceIdx - 2) { try { play.frames[k].close(); } catch (e) {} delete play.frames[k]; }
    }
  }

  function stopPlayback() {
    if (play.timer) { clearInterval(play.timer); play.timer = null; }
    play.active = false;
    previewPlayBtn.textContent = "▶ Play";
  }

  function startPlayback() {
    if (play.active) { stopPlayback(); return; }
    var si = selectedSlice;
    if (si < 0 || si >= timeline._slices.length) return;
    var keySlice = findKeySlice(si);
    if (keySlice < 0) { previewMsg.textContent = "Key frame not found"; return; }
    initPlayDecoder(function () {
      play.active = true;
      play.curSlice = si;
      previewPlayBtn.textContent = "⏸ Pause";
      previewMsg.textContent = "";
      feedSlices(si);
      play.timer = setInterval(function () {
        if (!play.active) return;
        if (play.curSlice >= timeline._slices.length - 1) { stopPlayback(); return; }
        showPlayFrame(play.curSlice);
        play.curSlice++;
        feedSlices(Math.min(play.curSlice + 10, timeline._slices.length - 1));
      }, 40);
    });
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
    return false;
  }
  function isKeyNal(type) {
    if (currentCodec === "avc") return type === 5;
    if (currentCodec === "hevc") return type === 16 || type === 17 || type === 18 || type === 19 || type === 20 || type === 21;
    return false;
  }

  function findNalByType(type) {
    for (var i = 0; i < currentData.nalus.length; i++)
      if (currentData.nalus[i].type === type) return i;
    return -1;
  }

  function codecString() {
    if (currentCodec === "avc") {
      var si = findNalByType(7);
      if (si < 0) return null;
      var sps = nalData(currentData.nalus[si]);
      function hx(v) { var s = v.toString(16); return (s.length < 2 ? "0" : "") + s; }
      return "avc3." + hx(sps[1]) + hx(sps[2]) + hx(sps[3]);
    }
    if (currentCodec === "hevc") {
      var si = findNalByType(33);
      if (si < 0) return null;
      var sps = nalData(currentData.nalus[si]);
      var b1 = sps[3];
      var space = (b1 >> 6) & 3;
      var profileIdc = b1 & 0x1F;
      var compat = (sps[4] << 24) | (sps[5] << 16) | (sps[6] << 8) | sps[7];
      var level = sps[14];
      var spaceChar = ["", "A", "B", "C"][space];
      // codec string 里 compat/constraint 是 bit-reversed（ISO 14496-15）
      function rev32(v) { var r = 0; for (var i = 0; i < 32; i++) { r = (r << 1) | (v & 1); v >>>= 1; } return r >>> 0; }
      var compatHex = rev32(compat).toString(16).toUpperCase();
      return "hev1." + spaceChar + profileIdc + "." + compatHex + ".L" + level + ".B0";
    }
    return null;
  }

  function drawVideoFrame(frame) {
    var c = previewCanvas;
    var w = frame.displayWidth, h = frame.displayHeight;
    if (w <= 0 || h <= 0) { w = frame.codedWidth; h = frame.codedHeight; }
    var maxW = previewView.clientWidth - 32;
    var maxH = 480;
    if (maxW < 160) maxW = 160;
    var scale = Math.min(1, maxW / w, maxH / h);
    c.width = Math.max(1, Math.round(w * scale));
    c.height = Math.max(1, Math.round(h * scale));
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

  function previewFrame(sliceIndex) {
    if (!fileBytes || !currentData || !timeline._slices) return;
    if (currentCodec === "vvc") {
      previewMsg.textContent = "H.266 (VVC) cannot be decoded in browser";
      return;
    }
    if (!("VideoDecoder" in window)) {
      previewMsg.textContent = "Browser does not support WebCodecs";
      return;
    }
    if (play.active) stopPlayback();
    var keySlice = findKeySlice(sliceIndex);
    if (keySlice < 0) { previewMsg.textContent = "Key frame not found"; return; }

    previewHint.textContent = "Decoding... (POC " + timeline._slices[sliceIndex].poc + ")";
    previewMsg.textContent = "";

    initPlayDecoder(function () {
      feedSlices(sliceIndex);
      var check = setInterval(function () {
        var frame = play.frames[sliceIndex];
        if (frame) {
          clearInterval(check);
          showPlayFrame(sliceIndex);
        }
      }, 10);
    });
  }

  // HDR
  function renderHdr(h) {
    hdrInfo.innerHTML = "";
    function row(k, v) {
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

    renderSyntax(fetchNalSyntax(index));
    renderHex(index);

    if (scrollTo) {
      var targetTop = index * ROW_HEIGHT;
      nalScroll.scrollTop = targetTop - nalScroll.clientHeight / 2;
      updateVisibleRows();
    }
  }

  nalRows.addEventListener("click", function (e) {
    var row = e.target.closest(".nal-row");
    if (row) selectNal(parseInt(row.dataset.index, 10), false);
  });

  nalScroll.addEventListener("scroll", updateVisibleRows);

  warningFilter.addEventListener("change", renderWarnings);

  // tab 切换
  var tabSyntax = document.getElementById("tabSyntax");
  var tabPreview = document.getElementById("tabPreview");
  var tabHex = document.getElementById("tabHex");

  function showTab(which) {
    tabSyntax.classList.toggle("active", which === "syntax");
    tabPreview.classList.toggle("active", which === "preview");
    tabHex.classList.toggle("active", which === "hex");
    syntaxTree.classList.toggle("hidden", which !== "syntax");
    previewView.classList.toggle("hidden", which !== "preview");
    hexView.classList.toggle("hidden", which !== "hex");
  }

  tabSyntax.addEventListener("click", function () { showTab("syntax"); });
  tabPreview.addEventListener("click", function () { showTab("preview"); });
  tabHex.addEventListener("click", function () { showTab("hex"); });

  previewPlayBtn.addEventListener("click", function () { startPlayback(); });
  previewPrevBtn.addEventListener("click", function () { stepFrame(-1); });
  previewNextBtn.addEventListener("click", function () { stepFrame(1); });

  function stepFrame(delta) {
    if (play.active) stopPlayback();
    var si = selectedSlice + delta;
    if (si < 0 || si >= timeline._slices.length) return;
    selectedSlice = si;
    selectNal(timeline._slices[si].index, true);
    renderTimeline();
    previewFrame(si);
  }

  // ---------- 文件处理 ----------
  function handleFile(file) {
    if (!file) return;
    setStatus("Parsing " + file.name + " ...");
    fileNameEl.textContent = file.name + " (" + (file.size / 1024 / 1024).toFixed(2) + " MB)";

    var reader = new FileReader();
    reader.onload = function (e) {
      var rawBytes = new Uint8Array(e.target.result);
      try {
        var t0 = performance.now();
        var srcNote = "";
        if (isMp4(rawBytes)) {
          var demuxed = demuxMp4(rawBytes);
          if (!demuxed || !demuxed.annexb.length) {
            setStatus("MP4 demux failed: no video track found");
            return;
          }
          fileBytes = demuxed.annexb;
          srcNote = " (MP4 demuxed)";
        } else {
          fileBytes = rawBytes;
        }
        var result = parseBuffer(fileBytes);
        var t1 = performance.now();
        currentCodec = result.codec;
        currentData = result.data;
        currentWarnings = result.data.warnings || [];

        codecBadge.textContent = currentCodec.toUpperCase();
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

        if (currentData.nalus.length > 0) selectNal(0, false);

        setStatus("Parsed: " + currentCodec.toUpperCase() + ", " + currentData.nalus.length + " NAL units" + srcNote + ", " + (t1 - t0).toFixed(0) + " ms");
      } catch (err) {
        setStatus("Parse error: " + err.message);
        console.error(err);
      }
    };
    reader.readAsArrayBuffer(file);
  }

  openBtn.addEventListener("click", function () { fileInput.click(); });
  fileInput.addEventListener("change", function () { handleFile(fileInput.files[0]); });

  function resetAll() {
    currentData = null;
    currentCodec = null;
    currentWarnings = [];
    fileBytes = null;
    selectedIndex = -1;

    codecBadge.classList.add("hidden");
    codecBadge.textContent = "";
    fileNameEl.textContent = "No file loaded";
    fileInput.value = "";

    dropzone.classList.remove("hidden");
    statsBar.classList.add("hidden");
    statsBar.innerHTML = "";
    timelinePanel.classList.add("hidden");
    if (play.timer) { clearInterval(play.timer); play.timer = null; }
    if (play.decoder) { try { play.decoder.close(); } catch (e) {} play.decoder = null; }
    for (var pk in play.frames) { try { play.frames[pk].close(); } catch (e) {} }
    play.frames = {};
    play.active = false;
    play.feedSlice = -1;
    previewPlayBtn.textContent = "▶ Play";
    mainArea.classList.add("hidden");
    bottomPanels.classList.add("hidden");
    resetBtn.classList.add("hidden");

    nalSpacer.style.height = "0px";
    nalCount.textContent = "";
    nalRows.innerHTML = "";
    syntaxTree.innerHTML = "";
    syntaxTitle.textContent = "";
    hexView.innerHTML = "";
    previewMsg.textContent = "Click a frame on the timeline to preview";
    previewHint.textContent = "";
    showTab("syntax");
    hdrInfo.innerHTML = "";
    warningBody.innerHTML = "";
    warningCount.textContent = "";

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

  window.addEventListener("resize", function () {
    if (currentData) { renderTimeline(); updateVisibleRows(); }
  });

  // ---------- 启动 ----------
  function boot() {
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
