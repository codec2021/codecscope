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
  var previewPanel = document.getElementById("previewPanel");
  var previewCanvas = document.getElementById("previewCanvas");
  var previewMsg = document.getElementById("previewMsg");
  var previewHint = document.getElementById("previewHint");

  var ROW_HEIGHT = 22;

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
    if (si.picWidth) stat("分辨率", si.picWidth + " x " + si.picHeight);
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
    if (!fileBytes || nal.length <= 0) { hexView.textContent = "无数据"; return; }
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

  // 帧时间轴（Slice 可视化，标记帧号 / POC + 参考帧箭头）
  function renderTimeline() {
    var slices = [];
    var pocMap = {};
    currentData.nalus.forEach(function (n, i) {
      if (n.sliceType >= 0) {
        var idx = slices.length;
        slices.push({ index: i, type: n.sliceType, poc: n.slicePoc, frame: n.frameNum, refs: n.refPocs || [] });
        if (n.slicePoc >= 0) pocMap[n.slicePoc] = idx;
      }
    });
    if (slices.length === 0) return;

    var dpr = window.devicePixelRatio || 1;
    var minBarW = 14;      // 每帧最小宽度
    var labelH = 26;       // 顶部帧号/POC 标记区
    var arrowH = 58;       // 参考帧箭头区
    var barH = 42;         // 色块区
    var wrapW = timeline.parentNode.clientWidth - 24;
    var w = Math.max(wrapW, slices.length * minBarW);
    var h = labelH + arrowH + barH;
    timeline.style.width = w + "px";
    timeline.width = w * dpr;
    timeline.height = h * dpr;
    var ctx = timeline.getContext("2d");
    ctx.scale(dpr, dpr);
    ctx.clearRect(0, 0, w, h);

    var barW = w / slices.length;
    var colors = { 2: "#E02020", 0: "#0066ff", 1: "#00B050" };
    var barTop = labelH + arrowH;

    // 参考帧箭头（参考帧 → 当前帧）
    var arrowColor = "rgba(255,190,60,0.55)";
    ctx.strokeStyle = arrowColor;
    ctx.fillStyle = arrowColor;
    ctx.lineWidth = 1;
    for (var i = 0; i < slices.length; i++) {
      var s = slices[i];
      for (var r = 0; r < s.refs.length; r++) {
        var j = pocMap[s.refs[r]];
        if (j === undefined || j === i) continue;
        var x1 = i * barW + barW / 2;   // 当前帧
        var x2 = j * barW + barW / 2;   // 参考帧
        var arc = Math.max(8, Math.min(44, Math.abs(x1 - x2) * 0.14));
        var mid = (x1 + x2) / 2;
        ctx.beginPath();
        ctx.moveTo(x2, barTop);
        ctx.quadraticCurveTo(mid, barTop - arc, x1, barTop);
        ctx.stroke();
        // 箭头头部（指向当前帧 x1）
        var ang = Math.atan2((x1 - mid), arc); // 终点切向
        var hs = 4;
        ctx.beginPath();
        ctx.moveTo(x1, barTop);
        ctx.lineTo(x1 - hs * 0.9, barTop - hs * 0.5);
        ctx.lineTo(x1 - hs * 0.9, barTop + hs * 0.5);
        ctx.closePath();
        ctx.fill();
      }
    }

    // 色块 + 帧号 / POC 标记
    var step = Math.max(1, Math.ceil(46 / barW)); // 每 ~46px 至少一个标记，避免重叠
    ctx.font = "9px monospace";
    ctx.textBaseline = "middle";
    for (var i = 0; i < slices.length; i++) {
      var s = slices[i];
      var x = i * barW;
      ctx.fillStyle = colors[s.type] || "#888";
      ctx.fillRect(x, barTop, Math.max(1, barW - 0.5), barH);
      if (i % step === 0) {
        ctx.fillStyle = "#bbb";
        ctx.fillText(String(s.frame), x + 1, 7);         // 帧号
        ctx.fillStyle = "#777";
        ctx.fillText(String(s.poc), x + 1, labelH - 7);  // POC
      }
    }
    timeline._slices = slices;
    timeline._barW = barW;
    timeline._labelH = labelH;

    var legend = document.getElementById("timelineLegend");
    legend.innerHTML = '<b style="color:#E02020">I</b> <b style="color:#0066ff">P</b> <b style="color:#00B050">B</b> <span style="color:#ffbe3c">↗</span><span style="color:var(--text-dim)"> 参考帧箭头｜上方帧号 / POC</span>';
  }

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
    var refStr = (s.refs && s.refs.length) ? "\n参考 POC " + s.refs.join(",") : "";
    timelineTip().textContent = "帧 " + s.frame + "  [" + t + "]\nPOC " + s.poc + refStr;
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
       selectNal(timeline._slices[idx].index, true);
       previewFrame(idx);
     }
   });
  timeline.addEventListener("mousemove", timelineMove);
  timeline.addEventListener("mouseleave", function () { timelineTip().style.display = "none"; });

  // ---------- 画面预览（WebCodecs 解码 H.264/H.265） ----------
  var previewDecoder = null;

  function nalData(nal) {
    var off = nal.offset, len = nal.length;
    var startLen = 3;
    if (fileBytes[off] === 0 && fileBytes[off + 1] === 0 && fileBytes[off + 2] === 0 && fileBytes[off + 3] === 1) startLen = 4;
    else if (fileBytes[off] === 0 && fileBytes[off + 1] === 0 && fileBytes[off + 2] === 1) startLen = 3;
    else startLen = 0;
    return fileBytes.subarray(off + startLen, off + len);
  }

  function removeEmulationPrevention(data) {
    var out = [];
    for (var i = 0; i < data.length; i++) {
      if (i + 2 < data.length && data[i] === 0 && data[i + 1] === 0 && data[i + 2] === 3) {
        out.push(0, 0);
        i += 2;
      } else {
        out.push(data[i]);
      }
    }
    return new Uint8Array(out);
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
  function isPrefixNal(type) {
    if (currentCodec === "avc") return type === 6 || type === 9;
    if (currentCodec === "hevc") return type === 35 || type === 39 || type === 40;
    return false;
  }

  function findNalByType(type) {
    for (var i = 0; i < currentData.nalus.length; i++)
      if (currentData.nalus[i].type === type) return i;
    return -1;
  }

  function buildDescription() {
    if (currentCodec === "avc") {
      var si = findNalByType(7), pi = findNalByType(8);
      if (si < 0 || pi < 0) return null;
      var sps = removeEmulationPrevention(nalData(currentData.nalus[si]));
      var pps = removeEmulationPrevention(nalData(currentData.nalus[pi]));
      if (sps.length < 4) return null;
      var desc = new Uint8Array(6 + sps.length + 1 + 2 + pps.length);
      var o = 0;
      desc[o++] = 1;                       // configurationVersion
      desc[o++] = sps[1];                  // AVCProfileIndication
      desc[o++] = sps[2];                  // profile_compatibility
      desc[o++] = sps[3];                  // AVCLevelIndication
      desc[o++] = 0xFF;                    // lengthSizeMinusOne(3) = 0xFF
      desc[o++] = 0xE1;                    // numOfSequenceParameterSets(1) = 0xE1
      desc[o++] = (sps.length >> 8) & 0xFF;
      desc[o++] = sps.length & 0xFF;
      desc.set(sps, o); o += sps.length;
      desc[o++] = 1;                       // numOfPictureParameterSets
      desc[o++] = (pps.length >> 8) & 0xFF;
      desc[o++] = pps.length & 0xFF;
      desc.set(pps, o);
      return desc;
    }
    if (currentCodec === "hevc") {
      var vi = findNalByType(32), si = findNalByType(33), pi = findNalByType(34);
      if (si < 0 || pi < 0) return null;
      var sps = removeEmulationPrevention(nalData(currentData.nalus[si]));
      var pps = removeEmulationPrevention(nalData(currentData.nalus[pi]));
      var vps = vi >= 0 ? removeEmulationPrevention(nalData(currentData.nalus[vi])) : new Uint8Array(0);
      if (sps.length < 4) return null;
      // 从 SPS 解析 profile_tier_level（跳过 2 字节 NAL header）
      var b1 = sps[3]; // profile_space(2) + tier(1) + profile_idc(5)
      var profileSpace = (b1 >> 6) & 3;
      var tierFlag = (b1 >> 5) & 1;
      var profileIdc = b1 & 0x1F;
      var compat = (sps[4] << 24) | (sps[5] << 16) | (sps[6] << 8) | sps[7];
      var constraint = 0;
      for (var c = 0; c < 6; c++) constraint = (constraint << 8) | sps[8 + c];
      var levelIdc = sps[14];
      // 构建 hvcC
      var arrays = [];
      var nalTypes = [];
      if (vps.length) { arrays.push(vps); nalTypes.push(32); }
      arrays.push(sps); nalTypes.push(33);
      arrays.push(pps); nalTypes.push(34);
      var headerLen = 23;
      var bodyLen = 0;
      for (var a = 0; a < arrays.length; a++) bodyLen += 3 + arrays[a].length;
      var desc = new Uint8Array(headerLen + bodyLen);
      var o = 0;
      desc[o++] = 1; // configurationVersion
      desc[o++] = (profileSpace << 6) | (tierFlag << 5) | profileIdc;
      desc[o++] = (compat >> 24) & 0xFF;
      desc[o++] = (compat >> 16) & 0xFF;
      desc[o++] = (compat >> 8) & 0xFF;
      desc[o++] = compat & 0xFF;
      desc[o++] = (constraint >> 40) & 0xFF;
      desc[o++] = (constraint >> 32) & 0xFF;
      desc[o++] = (constraint >> 24) & 0xFF;
      desc[o++] = (constraint >> 16) & 0xFF;
      desc[o++] = (constraint >> 8) & 0xFF;
      desc[o++] = constraint & 0xFF;
      desc[o++] = levelIdc;
      desc[o++] = 0xF0; // min_spatial_segmentation_idc = 0
      desc[o++] = 0x00; // parallelismType
      desc[o++] = 0xFC | 1; // chromaFormat = 1 (4:2:0)
      desc[o++] = 0xF8 | 0; // bitDepthLumaMinus8 = 0
      desc[o++] = 0xF8 | 0; // bitDepthChromaMinus8 = 0
      desc[o++] = 0x00; desc[o++] = 0x00; // avgFrameRate
      desc[o++] = 0x0F; // constantFrameRate(2)=0, numTemporalLayers(3)=1, temporalIdNested(1)=1, lengthSizeMinusOne(2)=3
      desc[o++] = arrays.length; // numOfArrays
      for (var a = 0; a < arrays.length; a++) {
        desc[o++] = (0x80) | (nalTypes[a] & 0x3F); // array_completeness=1, NAL_unit_type
        desc[o++] = 0x00; desc[o++] = 0x01; // numNalus = 1
        desc[o++] = (arrays[a].length >> 8) & 0xFF;
        desc[o++] = arrays[a].length & 0xFF;
        desc.set(arrays[a], o); o += arrays[a].length;
      }
      return desc;
    }
    return null;
  }

  function codecString() {
    if (currentCodec === "avc") {
      var si = findNalByType(7);
      if (si < 0) return null;
      var sps = nalData(currentData.nalus[si]);
      function hx(v) { var s = v.toString(16); return (s.length < 2 ? "0" : "") + s; }
      return "avc1." + hx(sps[1]) + hx(sps[2]) + hx(sps[3]);
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
      return "hev1." + spaceChar + profileIdc + "." + compat.toString(16).toUpperCase() + ".L" + level + ".B0";
    }
    return null;
  }

  function extractAccessUnits(keyIdx, targetIdx) {
    var units = [];
    var prefix = []; // 当前 AU 的前缀 NAL（SEI/AUD）
    for (var i = keyIdx; i <= targetIdx; i++) {
      var n = currentData.nalus[i];
      if (isVclNal(n.type)) {
        // 组装一个 AU：前缀 + 当前 VCL
        var nals = prefix.concat([i]);
        var datas = [];
        var len = 0;
        for (var k = 0; k < nals.length; k++) {
          var nd = removeEmulationPrevention(nalData(currentData.nalus[nals[k]]));
          datas.push(nd);
          len += 4 + nd.length;
        }
        var data = new Uint8Array(len);
        var o = 0;
        for (var k2 = 0; k2 < datas.length; k2++) {
          var nd = datas[k2];
          data[o++] = (nd.length >> 24) & 0xFF;
          data[o++] = (nd.length >> 16) & 0xFF;
          data[o++] = (nd.length >> 8) & 0xFF;
          data[o++] = nd.length & 0xFF;
          data.set(nd, o); o += nd.length;
        }
        units.push({ data: data, poc: n.slicePoc, isKey: isKeyNal(n.type) });
        prefix = [];
      } else if (isPrefixNal(n.type)) {
        prefix.push(i);
      }
    }
    return units;
  }

  function drawVideoFrame(frame) {
    var c = previewCanvas;
    var w = frame.displayWidth, h = frame.displayHeight;
    if (w <= 0 || h <= 0) { w = frame.codedWidth; h = frame.codedHeight; }
    var scale = Math.min(1, 480 / w);
    c.width = Math.round(w * scale);
    c.height = Math.round(h * scale);
    var ctx = c.getContext("2d");
    ctx.drawImage(frame, 0, 0, c.width, c.height);
  }

  function previewFrame(sliceIndex) {
    if (!fileBytes || !currentData || !timeline._slices) return;
    if (currentCodec === "vvc") {
      previewMsg.textContent = "H.266 (VVC) 暂无法在浏览器中解码";
      return;
    }
    if (!("VideoDecoder" in window)) {
      previewMsg.textContent = "当前浏览器不支持 WebCodecs";
      return;
    }
    var target = timeline._slices[sliceIndex];
    var targetNalIndex = target.index;

    var keyIdx = -1;
    for (var i = targetNalIndex; i >= 0; i--)
      if (isKeyNal(currentData.nalus[i].type)) { keyIdx = i; break; }
    if (keyIdx < 0) { previewMsg.textContent = "未找到关键帧"; return; }

    var desc = buildDescription();
    if (!desc) { previewMsg.textContent = "缺少 SPS/PPS"; return; }

    var units = extractAccessUnits(keyIdx, targetNalIndex);
    if (!units.length) { previewMsg.textContent = "无解码数据"; return; }

    previewHint.textContent = "解码中…（POC " + target.poc + "）";
    previewMsg.textContent = "";

    var cs = codecString();
    VideoDecoder.isConfigSupported({ codec: cs, description: desc }).then(function (support) {
      if (!support.supported) {
        previewMsg.textContent = "当前浏览器不支持解码 " + cs;
        previewHint.textContent = "";
        return;
      }
      if (previewDecoder && previewDecoder.state !== "closed") previewDecoder.close();
      previewDecoder = new VideoDecoder({
        output: function (frame) {
          if (frame.timestamp === target.poc) {
            drawVideoFrame(frame);
            previewHint.textContent = "POC " + target.poc + "  " + frame.codedWidth + "×" + frame.codedHeight;
          }
          frame.close();
        },
        error: function (err) {
          previewMsg.textContent = "解码失败：" + err.message;
          previewHint.textContent = "";
        }
      });
      previewDecoder.configure({ codec: cs, description: desc, optimizeForLatency: true });
      var idx = 0;
      function feed() {
        if (idx >= units.length || previewDecoder.state !== "configured") return;
        var au = units[idx++];
        previewDecoder.decode(new EncodedVideoChunk({
          type: au.isKey ? "key" : "delta",
          timestamp: au.poc,
          data: au.data
        }));
        feed();
      }
      try { feed(); } catch (err) {
        previewMsg.textContent = "解码失败：" + err.message;
      }
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
  var tabHex = document.getElementById("tabHex");
  tabSyntax.addEventListener("click", function () {
    tabSyntax.classList.add("active"); tabHex.classList.remove("active");
    syntaxTree.classList.remove("hidden"); hexView.classList.add("hidden");
  });
  tabHex.addEventListener("click", function () {
    tabHex.classList.add("active"); tabSyntax.classList.remove("active");
    hexView.classList.remove("hidden"); syntaxTree.classList.add("hidden");
  });

  // ---------- 文件处理 ----------
  function handleFile(file) {
    if (!file) return;
    setStatus("正在解析 " + file.name + " …");
    fileNameEl.textContent = file.name + " (" + (file.size / 1024 / 1024).toFixed(2) + " MB)";

    var reader = new FileReader();
    reader.onload = function (e) {
      fileBytes = new Uint8Array(e.target.result);
      try {
        var t0 = performance.now();
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
        if (currentCodec === "avc" || currentCodec === "hevc") previewPanel.classList.remove("hidden");
        mainArea.classList.remove("hidden");
        bottomPanels.classList.remove("hidden");

        renderStats(currentData.streamInfo);
        renderNalTable();
        renderHdr(currentData.hdr);
        renderWarnings();
        renderTimeline();
        syntaxTree.innerHTML = "";
        syntaxTitle.textContent = "";

        if (currentData.nalus.length > 0) selectNal(0, false);

        setStatus("解析完成：" + currentCodec.toUpperCase() + " 共 " + currentData.nalus.length + " 个 NAL 单元，耗时 " + (t1 - t0).toFixed(0) + " ms");
      } catch (err) {
        setStatus("解析失败：" + err.message);
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
    fileNameEl.textContent = "未加载文件";
    fileInput.value = "";

    dropzone.classList.remove("hidden");
    statsBar.classList.add("hidden");
    statsBar.innerHTML = "";
    timelinePanel.classList.add("hidden");
    previewPanel.classList.add("hidden");
    if (previewDecoder && previewDecoder.state !== "closed") previewDecoder.close();
    previewDecoder = null;
    mainArea.classList.add("hidden");
    bottomPanels.classList.add("hidden");
    resetBtn.classList.add("hidden");

    nalSpacer.style.height = "0px";
    nalCount.textContent = "";
    nalRows.innerHTML = "";
    syntaxTree.innerHTML = "";
    syntaxTitle.textContent = "";
    hexView.innerHTML = "";
    hexView.classList.add("hidden");
    syntaxTree.classList.remove("hidden");
    tabSyntax.classList.add("active");
    tabHex.classList.remove("active");
    hdrInfo.innerHTML = "";
    warningBody.innerHTML = "";
    warningCount.textContent = "";

    setStatus("解析模块已就绪（H.264/H.265/H.266），请打开或拖入码流文件");
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
      setStatus("错误：未找到 WASM 模块 (hevc.js)");
      return;
    }
    createHevcModule().then(function (m) {
      Module = m;
      setStatus("解析模块已就绪（H.264/H.265/H.266），请打开或拖入码流文件");
    }).catch(function (err) {
      setStatus("加载 WASM 模块失败：" + err);
      console.error(err);
    });
  }

  boot();
})();
