(function () {
  "use strict";

  function readU16(d, o) { return (d[o] << 8) | d[o + 1]; }

  var MARKER_NAMES = {
    0x00: "FF00", 0x01: "TEM",
    0xC0: "SOF0", 0xC1: "SOF1", 0xC2: "SOF2", 0xC3: "SOF3",
    0xC4: "DHT", 0xC5: "SOF5", 0xC6: "SOF6", 0xC7: "SOF7",
    0xC8: "JPG", 0xC9: "SOF9", 0xCA: "SOF10", 0xCB: "SOF11",
    0xCC: "DAC", 0xCD: "SOF13", 0xCE: "SOF14", 0xCF: "SOF15",
    0xD0: "RST0", 0xD1: "RST1", 0xD2: "RST2", 0xD3: "RST3",
    0xD4: "RST4", 0xD5: "RST5", 0xD6: "RST6", 0xD7: "RST7",
    0xD8: "SOI", 0xD9: "EOI", 0xDA: "SOS", 0xDB: "DQT",
    0xDC: "DNL", 0xDD: "DRI", 0xDE: "DHP", 0xDF: "EXP",
    0xE0: "APP0", 0xE1: "APP1", 0xE2: "APP2", 0xE3: "APP3",
    0xE4: "APP4", 0xE5: "APP5", 0xE6: "APP6", 0xE7: "APP7",
    0xE8: "APP8", 0xE9: "APP9", 0xEA: "APP10", 0xEB: "APP11",
    0xEC: "APP12", 0xED: "APP13", 0xEE: "APP14", 0xEF: "APP15",
    0xFE: "COM"
  };

  var SOF_TYPES = { 0xC0: 1, 0xC1: 1, 0xC2: 1, 0xC3: 1, 0xC5: 1, 0xC6: 1, 0xC7: 1, 0xC9: 1, 0xCA: 1, 0xCB: 1, 0xCD: 1, 0xCE: 1, 0xCF: 1 };

  var EXIF_TAGS = {
    0x0100: "ImageWidth", 0x0101: "ImageLength", 0x0102: "BitsPerSample",
    0x0103: "Compression", 0x0106: "PhotometricInterpretation",
    0x010E: "ImageDescription", 0x010F: "Make", 0x0110: "Model",
    0x0112: "Orientation", 0x0115: "SamplesPerPixel",
    0x011A: "XResolution", 0x011B: "YResolution",
    0x0128: "ResolutionUnit", 0x0131: "Software", 0x0132: "DateTime",
    0x013B: "Artist", 0x0213: "YCbCrPositioning", 0x8298: "Copyright",
    0x8769: "ExifIFDPointer", 0x8825: "GPSInfoIFDPointer"
  };
  var EXIF_TAGS_EXIF = {
    0x829A: "ExposureTime", 0x829D: "FNumber", 0x8822: "ExposureProgram",
    0x8827: "ISOSpeedRatings", 0x8830: "SensitivityType", 0x8832: "RecommendedExposureIndex",
    0x9000: "ExifVersion", 0x9003: "DateTimeOriginal",
    0x9004: "DateTimeDigitized", 0x9201: "ShutterSpeedValue", 0x9202: "ApertureValue",
    0x9204: "ExposureBiasValue", 0x9206: "SubjectDistance", 0x9207: "MeteringMode", 0x9209: "Flash",
    0x920A: "FocalLength", 0x9291: "SubSecTimeOriginal", 0x9292: "SubSecTimeDigitized",
    0xA001: "ColorSpace", 0xA002: "PixelXDimension", 0xA003: "PixelYDimension",
    0xA20E: "FocalPlaneXResolution", 0xA20F: "FocalPlaneYResolution", 0xA210: "FocalPlaneResolutionUnit",
    0xA300: "FileSource", 0xA301: "SceneType", 0xA401: "CustomRendered",
    0xA402: "ExposureMode", 0xA403: "WhiteBalance", 0xA406: "SceneCaptureType",
    0xA431: "SerialNumber", 0xA432: "LensInfo", 0xA434: "LensModel", 0xA435: "LensMake"
  };

  function ascii(d, o, len) {
    var s = "";
    for (var i = 0; i < len && o + i < d.length; i++) {
      var b = d[o + i];
      if (b === 0) break;
      s += String.fromCharCode(b);
    }
    return s;
  }

  function rational(d, o, bigEndian) {
    var num, den;
    if (bigEndian) {
      num = (d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3];
      den = (d[o + 4] << 24) | (d[o + 5] << 16) | (d[o + 6] << 8) | d[o + 7];
    } else {
      num = d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (d[o + 3] << 24);
      den = d[o + 4] | (d[o + 5] << 8) | (d[o + 6] << 16) | (d[o + 7] << 24);
    }
    if (den === 0) return "0";
    return (num / den).toFixed(2).replace(/\.00$/, "") + " (" + num + "/" + den + ")";
  }

  function parseExif(d, start, end) {
    // APP1: "Exif\0\0" + TIFF
    var o = start;
    if (ascii(d, o, 4) !== "Exif") return null;
    o += 6;
    if (o + 8 > end) return null;
    var bigEndian = (d[o] === 0x4D && d[o + 1] === 0x4D);
    if (d[o] === 0x49 && d[o + 1] === 0x49) bigEndian = false;
    else if (!bigEndian) return null;
    var u16 = function (p) { return bigEndian ? ((d[p] << 8) | d[p + 1]) : (d[p] | (d[p + 1] << 8)); };
    var u32 = function (p) { return bigEndian ? ((d[p] << 24) | (d[p + 1] << 16) | (d[p + 2] << 8) | d[p + 3]) : (d[p] | (d[p + 1] << 8) | (d[p + 2] << 16) | (d[p + 3] << 24)); };
    var tiffMagic = u16(o + 2);
    if (tiffMagic !== 0x002A) return null;
    var ifd0 = u32(o + 4);
    if (ifd0 === 0) ifd0 = 8;
    var tiffBase = o;

    function readIfd(base, ifdOff) {
      var off = tiffBase + ifdOff;
      if (off + 2 > end) return [];
      var count = u16(off);
      off += 2;
      var entries = [];
      for (var i = 0; i < count && off + 12 <= end; i++) {
        var tag = u16(off);
        var type = u16(off + 2);
        var cnt = u32(off + 4);
        var valBytes;
        switch (type) {
          case 1: case 7: valBytes = cnt; break;
          case 2: valBytes = cnt; break;
          case 3: valBytes = cnt * 2; break;
          case 4: case 9: valBytes = cnt * 4; break;
          case 5: case 10: valBytes = cnt * 8; break;
          default: valBytes = cnt * 4;
        }
        var value;
        var valOff = off + 8;
        if (valBytes <= 4) {
          value = readValue(d, valOff, type, cnt, bigEndian);
        } else {
          var dataOff = tiffBase + u32(valOff);
          if (dataOff + valBytes <= end) value = readValue(d, dataOff, type, cnt, bigEndian);
          else value = "(out of range)";
        }
        entries.push({ tag: tag, value: value });
        off += 12;
      }
      return entries;
    }

    function toTree(entries, tagTable) {
      return entries.map(function (e) {
        var name = tagTable[e.tag] || ("Tag 0x" + e.tag.toString(16).toUpperCase());
        return { n: name + " = " + e.value };
      });
    }

    function readValue(d, o, type, cnt, be) {
      var u16f = function (p) { return be ? ((d[p] << 8) | d[p + 1]) : (d[p] | (d[p + 1] << 8)); };
      var u32f = function (p) { return be ? ((d[p] << 24) | (d[p + 1] << 16) | (d[p + 2] << 8) | d[p + 3]) : (d[p] | (d[p + 1] << 8) | (d[p + 2] << 16) | (d[p + 3] << 24)); };
      switch (type) {
        case 2: return '"' + ascii(d, o, Math.min(cnt, 64)) + '"';
        case 3: return String(u16f(o));
        case 4: return String(u32f(o));
        case 5: return rational(d, o, be);
        case 7: {
          var s = "";
          var printable = cnt > 0;
          for (var k = 0; k < cnt && k < 8; k++) {
            var b = d[o + k];
            if (b >= 32 && b <= 126) s += String.fromCharCode(b);
            else printable = false;
          }
          if (printable && s) return '"' + s + '"';
          return String(d[o]);
        }
        case 9: return String(u32f(o));
        case 10: return rational(d, o, be);
        default: return String(u16f(o));
      }
    }

    var tree = [];
    tree.push({ n: "Byte order = " + (bigEndian ? "Big-endian (MM)" : "Little-endian (II)") });
    var ifd0Entries = readIfd(start, ifd0);
    tree.push({ n: "IFD0 (" + ifd0Entries.length + " entries)", c: toTree(ifd0Entries, EXIF_TAGS) });

    for (var i = 0; i < ifd0Entries.length; i++) {
      if (ifd0Entries[i].tag === 0x8769) {
        var exifOff = parseInt(ifd0Entries[i].value, 10);
        if (!isNaN(exifOff) && exifOff > 0 && tiffBase + exifOff < end) {
          var exifEntries = readIfd(tiffBase, exifOff);
          tree.push({ n: "ExifIFD (" + exifEntries.length + " entries)", c: toTree(exifEntries, EXIF_TAGS_EXIF) });
        }
      }
    }
    return tree;
  }

  function parseSof(d, o, marker) {
    var precision = d[o];
    var height = readU16(d, o + 1);
    var width = readU16(d, o + 3);
    var comps = d[o + 5];
    var c = [
      { n: "Precision = " + precision + " bits" },
      { n: "Height = " + height },
      { n: "Width = " + width },
      { n: "Number of components = " + comps }
    ];
    for (var i = 0; i < comps; i++) {
      var cp = o + 6 + i * 3;
      var id = d[cp];
      var sf = d[cp + 1];
      var h = (sf >> 4) & 0xF;
      var v = sf & 0xF;
      var qt = d[cp + 2];
      c.push({ n: "Component " + (i + 1) + ": ID=" + id + ", sampling=" + h + "x" + v + ", quant table=" + qt });
    }
    return { width: width, height: height, tree: c };
  }

  function parseDqt(d, o, len) {
    var c = [];
    var p = o;
    while (p < o + len) {
      var pq = d[p] >> 4;
      var tq = d[p] & 0x0F;
      p++;
      var precision = pq ? 16 : 8;
      var n = pq ? 128 : 64;
      if (p + n > o + len) break;
      var vals = [];
      for (var i = 0; i < 64 && p + (pq ? 2 : 1) <= o + len; i++) {
        vals.push(pq ? readU16(d, p) : d[p]);
        p += pq ? 2 : 1;
      }
      c.push({ n: "Quantization table " + tq + " (" + precision + "-bit)" });
      var rows = [];
      for (var r = 0; r < 8; r++) rows.push(vals.slice(r * 8, r * 8 + 8).join(" "));
      rows.forEach(function (row) { c.push({ n: row }); });
    }
    return c;
  }

  function parseDht(d, o, len) {
    var c = [];
    var p = o;
    while (p < o + len) {
      var tc = d[p] >> 4;
      var th = d[p] & 0x0F;
      p++;
      var counts = [];
      var total = 0;
      for (var i = 0; i < 16; i++) { counts.push(d[p]); total += d[p]; p++; }
      var classStr = tc === 0 ? "DC" : "AC";
      c.push({ n: "Huffman table " + classStr + " #" + th + " (" + total + " symbols)" });
      c.push({ n: "Code counts: " + counts.join(", ") });
      p += total; // 跳过符号值
    }
    return c;
  }

  function parseSos(d, o) {
    var comps = d[o];
    var c = [{ n: "Number of components = " + comps }];
    for (var i = 0; i < comps; i++) {
      var id = d[o + 1 + i * 2];
      var sel = d[o + 2 + i * 2];
      c.push({ n: "Component " + id + ": DC table=" + (sel >> 4) + ", AC table=" + (sel & 0x0F) });
    }
    var s = o + 1 + comps * 2;
    c.push({ n: "Spectral selection: Ss=" + d[s] + ", Se=" + d[s + 1] });
    c.push({ n: "Successive approximation: Ah=" + (d[s + 2] >> 4) + ", Al=" + (d[s + 2] & 0x0F) });
    return c;
  }

  function parseApp0(d, o, len) {
    if (ascii(d, o, 4) === "JFIF") {
      var version = d[o + 5] + "." + d[o + 6];
      var units = ["none", "dots per inch", "dots per cm"][d[o + 7]] || "?";
      var xd = readU16(d, o + 8);
      var yd = readU16(d, o + 10);
      var xt = d[o + 12];
      var yt = d[o + 13];
      return [{ n: "JFIF version " + version }, { n: "Units = " + units }, { n: "Density = " + xd + " x " + yd + (xt && yt ? " (thumbnail " + xt + "x" + yt + ")" : "") }];
    }
    return null;
  }

  function parseApp14(d, o) {
    if (ascii(d, o, 5) === "Adobe") {
      var version = readU16(d, o + 5);
      return [{ n: "Adobe APP14, version " + version }];
    }
    return null;
  }

  function parseApp2(d, o, len) {
    if (ascii(d, o, 11) === "ICC_PROFILE") {
      return [{ n: "ICC color profile" }];
    }
    return null;
  }

  function markerInfo(marker) {
    var names = {
      0xC0: "Baseline DCT", 0xC1: "Extended sequential DCT", 0xC2: "Progressive DCT",
      0xC3: "Lossless", 0xC4: "Huffman table", 0xDB: "Quantization table",
      0xDA: "Start of scan", 0xDD: "Restart interval", 0xFE: "Comment",
      0xD8: "Start of image", 0xD9: "End of image"
    };
    return names[marker] || "";
  }

  function parseJpeg(d) {
    if (d.length < 4 || d[0] !== 0xFF || d[1] !== 0xD8) return null;

    var segments = [];
    var width = 0, height = 0;
    var pos = 2;

    // SOI
    segments.push({
      offset: 0, length: 2, type: 0xD8, typeName: "SOI",
      info: "Start of Image", color: "#FF8888", syntax: { n: "SOI (Start of Image)" }
    });

    while (pos < d.length) {
      if (d[pos] !== 0xFF) { pos++; continue; }
      // 跳过填充 FF
      var markerStart = pos;
      while (pos < d.length && d[pos] === 0xFF) pos++;
      if (pos >= d.length) break;
      var marker = d[pos]; pos++;

      if (marker === 0x00) continue; // FF00 填充字节

      if (marker === 0xD9) { // EOI
        segments.push({
          offset: markerStart, length: d.length - markerStart, type: 0xD9, typeName: "EOI",
          info: "End of Image", color: "#FF8888", syntax: { n: "EOI (End of Image)" }
        });
        break;
      }

      if (marker >= 0xD0 && marker <= 0xD7) { // RSTn 无长度
        segments.push({
          offset: markerStart, length: 2, type: marker, typeName: MARKER_NAMES[marker],
          info: "Restart marker", color: "#888888", syntax: { n: MARKER_NAMES[marker] + " (Restart marker)" }
        });
        continue;
      }

      // 其他 marker：2 字节长度
      if (pos + 2 > d.length) break;
      var segLen = readU16(d, pos);
      if (segLen < 2 || pos + segLen > d.length) break;

      var name = MARKER_NAMES[marker] || ("0xFF" + marker.toString(16).toUpperCase());
      var contentStart = pos + 2;
      var contentLen = segLen - 2;
      var info = markerInfo(marker);
      var syntax = { n: name + (info ? " (" + info + ")" : "") };

      if (marker in SOF_TYPES) {
        var sof = parseSof(d, contentStart, marker);
        if (sof.width) { width = sof.width; height = sof.height; }
        syntax.c = sof.tree;
      } else if (marker === 0xDB) {
        syntax.c = parseDqt(d, contentStart, contentLen);
      } else if (marker === 0xC4) {
        syntax.c = parseDht(d, contentStart, contentLen);
      } else if (marker === 0xDA) {
        syntax.c = parseSos(d, contentStart);
      } else if (marker === 0xDD) {
        var ri = readU16(d, contentStart);
        syntax.c = [{ n: "Restart interval = " + ri + " MCUs" }];
      } else if (marker === 0xFE) {
        syntax.c = [{ n: 'Comment = "' + ascii(d, contentStart, contentLen) + '"' }];
      } else if (marker === 0xE0) {
        var jfif = parseApp0(d, contentStart, contentLen);
        if (jfif) { syntax.c = jfif; info = "JFIF"; }
      } else if (marker === 0xE1) {
        var exif = parseExif(d, contentStart, contentStart + contentLen);
        if (exif) { syntax.c = exif; info = "EXIF"; }
        else { syntax.c = [{ n: "APP1 (" + contentLen + " bytes)" }]; if (ascii(d, contentStart, 4) === "http") info = "XMP"; }
      } else if (marker === 0xE2) {
        var icc = parseApp2(d, contentStart, contentLen);
        if (icc) { syntax.c = icc; info = "ICC profile"; }
      } else if (marker === 0xED) {
        if (ascii(d, contentStart, 13) === "Photoshop 3.0") { info = "Photoshop"; syntax.c = [{ n: "Adobe Photoshop IRB" }]; }
      } else if (marker === 0xEE) {
        var adobe = parseApp14(d, contentStart);
        if (adobe) { syntax.c = adobe; info = "Adobe"; }
      }

      segments.push({
        offset: markerStart, length: segLen + 2, type: marker, typeName: name,
        info: info, color: (marker in SOF_TYPES || marker === 0xDA) ? "#CD9B1D" : "#4d94e8",
        syntax: syntax
      });

      pos += segLen;

      // SOS 之后的熵编码数据：扫描到下一个非 RST、非 FF00 的 marker
      if (marker === 0xDA) {
        var dataStart = pos;
        var dataEnd = pos;
        var found = false;
        var i = pos;
        while (i < d.length) {
          if (d[i] === 0xFF) {
            if (i + 1 < d.length) {
              var nm = d[i + 1];
              if (nm === 0x00) { i += 2; continue; }        // 填充
              if (nm >= 0xD0 && nm <= 0xD7) { i += 2; continue; } // RST
              dataEnd = i;
              found = true;
              break;
            }
          }
          i++;
        }
        if (!found) dataEnd = d.length;
        if (dataEnd > dataStart) {
          segments.push({
            offset: dataStart, length: dataEnd - dataStart, type: 0xDA, typeName: "SCAN",
            info: "Entropy-coded scan data", color: "#00B050",
            syntax: { n: "Scan data (" + (dataEnd - dataStart) + " bytes)" }
          });
          pos = dataEnd;
        }
      }
    }

    return { codec: "jpeg", segments: segments, width: width, height: height };
  }

  window.JpegParser = {
    parseJpeg: parseJpeg
  };
})();
