const { test } = require("node:test");
const assert = require("node:assert");
const fs = require("fs");
const path = require("path");

global.window = {};
eval(fs.readFileSync(path.join(__dirname, "..", "www/js/demux.js"), "utf8"));
const H = global.window.H26xDemux;

test("isFlv detects FLV magic", () => {
  const d = new Uint8Array([0x46, 0x4C, 0x56, 0x01, 0x05, 0, 0, 0, 9]);
  assert.strictEqual(H.isFlv(d), true);
});

test("isFlv rejects non-FLV", () => {
  assert.strictEqual(H.isFlv(new Uint8Array([0, 1, 2, 3])), false);
});

test("isWebm detects EBML magic", () => {
  const d = new Uint8Array([0x1A, 0x45, 0xDF, 0xA3, 0, 0, 0, 0]);
  assert.strictEqual(H.isWebm(d), true);
});

test("isTs detects sync bytes", () => {
  const d = new Uint8Array(188 * 3);
  d[0] = 0x47;
  d[188] = 0x47;
  d[376] = 0x47;
  assert.strictEqual(H.isTs(d), true);
});

test("isTs rejects non-TS", () => {
  const d = new Uint8Array(188 * 3);
  assert.strictEqual(H.isTs(d), false);
});

test("parseTsInfo parses PAT/PMT", () => {
  // 构造最小 MPEG-TS：PAT (PID 0) + PMT (PID 0x100) 含 H.264 (PID 0x101)
  const packets = [];

  function tsPacket(pid, payload, pusi) {
    const pkt = new Uint8Array(188);
    pkt[0] = 0x47;
    pkt[1] = ((pusi ? 1 : 0) << 6) | ((pid >> 8) & 0x1F);
    pkt[2] = pid & 0xFF;
    pkt[3] = 0x10; // 仅 payload
    pkt.set(payload, 4);
    for (let i = 4 + payload.length; i < 188; i++) pkt[i] = 0xFF;
    return pkt;
  }

  // PAT: table_id=0, section_length, ts_id(2), version(1), sec(1), last(1),
  //      然后 program_number(2) + PMT PID(2)
  // PAT payload: pointer_field=0, table_id=0x00, section_length=13, ...
  const patPayload = new Uint8Array([
    0,        // pointer_field
    0x00,     // table_id = PAT
    0xB0, 0x0D, // section_length = 13 (含 0xB0 高2位)
    0x00, 0x01, // transport_stream_id
    0xC1,     // version/current_next
    0x00, 0x00, // section_number, last_section_number
    0x00, 0x01, // program_number = 1
    0xE1, 0x00, // PMT PID = 0x100 (高3位 0xE1 = 111 00001)
  ]);
  packets.push(tsPacket(0, patPayload, 1));

  // PMT: table_id=0x02, program_info, stream H.264 (type 0x1B, PID 0x101)
  const pmtPayload = new Uint8Array([
    0,        // pointer_field
    0x02,     // table_id = PMT
    0xB0, 0x12, // section_length = 18
    0x00, 0x01, // program_number
    0xC1,     // version
    0x00, 0x00, // section_number, last_section_number
    0xE1, 0x00, // PCR PID = 0x100
    0xF0, 0x00, // program_info_length = 0
    0x1B,     // stream_type = H.264
    0xE1, 0x01, // elementary PID = 0x101
    0xF0, 0x00, // ES_info_length = 0
  ]);
  packets.push(tsPacket(0x100, pmtPayload, 1));

  const d = new Uint8Array(packets.length * 188);
  packets.forEach((p, i) => d.set(p, i * 188));

  const info = H.parseTsInfo(d);
  assert.ok(info, "parseTsInfo 返回非空");
  assert.strictEqual(info.format, "MPEG-TS");
  assert.strictEqual(info.programs.length, 1);
  assert.strictEqual(info.programs[0].number, 1);
  assert.strictEqual(info.streams.length, 1);
  assert.strictEqual(info.streams[0].streamType, 0x1B);
  assert.strictEqual(info.streams[0].handler, "vide");
  assert.strictEqual(info.streams[0].codec, "avc1");
});
