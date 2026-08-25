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
      var ipVer = d[ip]; ip++; ip += 3;
      var entryCount = readU32(d, ip); ip += 4;
      for (var ie = 0; ie < entryCount; ie++) {
        var itemId = readU16(d, ip); ip += 2;
        var propCount = d[ip]; ip += 1;
        var props = [];
        for (var ipj = 0; ipj < propCount; ipj++) {
          var propIdx = (ipVer >= 1) ? (readU16(d, ip) & 0x7FFF) : readU16(d, ip);
          ip += 2;
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
    var primaryId = pitm ? readU16(d, pitm.offset + 12) : 0;
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
    var firstTileAnnexb = null;
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

      tiles.push({ annexb: annexb, description: tDesc });
      if (!firstTileAnnexb) firstTileAnnexb = annexb;
    }

    // 分辨率
    var picWidth = 0, picHeight = 0;
    if (ispeOff) {
      picWidth = readU32(d, ispeOff + 12);
      picHeight = readU32(d, ispeOff + 16);
    }

    // grid 布局
    var grid = null;
    if (gridOff) {
      var gp = gridOff + 8;
      gp++; gp += 3; // version + flags
      var rows = readU16(d, gp) + 1; gp += 2;
      var outW = readU16(d, gp); gp += 2;
      var outH = readU16(d, gp); gp += 2;
      var cols = readU16(d, gp) + 1; gp += 2;
      grid = { rows: rows, cols: cols, outputWidth: outW, outputHeight: outH };
    }

    console.log("parseHeic: tiles=" + tiles.length + " targetIds=" + targetIds.length + " grid=" + JSON.stringify(grid));
    for (var li = 0; li < tiles.length; li++) {
      console.log("  tile[" + li + "] annexb=" + tiles[li].annexb.length + " desc=" + tiles[li].description.length);
    }

    return {
      codec: "hevc",
      annexb: firstTileAnnexb || new Uint8Array(0),
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
