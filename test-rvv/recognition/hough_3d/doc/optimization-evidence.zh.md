# Optimization Evidence

## 目前状态

当前已有两层证据链：

- diagnostic / production-shaped diagnostic：vote generation helper 正向。
- production direct：真实 `houghVoting()` repeated board 为 `neutral`，不支持采纳。
- accumulator / interpolation ablation：关闭插值后 full 入口仍是 `neutral`，只保留弱信号。
- default distance weight production direct：默认 `use_distance_weight=false` 仍是
  `neutral`，不支持采纳。

## 下一步

1. 保留当前 production patch 作为 attempted 证据，不标 adopted。
2. 不继续扩 `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA`，因为当前默认入口没有正收益。
3. 只有 production direct 出现可接受正收益后，才创建 `doc-rvv`。
4. 不继续单独跑 `no-interpolation + no-distance-weight`，因为 phase 020 和 phase 030
   已分别证明相关维度都只是 near-threshold neutral。
