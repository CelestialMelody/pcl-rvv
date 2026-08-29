# BRISK 2D 优化证据索引

| candidate family | 状态 | 代码 / 证据 | 采用或暂缓原因 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| portable scalar fallback | adopted | `keypoints/src/brisk_2d.cpp`、`run_test_compare` | 非 SSSE3 / 非 RVV 平台不再报空实现，按 RISC-V portable scalar 语义生成派生层。 | 不追求与 SSSE3 `_mm_avg_epu8` full-block 舍入逐位一致。 |
| RVV `halfsample` stride-load | adopted with near-threshold note | `vlse8` + `vzext` + `/4` + `vse8`，Phase 010 current repeated board median 1.047x / 1.039x | 收益弱但无反向，代码形态简单，fallback 清楚，是 `constructPyramid` 组合收益的一部分。 | 不单独写成强加速；若后续完整 BRISK pipeline 中 halfsample 成本很低，可维持当前实现。 |
| RVV `twothirdsample` stride-load | adopted | `vlse8` + `vzext` + weighted `/9` + `vsse8`，Phase 010 current median 1.108x | 贡献稳定弱正向，是组合收益主来源。 | 暂不继续尝试复杂 shuffle / lookup 形态，因为当前实现已简单且正向。 |
| `ScaleSpace::constructPyramid` helper chain | adopted | Phase 010 current repeated board median 1.136x，0/5 反向 | 真实生产 helper 链正向，支持保留 production patch。 | public compute synthetic case 显示端到端 near-neutral，不能外推到真实 workload 显著加速。 |
| `getValue` / AGAST score / keypoint refinement | deferred | 未在本阶段修改 | 这些阶段包含插值、深分支 detector 和输出状态机，当前证据只说明 downsample。 | 需要 profile 或新的 phase 证明它们是热点并建立 correctness oracle。 |
