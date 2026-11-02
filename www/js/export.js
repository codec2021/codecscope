// export.js — 导出 Markdown 分析报告
(function () {
  "use strict";

  var ctx = null;

  function dumpSyntaxText(node, lines, depth) {
    if (!node) return;
    var pad = "";
    for (var d = 0; d < depth; d++) pad += "  ";
    lines.push(pad + node.n);
    if (node.c) for (var c = 0; c < node.c.length; c++) dumpSyntaxText(node.c[c], lines, depth + 1);
  }

  function buildReport() {
    var data = ctx.data;
    if (!data) return null;
    var si = data.streamInfo;
    var hdr = data.hdr || {};
    var L = [];
    L.push("# CodecScope Analysis Report");
    L.push("");
    L.push("## File");
    L.push("- Name: " + ctx.fileName);
    L.push("- Codec: " + ctx.codecFullName() + " (" + ctx.codec + ")");
    L.push("- NAL units: " + si.nalus);
    L.push("- Slices: " + si.slices);
    L.push("- I/P/B: " + si.i + " / " + si.p + " / " + si.b);
    L.push("- Profile: " + si.profile + "  Level: " + si.level);
    var w = si.picWidth || hdr.picWidth;
    var h = si.picHeight || hdr.picHeight;
    if (w) L.push("- Resolution: " + w + " x " + h);
    if (si.fps && parseFloat(si.fps) > 0) L.push("- FPS: " + si.fps);
    if (si.bitDepth !== undefined) L.push("- Bit depth: " + si.bitDepth);
    if (si.chromaFormat !== undefined) L.push("- Chroma: " + ctx.chromaFormatName(si.chromaFormat));
    L.push("");

    L.push("## NAL Type Distribution");
    var dist = {};
    for (var i = 0; i < data.nalus.length; i++) {
      var tn = data.nalus[i].typeName;
      dist[tn] = (dist[tn] || 0) + 1;
    }
    Object.keys(dist).sort().forEach(function (t) { L.push("- " + t + ": " + dist[t]); });
    L.push("");

    L.push("## NAL Unit List");
    L.push("");
    L.push("| # | Offset | Length | Type | Frame | Info |");
    L.push("| --- | --- | --- | --- | --- | --- |");
    for (var j = 0; j < data.nalus.length; j++) {
      var n = data.nalus[j];
      var info = (n.info || "").replace(/\|/g, "\\|");
      var fr = (n.frameIdx !== undefined && n.frameIdx >= 0) ? String(n.frameIdx) : "";
      L.push("| " + j + " | " + ctx.hex8(n.offset) + " | " + n.length + " | " + n.typeName + " | " + fr + " | " + info + " |");
    }
    L.push("");

    var sel = ctx.selectedIndex;
    if (sel >= 0 && sel < data.nalus.length) {
      L.push("## Syntax Tree (NAL #" + sel + " " + data.nalus[sel].typeName + ")");
      L.push("```");
      var node = data.nalus[sel].jpegSyntax || ctx.fetchNalSyntax(sel);
      dumpSyntaxText(node, L, 0);
      L.push("```");
    }
    return L.join("\n");
  }

  function exportReport() {
    var text = buildReport();
    if (text == null) { ctx.setStatus("Nothing to export"); return; }
    var base = (ctx.fileName || "report").replace(/\s*\([\d.]+ MB\)$/, "").replace(/\.[^.]+$/, "");
    var blob = new Blob([text], { type: "text/markdown" });
    var url = URL.createObjectURL(blob);
    var a = document.createElement("a");
    a.href = url;
    a.download = base + ".md";
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
  }

  window.ReportExport = {
    init: function (c) { ctx = c; },
    run: exportReport
  };
})();
