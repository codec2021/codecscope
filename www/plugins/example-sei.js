// 示例 SEI 插件 —— 解析 HEVC payloadType 137 的自定义数据
//
// 用法：
//   1. 点击页面右上角 "⚙ Plugin" 按钮，选择本文件。
//   2. 打开一个含 payloadType 137 SEI 的 HEVC 码流。
//   3. 在 NAL 列表中点击该 SEI NAL，语法树里会出现插件解析的字段。
//
// 插件 API：
//   CodecScope.registerPlugin({
//     name: "插件名",
//     codec: "hevc",            // 可选，"hevc" / "avc"，缺省匹配所有
//     payloadType: 137,         // 单个类型
//     // 或 payloadTypes: [137, 138]
//     parse: function (ctx, meta) {
//       // ctx 位读取器：ctx.readBits(n) / ctx.readUe() / ctx.readSe() / ctx.readU(n)
//       // ctx.bitsLeft 剩余位数
//       // meta: { codec, payloadType, payloadSize, bitReader }
//       return {
//         name: "组名",            // 可选
//         children: [
//           { name: "字段名", value: 值 },              // 叶子节点
//           { name: "子组", children: [ ... ] }         // 子节点
//         ]
//       };
//     }
//   });
//

CodecScope.registerPlugin({
  name: "Example SEI (payloadType 137)",
  codec: "hevc",
  payloadType: 137,
  parse: function (ctx, meta) {
    var version = ctx.readUe();
    var flags = ctx.readBits(8);
    var count = ctx.readUe();

    var items = [];
    for (var i = 0; i < count && ctx.bitsLeft > 0; i++) {
      items.push({
        name: "item[" + i + "]",
        children: [
          { name: "id", value: ctx.readUe() },
          { name: "size", value: ctx.readUe() }
        ]
      });
    }

    return {
      name: "example_sei(137)",
      children: [
        { name: "version", value: version },
        { name: "flags", value: flags },
        { name: "count", value: count },
        { name: "items", children: items }
      ]
    };
  }
});
