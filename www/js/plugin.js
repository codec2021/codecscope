// CodecScope 插件系统
// 用户可通过 window.CodecScope.registerPlugin(...) 注册自定义 SEI 解析器。
// 插件文件示例见 docs/sei-plugin.example.js

(function () {
  "use strict";

  var plugins = [];
  var pluginSeq = 0;

  // ---------- 位读取器 ----------
  function BitReader(bytes) {
    this.bytes = bytes;
    this.bitPos = 0;
  }
  BitReader.prototype = {
    get bitsLeft() { return this.bytes.length * 8 - this.bitPos; },
    readBits: function (n) {
      if (n <= 0) return 0;
      var v = 0;
      for (var i = 0; i < n; i++) {
        var byteIdx = this.bitPos >> 3;
        if (byteIdx >= this.bytes.length) { this.bitPos++; continue; }
        var bitIdx = 7 - (this.bitPos & 7);
        v = (v << 1) | ((this.bytes[byteIdx] >> bitIdx) & 1);
        this.bitPos++;
      }
      return v >>> 0;
    },
    readU: function (n) { return this.readBits(n); },
    // 无符号 Exp-Golomb
    readUe: function () {
      var zeros = 0;
      while (this.bitPos < this.bytes.length * 8 && this.readBits(1) === 0) zeros++;
      if (zeros >= 31) return 0;
      var suffix = this.readBits(zeros);
      return ((1 << zeros) - 1 + suffix) >>> 0;
    },
    // 有符号 Exp-Golomb
    readSe: function () {
      var ue = this.readUe();
      var k = ue & 1;
      return k === 0 ? (ue >> 1) : -((ue + 1) >> 1);
    }
  };

  // ---------- 插件注册 ----------
  function toList(single, arr) {
    if (Array.isArray(arr)) return arr.slice();
    if (typeof single === "number") return [single];
    return [];
  }

  function registerPlugin(plugin) {
    if (!plugin || typeof plugin.parse !== "function") {
      console.warn("[CodecScope] 插件缺少 parse 函数，忽略", plugin);
      return false;
    }
    var payloadTypes = toList(plugin.payloadType, plugin.payloadTypes);
    var nalTypes = toList(plugin.nalType, plugin.nalTypes);
    if (payloadTypes.length === 0 && nalTypes.length === 0) {
      console.warn("[CodecScope] 插件未指定 payloadType 或 nalType，忽略", plugin);
      return false;
    }
    var p = {
      id: "plg" + (++pluginSeq),
      name: plugin.name || ("plugin " + pluginSeq),
      codec: plugin.codec || null,
      payloadTypes: payloadTypes,
      nalTypes: nalTypes,
      parse: plugin.parse,
      enabled: true
    };
    plugins.push(p);
    return p.id;
  }

  function getPlugins() { return plugins; }
  function clearPlugins() { plugins = []; }

  function findPlugin(id) {
    for (var i = 0; i < plugins.length; i++) if (plugins[i].id === id) return plugins[i];
    return null;
  }
  function setPluginEnabled(id, enabled) {
    var p = findPlugin(id);
    if (p) { p.enabled = !!enabled; return true; }
    return false;
  }
  function removePlugin(id) {
    for (var i = 0; i < plugins.length; i++) {
      if (plugins[i].id === id) { plugins.splice(i, 1); return true; }
    }
    return false;
  }

  // ---------- SEI payload 提取 ----------
  // 返回 { payloadType, payloadData }，仅解析 NAL 内第一条 SEI message。
  function extractSeiPayload(fileBytes, nal, codec) {
    var d = fileBytes;
    var off = nal.offset, len = nal.length;
    if (off < 0 || off + len > d.length || len < 4) return null;

    var p = off;
    if (d[p] === 0 && d[p + 1] === 0 && d[p + 2] === 1) p += 3;
    else if (d[p] === 0 && d[p + 1] === 0 && d[p + 2] === 0 && d[p + 3] === 1) p += 4;
    else return null;

    var nalHeaderLen = (codec === "avc") ? 1 : 2;
    p += nalHeaderLen;
    if (p >= off + len) return null;

    // payloadType（0xff 延续）
    var payloadType = 0;
    while (p < off + len && d[p] === 0xff) { payloadType += 255; p++; }
    if (p >= off + len) return null;
    payloadType += d[p]; p++;

    // payloadSize（0xff 延续）
    var payloadSize = 0;
    while (p < off + len && d[p] === 0xff) { payloadSize += 255; p++; }
    if (p >= off + len) return null;
    payloadSize += d[p]; p++;

    var end = Math.min(p + payloadSize, off + len);
    return { payloadType: payloadType, payloadData: d.subarray(p, end) };
  }

  // ---------- 插件结果 → 语法树节点 ----------
  function toTreeNode(item) {
    if (!item) return null;
    if (typeof item === "string" || typeof item === "number" || typeof item === "boolean")
      return { n: String(item) };
    var node = { n: item.name != null ? String(item.name) : "" };
    if (item.value !== undefined && item.value !== null && !item.children)
      node.n += " = " + item.value;
    if (item.children && item.children.length > 0) {
      node.c = [];
      for (var i = 0; i < item.children.length; i++) {
        var child = toTreeNode(item.children[i]);
        if (child) node.c.push(child);
      }
    }
    return node;
  }

  function runSeiPlugin(fileBytes, nal, codec) {
    var sei = extractSeiPayload(fileBytes, nal, codec);
    if (!sei) return null;
    var plugin = null;
    for (var i = 0; i < plugins.length; i++) {
      var pl = plugins[i];
      if (!pl.enabled) continue;
      if (pl.codec && pl.codec !== codec) continue;
      if (pl.payloadTypes.indexOf(sei.payloadType) >= 0) { plugin = pl; break; }
    }
    if (!plugin) return null;

    var ctx = new BitReader(sei.payloadData);
    var result;
    try {
      result = plugin.parse(ctx, {
        codec: codec,
        payloadType: sei.payloadType,
        payloadSize: sei.payloadData.length,
        bitReader: ctx
      });
    } catch (err) {
      return { n: plugin.name + " (plugin error: " + err.message + ")" };
    }
    if (result == null) return null;

    var children = Array.isArray(result) ? result : (result.children || []);
    var node = { n: "sei_payload(" + sei.payloadType + ") [" + plugin.name + "]" };
    if (result && result.name && !Array.isArray(result)) node.n = result.name;
    if (children.length > 0) node.c = children.map(toTreeNode).filter(Boolean);
    return node;
  }

  // ---------- NAL 插件（匹配 NAL type） ----------
  function extractNalPayload(fileBytes, nal, codec) {
    var d = fileBytes;
    var off = nal.offset, len = nal.length;
    if (off < 0 || off + len > d.length || len < 4) return null;
    var p = off;
    if (d[p] === 0 && d[p + 1] === 0 && d[p + 2] === 1) p += 3;
    else if (d[p] === 0 && d[p + 1] === 0 && d[p + 2] === 0 && d[p + 3] === 1) p += 4;
    else return null;
    var nalHeaderLen = (codec === "avc") ? 1 : 2;
    p += nalHeaderLen;
    if (p >= off + len) return null;
    return d.subarray(p, off + len);
  }

  function runNalPlugin(fileBytes, nal, codec) {
    var plugin = null;
    for (var i = 0; i < plugins.length; i++) {
      var pl = plugins[i];
      if (!pl.enabled) continue;
      if (pl.codec && pl.codec !== codec) continue;
      if (pl.nalTypes.indexOf(nal.type) >= 0) { plugin = pl; break; }
    }
    if (!plugin) return null;
    var payload = extractNalPayload(fileBytes, nal, codec);
    if (!payload) return null;

    var ctx = new BitReader(payload);
    var result;
    try {
      result = plugin.parse(ctx, {
        codec: codec,
        nalType: nal.type,
        payloadSize: payload.length,
        bitReader: ctx
      });
    } catch (err) {
      return { n: plugin.name + " (plugin error: " + err.message + ")" };
    }
    if (result == null) return null;

    var children = Array.isArray(result) ? result : (result.children || []);
    var node = { n: "nal_type(" + nal.type + ") [" + plugin.name + "]" };
    if (result && result.name && !Array.isArray(result)) node.n = result.name;
    if (children.length > 0) node.c = children.map(toTreeNode).filter(Boolean);
    return node;
  }

  window.CodecScope = window.CodecScope || {};
  window.CodecScope.registerPlugin = registerPlugin;
  window.CodecScope.getPlugins = getPlugins;
  window.CodecScope.clearPlugins = clearPlugins;
  window.CodecScope.setPluginEnabled = setPluginEnabled;
  window.CodecScope.removePlugin = removePlugin;
  window.CodecScope.runSeiPlugin = runSeiPlugin;
  window.CodecScope.runNalPlugin = runNalPlugin;
})();
