# Integral Image Normal 优化证据索引

## 当前结论摘要

| candidate family | 状态 | 证据 | 当前边界 |
| --- | --- | --- | --- |
| map-prep RVV masked stores | diagnostic-positive / production-probe-source | correctness、asm、board 5-run、Evidence Doctor 均闭合 | diagnostic helper 证明候选来源；production 结论看 Phase 050 |
| map-prep production probe | adopted weak-positive production-public | public compute correctness、asm、board 5-run、Evidence Doctor 均闭合；`prod_compute_*` mean / median 均 1.06x；用户已确认采纳 | 长期文档为 `doc-rvv/features/integral_image_normal-RVV.zh.md` |
| distance transform rewrite | deferred | Phase 000 未证明其为主成本，且行内依赖强 | 需要 full profile 或 component ablation |
| full / part normal output loop | deferred | 未进入本阶段 | 需要 production dataflow audit |
| average 3D gradient diff buffers | weak/unstable exact-PCL production-shaped diagnostic / no-production-now | correctness、asm、board 5-run、Evidence Doctor 已闭合；diff-only 大图 positive，但完整 profile / PCL component 存在退化频率和长尾 | test helper diagnostic；不建议生产化 |

## 标量路径与 RVV 路径差异

标量 map-prep 对每个内部像素逐点检查右侧和下侧 depth，并按分支写 0。RVV candidate 按 VL chunk（可变向量长度分块）批量加载当前、右邻和下邻 depth，生成 edge mask（边缘掩码），再用 masked byte store（掩码字节存储）写当前、右邻和下邻 map 位置。多次写 0 是 idempotent（幂等）操作，因此保持标量 map 语义。distance initialization 使用 `vmseq` 生成 0-mask，再在 `0.0f` 和 `far_distance` 之间选择。

Phase 050 将 map-prep 候选接入 production 后，public `compute()` 的 production-direct mean / median 为 1.06x。
因为两项各有 1/5 run 低于 1x，当前证据是 weak-positive；用户已确认按该证据采纳并保留 patch。

diff-buffer candidate 对应 `initAverage3DGradientMethod()` 内圈差分。标量路径按像素读取左右 / 上下邻点的 `x/y/z` 字段，写入 `diff_x` / `diff_y` 的前三个 float，第四通道和边界来自零初始化。RVV candidate 使用 `vlse32.v` 按 4-float stride 读取 `x/y/z`，用 `vfsub.vv` 计算差值，再用 `vsse32.v` 按 diff buffer stride 写回。Phase 030/040 将该局部候选放入 production-shaped profile 和 exact PCL boundary profile 后，完整链路仍不稳定，说明局部收益不足以支撑生产探针。

## 证据索引

| 证据 | 路径 / 命令 | 说明 |
| --- | --- | --- |
| correctness | `make run_test_compare` | Std/RVV 两侧各 5 个 gtest 通过，包含 public compute distance-map 对拍。 |
| asm | `make dump_test_rvv` / `make dump_bench_rvv` | RVV binary 中出现 map-prep 和 diff-buffer 目标 RVV 指令。 |
| board repeated | `log/board/repeated-summary.md` | map-prep diagnostic 稳定 positive；`prod_compute_*` production public weak-positive；diff-buffer profile total 弱 / 不稳定。 |
| doctor | `log/board/evidence_doctor.md` | Errors=3 / Warnings=21 / Suggestions=8，production-direct 两项为 warning，非 checksum error。 |
| matrix | `doc/phases/optimization-matrix.zh.md` | 记录 candidate family 的状态和下一动作。 |
| roadmap | `doc/optimization-roadmap.zh.md` | 记录后续 production probe 和其它候选。 |

## 结论边界

当前 evidence role（证据角色）分层如下：`map_prep_*` 是 diagnostic，`prod_compute_*` 是 production direct / production public。
它足以支持当前 map-prep production patch 的 adopted 状态，但不支持 diff-buffer 生产化，也不能证明：

- 所有 `PointInT` z / xyz 字段 layout 都有性能收益；当前 production gate 只覆盖 `RVVXYZAoSFloatLayout<PointInT>`。
- 所有 public `computeFeature()` 配置都已经加速；当前 board case 是 `PointXYZ -> Normal`、organized full image 和 `AVERAGE_DEPTH_CHANGE`。
- distance transform、integral image 构建、normal solver 或 output writes 已加速。
- 该候选优于未来可能出现的另一种 RVV family。
