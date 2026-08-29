# geometric_consistency RVV 生产说明

## 当前状态

`recognition/include/pcl/recognition/impl/cg/geometric_consistency.hpp` 当前已采纳窄范围 production RVV。
`GeometricConsistencyGrouping<PointModelT, PointSceneT>::clusterCorrespondences()` 在 `__RVV10__`
下先尝试 `geometricConsistencyPairwiseConsistencyRVV()`，失败后自然回落原标量 pairwise loop；
`recognize()`、`std::sort`、`taken_corresps`、RANSAC rejector 和 transformation 输出仍保持原实现。

当前采用依据由两部分组成：

1. `run_upstream_test_compare` 通过，真实上游 `test/recognition/test_recognition_cg.cpp` 直接覆盖
   `GeometricConsistencyGrouping<PointType, PointType>::recognize()`，Std/RVV 两侧 correctness 一致。
2. `phase010-cluster-growth-diagnostic` 和 `phase020-cluster-growth-production-probe` 的 repeated board
   都显示稳定正向，其中 phase 020 median `2.510x`、`B/A < 1` 为 `0/5`、checksum stable。它们仍是
   diagnostic proxy（诊断代理）和 production-shaped diagnostic，不等价于 production-direct board bench，
   但都和生产 helper 共享同一 pairwise kernel 形状，可作为采纳参考；phase 020 仍未形成新的 production boundary。

证据路径：

- upstream correctness: `make -C test-rvv/recognition/geometric_consistency run_upstream_test_compare`
- production helper asm: `make -C test-rvv/recognition/geometric_consistency check_gc_rvv_asm`
- diagnostic board proxy: `test-rvv/recognition/geometric_consistency/log/board/repeated_phase010_cluster_growth_diagnostic/summary.md`
- diagnostic board doctor: `test-rvv/recognition/geometric_consistency/log/board/repeated_phase010_cluster_growth_diagnostic/evidence_doctor.md`
- diagnostic board manifest: `test-rvv/recognition/geometric_consistency/log/board/repeated_phase010_cluster_growth_diagnostic/evidence_manifest.json`
- diagnostic board proxy (phase 020): `test-rvv/recognition/geometric_consistency/log/board/repeated_phase020_cluster_growth_production_probe/summary.md`
- diagnostic board doctor (phase 020): `test-rvv/recognition/geometric_consistency/log/board/repeated_phase020_cluster_growth_production_probe/evidence_doctor.md`
- diagnostic board manifest (phase 020): `test-rvv/recognition/geometric_consistency/log/board/repeated_phase020_cluster_growth_production_probe/evidence_manifest.json`
- phase result: `test-rvv/recognition/geometric_consistency/doc/phases/010-cluster-growth-diagnostic/result.zh.md`
- phase result (phase 020): `test-rvv/recognition/geometric_consistency/doc/phases/020-cluster-growth-production-probe/result.zh.md`
- evaluation: `test-rvv/recognition/geometric_consistency/doc/geometric_consistency-evaluation.zh.md`

## 函数语义和标量路径

`recognize()` 先校验输入，再进入 `clusterCorrespondences()`；后者先排序 correspondences，
然后用 `taken_corresps` 构造 consensus set，逐候选检查 pairwise distance consistency，最后交给
RANSAC rejector 生成 transformation。

标量路径仍然是原有实现：

1. `std::sort` 对 correspondences 排序。
2. `taken_corresps` 控制 consensus growth。
3. pairwise predicate 读取 `scene_point_k - scene_point_j` 和 `model_point_k - model_point_j`，
   计算两侧范数差，再和 `gc_size_` 比较。
4. RANSAC rejector 和 transformation 输出保持不变。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch / fallback | adopted | `clusterCorrespondences()` 先试 RVV helper，失败后回退标量 | upstream correctness、asm | 非 RVV 构建和不满足 gate 的输入回退标量 |
| layout / traits gate | adopted narrow | 只对 `RVVXYZAoSFloatLayout` 命中的 `x/y/z` AoS 路径开放 RVV | 源码审计、upstream test | 其它点型或布局保持标量 |
| staging / reduction | adopted narrow | pairwise predicate 在 VL chunk 内 gather、sqrt、compare | asm、diagnostic board proxy | 外层 `taken_corresps` growth 仍标量 |
| production scope | adopted narrow | 当前只接入 inner pairwise predicate，未重写排序 / RANSAC / 输出 | 源码 + upstream correctness | 若要扩到 full growth，需新 phase |
| 后续优化 | deferred | full cluster growth production integration 需要新的 board / bench 边界 | roadmap / matrix | 先把当前 adopted helper 维护好 |

