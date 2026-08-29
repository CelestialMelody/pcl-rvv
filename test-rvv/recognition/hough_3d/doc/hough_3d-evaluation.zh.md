# Hough 3D 函数级评估

## 函数级结论

`recognition/include/pcl/recognition/impl/cg/hough_3d.hpp` 中的
`Hough3DGrouping::houghVoting()` 是当前 RVV 评估入口。它先把
`model_scene_corrs_` 中的 correspondence（对应关系）展开成 scene vote，
再做 `d_min / d_max` reduction（规约），最后把候选写入 `HoughSpace3D`。

当前阶段先做 vote generation 诊断，不接入 production。原因很直接：
scatter 到 Hough accumulator、`vote()` / `voteInt()` 的邻域写入和 `findMaxima()`
都还没有独立证据，先把前半段和 accumulator 分开更容易审查。

后来又补了 production direct probe，结果是 full 入口 `neutral`，而且 5/5 都低于 1。
这说明当前 helper 的局部收益没有转成可采纳的 production 证据。

phase 020 关闭 `use_interpolation` 做消融后，full 入口 3-run median 为 `1.002x`。
这个结果说明 `voteInt()` 插值成本很高，但仍不足以支撑把当前 production patch 采纳。

phase 030 又补测源码默认的 `use_distance_weight=false` 配置，production direct
5-run median 为 `1.005x`，`1/5` 低于 `1.0`，Evidence Doctor 报
`1 Warning / 1 Suggestion`。默认配置没有推翻不采纳结论。

## 范围和目标源码

- 主源码：`recognition/include/pcl/recognition/impl/cg/hough_3d.hpp`
- 支撑文件：`recognition/include/pcl/recognition/cg/hough_3d.h`
- 诊断 helper：`test-rvv/recognition/hough_3d/include/impl/hough_3d_candidates.hpp`

## 后续方向

- 先把 vote generation 的正确性和 path-hit 做实。
- 再决定是否单独评估 accumulator scatter 或 maxima scan。
- `accumulator-scatter-audit` 已完成初筛，结果接近阈值；继续做 `voteInt()` RVV
  需要新的明确实现阶段和更强证据。
- 默认 distance weight 配置已补测，仍为 `neutral`。
- 当前没有 adopted production behavior，所以不创建 `doc-rvv`。
