// hdr.js — HDR / Warn 面板渲染
(function () {
  "use strict";

  var hdrEl = null;
  var warnFilterEl = null;
  var warnCountEl = null;
  var warnBodyEl = null;
  var hex8fn = null;

  function renderHdr(h) {
    hdrEl.innerHTML = "";
    function row(k, v) {
      if (v === undefined || v === null || v === "") return;
      var d = document.createElement("div");
      d.className = "kv-row";
      d.innerHTML = '<span class="k">' + k + '</span><span class="v">' + v + "</span>";
      hdrEl.appendChild(d);
    }
    function frac(n, d) { return n + "/" + d + " (" + (n / d).toFixed(4) + ")"; }
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
    if (h.hasHdrVivid && h.hdrVivid) {
      var hv = h.hdrVivid;
      row("HDR Vivid (CUVA 005.1)", "present (system_start_code=" + hv.systemStartCode + ", windows=" + hv.numWindows + ")");
      (hv.windows || []).forEach(function (w, wi) {
        var prefix = hv.numWindows > 1 ? "  win" + wi + " " : "  ";
        row(prefix + "maxRGB min/avg/var/max",
          frac(w.minimumMaxrgb, 4095) + "  " + frac(w.averageMaxrgb, 4095) + "  " +
          frac(w.varianceMaxrgb, 4095) + "  " + frac(w.maximumMaxrgb, 4095));
        (w.toneMapping || []).forEach(function (tm, ti) {
          var tp = prefix + "  TM" + ti + " target luminance";
          row(tp, frac(tm.targetedLuminance, 4095) + " (" + (tm.targetedLuminance / 4095 * 10000).toFixed(0) + " nit)");
          if (tm.baseEnableFlag) {
            row(prefix + "  TM" + ti + " base m_p/m_m/m_a/m_b/m_n",
              frac(tm.baseParamMP, 16383) + " " + frac(tm.baseParamMM, 10) + " " +
              frac(tm.baseParamMA, 1023) + " " + frac(tm.baseParamMB, 1023) + " " + frac(tm.baseParamMN, 10));
            row(prefix + "  TM" + ti + " base k1/k2/k3/delta",
              tm.baseParamK1 + "/" + tm.baseParamK2 + "/" + tm.baseParamK3 + "  delta=" +
              frac(tm.baseParamDelta, 127) + " mode=" + tm.baseParamDeltaEnableMode);
          }
          (tm.threeSpline || []).forEach(function (sp, si) {
            row(prefix + "  TM" + ti + " 3Spline" + si + " th_enable/d1/d2/str",
              frac(sp.thEnable, 4095) + " " + frac(sp.thDelta1, 1023) + " " +
              frac(sp.thDelta2, 1023) + " " + frac(sp.enableStrength, 255) + "  mode=" + sp.thMode);
          });
        });
        if (w.colorSaturationMappingFlag) {
          row(prefix + "  color saturation gain",
            (w.colorSaturationGain || []).map(function (g) { return frac(g, 128); }).join("  "));
        }
      });
    }
  }

  function renderWarnings(warnings) {
    var filter = parseInt(warnFilterEl.value, 10);
    var filtered = warnings.filter(function (w) { return filter === -1 || w.type === filter; });
    warnCountEl.textContent = "(" + filtered.length + ")";
    warnBodyEl.innerHTML = "";
    var frag = document.createDocumentFragment();
    filtered.forEach(function (w) {
      var tr = document.createElement("tr");
      var td0 = document.createElement("td");
      td0.className = "mono";
      td0.textContent = hex8fn(w.position) + " (" + w.position + ")";
      var td1 = document.createElement("td");
      td1.textContent = w.message;
      tr.appendChild(td0); tr.appendChild(td1);
      frag.appendChild(tr);
    });
    warnBodyEl.appendChild(frag);
  }

  window.HdrPanel = {
    init: function (hdr, warnFilter, warnCount, warnBody, h8) {
      hdrEl = hdr; warnFilterEl = warnFilter; warnCountEl = warnCount; warnBodyEl = warnBody; hex8fn = h8;
    },
    renderHdr: renderHdr,
    renderWarnings: renderWarnings
  };
})();