## 覆盖范围与 fallback

| 范围 | 当前状态 | 证据 / 原因 |
| --- | --- | --- |
| `PointXYZ-like` `x/y/z` AoS 路径 | adopted | `RVVXYZAoSFloatLayout` gate 通过 |
| 非 RVV 构建 | scalar fallback | `__RVV10__` 未启用时自然回标量 |
| 输入为空或索引非法 | scalar fallback | helper 早退，保持原语义 |
| `taken_corresps` growth loop | scalar | 当前仍在生产主函数内 |
| `std::sort` / RANSAC / transformations | scalar | 未进入 RVV 证据边界 |

## 详细设计

1. 生产 helper 先把 candidate correspondence 的 model / scene 索引转成 32-bit byte offset。
2. 再用公共 RVV load wrapper 读取 `x/y/z` 三个字段，复刻 pairwise distance consistency。
3. `vfsqrt` 和 `vfabs` 只用于局部距离判定，不改变输出排序和后段状态机。
4. public 入口保留原标量主体，RVV 失败后继续走 Std fallback。

## 数值算例

若某个 consensus candidate 在 model 侧位移为 `(dx, dy, dz)`，scene 侧位移为
`(sx, sy, sz)`，则 predicate 实际比较的是：

```text
abs( sqrt(sx^2 + sy^2 + sz^2) - sqrt(dx^2 + dy^2 + dz^2) ) <= gc_size_
```

RVV helper 只是把这段标量算式按 VL chunk 做并行，不改变这个判定式本身。

## Bench case 说明

| case | 入口 | 计时边界 | 证明点 | 不能证明 |
| --- | --- | --- | --- | --- |
| `pairwise_consistency_batch` | `bench_gc` | 固定 consensus set 上的 batch predicate | 当前 adopted pairwise kernel 是否有收益 | 不能直接外推到 production `recognize()` 的整体性能 |

`phase010-cluster-growth-diagnostic` 和 `phase020-cluster-growth-production-probe` 的 repeated board
仍是当前可复用的诊断代理，说明同一 pairwise kernel 在更宽的 cluster growth 形状里也有稳定收益，但它们
都不等于 production-direct board bench，phase 020 也没有再长出新的生产边界。

## 测试、QEMU、反汇编和板卡证据

- correctness: `make -C test-rvv/recognition/geometric_consistency run_test_compare` 通过。
- production direct correctness: `make -C test-rvv/recognition/geometric_consistency run_upstream_test_compare` 通过。
- asm: `make -C test-rvv/recognition/geometric_consistency check_gc_rvv_asm` 通过，匹配预期 RVV 指令。
- diagnostic board proxy: phase 010 / phase 020 repeated board 都正向，其中 phase 020 median `2.510x`，`0/5` 退化。
- Evidence Doctor: `Errors=0, Warnings=0, Suggestions=1`，仅缺环境 metadata。

## 正确性与高效性证据链

当前证据链覆盖：

- correctness: 生产入口 `recognize()` 在上游测试源上 Std/RVV 一致。
- performance: 当前 board 仍是 diagnostic proxy，但已显示稳定收益；phase 020 没有导出新的生产边界。
- boundary: 当前只覆盖 `PointXYZ-like` `x/y/z` AoS 路径和 pairwise predicate。
- risk: full cluster growth、排序、RANSAC 和 transformation 输出仍保留标量；若要继续扩展，应新开 production-direct phase。

## 生产接入评估

当前 production patch 采纳为窄范围生产行为。它足够说明当前 pairwise kernel 值得保留，
但不自动证明 full cluster growth、其它 row source policy 或更宽点型泛型结论。
phase 020 进一步确认了 growth 形态仍然正向，但仍未形成新的 production boundary。
后续若要扩成更宽的 production helper，必须重新补 production-direct bench、board、asm 和 Evidence Doctor。
