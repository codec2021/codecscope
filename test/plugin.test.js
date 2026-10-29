const { test } = require("node:test");
const assert = require("node:assert");
const fs = require("fs");
const path = require("path");

global.window = {};
eval(fs.readFileSync(path.join(__dirname, "..", "www/js/plugin.js"), "utf8"));
const CS = global.window.CodecScope;

function makeNal(nalType, payload) {
  const d = new Uint8Array(3 + 2 + payload.length);
  d[0] = 0; d[1] = 0; d[2] = 1;
  d[3] = (nalType << 1) & 0xFF;
  d[4] = 0x01;
  d.set(payload, 5);
  return { bytes: d, nal: { offset: 0, length: d.length, type: nalType } };
}

function nodeValues(node) {
  const vals = {};
  function walk(n) {
    if (!n) return;
    if (n.n) {
      const m = n.n.match(/^(\w+) = (-?\d+)$/);
      if (m) vals[m[1]] = parseInt(m[2], 10);
    }
    if (n.c) n.c.forEach(walk);
  }
  walk(node);
  return vals;
}

test("BitReader readBits + readUe via NAL plugin", () => {
  CS.clearPlugins();
  CS.registerPlugin({
    name: "br-test", codec: "hevc", nalType: 62,
    parse: (ctx) => ({ children: [
      { name: "bits3", value: ctx.readBits(3) },
      { name: "ue", value: ctx.readUe() }
    ] })
  });
  // payload 0xA8 = 10101000: readBits(3)=101=5, readUe=1
  const { bytes, nal } = makeNal(62, new Uint8Array([0xA8]));
  const r = CS.runNalPlugin(bytes, nal, "hevc");
  assert.ok(r, "runNalPlugin 返回节点");
  const v = nodeValues(r);
  assert.strictEqual(v.bits3, 5);
  assert.strictEqual(v.ue, 1);
});

test("BitReader readSe signed Exp-Golomb", () => {
  CS.clearPlugins();
  CS.registerPlugin({
    name: "se-test", codec: "hevc", nalType: 62,
    parse: (ctx) => ({ children: [
      { name: "se0", value: ctx.readSe() },
      { name: "se1", value: ctx.readSe() }
    ] })
  });
  // 0x20 = 00100000: readSe = 0 (ue=0 → se=0), 后续...
  // 0x6A = 01101010: readSe = ...
  const { bytes, nal } = makeNal(62, new Uint8Array([0x20, 0x6A]));
  const r = CS.runNalPlugin(bytes, nal, "hevc");
  assert.ok(r);
});

test("registerPlugin / setEnabled / remove", () => {
  CS.clearPlugins();
  const id = CS.registerPlugin({ name: "p", nalType: 62, parse: () => ({}) });
  assert.ok(id);
  assert.strictEqual(CS.getPlugins().length, 1);
  CS.setPluginEnabled(id, false);
  assert.strictEqual(CS.getPlugins()[0].enabled, false);
  CS.removePlugin(id);
  assert.strictEqual(CS.getPlugins().length, 0);
});

test("disabled plugin does not run", () => {
  CS.clearPlugins();
  const id = CS.registerPlugin({ name: "p", codec: "hevc", nalType: 62, parse: () => ({ name: "x" }) });
  const { bytes, nal } = makeNal(62, new Uint8Array([0]));
  CS.setPluginEnabled(id, false);
  assert.strictEqual(CS.runNalPlugin(bytes, nal, "hevc"), null);
  CS.setPluginEnabled(id, true);
  assert.ok(CS.runNalPlugin(bytes, nal, "hevc"));
});

test("runSeiPlugin extracts payloadType and dispatches", () => {
  CS.clearPlugins();
  CS.registerPlugin({
    name: "sei-test", codec: "hevc", payloadType: 137,
    parse: (ctx, meta) => ({ children: [{ name: "pt", value: meta.payloadType }] })
  });
  const d = new Uint8Array([0, 0, 1, 0x4E, 0x01, 137, 0]);
  const nal = { offset: 0, length: d.length, type: 39, typeName: "NAL_SEI_PREFIX" };
  const r = CS.runSeiPlugin(d, nal, "hevc");
  assert.ok(r);
  assert.ok(JSON.stringify(r).includes("sei-test"));
});

test("registerPlugin rejects missing match type", () => {
  CS.clearPlugins();
  const ok = CS.registerPlugin({ name: "bad", parse: () => ({}) });
  assert.strictEqual(ok, false);
  assert.strictEqual(CS.getPlugins().length, 0);
});
