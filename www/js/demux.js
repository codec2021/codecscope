(function () {
  "use strict";

  function readU32(d, o) { return ((d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]) >>> 0; }
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
    // 解析 HEIC/HEIF 单帧图像：meta → iprp/ipco 里的 ispe(分辨率) + hvcC(HEVC 配置)
    var meta = findBox(d, 0, d.length, "meta");
    if (!meta) return null;
    // meta 是 FullBox：content 从 offset+12（size4+type4+version/flags4）
    var metaContent = meta.offset + 12;
    var metaEnd = meta.offset + meta.size;

    // 在 meta 内找 iprp（item properties，普通 Box）
    var iprp = findBox(d, metaContent, metaEnd, "iprp");
    if (!iprp) return null;
    // iprp 是普通 Box：content 从 offset+8
    var iprpContent = iprp.offset + 8;
    var iprpEnd = iprp.offset + iprp.size;

    // 在 iprp 内找 ipco（property container，普通 Box）
    var ipco = findBox(d, iprpContent, iprpEnd, "ipco");
    if (!ipco) return null;
    var ipcoContent = ipco.offset + 8;
    var ipcoEnd = ipco.offset + ipco.size;

    // 找 ispe（分辨率，FullBox）和 hvcC（HEVC 配置，普通 Box）
    var ispe = findBox(d, ipcoContent, ipcoEnd, "ispe");
    var hvcc = findBox(d, ipcoContent, ipcoEnd, "hvcC");
    if (!hvcc) return null;

    // 从 hvcC 提取 VPS/SPS/PPS（HEVCDecoderConfigurationRecord）
    var c = hvcc.offset + 8; // 跳过 size(4)+type(4)，到 configurationVersion
    var description = d.subarray(c, hvcc.offset + hvcc.size);
    var paramNals = [];
    var na = d[c + 22]; // numOfArrays
    var np = c + 23;
    for (var q2 = 0; q2 < na; q2++) {
      var ntype = d[np] & 0x3F;
      var numNalus = readU16(d, np + 1);
      np += 3;
      for (var q3 = 0; q3 < numNalus; q3++) {
        var nlen = readU16(d, np);
        if (ntype === 32 || ntype === 33 || ntype === 34)
          paramNals.push(d.subarray(np + 2, np + 2 + nlen));
        np += 2 + nlen;
      }
    }

    // 组装 annexb（仅参数集，图像数据在 mdat 里，这里不取）
    var totalSize = 0;
    for (var q4 = 0; q4 < paramNals.length; q4++) totalSize += paramNals[q4].length + 4;
    var out = new Uint8Array(totalSize);
    var w = 0;
    for (var q5 = 0; q5 < paramNals.length; q5++) {
      out[w] = 0; out[w + 1] = 0; out[w + 2] = 0; out[w + 3] = 1;
      w += 4;
      out.set(paramNals[q5], w);
      w += paramNals[q5].length;
    }

    var picWidth = 0, picHeight = 0;
    if (ispe) {
      // ispe FullBox: version/flags(4) + width(4) + height(4)
      picWidth = readU32(d, ispe.offset + 12);
      picHeight = readU32(d, ispe.offset + 16);
    }

    return {
      codec: "hevc",
      annexb: out,
      description: description,
      nalLengthSize: 1 + (d[c + 21] & 0x03),
      picWidth: picWidth,
      picHeight: picHeight,
      isImage: true
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
          info.creationTime = isoDate(readU32(d, mvhd.offset + 20));
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
    var timescale = mdhd ? readU32(d, mdhd.offset + 20) : 0;
    var duration = mdhd ? readU32(d, mdhd.offset + 24) : 0;
    var track = {
      id: tkhd ? readU32(d, tkhd.offset + 20) : -1,
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
    parseContainerInfo: parseContainerInfo,
    nalToAnnexB: nalToAnnexB
  };
})();
