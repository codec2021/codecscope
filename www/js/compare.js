// compare.js — 多文件对比
(function () {
  "use strict";

  var ctx = null;

  function codecNameOf(codec) {
    if (codec === "avc") return "H.264 / AVC";
    if (codec === "hevc") return "H.265 / HEVC";
    if (codec === "vvc") return "H.266 / VVC";
    if (codec === "av1") return "AV1";
    if (codec === "vp9") return "VP9";
    return codec || "?";
  }

  function parseForCompare(rawBytes) {
    var fileBytes = null, hintCodec = null, containerInfo = null;
    if (H26xDemux.isMp4(rawBytes)) {
      var m = H26xDemux.demuxMp4(rawBytes);
      if (!m || !m.annexb || !m.annexb.length) return null;
      fileBytes = m.annexb; hintCodec = m.codec;
      containerInfo = H26xDemux.parseContainerInfo(rawBytes);
    } else if (H26xDemux.isTs(rawBytes)) {
      var t = H26xDemux.demuxTs(rawBytes);
      if (!t || !t.annexb || !t.annexb.length) return null;
      fileBytes = t.annexb; hintCodec = t.codec;
      containerInfo = H26xDemux.parseTsInfo(rawBytes);
    } else if (H26xDemux.isFlv(rawBytes)) {
      var f = H26xDemux.demuxFlv(rawBytes);
      if (!f || !f.annexb || !f.annexb.length) return null;
      fileBytes = f.annexb; hintCodec = f.codec;
    } else if (H26xDemux.isWebm(rawBytes) || H26xDemux.isIvf(rawBytes) || H26xDemux.isAv1AnnexB(rawBytes)) {
      var av = H26xDemux.isWebm(rawBytes) ? H26xDemux.demuxWebm(rawBytes) : H26xDemux.parseIvf(rawBytes);
      if (!av || !av.frames || !av.frames.length) return null;
      var units = av.units || av.obus || [];
      return {
        codec: av.codec,
        data: {
          streamInfo: { nalus: units.length, slices: av.frames.length, i: av.frames.length, p: 0, b: 0, profile: (av.codec === "vp9" ? "VP9" : "AV1"), level: "-", picWidth: av.width, picHeight: av.height },
          nalus: units.map(function (u) { return { type: u.type, typeName: u.typeName }; }),
          hdr: {}
        },
        containerInfo: null
      };
    } else {
      fileBytes = rawBytes;
    }
    if (!fileBytes || fileBytes.length === 0) return null;
    try {
      var r = ctx.parseBuffer(fileBytes, hintCodec);
      return { codec: r.codec, data: r.data, containerInfo: containerInfo };
    } catch (e) { return null; }
  }

  function nalDist(data) {
    var dist = {};
    for (var i = 0; i < data.nalus.length; i++) {
      var t = data.nalus[i].typeName || ("type" + data.nalus[i].type);
      dist[t] = (dist[t] || 0) + 1;
    }
    return dist;
  }
  function escHtml(s) { return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;"); }
  function compareRow(label, a, b) {
    var same = String(a) === String(b);
    var cls = same ? "same" : "diff";
    return '<tr><th>' + escHtml(label) + '</th><td class="' + cls + '">' + escHtml(a) + '</td><td class="' + cls + '">' + escHtml(b) + '</td></tr>';
  }

  function showCompare(b) {
    var a = ctx.data;
    var siA = a.streamInfo, siB = b.data.streamInfo;
    var wA = siA.picWidth || (a.hdr && a.hdr.picWidth);
    var hA = siA.picHeight || (a.hdr && a.hdr.picHeight);
    var wB = siB.picWidth || (b.data.hdr && b.data.hdr.picWidth);
    var hB = siB.picHeight || (b.data.hdr && b.data.hdr.picHeight);
    var html = '<table class="compare-table">';
    html += '<tr><th></th><th>File A</th><th>File B</th></tr>';
    html += compareRow("Codec", codecNameOf(ctx.codec), codecNameOf(b.codec));
    html += compareRow("NAL units", siA.nalus, siB.nalus);
    html += compareRow("Slices", siA.slices, siB.slices);
    html += compareRow("I / P / B", siA.i + "/" + siA.p + "/" + siA.b, siB.i + "/" + siB.p + "/" + siB.b);
    html += compareRow("Profile", siA.profile, siB.profile);
    html += compareRow("Level", siA.level, siB.level);
    html += compareRow("Resolution", (wA ? wA + "x" + hA : "?"), (wB ? wB + "x" + hB : "?"));
    html += compareRow("FPS", siA.fps, siB.fps);
    html += compareRow("Bit depth", siA.bitDepth, siB.bitDepth);
    html += compareRow("Chroma", siA.chromaFormat !== undefined ? ctx.chromaFormatName(siA.chromaFormat) : "?", siB.chromaFormat !== undefined ? ctx.chromaFormatName(siB.chromaFormat) : "?");
    html += '</table>';

    var distA = nalDist(a), distB = nalDist(b.data);
    var typeKeys = {};
    Object.keys(distA).forEach(function (t) { typeKeys[t] = 1; });
    Object.keys(distB).forEach(function (t) { typeKeys[t] = 1; });
    var keys = Object.keys(typeKeys).sort();
    html += '<h4 style="margin:14px 0 6px">NAL Type Distribution</h4>';
    html += '<table class="compare-table">';
    html += '<tr><th>NAL Type</th><th>File A</th><th>File B</th></tr>';
    keys.forEach(function (t) { html += compareRow(t, distA[t] || 0, distB[t] || 0); });
    html += '</table>';

    ctx.compareBody.innerHTML = html;
    ctx.compareModal.classList.remove("hidden");
  }

  function doCompare(file) {
    var reader = new FileReader();
    reader.onload = function (e) {
      var b = parseForCompare(new Uint8Array(e.target.result));
      if (!b) { ctx.setStatus("Compare: cannot parse second file"); return; }
      showCompare(b);
    };
    reader.readAsArrayBuffer(file);
  }

  window.FileCompare = {
    init: function (c) { ctx = c; },
    doCompare: doCompare
  };
})();
