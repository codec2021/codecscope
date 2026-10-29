const { test } = require("node:test");
const assert = require("node:assert");
const fs = require("fs");
const path = require("path");

global.window = {};
eval(fs.readFileSync(path.join(__dirname, "..", "www/js/jpeg.js"), "utf8"));
const JpegParser = global.window.JpegParser || global.JpegParser;

// 标准 luma 量化表（zigzag 顺序，quality 50 基准）
const STD_LUMA = [16,11,12,14,12,10,16,14,13,14,18,17,16,19,24,40,
                  26,24,22,22,24,49,35,37,29,40,58,51,61,60,57,51,
                  56,55,64,72,92,78,64,68,87,69,55,56,80,109,81,87,
                  95,98,103,104,103,62,77,113,121,112,100,120,92,101,103,99];

// 用 IJG 公式生成某 quality 的量化表
function ijqTable(base, quality) {
  const scale = quality < 50 ? Math.floor(5000 / quality) : 200 - 2 * quality;
  return base.map(v => {
    const q = Math.floor((v * scale + 50) / 100);
    return q < 1 ? 1 : (q > 255 ? 255 : q);
  });
}

function makeJpeg(quantTable) {
  // SOI + DQT + EOI
  const dqtLen = 2 + 1 + 64; // 长度字段 + Pq/Tq + 64 值
  const bytes = [0xFF, 0xD8];
  bytes.push(0xFF, 0xDB);
  bytes.push((dqtLen >> 8) & 0xFF, dqtLen & 0xFF);
  bytes.push(0x00); // Pq=0 (8bit), Tq=0
  for (let i = 0; i < 64; i++) bytes.push(quantTable[i]);
  bytes.push(0xFF, 0xD9);
  return new Uint8Array(bytes);
}

function extractQuality(segments) {
  const results = [];
  function walk(seg) {
    if (!seg) return;
    if (seg.n && seg.n.indexOf("Quantization table ") === 0) results.push(seg.n);
    if (seg.c) seg.c.forEach(walk);
  }
  segments.forEach(s => walk(s.syntax));
  return results;
}

test("JPEG quality detection matches IJG for q50/80/95", () => {
  for (const q of [50, 80, 95]) {
    const r = JpegParser.parseJpeg(makeJpeg(ijqTable(STD_LUMA, q)));
    const qts = extractQuality(r.segments);
    assert.ok(qts.length > 0, `quality ${q}: 有量化表`);
    const label = qts[0];
    const m = label.match(/Approx quality factor = ([\d.]+)/);
    assert.ok(m, `quality ${q}: 有 quality 因子，实际: ${label}`);
    const detected = parseFloat(m[1]);
    assert.ok(
      Math.abs(detected - q) < 1.5,
      `quality ${q} 应检测为 ~${q}，实际: ${detected}`
    );
  }
});

test("JPEG parses SOI/DQT/EOI segments", () => {
  const r = JpegParser.parseJpeg(makeJpeg(STD_LUMA));
  assert.strictEqual(r.segments.length, 3);
  assert.strictEqual(r.segments[0].typeName, "SOI");
  assert.strictEqual(r.segments[1].typeName, "DQT");
  assert.strictEqual(r.segments[2].typeName, "EOI");
});

test("JPEG rejects non-JPEG data", () => {
  assert.strictEqual(JpegParser.parseJpeg(new Uint8Array([0, 1, 2, 3])), null);
});
