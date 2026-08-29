# Correctness Tests

## 这个测试验证什么

当前只先验证一件事：`Hough3DGrouping::houghVoting()` 的 vote generation 诊断 helper
是否和标量参考链路保持一致。

随后又补了一层 production direct smoke，确认真实公开入口 `houghVoting()` 在
`PointXYZ` / `ReferenceFrame` 这条边界上能跑通，并和标量 build 保持同一 checksum。
这层测试只证明入口和回退链路没断，不证明当前生产 patch 值得采纳。

## 为什么需要

如果 scene vote 的线性组合、min/max reduction 或保序候选索引已经跑偏，后面的
RVV 分支就没有意义。

## 当前不覆盖什么

- accumulator scatter。
- `HoughSpace3D::vote()` / `voteInt()`。
- `findMaxima()`。
- production direct 的 board 性能是否值得采纳。
