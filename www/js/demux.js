(function () {
  "use strict";

  function readU32(d, o) { return ((d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]) >>> 0; }
  function readU16(d, o) { return (d[o] << 8) | d[o + 1]; }
  function fourCC(d, o) { return String.fromCharCode(d[o], d[o + 1], d[o + 2], d[o + 3]); }
  function isMp4(d) { return d.length >= 12 && fourCC(d, 4) === "ftyp"; }
  function isIvf(d) { return d.length >= 32 && d[0] === 0x44 && d[1] === 0x4B && d[2] === 0x49 && d[3] === 0x46; } // "DKIF"
  function isAv1AnnexB(d) {
    if (d.length < 2) return false;
    var t = (d[0] >> 3) & 0xF;
    return t === 2 || t === 1;
  }
  function detectImageType(d) {
    // 检测浏览器可原生解码的图片格式，返回 MIME 类型或 null
    if (d.length >= 8 && d[0] === 0x89 && d[1] === 0x50 && d[2] === 0x4E && d[3] === 0x47 &&
        d[4] === 0x0D && d[5] === 0x0A && d[6] === 0x1A && d[7] === 0x0A) return "image/png";
    if (d.length >= 3 && d[0] === 0xFF && d[1] === 0xD8 && d[2] === 0xFF) return "image/jpeg";
    if (d.length >= 6 && d[0] === 0x47 && d[1] === 0x49 && d[2] === 0x46 && d[3] === 0x38 &&
        (d[4] === 0x37 || d[4] === 0x39) && d[5] === 0x61) return "image/gif";
    if (d.length >= 12 && d[0] === 0x52 && d[1] === 0x49 && d[2] === 0x46 && d[3] === 0x46 &&
        d[8] === 0x57 && d[9] === 0x45 && d[10] === 0x42 && d[11] === 0x50) return "image/webp";
    if (d.length >= 2 && d[0] === 0x42 && d[1] === 0x4D) return "image/bmp";
    return null;
  }

  // ---------- AV1 OBU 解析 ----------
  var AV1_OBU_NAMES = {
    1: "OBU_SEQUENCE_HEADER", 2: "OBU_TEMPORAL_DELIMITER", 3: "OBU_FRAME_HEADER",
    4: "OBU_TILE_GROUP", 5: "OBU_METADATA", 6: "OBU_FRAME", 7: "OBU_REDUNDANT_FRAME_HEADER",
    8: "OBU_TILE_LIST", 15: "OBU_PADDING"
  };
  var AV1_OBU_COLORS = { 1: "#FF8888", 3: "#CD9B1D", 6: "#CD9B1D", 4: "#00B050", 2: "#888" };

  function readLeb128(d, pos) {
    var val = 0, shift = 0, b;
    do {
      b = d[pos++];
      val |= (b & 0x7F) << shift;
      shift += 7;
    } while (b & 0x80);
    return { value: val, pos: pos };
  }

  function parseAv1SeqHdr(d, start, end) {
    var bitPos = start * 8, bitEnd = end * 8;
    function readBit() { if (bitPos >= bitEnd) return 0; var b = (d[bitPos >> 3] >> (7 - (bitPos & 7))) & 1; bitPos++; return b; }
    function readBits(n) { var v = 0; for (var i = 0; i < n; i++) v = (v << 1) | readBit(); return v; }
    function uvlc() { var z = 0; while (readBit() === 0) z++; if (z >= 32) return 0; var v = 0; for (var i = 0; i < z; i++) v = (v << 1) | readBit(); return (1 << z) - 1 + v; }

    var tree = [];
    var profile = readBits(3);
    tree.push({ n: "seq_profile = " + profile });
    tree.push({ n: "still_picture = " + readBit() });
    var reduced = readBit();
    tree.push({ n: "reduced_still_picture_header = " + reduced });
    var levelIdx = 0;
    if (!reduced) {
      var timing = readBit();
      tree.push({ n: "timing_info_present_flag = " + timing });
      if (timing) { readBits(32); readBits(32); if (readBit()) uvlc(); }
      var initial = readBit();
      if (initial) readBits(4);
      var opCnt = readBits(5);
      for (var i = 0; i <= opCnt; i++) {
        readBits(12);
        levelIdx = readBits(5);
        if (levelIdx > 7) readBits(1);
        if (initial) readBits(4);
      }
      tree.push({ n: "operating_points = " + (opCnt + 1) + ", seq_level_idx = " + levelIdx });
    }
    var fwBits = readBits(4) + 1;
    var fhBits = readBits(4) + 1;
    var maxW = readBits(fwBits) + 1;
    var maxH = readBits(fhBits) + 1;
    tree.push({ n: "max_frame_width = " + maxW });
    tree.push({ n: "max_frame_height = " + maxH });
    return { tree: tree, profile: profile, level: levelIdx, width: maxW, height: maxH };
  }

  function parseAv1Obus(d, start, end) {
    var obus = [];
    var pos = start;
    while (pos + 1 <= end) {
      var obuStart = pos;
      var hdr = d[pos];
      var forbidden = (hdr >> 7) & 1;
      var type = (hdr >> 3) & 0xF;
      var extension = (hdr >> 2) & 1;
      var hasSize = (hdr >> 1) & 1;
      pos++;
      if (extension) pos++;
      var size = 0;
      if (hasSize) { var lb = readLeb128(d, pos); pos = lb.pos; size = lb.value; }
      else size = end - pos;
      var payloadStart = pos;
      var payloadEnd = Math.min(payloadStart + size, end);
      if (type === 0 || forbidden) { pos = payloadEnd; continue; } // reserved

      var name = AV1_OBU_NAMES[type] || ("OBU_" + type);
      var syntax = { n: name };
      if (type === 1) {
        var sh = parseAv1SeqHdr(d, payloadStart, payloadEnd);
        syntax.c = sh.tree;
      }
      obus.push({ offset: obuStart, length: payloadEnd - obuStart, type: type, typeName: name, info: name, color: AV1_OBU_COLORS[type] || "#4d94e8", syntax: syntax, payloadStart: payloadStart, payloadEnd: payloadEnd });
      pos = payloadEnd;
    }
    return obus;
  }

  function parseIvf(d) {
    if (!isIvf(d)) return null;
    var hdrSize = d[6] | (d[7] << 8);
    var width = d[12] | (d[13] << 8);
    var height = d[14] | (d[15] << 8);
    var fourcc = fourCC(d, 8);
    if (fourcc !== "AV01" && fourcc !== "AV02") return null;

    var frames = [];
    var obus = [];
    var pos = hdrSize;
    while (pos + 12 <= d.length) {
      var sz = (d[pos] | (d[pos + 1] << 8) | (d[pos + 2] << 16) | (d[pos + 3] << 24)) >>> 0;
      pos += 12; // size(4) + timestamp(8)
      if (sz <= 0 || pos + sz > d.length) break;
      frames.push({ data: d.subarray(pos, pos + sz) });
      var obuList = parseAv1Obus(d, pos, pos + sz);
      for (var i = 0; i < obuList.length; i++) obus.push(obuList[i]);
      pos += sz;
    }

    // 从 sequence header 提取分辨率
    var picW = width, picH = height, profile = 0, level = 0;
    for (var k = 0; k < obus.length; k++) {
      if (obus[k].type === 1) {
        var sh = parseAv1SeqHdr(d, obus[k].payloadStart, obus[k].payloadEnd);
        if (sh.width) { picW = sh.width; picH = sh.height; }
        profile = sh.profile; level = sh.level;
        break;
      }
    }

    return { codec: "av1", frames: frames, obus: obus, width: picW, height: picH, profile: profile, level: level };
  }
  function boxSize(d, o) {
    var size = readU32(d, o);
    if (size === 1) {
      if (o + 16 > d.length) return NaN;
      return readU32(d, o + 8) * 4294967296 + readU32(d, o + 12); // 64-bit largesize
    }
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

    // fMP4 支持：若 moov/stbl 样本表为空（样本在 moof+mdat 片段里），解析 fragment 追加
    if (result && result.codec) {
      var fragNals = parseFragmentNals(d);
      if (fragNals.length > 0) {
        var newTotal = result.annexb.length;
        for (var f = 0; f < fragNals.length; f++) newTotal += fragNals[f].length + 4;
        var newOut = new Uint8Array(newTotal);
        newOut.set(result.annexb, 0);
        var fw = result.annexb.length;
        for (var f2 = 0; f2 < fragNals.length; f2++) {
          newOut[fw] = 0; newOut[fw + 1] = 0; newOut[fw + 2] = 0; newOut[fw + 3] = 1;
          fw += 4;
          newOut.set(fragNals[f2], fw);
          fw += fragNals[f2].length;
        }
        result.annexb = newOut;
      }
    }
    return result;
  }

  function parseStbl(d, start, end) {
    var stsd = findBox(d, start, end, "stsd");
    var stsz = findBox(d, start, end, "stsz");
    var stsc = findBox(d, start, end, "stsc");
    var stco = findBox(d, start, end, "stco") || findBox(d, start, end, "co64");

    if (!stsd || !stsz || !stco) return null;

    var codec = null;
    var paramNals = [];
    var description = null;
    var nalLengthSize = 4;
    var sdEnd = stsd.offset + stsd.size;
    var p = stsd.offset + 16;
    if (p + 8 <= sdEnd) {
      var fmt = fourCC(d, p + 4);
      if (fmt === "avc1" || fmt === "avc3") codec = "avc";
      if (fmt === "hev1" || fmt === "hvc1") codec = "hevc";
      if (fmt === "vvc1" || fmt === "vvi1") codec = "vvc";
      var cfg = findFourCC(d, stsd.offset + 8, sdEnd, "avcC") || findFourCC(d, stsd.offset + 8, sdEnd, "hvcC") || findFourCC(d, stsd.offset + 8, sdEnd, "vvcC");
      if (cfg) {
        var c = cfg.offset + 8;
        var cEnd = cfg.offset + cfg.size;
        description = d.subarray(c, cEnd);
        if (codec === "avc") {
          nalLengthSize = 1 + (description[4] & 0x03);
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
          nalLengthSize = 1 + (description[21] & 0x03);
          var na = d[c + 22];
          var np2 = c + 23;
          for (var q2 = 0; q2 < na; q2++) {
            var ntype = d[np2] & 0x3F;
            var numNalus = readU16(d, np2 + 1);
            np2 += 3;
            for (var q3 = 0; q3 < numNalus; q3++) {
              var nlen = readU16(d, np2);
              if (ntype === 32 || ntype === 33 || ntype === 34)
                paramNals.push(d.subarray(np2 + 2, np2 + 2 + nlen));
              np2 += 2 + nlen;
            }
          }
        }
      }
    }
    if (!codec) return null;

    var sampleSizes = [];
    var uniformSize = readU32(d, stsz.offset + 12);
    var szCount = readU32(d, stsz.offset + 16);
    if (uniformSize > 0) {
      for (var s1 = 0; s1 < szCount; s1++) sampleSizes.push(uniformSize);
    } else {
      for (var s2 = 0; s2 < szCount; s2++) sampleSizes.push(readU32(d, stsz.offset + 20 + s2 * 4));
    }

    var chunkOffsets = [];
    var coCount = readU32(d, stco.offset + 12);
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

    var stscEntries = [];
    if (stsc) {
      var scCount = readU32(d, stsc.offset + 12);
      for (var sc = 0; sc < scCount; sc++) {
        stscEntries.push({
          firstChunk: readU32(d, stsc.offset + 16 + sc * 12),
          samplesPerChunk: readU32(d, stsc.offset + 20 + sc * 12)
        });
      }
    } else {
      stscEntries.push({ firstChunk: 1, samplesPerChunk: 1 });
    }

    var sampleOffsets = [];
    var sampleIndex = 0;
    var chunkIndex = 0;
    var dataOffset = 0;
    while (sampleIndex < sampleSizes.length && chunkIndex < chunkOffsets.length) {
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
    return { codec: codec, annexb: out, description: description, nalLengthSize: nalLengthSize };
  }

  function demuxAv1Mp4(d) {
    var moov = null, i = 0;
    while (i + 8 <= d.length) {
      var msize = boxSize(d, i);
      if (msize < 8 || i + msize > d.length) break;
      if (fourCC(d, i + 4) === "moov") { moov = { offset: i + 8, end: i + msize }; break; }
      i += msize;
    }
    if (!moov) return null;

    var trakStart = moov.offset;
    var stsd = null, stsz = null, stsc = null, stco = null;
    while (trakStart + 8 <= moov.end && !stsd) {
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
              if (stbl) {
                stsd = findBox(d, stbl.offset + 8, stbl.offset + stbl.size, "stsd");
                stsz = findBox(d, stbl.offset + 8, stbl.offset + stbl.size, "stsz");
                stsc = findBox(d, stbl.offset + 8, stbl.offset + stbl.size, "stsc");
                stco = findBox(d, stbl.offset + 8, stbl.offset + stbl.size, "stco") || findBox(d, stbl.offset + 8, stbl.offset + stbl.size, "co64");
              }
            }
          }
        }
      }
      trakStart += trakSize;
    }
    if (!stsd || !stsz || !stco) return null;
    // 识别 av01
    var fmt = fourCC(d, stsd.offset + 20);
    if (fmt !== "av01") return null;

    var sampleSizes = [];
    var uniformSize = readU32(d, stsz.offset + 12);
    var szCount = readU32(d, stsz.offset + 16);
    if (uniformSize > 0) { for (var s1 = 0; s1 < szCount; s1++) sampleSizes.push(uniformSize); }
    else { for (var s2 = 0; s2 < szCount; s2++) sampleSizes.push(readU32(d, stsz.offset + 20 + s2 * 4)); }

    var chunkOffsets = [];
    var coCount = readU32(d, stco.offset + 12);
    var co64 = fourCC(d, stco.offset + 4) === "co64";
    for (var c1 = 0; c1 < coCount; c1++) {
      if (co64) chunkOffsets.push(readU32(d, stco.offset + 16 + c1 * 8) * 4294967296 + readU32(d, stco.offset + 20 + c1 * 8));
      else chunkOffsets.push(readU32(d, stco.offset + 16 + c1 * 4));
    }

    var stscEntries = [];
    if (stsc) {
      var scCount = readU32(d, stsc.offset + 12);
      for (var sc = 0; sc < scCount; sc++) stscEntries.push({ firstChunk: readU32(d, stsc.offset + 16 + sc * 12), samplesPerChunk: readU32(d, stsc.offset + 20 + sc * 12) });
    } else stscEntries.push({ firstChunk: 1, samplesPerChunk: 1 });

    var sampleOffsets = [];
    var sampleIndex = 0, chunkIndex = 0, dataOffset = 0;
    while (sampleIndex < sampleSizes.length && chunkIndex < chunkOffsets.length) {
      var spc = 1;
      for (var e = 0; e < stscEntries.length; e++) if (stscEntries[e].firstChunk <= chunkIndex + 1) spc = stscEntries[e].samplesPerChunk;
      for (var k = 0; k < spc && sampleIndex < sampleSizes.length; k++) { sampleOffsets.push(chunkOffsets[chunkIndex] + dataOffset); dataOffset += sampleSizes[sampleIndex]; sampleIndex++; }
      chunkIndex++; dataOffset = 0;
    }

    var frames = [], obus = [];
    for (var si2 = 0; si2 < sampleOffsets.length; si2++) {
      var off = sampleOffsets[si2], len = sampleSizes[si2];
      if (off < 0 || off + len > d.length) continue;
      var sample = d.subarray(off, off + len);
      frames.push({ data: sample });
      var obuList = parseAv1Obus(d, off, off + len);
      for (var oi = 0; oi < obuList.length; oi++) obus.push(obuList[oi]);
    }

    var picW = 0, picH = 0, profile = 0, level = 0;
    for (var k2 = 0; k2 < obus.length; k2++) {
      if (obus[k2].type === 1) {
        var sh = parseAv1SeqHdr(d, obus[k2].payloadStart, obus[k2].payloadEnd);
        if (sh.width) { picW = sh.width; picH = sh.height; }
        profile = sh.profile; level = sh.level;
        break;
      }
    }

    return { codec: "av1", frames: frames, obus: obus, width: picW, height: picH, profile: profile, level: level };
  }

  // ---------- WebM/Matroska (EBML) ----------
  function isWebm(d) { return d.length >= 4 && d[0] === 0x1A && d[1] === 0x45 && d[2] === 0xDF && d[3] === 0xA3; }

  function readVint(d, pos) {
    var b = d[pos], mask = 0x80, len = 0;
    while (mask && !(b & mask)) { len++; mask >>= 1; }
    len++;
    var value = b & (mask - 1);
    for (var i = 1; i < len; i++) value = value * 256 + d[pos + i];
    return { value: value, len: len, end: pos + len };
  }

  function demuxWebm(d) {
    if (!isWebm(d)) return null;
    var pos = 0;
    var el = readVint(d, pos);                     // EBML id
    var sz = readVint(d, el.end);                  // EBML size
    pos = sz.end + sz.value;

    var width = 0, height = 0, isAv1 = false, codecId = "";
    var frames = [], obus = [];

    // 读 Segment
    if (pos + 2 > d.length) return null;
    el = readVint(d, pos);                          // Segment id
    sz = readVint(d, el.end);                       // Segment size
    var segStart = sz.end;
    var segEnd = segStart + sz.value;
    if (segEnd > d.length) segEnd = d.length;
    pos = segStart;

    while (pos < segEnd) {
      el = readVint(d, pos); var id = el.value;
      sz = readVint(d, el.end);
      var elemStart = sz.end;
      var elemEnd = elemStart + sz.value;
      if (sz.value < 0 || elemEnd > segEnd) break;
      var dataEnd = elemEnd;

      if (id === 0x654AE6B) { // Tracks
        var tp = elemStart;
        while (tp + 2 <= dataEnd) {
          var te = readVint(d, tp); var tid = te.value; tp = te.end;
          var tsz = readVint(d, tp); tp = tsz.end;
          if (tsz.value < 0 || tp + tsz.value > dataEnd) break;
          var tEnd = tp + tsz.value;
          if (tid === 0x2E) { // TrackEntry
            codecId = ""; width = 0; height = 0;
            var cp = tp;
            while (cp + 2 <= tEnd) {
              var ce = readVint(d, cp); var cid = ce.value; cp = ce.end;
              var csz = readVint(d, cp); cp = csz.end;
              if (csz.value < 0 || cp + csz.value > tEnd) break;
              if (cid === 0x06) codecId = String.fromCharCode.apply(null, Array.prototype.slice.call(d.subarray(cp, cp + csz.value)));
              else if (cid === 0x60) { // Video
                var vp = cp, vend = cp + csz.value;
                while (vp + 2 <= vend) {
                  var ve = readVint(d, vp); var vid = ve.value; vp = ve.end;
                  var vsz = readVint(d, vp); vp = vsz.end;
                  if (vsz.value < 0) break;
                  if (vid === 0x30) width = (d[vp] << 8) | d[vp + 1];
                  else if (vid === 0x3A) height = (d[vp] << 8) | d[vp + 1];
                  vp += vsz.value;
                }
              }
              cp += csz.value;
            }
            if (codecId === "V_AV1") isAv1 = true;
          }
          tp = tEnd;
        }
      }
      else if (id === 0xF43B675 && isAv1) { // Cluster
        var cp = elemStart;
        while (cp + 2 <= dataEnd) {
          var ce = readVint(d, cp); var cid = ce.value;
          var csz = readVint(d, ce.end);
          var elemStart = csz.end;
          var elemEnd = elemStart + csz.value;
          if (csz.value < 0 || elemEnd > dataEnd) break;
          if (cid === 0x23) { // SimpleBlock
            var tn = readVint(d, elemStart);
            var fp = tn.end + 2 + 1;  // track number + timestamp(2) + flags(1)
            var frameLen = csz.value - (fp - elemStart);
            if (frameLen > 0 && fp + frameLen <= dataEnd) {
              frames.push({ data: d.subarray(fp, fp + frameLen) });
            }
          }
          cp = elemEnd;
        }
      }
      pos = dataEnd;
    }

    if (!isAv1 || frames.length === 0) return null;

    for (var f = 0; f < frames.length; f++) {
      var fd = frames[f].data;
      var absOff = -1;
      for (var a = 0; a + 8 <= d.length; a++) {
        var m = true;
        for (var b2 = 0; b2 < Math.min(8, fd.length); b2++) { if (d[a + b2] !== fd[b2]) { m = false; break; } }
        if (m) { absOff = a; break; }
      }
      if (absOff >= 0) {
        var obuList = parseAv1Obus(d, absOff, absOff + fd.length);
        for (var oi = 0; oi < obuList.length; oi++) obus.push(obuList[oi]);
      }
    }

    var picW = width, picH = height, profile = 0, level = 0;
    for (var k = 0; k < obus.length; k++) {
      if (obus[k].type === 1) {
        var sh = parseAv1SeqHdr(d, obus[k].payloadStart, obus[k].payloadEnd);
        if (sh.width) { picW = sh.width; picH = sh.height; }
        profile = sh.profile; level = sh.level;
        break;
      }
    }

    return { codec: "av1", frames: frames, obus: obus, width: picW, height: picH, profile: profile, level: level };
  }

  function parseFragmentNals(d) {
    // 解析 fragmented MP4 的 moof/traf/trun，提取 length-prefixed NAL（不含 start code）
    var nals = [];
    var i = 0;
    while (i + 8 <= d.length) {
      var moofSize = boxSize(d, i);
      if (moofSize < 8 || i + moofSize > d.length) break;
      if (fourCC(d, i + 4) !== "moof") { i += moofSize; continue; }

      var moofEnd = i + moofSize;
      var p = i + 8;
      var baseDataOffset = -1;      // tfhd 里的绝对 base offset（-1 未设置）
      var defaultSampleSize = 0;    // tfhd 的 default_sample_size
      var runningRel = 0;           // 无 data_offset 时的累积相对偏移（相对 base）

      while (p + 8 <= moofEnd) {
        var bsz = boxSize(d, p);
        if (bsz < 8 || p + bsz > moofEnd) break;
        if (fourCC(d, p + 4) !== "traf") { p += bsz; continue; }

        var trafEnd = p + bsz;
        var q = p + 8;
        while (q + 8 <= trafEnd) {
          var tsz = boxSize(d, q);
          if (tsz < 8 || q + tsz > trafEnd) break;
          var tt = fourCC(d, q + 4);

          if (tt === "tfhd") {
            var hflags = (d[q + 9] << 16) | (d[q + 10] << 8) | d[q + 11];
            var r = q + 12;
            r += 4; // track_ID
            if (hflags & 1) { baseDataOffset = readU32(d, r) * 4294967296 + readU32(d, r + 4); r += 8; }
            else if (hflags & 2) r += 4;
            if (hflags & 8) r += 4;      // default_sample_duration
            if (hflags & 0x10) { defaultSampleSize = readU32(d, r); r += 4; }
            if (hflags & 0x20) r += 4;   // default_sample_flags
          }
          else if (tt === "trun") {
            var tflags = (d[q + 9] << 16) | (d[q + 10] << 8) | d[q + 11];
            var r = q + 12;
            var sampleCount = readU32(d, r); r += 4;
            var dataOffset = 0;
            if (tflags & 1) { dataOffset = readU32(d, r); r += 4; }
            if (tflags & 4) r += 4;      // first_sample_flags

            // base：tfhd 的 base_data_offset 优先，否则 moof 起始
            var absBase = (baseDataOffset >= 0) ? baseDataOffset : i;
            // 相对 base 的起始偏移：有 data_offset 用 data_offset，否则紧接上一个 trun 末尾
            var relStart = (tflags & 1) ? dataOffset : runningRel;
            var sampleOffset = absBase + relStart;

            var totalSize = 0;
            for (var s = 0; s < sampleCount; s++) {
              if (tflags & 0x100) r += 4;                    // sample_duration
              var sampleSize = defaultSampleSize;
              if (tflags & 0x200) { sampleSize = readU32(d, r); r += 4; }
              if (tflags & 0x400) r += 4;                    // sample_flags
              if (tflags & 0x800) r += 4;                    // composition_time_offset

              if (sampleSize > 0 && sampleOffset >= 0 && sampleOffset + sampleSize <= d.length) {
                var sample = d.subarray(sampleOffset, sampleOffset + sampleSize);
                var p2 = 0;
                while (p2 + 4 <= sample.length) {
                  var nlen = readU32(sample, p2); p2 += 4;
                  if (p2 + nlen > sample.length) break;
                  nals.push(sample.subarray(p2, p2 + nlen));
                  p2 += nlen;
                }
              }
              sampleOffset += sampleSize;
              totalSize += sampleSize;
            }
            runningRel = relStart + totalSize;
          }

          q += tsz;
        }
        p += bsz;
      }
      i += moofSize;
    }
    return nals;
  }

  function isHeic(d) {
    if (d.length < 12 || fourCC(d, 4) !== "ftyp") return false;
    // 检查 ftyp 的 compatible brands 里是否有 heic/heix/mif1
    var end = boxSize(d, 0);
    var b = 16; // 跳过 size(4)+ftyp(4)+major_brand(4)+minor(4)
    while (b + 4 <= end && b + 4 <= d.length) {
      var cc = fourCC(d, b);
      if (cc === "heic" || cc === "heix" || cc === "mif1" || cc === "hevc" || cc === "hevm" || cc === "hevs" || cc === "avif") return true;
      b += 4;
    }
    return false;
  }

  function parseHeic(d) {
    // 解析 HEIC/HEIF 图像：meta → iprp/ipco 里的 ispe(分辨率) + hvcC(HEVC 配置) + iloc(图像数据)
    var meta = findBox(d, 0, d.length, "meta");
    if (!meta) return null;
    // meta 是 FullBox：content 从 offset+12（size4+type4+version/flags4）
    var metaContent = meta.offset + 12;
    var metaEnd = meta.offset + meta.size;

    // 在 meta 内找 iprp（item properties，普通 Box）
    var iprp = findBox(d, metaContent, metaEnd, "iprp");
    if (!iprp) return null;
    var iprpContent = iprp.offset + 8;
    var iprpEnd = iprp.offset + iprp.size;

    // 在 iprp 内找 ipco（property container，普通 Box）
    var ipco = findBox(d, iprpContent, iprpEnd, "ipco");
    if (!ipco) return null;
    var ipcoContent = ipco.offset + 8;
    var ipcoEnd = ipco.offset + ipco.size;

    // 收集 ipco 里所有 box（按顺序），找 ispe / hvcC / grid
    var ipcoBoxes = [];
    var ispeOff = null, hvccOff = null, gridOff = null;
    var bp = ipcoContent;
    while (bp + 8 <= ipcoEnd) {
      var bsz = boxSize(d, bp);
      if (bsz < 8 || bp + bsz > ipcoEnd) break;
      var bcc = fourCC(d, bp + 4);
      ipcoBoxes.push({ off: bp, sz: bsz, cc: bcc });
      if (bcc === "ispe" && !ispeOff) ispeOff = bp;
      if (bcc === "hvcC" && !hvccOff) hvccOff = bp;
      if (bcc === "grid" && !gridOff) gridOff = bp;
      bp += bsz;
    }
    if (!hvccOff) return null;

    // 解析 ipma（item → property 关联）
    var ipma = findBox(d, metaContent, metaEnd, "ipma");
    var itemToProps = {};
    if (ipma) {
      var ip = ipma.offset + 8;
      var ipVer = d[ip];
      var ipFlags = (d[ip + 1] << 16) | (d[ip + 2] << 8) | d[ip + 3];
      ip += 4;
      var ipEnd = ipma.offset + ipma.size;
      var entryCount = readU32(d, ip); ip += 4;
      for (var ie = 0; ie < entryCount && ip + 4 <= ipEnd; ie++) {
        var itemId = (ipVer < 1) ? readU16(d, ip) : readU32(d, ip);
        ip += (ipVer < 1 ? 2 : 4);
        var propCount = d[ip]; ip += 1;
        var props = [];
        for (var ipj = 0; ipj < propCount && ip + 1 <= ipEnd; ipj++) {
          var propIdx;
          if (ipFlags & 1) {
            propIdx = readU16(d, ip) & 0x7FFF; ip += 2;
          } else {
            propIdx = d[ip] & 0x7F; ip += 1;
          }
          props.push(propIdx);
        }
        itemToProps[itemId] = props;
      }
    }

    // 为某个 item 找到其 hvcC box offset
    function itemHvcc(itemId) {
      var props = itemToProps[itemId];
      if (props) {
        for (var pi = 0; pi < props.length; pi++) {
          var idx = props[pi] - 1;
          if (idx >= 0 && idx < ipcoBoxes.length && ipcoBoxes[idx].cc === "hvcC")
            return ipcoBoxes[idx].off;
        }
      }
      return hvccOff;
    }

    // 从 hvcC 提取 VPS/SPS/PPS
    function parseHvccNals(off) {
      var c2 = off + 8;
      var nals = [];
      var na2 = d[c2 + 22];
      var np2 = c2 + 23;
      for (var a = 0; a < na2; a++) {
        var nt = d[np2] & 0x3F;
        var nn = readU16(d, np2 + 1);
        np2 += 3;
        for (var b = 0; b < nn; b++) {
          var nl = readU16(d, np2);
          if (nt === 32 || nt === 33 || nt === 34)
            nals.push(d.subarray(np2 + 2, np2 + 2 + nl));
          np2 += 2 + nl;
        }
      }
      return nals;
    }

    // 主 hvcC：用于 WASM 解析器 + description
    var mainC = hvccOff + 8;
    var description = d.subarray(mainC, hvccOff + boxSize(d, hvccOff));
    var paramNals = parseHvccNals(hvccOff);

    // 解析 iloc
    var iloc = findBox(d, metaContent, metaEnd, "iloc");
    var ilocItems = [];
    if (iloc) {
      var p = iloc.offset + 8;
      var version = d[p]; p++; p += 3;
      var offsetSize = d[p] >> 4, lengthSize = d[p] & 0x0F; p++;
      var baseOffsetSize = d[p] >> 4, indexSize = (version === 1 || version === 2) ? (d[p] & 0x0F) : 0; p++;
      var itemCount = (version < 2) ? readU16(d, p) : readU32(d, p); p += (version < 2 ? 2 : 4);
      for (var qi = 0; qi < itemCount; qi++) {
        var itemId = (version < 2) ? readU16(d, p) : readU32(d, p); p += (version < 2 ? 2 : 4);
        var constructionMethod = 0;
        if (version === 1 || version === 2) { constructionMethod = readU16(d, p) & 0x0F; p += 2; }
        p += 2;
        var baseOffset = 0;
        for (var kb = 0; kb < baseOffsetSize; kb++) { baseOffset = baseOffset * 256 + d[p]; p++; }
        var extentCount = readU16(d, p); p += 2;
        var extents = [];
        for (var qe = 0; qe < extentCount; qe++) {
          if (indexSize > 0) p += indexSize;
          var eoff = 0, elen = 0;
          for (var ko = 0; ko < offsetSize; ko++) { eoff = eoff * 256 + d[p]; p++; }
          for (var kl = 0; kl < lengthSize; kl++) { elen = elen * 256 + d[p]; p++; }
          extents.push({ offset: baseOffset + eoff, length: elen });
        }
        ilocItems.push({ id: itemId, constructionMethod: constructionMethod, extents: extents });
      }
    }

    // 解析 iref：primary → tile (dimg)
    var pitm = findBox(d, metaContent, metaEnd, "pitm");
    var primaryId = 0;
    if (pitm) {
      var pitmVer = d[pitm.offset + 8];
      primaryId = (pitmVer === 0) ? readU16(d, pitm.offset + 12) : readU32(d, pitm.offset + 12);
    }
    var tileIds = [];
    var iref = findBox(d, metaContent, metaEnd, "iref");
    if (iref) {
      var rp = iref.offset + 12;
      var rEnd = iref.offset + iref.size;
      while (rp + 8 <= rEnd) {
        var rSize = boxSize(d, rp);
        if (rSize < 8 || rp + rSize > rEnd) break;
        if (fourCC(d, rp + 4) === "dimg") {
          var fromItem = readU16(d, rp + 8);
          var refCount = readU16(d, rp + 10);
          var refs = [];
          for (var kr = 0; kr < refCount; kr++) refs.push(readU16(d, rp + 12 + kr * 2));
          if (fromItem === primaryId) tileIds = refs;
        }
        rp += rSize;
      }
    }

    // 构建目标 item 列表
    var targetIds = tileIds.length > 0 ? tileIds : [];
    if (targetIds.length === 0) {
      for (var tt = 0; tt < ilocItems.length; tt++) {
        if (ilocItems[tt].constructionMethod === 0) targetIds.push(ilocItems[tt].id);
      }
    }

    // 提取每个 tile：读取 extent 里全部 length-prefixed NAL，用 tile 自己的 hvcC 参数集
    var tiles = [];
    for (var ti = 0; ti < targetIds.length; ti++) {
      var it = null;
      for (var fj = 0; fj < ilocItems.length; fj++) {
        if (ilocItems[fj].id === targetIds[ti]) { it = ilocItems[fj]; break; }
      }
      if (!it || it.extents.length === 0) continue;
      var ext = it.extents[0];
      var dataOff = ext.offset;
      if (dataOff < 0 || dataOff + ext.length > d.length) continue;

      // 读取 extent 里所有 length-prefixed NAL
      var tileNals = [];
      var pos = dataOff, endPos = dataOff + ext.length;
      var rawBytes = d.subarray(dataOff, endPos); // 原始 length-prefixed 数据
      while (pos + 4 <= endPos) {
        var nalLen = readU32(d, pos); pos += 4;
        if (nalLen > 0 && pos + nalLen <= endPos) {
          tileNals.push(d.subarray(pos, pos + nalLen));
          pos += nalLen;
        } else break;
      }
      if (tileNals.length === 0) continue;

      // tile 的 hvcC 参数集
      var tHvcc = itemHvcc(it.id);
      var tParamNals = parseHvccNals(tHvcc);
      var tDesc = d.subarray(tHvcc + 8, tHvcc + boxSize(d, tHvcc));

      // 组装 annexb
      var totalSize = 0;
      for (var tq = 0; tq < tParamNals.length; tq++) totalSize += tParamNals[tq].length + 4;
      for (var tq2 = 0; tq2 < tileNals.length; tq2++) totalSize += tileNals[tq2].length + 4;
      var annexb = new Uint8Array(totalSize);
      var tw = 0;
      for (var tq3 = 0; tq3 < tParamNals.length; tq3++) {
        annexb[tw] = 0; annexb[tw+1] = 0; annexb[tw+2] = 0; annexb[tw+3] = 1; tw += 4;
        annexb.set(tParamNals[tq3], tw); tw += tParamNals[tq3].length;
      }
      for (var tq4 = 0; tq4 < tileNals.length; tq4++) {
        annexb[tw] = 0; annexb[tw+1] = 0; annexb[tw+2] = 0; annexb[tw+3] = 1; tw += 4;
        annexb.set(tileNals[tq4], tw); tw += tileNals[tq4].length;
      }

      tiles.push({ annexb: annexb, raw: rawBytes, description: tDesc });
    }

    // 拼接所有 tile 的 annexb
    var totalAnnexbSize = 0;
    for (var tj = 0; tj < tiles.length; tj++) totalAnnexbSize += tiles[tj].annexb.length;
    var allAnnexb = new Uint8Array(totalAnnexbSize);
    var aw = 0;
    for (var tk = 0; tk < tiles.length; tk++) {
      allAnnexb.set(tiles[tk].annexb, aw);
      aw += tiles[tk].annexb.length;
    }

    // 分辨率
    var picWidth = 0, picHeight = 0;
    if (ispeOff) {
      picWidth = readU32(d, ispeOff + 12);
      picHeight = readU32(d, ispeOff + 16);
    }

    // grid 布局（ImageGrid FullBox: version(1)+flags(3) 后 rows_minus_one(1)+columns_minus_one(1)+output_w(4)+output_h(4)）
    var grid = null;
    if (gridOff) {
      var gp = gridOff + 12; // 跳过 size(4)+type(4)+version(1)+flags(3)
      var gEnd = gridOff + boxSize(d, gridOff);
      if (gp + 10 <= gEnd) {
        var rows = d[gp] + 1; gp += 1;
        var cols = d[gp] + 1; gp += 1;
        var outW = readU32(d, gp); gp += 4;
        var outH = readU32(d, gp); gp += 4;
        grid = { rows: rows, cols: cols, outputWidth: outW, outputHeight: outH };
      }
    }

    return {
      codec: "hevc",
      annexb: allAnnexb,
      description: description,
      nalLengthSize: 1 + (d[mainC + 21] & 0x03),
      picWidth: picWidth,
      picHeight: picHeight,
      isImage: true,
      grid: grid,
      tiles: tiles.length > 1 ? tiles : null
    };
  }

  function isoDate(secSince1904) {
    // Mac epoch 1904-01-01 到 Unix epoch 1970-01-01 差 2082844800 秒
    var unix = secSince1904 - 2082844800;
    if (unix < 0) return null;
    return new Date(unix * 1000).toISOString().replace("T", " ").replace(/\.\d+Z$/, " UTC");
  }
  function fmtDuration(seconds) {
    if (seconds == null) return null;
    var ms = Math.round(seconds * 1000);
    var h = Math.floor(ms / 3600000);
    var m = Math.floor((ms % 3600000) / 60000);
    var s = Math.floor((ms % 60000) / 1000);
    var mm = ms % 1000;
    var out = "";
    if (h > 0) out += h + " 小时 ";
    if (m > 0 || h > 0) out += m + " 分 ";
    out += s + " 秒 " + mm + " 毫秒";
    return out;
  }
  function fmtBitrate(bitsPerSec) {
    if (bitsPerSec == null) return null;
    if (bitsPerSec >= 1000000) return (bitsPerSec / 1000000).toFixed(1) + " Mb/s";
    return (bitsPerSec / 1000).toFixed(0) + " kb/s";
  }
  function fmtSize(bytes) {
    if (bytes == null) return null;
    if (bytes >= 1073741824) return (bytes / 1073741824).toFixed(2) + " GiB";
    if (bytes >= 1048576) return (bytes / 1048576).toFixed(1) + " MiB";
    if (bytes >= 1024) return (bytes / 1024).toFixed(1) + " KiB";
    return bytes + " B";
  }

  function parseContainerInfo(d) {
    // 解析 MP4/MOV 容器级信息（General/Video/Audio/Other），类似 MediaInfo
    var info = { isContainer: false };
    if (d.length < 12 || fourCC(d, 4) !== "ftyp") return info;
    info.isContainer = true;

    // ftyp
    var ftypSize = boxSize(d, 0);
    var majorBrand = fourCC(d, 8);
    var brands = [];
    var b = 16;
    while (b + 4 <= ftypSize && b + 4 <= d.length) { brands.push(fourCC(d, b)); b += 4; }
    info.format = majorBrand;
    info.formatProfile = brands.join("/");

    // moov / mvhd
    var moov = null, i = 0;
    while (i + 8 <= d.length) {
      var s = boxSize(d, i);
      if (s < 8 || i + s > d.length) break;
      if (fourCC(d, i + 4) === "moov") { moov = { offset: i + 8, end: i + s }; break; }
      i += s;
    }
    var movieTimescale = 1000, movieDuration = 0;
    if (moov) {
      var mvhd = findBox(d, moov.offset, moov.end, "mvhd");
      if (mvhd) {
        var ver = d[mvhd.offset + 8];
        if (ver === 0) {
          info.creationTime = isoDate(readU32(d, mvhd.offset + 12));
          movieTimescale = readU32(d, mvhd.offset + 20);
          movieDuration = readU32(d, mvhd.offset + 24);
        } else {
          info.creationTime = isoDate(readU32(d, mvhd.offset + 12) * 4294967296 + readU32(d, mvhd.offset + 16));
          movieTimescale = readU32(d, mvhd.offset + 28);
          movieDuration = readU32(d, mvhd.offset + 32) * 4294967296 + readU32(d, mvhd.offset + 36);
        }
      }
    }
    var movieSeconds = movieTimescale > 0 ? movieDuration / movieTimescale : 0;
    info.duration = movieSeconds;
    info.fileSize = d.length;
    info.overallBitrate = movieSeconds > 0 ? (d.length * 8) / movieSeconds : null;

    // 遍历 trak
    info.tracks = [];
    if (moov) {
      var j = moov.offset;
      while (j + 8 <= moov.end) {
        var sz = boxSize(d, j);
        if (sz < 8 || j + sz > moov.end) break;
        if (fourCC(d, j + 4) === "trak") {
          var track = parseTrak(d, j + 8, j + sz);
          if (track) info.tracks.push(track);
        }
        j += sz;
      }
    }
    return info;
  }

  function parseTrak(d, start, end) {
    var tkhd = findBox(d, start, end, "tkhd");
    var mdia = findBox(d, start, end, "mdia");
    if (!mdia) return null;
    var mdhd = findBox(d, mdia.offset + 8, mdia.offset + mdia.size, "mdhd");
    var hdlr = findBox(d, mdia.offset + 8, mdia.offset + mdia.size, "hdlr");
    var minf = findBox(d, mdia.offset + 8, mdia.offset + mdia.size, "minf");
    var stbl = minf ? findBox(d, minf.offset + 8, minf.offset + minf.size, "stbl") : null;
    var stsd = stbl ? findBox(d, stbl.offset + 8, stbl.offset + stbl.size, "stsd") : null;

    var handler = hdlr ? fourCC(d, hdlr.offset + 16) : "?";
    var timescale = 0, duration = 0, trackId = -1;
    if (mdhd) {
      var mVer = d[mdhd.offset + 8];
      if (mVer === 0) {
        timescale = readU32(d, mdhd.offset + 20);
        duration = readU32(d, mdhd.offset + 24);
      } else {
        timescale = readU32(d, mdhd.offset + 28);
        duration = readU32(d, mdhd.offset + 32) * 4294967296 + readU32(d, mdhd.offset + 36);
      }
    }
    if (tkhd) {
      var tVer = d[tkhd.offset + 8];
      trackId = (tVer === 0) ? readU32(d, tkhd.offset + 20) : readU32(d, tkhd.offset + 28);
    }
    var track = {
      id: trackId,
      handler: handler,
      timescale: timescale,
      duration: duration,
      seconds: timescale > 0 ? duration / timescale : 0
    };

    // stsz 计算流大小（字节）
    if (stbl) {
      var stsz = findBox(d, stbl.offset + 8, stbl.offset + stbl.size, "stsz");
      if (stsz) {
        var uniform = readU32(d, stsz.offset + 12);
        var cnt = readU32(d, stsz.offset + 16);
        var streamBytes = 0;
        if (uniform > 0) streamBytes = uniform * cnt;
        else {
          for (var k = 0; k < cnt; k++) streamBytes += readU32(d, stsz.offset + 20 + k * 4);
        }
        track.streamSize = streamBytes;
        track.bitrate = track.seconds > 0 ? (streamBytes * 8) / track.seconds : null;
      }
    }

    // stsd sample entry
    if (stsd) {
      var p = stsd.offset + 16;
      var eEnd = stsd.offset + stsd.size;
      if (p + 8 <= eEnd) {
        var esize = readU32(d, p);
        var fmt = fourCC(d, p + 4);
        track.codec = fmt;
        if (fmt === "hvc1" || fmt === "hev1") {
          track.width = readU16(d, p + 32);
          track.height = readU16(d, p + 34);
        } else if (fmt === "avc1" || fmt === "avc3") {
          track.width = readU16(d, p + 32);
          track.height = readU16(d, p + 34);
        } else if (fmt === "twos" || fmt === "sowt" || fmt === "lpcm" || fmt === "in24" || fmt === "in32" || fmt === "fl32" || fmt === "fl64") {
          // QuickTime AudioSampleEntry（twos/sowt 等）
          track.channels = readU16(d, p + 24);
          track.bitDepth = readU16(d, p + 26);
          track.sampleRate = (readU32(d, p + 32) >>> 16);
        } else if (fmt === "mp4a") {
          track.channels = readU16(d, p + 24);
          track.bitDepth = readU16(d, p + 26);
          track.sampleRate = (readU32(d, p + 32) >>> 16);
        }
      }
    }
    return track;
  }

  window.H26xDemux = {
    isMp4: isMp4,
    demuxMp4: demuxMp4,
    isHeic: isHeic,
    parseHeic: parseHeic,
    detectImageType: detectImageType,
    parseContainerInfo: parseContainerInfo,
    nalToAnnexB: nalToAnnexB,
    isIvf: isIvf,
    isAv1AnnexB: isAv1AnnexB,
    parseIvf: parseIvf,
    parseAv1Obus: parseAv1Obus,
    demuxAv1Mp4: demuxAv1Mp4,
    isWebm: isWebm,
    demuxWebm: demuxWebm
  };
})();
