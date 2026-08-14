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

  // 帧时间轴（Slice 可视化）
  function renderTimeline() {
    var slices = [];
    currentData.nalus.forEach(function (n, i) {
      if (n.sliceType >= 0) slices.push({ index: i, type: n.sliceType });
    });
    if (slices.length === 0) return;

    var dpr = window.devicePixelRatio || 1;
    var w = timeline.clientWidth;
    var h = timeline.clientHeight;
    timeline.width = w * dpr;
    timeline.height = h * dpr;
    var ctx = timeline.getContext("2d");
    ctx.scale(dpr, dpr);
    ctx.clearRect(0, 0, w, h);

    var barW = w / slices.length;
    var colors = { 2: "#CD9B1D", 0: "#0066ff", 1: "#FF83FA" };
    for (var i = 0; i < slices.length; i++) {
      var s = slices[i];
      ctx.fillStyle = colors[s.type] || "#888";
      ctx.fillRect(i * barW, 0, Math.max(1, barW - 0.5), h);
    }
    timeline._slices = slices;
    timeline._barW = barW;

    var legend = document.getElementById("timelineLegend");
    legend.innerHTML = '<b style="color:#CD9B1D">I</b> <b style="color:#0066ff">P</b> <b style="color:#FF83FA">B</b>';
  }

  timeline.addEventListener("click", function (e) {
    if (!timeline._slices) return;
    var rect = timeline.getBoundingClientRect();
    var x = e.clientX - rect.left;
    var idx = Math.floor(x / timeline._barW);
    if (idx >= 0 && idx < timeline._slices.length) {
      selectNal(timeline._slices[idx].index, true);
    }
  });

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
        statsBar.classList.remove("hidden");
        timelinePanel.classList.remove("hidden");
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
