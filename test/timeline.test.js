const { test } = require("node:test");
const assert = require("node:assert");
const fs = require("fs");
const path = require("path");

global.window = {};
eval(fs.readFileSync(path.join(__dirname, "..", "www/js/demux.js"), "utf8"));
const H = global.window.H26xDemux;

// 构造一个模拟时间轴：584 帧，barW=5px，帧间隙 frameGap=4px，每帧间隔 9px
function makeXs(count, barW, gap) {
  const step = barW + gap;
  const xs = new Array(count);
  for (let i = 0; i < count; i++) xs[i] = i * step;
  return xs;
}

test("sliceIndexAtX: 空数组返回 -1", () => {
  assert.strictEqual(H.sliceIndexAtX([], 5, 100), -1);
});

test("sliceIndexAtX: x 在第一个 slice 之前返回 -1", () => {
  const xs = makeXs(584, 5, 4);
  assert.strictEqual(H.sliceIndexAtX(xs, 5, -1), -1);
});

test("sliceIndexAtX: x 超出最后一个 slice 之后返回 -1（回归：之前一直返回最后一个）", () => {
  const xs = makeXs(584, 5, 4);
  const xsLast = xs[xs.length - 1]; // 583 * 9 = 5247
  // 超出末尾（如滚动到末尾后 hover 右端），应返回 -1 而非最后一个 slice
  assert.strictEqual(H.sliceIndexAtX(xs, 5, xsLast + 6), -1);
  assert.strictEqual(H.sliceIndexAtX(xs, 5, xsLast + 1000), -1);
});

test("sliceIndexAtX: x 落在色块内返回正确索引", () => {
  const xs = makeXs(584, 5, 4);
  assert.strictEqual(H.sliceIndexAtX(xs, 5, 0), 0);      // 第一个 slice 起点
  assert.strictEqual(H.sliceIndexAtX(xs, 5, 3), 0);      // 第一个 slice 内
  assert.strictEqual(H.sliceIndexAtX(xs, 5, 9), 1);      // 第二个 slice 起点
  assert.strictEqual(H.sliceIndexAtX(xs, 5, 450), 50);   // 450 / 9 = 50
});

test("sliceIndexAtX: x 落在帧间隙返回左侧最近 slice（回归：之前返回 -1 导致点击落空）", () => {
  const xs = makeXs(584, 5, 4);
  // slice 0: [0,5], gap [5,9], slice 1: [9,14]
  assert.strictEqual(H.sliceIndexAtX(xs, 5, 6), 0);  // 落在 gap，返回 slice 0
  assert.strictEqual(H.sliceIndexAtX(xs, 5, 8), 0);  // 落在 gap，返回 slice 0
  assert.strictEqual(H.sliceIndexAtX(xs, 5, 13), 1); // 落在 gap，返回 slice 1
});

test("sliceIndexAtX: 边界值", () => {
  const xs = makeXs(584, 5, 4);
  const xsLast = xs[xs.length - 1];
  assert.strictEqual(H.sliceIndexAtX(xs, 5, xs[0]), 0);              // 正好第一个起点
  assert.strictEqual(H.sliceIndexAtX(xs, 5, xsLast), 583);           // 正好最后一个起点
  assert.strictEqual(H.sliceIndexAtX(xs, 5, xsLast + 5), 583);       // 最后一个 slice 右边界
});

test("回归：往后点再往前点，坐标映射保持一致（scrollLeft 已含在 x 中）", () => {
  // 模拟真实场景：rect.left = wrapLeft - scrollLeft
  // x = clientX - rect.left 已经是完整坐标，sliceIndexAtX 不再额外加 scrollLeft
  const xs = makeXs(584, 5, 4);
  const wrapLeft = 12;

  function clientXAt(fullX, scrollLeft) {
    // 反向推导 clientX：fullX = clientX - (wrapLeft - scrollLeft)
    return fullX + wrapLeft - scrollLeft;
  }
  function fullXAt(clientX, scrollLeft) {
    const rectLeft = wrapLeft - scrollLeft;
    return clientX - rectLeft;
  }

  // 往后滚动到 scrollLeft=2240 后，点击可见区域左端 clientX=32（即可见 x=20）
  const scrollLeft = 2240;
  const clientX = 32;
  const fullX = fullXAt(clientX, scrollLeft);
  assert.strictEqual(fullX, 2260); // 32 - (12-2240) = 2260
  // 应选中 slice 2260/9 = 251（而非 500，即不能重复加 scrollLeft）
  assert.strictEqual(H.sliceIndexAtX(xs, 5, fullX), 251);

  // 往前滚动到 scrollLeft=0 后，点击可见区域左端 clientX=32
  const fullX0 = fullXAt(clientX, 0);
  assert.strictEqual(fullX0, 20);
  assert.strictEqual(H.sliceIndexAtX(xs, 5, fullX0), 2); // 20/9 = 2
});

test("回归：单帧时间轴（帧数少、barW 大、间隙大）也能命中", () => {
  // 50 帧，barW=17，frameGap=14，每帧间隔 31
  const xs = makeXs(50, 17, 14);
  // 之前 bug：间隙(14px)比色块(17px)小不了多少，点击落在间隙返回 -1
  assert.strictEqual(H.sliceIndexAtX(xs, 17, 167), 5);  // 167/31 = 5.38 → slice 5
  assert.strictEqual(H.sliceIndexAtX(xs, 17, 503), 16); // 503/31 = 16.2 → slice 16
});
