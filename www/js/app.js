(function () {
  "use strict";

  var Module = null;
  var currentData = null; // 汇总 JSON
  var currentWarnings = [];

  var statusEl = document.getElementById("status");
  var fileInput = document.getElementById("fileInput");
  var openBtn = document.getElementById("openBtn");
  var dropzone = document.getElementById("dropzone");
  var fileNameEl = document.getElementById("fileName");
  var statsBar = document.getElementById("statsBar");
  var mainArea = document.getElementById("mainArea");
  var bottomPanels = document.getElementById("bottomPanels");

  var nalBody = document.getElementById("nalBody");
  var nalCount = document.getElementById("nalCount");
  var syntaxTree = document.getElementById("syntaxTree");
  var syntaxTitle = document.getElementById("syntaxTitle");
  var hdrInfo = document.getElementById("hdrInfo");
  var warningBody = document.getElementById("warningBody");
  var warningCount = document.getElementById("warningCount");
  var warningFilter = document.getElementById("warningFilter");

  function setStatus(msg) { statusEl.textContent = msg; }

  function hex8(v) {
    var s = v.toString(16);
    while (s.length < 8) s = "0" + s;
    return "0x" + s;
  }

  // ---------- WASM 封装 ----------
  function parseBuffer(bytes) {
    var ptr = Module._malloc(bytes.length);
    Module.HEAPU8.set(bytes, ptr);
    var outPtr = Module._hevc_parse(ptr, bytes.length);
    Module._free(ptr);
    var json = Module.UTF8ToString(outPtr);
    Module._hevc_free(outPtr);
    return JSON.parse(json);
  }

  function fetchNalSyntax(index) {
    var outPtr = Module._hevc_get_nal_syntax(index);
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
    stat("Profile", si.profile);
    stat("Level", si.level);
    stat("Tier", si.tier);
  }

  function renderNalTable(nalus) {
    nalBody.innerHTML = "";
    nalCount.textContent = "(" + nalus.length + ")";

    var frag = document.createDocumentFragment();
    nalus.forEach(function (n, idx) {
      var tr = document.createElement("tr");
      tr.dataset.index = idx;

      var td0 = document.createElement("td");
      td0.className = "mono";
      td0.textContent = hex8(n.offset);

      var td1 = document.createElement("td");
      td1.className = "mono";
      td1.textContent = n.length;

      var td2 = document.createElement("td");
      td2.className = "mono";
      td2.textContent = n.typeName;

      var td3 = document.createElement("td");
      td3.textContent = n.info;
      if (n.color) td3.style.color = n.color;

      tr.appendChild(td0);
      tr.appendChild(td1);
      tr.appendChild(td2);
      tr.appendChild(td3);
      frag.appendChild(tr);
    });
    nalBody.appendChild(frag);
  }

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
    var isLeaf = !hasChildren;

    var toggle = document.createElement("span");
    toggle.className = "toggle";
    if (hasChildren) {
      toggle.textContent = "\u25bc"; // ▼ 展开
    }
    row.appendChild(toggle);

    var label = document.createElement("span");
    label.className = "label" + (hasChildren ? " group" : "");
    label.textContent = node.n;
    row.appendChild(label);

    container.appendChild(row);

    if (hasChildren) {
      var childrenWrap = document.createElement("div");
      childrenWrap.className = "tree-children";
      node.c.forEach(function (child) {
        childrenWrap.appendChild(buildTree(child));
      });
      container.appendChild(childrenWrap);

      var collapsed = false;
      toggle.addEventListener("click", function () {
        collapsed = !collapsed;
        childrenWrap.style.display = collapsed ? "none" : "block";
        toggle.textContent = collapsed ? "\u25b6" : "\u25bc";
      });
    }

    return container;
  }

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
    row("Chroma loc (top)", h.chromaLocTop);
    row("Chroma loc (bottom)", h.chromaLocBottom);
    row("Full range", h.fullRange);
    if (h.hasCll) {
      row("Max CLL", h.maxCll);
      row("Avg CLL", h.avgCll);
    }
    if (h.hasMdi) {
      row("Mastering display", h.masteringDisplay);
    }
  }

  function renderWarnings(warnings) {
    var filter = parseInt(warningFilter.value, 10);
    var filtered = warnings.filter(function (w) {
      return filter === -1 || w.type === filter;
    });

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
      tr.appendChild(td0);
      tr.appendChild(td1);
      frag.appendChild(tr);
    });
    warningBody.appendChild(frag);
  }

  // ---------- 处理文件 ----------
  function handleFile(file) {
    if (!file) return;
    setStatus("正在解析 " + file.name + " …");
    fileNameEl.textContent = file.name + " (" + (file.size / 1024 / 1024).toFixed(2) + " MB)";

    var reader = new FileReader();
    reader.onload = function (e) {
      var bytes = new Uint8Array(e.target.result);
      try {
        var t0 = performance.now();
        currentData = parseBuffer(bytes);
        var t1 = performance.now();
        currentWarnings = currentData.warnings || [];

        dropzone.classList.add("hidden");
        statsBar.classList.remove("hidden");
        mainArea.classList.remove("hidden");
        bottomPanels.classList.remove("hidden");

        renderStats(currentData.streamInfo);
        renderNalTable(currentData.nalus);
        renderHdr(currentData.hdr);
        renderWarnings(currentWarnings);
        syntaxTree.innerHTML = "";
        syntaxTitle.textContent = "";

        if (currentData.nalus.length > 0) {
          selectNal(0);
        }

        setStatus("解析完成：共 " + currentData.nalus.length + " 个 NAL 单元，耗时 " + (t1 - t0).toFixed(0) + " ms");
      } catch (err) {
        setStatus("解析失败：" + err.message);
        console.error(err);
      }
    };
    reader.readAsArrayBuffer(file);
  }

  function selectNal(index) {
    if (!currentData) return;
    var nal = currentData.nalus[index];
    syntaxTitle.textContent = "#" + index + "  " + nal.typeName + "  @ " + hex8(nal.offset);

    var rows = nalBody.rows;
    for (var i = 0; i < rows.length; i++) {
      rows[i].classList.toggle("selected", i === index);
    }

    var node = fetchNalSyntax(index);
    renderSyntax(node);

    if (rows[index]) {
      rows[index].scrollIntoView({ block: "nearest" });
    }
  }

  nalBody.addEventListener("click", function (e) {
    var tr = e.target.closest("tr");
    if (tr) selectNal(parseInt(tr.dataset.index, 10));
  });

  warningFilter.addEventListener("change", function () {
    renderWarnings(currentWarnings);
  });

  openBtn.addEventListener("click", function () { fileInput.click(); });
  fileInput.addEventListener("change", function () { handleFile(fileInput.files[0]); });

  dropzone.addEventListener("click", function () { fileInput.click(); });
  dropzone.addEventListener("dragover", function (e) {
    e.preventDefault();
    dropzone.classList.add("dragover");
  });
  dropzone.addEventListener("dragleave", function () {
    dropzone.classList.remove("dragover");
  });
  dropzone.addEventListener("drop", function (e) {
    e.preventDefault();
    dropzone.classList.remove("dragover");
    if (e.dataTransfer.files.length) handleFile(e.dataTransfer.files[0]);
  });

  // ---------- 启动 ----------
  function boot() {
    if (typeof createHevcModule !== "function") {
      setStatus("错误：未找到 WASM 模块 (hevc.js)");
      return;
    }
    createHevcModule().then(function (m) {
      Module = m;
      setStatus("解析模块已就绪，请打开或拖入 .h265 文件");
    }).catch(function (err) {
      setStatus("加载 WASM 模块失败：" + err);
      console.error(err);
    });
  }

  boot();
})();
