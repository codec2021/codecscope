// hexview.js — Hex 视图 + 字节选择解释（imhex 风格：拖选显示 U8/U16/U32/F32/F64 等）
(function () {
  "use strict";

  var dumpEl = null;
  var inspEl = null;
  var hex8fn = null;
  var fileBytes = null;
  var selStart = -1, selEnd = -1, dragging = false;

  function hex8(v) { return hex8fn(v); }

  function interpret(bytes) {
    function u8(off) { return bytes[off]; }
    function num2(le) { return le ? u8(0) | (u8(1) << 8) : (u8(0) << 8) | u8(1); }
    function num4(le) { var v = 0; for (var i = 0; i < 4; i++) v = v * 256 + u8(le ? 3 - i : i); return v >>> 0; }
    function s4(le) { var v = num4(le); return v > 0x7fffffff ? v - 0x100000000 : v; }
    function f4(le) { var b = new Uint8Array(4); for (var i = 0; i < 4; i++) b[i] = u8(i); return new DataView(b.buffer).getFloat32(0, le); }
    function f8(le) { var b = new Uint8Array(8); for (var i = 0; i < 8; i++) b[i] = u8(i); return new DataView(b.buffer).getFloat64(0, le); }
    function u64(le) {
      if (typeof BigInt === "undefined") return "—";
      var v = 0n;
      for (var i = 0; i < 8; i++) v = (v << 8n) | BigInt(u8(le ? 7 - i : i));
      return v.toString();
    }
    function s64(le) {
      if (typeof BigInt === "undefined") return "—";
      var v = 0n;
      for (var i = 0; i < 8; i++) v = (v << 8n) | BigInt(u8(le ? 7 - i : i));
      var m = 1n << 63n;
      return (v >= m ? v - (1n << 64n) : v).toString();
    }
    var n = bytes.length;
    var out = [];
    out.push("U8", bytes.join(", "));
    out.push("I8", Array.prototype.map.call(bytes, function (b) { return b > 127 ? b - 256 : b; }).join(", "));
    if (n === 1) out.push("Bits", u8(0).toString(2).padStart(8, "0"));
    if (n >= 2) {
      out.push("U16 LE", num2(true));
      out.push("U16 BE", num2(false));
      out.push("I16 LE", num2(true) > 32767 ? num2(true) - 65536 : num2(true));
      out.push("I16 BE", num2(false) > 32767 ? num2(false) - 65536 : num2(false));
    }
    if (n >= 4) {
      out.push("U32 LE", num4(true));
      out.push("U32 BE", num4(false));
      out.push("I32 LE", s4(true));
      out.push("I32 BE", s4(false));
      out.push("F32 LE", f4(true));
      out.push("F32 BE", f4(false));
    }
    if (n >= 8) {
      out.push("U64 LE", u64(true));
      out.push("U64 BE", u64(false));
      out.push("I64 LE", s64(true));
      out.push("I64 BE", s64(false));
      out.push("F64 LE", f8(true));
      out.push("F64 BE", f8(false));
    }
    return out;
  }

  function inspector() {
    if (selStart < 0 || selEnd < 0) { inspEl.innerHTML = ""; return; }
    var s = Math.min(selStart, selEnd);
    var e = Math.max(selStart, selEnd);
    var n = e - s + 1;
    var bytes = [];
    for (var i = s; i <= e; i++) bytes.push(fileBytes[i]);
    var hexStr = "", asciiStr = "";
    for (var i = 0; i < n; i++) {
      var b = bytes[i];
      hexStr += (b < 16 ? "0" : "") + b.toString(16) + " ";
      asciiStr += (b >= 32 && b <= 126) ? String.fromCharCode(b) : ".";
    }
    var html = '<div class="hi-range">0x' + hex8(s) + " .. 0x" + hex8(e) + "  (" + n + " byte" + (n > 1 ? "s" : "") + ")</div>";
    html += '<div class="hi-row"><span class="hi-label">Hex</span><span class="hi-val">' + hexStr + "</span></div>";
    html += '<div class="hi-row"><span class="hi-label">ASCII</span><span class="hi-val">' + asciiStr + "</span></div>";
    var interp = interpret(bytes);
    for (var i = 0; i < interp.length; i += 2) {
      html += '<div class="hi-row"><span class="hi-label">' + interp[i] + '</span><span class="hi-val">' + interp[i + 1] + "</span></div>";
    }
    inspEl.innerHTML = html;
  }

  function render(bytes, nal) {
    fileBytes = bytes;
    selStart = selEnd = -1;
    inspEl.innerHTML = "";
    if (!fileBytes || nal.length <= 0) { dumpEl.textContent = "No data"; return; }
    var end = Math.min(nal.offset + nal.length, fileBytes.length);
    var out = "";
    for (var i = nal.offset; i < end; i += 16) {
      var lineHex = '<span class="hex-offset">' + hex8(i) + "</span>  ";
      var asciiStr = "";
      for (var j = 0; j < 16; j++) {
        if (i + j < end) {
          var b = fileBytes[i + j];
          lineHex += '<span class="hb" data-i="' + (i + j) + '">' + (b < 16 ? "0" : "") + b.toString(16) + "</span>";
          asciiStr += (b >= 32 && b <= 126) ? String.fromCharCode(b) : ".";
        } else {
          lineHex += '<span class="hb hb-pad"></span>';
          asciiStr += " ";
        }
      }
      out += lineHex + ' <span class="hex-ascii">|' + asciiStr + "|</span>\n";
    }
    dumpEl.innerHTML = out;
  }

  function updateSelection() {
    var s = Math.min(selStart, selEnd);
    var e = Math.max(selStart, selEnd);
    var els = dumpEl.querySelectorAll(".hb");
    for (var i = 0; i < els.length; i++) {
      var off = parseInt(els[i].dataset.i, 10);
      els[i].classList.toggle("sel", off >= s && off <= e);
    }
  }

  function dumpByte(e) {
    var t = e.target;
    while (t && t !== dumpEl && !(t.classList && t.classList.contains("hb"))) t = t.parentNode;
    if (!t || t === dumpEl) return -1;
    return parseInt(t.dataset.i, 10);
  }

  function bind() {
    dumpEl.addEventListener("mousedown", function (e) {
      var off = dumpByte(e);
      if (off < 0) return;
      dragging = true;
      selStart = selEnd = off;
      updateSelection();
      inspector();
      e.preventDefault();
    });
    dumpEl.addEventListener("mousemove", function (e) {
      if (!dragging) return;
      var off = dumpByte(e);
      if (off < 0) return;
      selEnd = off;
      updateSelection();
      inspector();
    });
    document.addEventListener("mouseup", function () {
      if (dragging) {
        dragging = false;
        inspector();
      }
    });
  }

  function clear() {
    dumpEl.innerHTML = "";
    inspEl.innerHTML = "";
  }

  window.HexView = {
    init: function (dump, insp, h8) { dumpEl = dump; inspEl = insp; hex8fn = h8; },
    render: render,
    bind: bind,
    clear: clear
  };
})();
