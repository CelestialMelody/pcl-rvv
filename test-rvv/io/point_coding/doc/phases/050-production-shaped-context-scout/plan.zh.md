# Phase 050：decode production-shaped context scout 计划

## 阶段意图和边界

Phase 040 证明 `decode_contiguous_*` 在 10-run board repeated（重复板卡性能采集）中保持 weak-positive diagnostic（弱正向诊断证据），但它只写入 `std::vector<PointXYZ>`，不覆盖 `PointCoding::decodePoints` 的对象状态。本阶段在 `test-rvv/io/point_coding/**` 内新增 test-only production-shaped context scout（测试专用生产形态上下文侦察）：用真实 `pcl::octree::PointCoding<PointXYZ>` 标量对象产生 reference path（参考链路），RVV candidate 写入同形状的 `PointCloud<PointXYZ>` range（输出点云区间），观察 decode RVV 在更接近 production 的输出容器、begin/end offset 和 diff vector 状态下是否仍成立。

本阶段不修改 `io/include/pcl/compression/point_coding.h`，不进入 production integration loop（生产接入闭环），也不创建 `doc-rvv/io/point_coding-RVV.zh.md`。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| correctness | `make run_test_compare` 已覆盖 component ablation；尚未覆盖真实 `PointCoding` object state reference。 |
| board evidence | Phase 040 decode-only 10-run median 1.15x 到 1.29x，Evidence Doctor `Errors=0，Warnings=1`。 |
| production boundary | 真实 production decode 仍是标量；当前没有 dispatch（分流逻辑）或 production direct（真实生产入口直连）证据。 |
| risk | `decode_contiguous_64` 有 long-tail warning；context scout 必须保留 min/median/max，不用单次值做结论。 |

## validated_scope / unvalidated_scope

| scope | 内容 |
| --- | --- |
| validated_scope | `PointXYZ`、float output、AoS（结构数组）布局、contiguous diff vector、`beginIdx/endIdx` 输出区间、test-only helper boundary。 |
| unvalidated_scope | 泛型 `PointT`、真实 octree decompression pipeline（解压流水线）、完整 iterator 生命周期、entropy context（熵编码上下文）、其它 `Scalar` 或 production dispatch。 |
| phase_closeout_boundary | 本阶段最多关闭 `decode_context_*` 的 test-only diagnostic 条目；不能关闭 production point_coding patch。 |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 增加 production-shaped decode helper | `include/impl/point_coding_support.hpp` | 提供标量生产对象 reference 和 RVV candidate-to-cloud range helper；中文注释说明证据边界。 |
| 增加 correctness test | `src/test_point_coding.cpp` | 新增 gtest 对比真实 `PointCoding<PointXYZ>::decodePoints` 和 candidate output cloud range。 |
| 增加 bench case | `src/bench_point_coding.cpp` | 新增 `decode_context_16/64/256/1024/4096/16384`，输出格式仍可被现有 repeated summary / Evidence Doctor 解析。 |
| QEMU / asm / board 验证 | Make target | 运行 `make run_test_compare`、`make run_qemu_bench_smoke`、`make dump_bench_rvv`；若本地验证通过，运行 decode_context board repeated + Evidence Doctor。 |
| 文档刷新 | phase result、matrix、roadmap、evaluation、bench/evidence | 回填命令、summary、Warnings 和 EvidenceDecision。 |

## Evidence Doctor 和 registry 规则

输出目录使用 `log/board/repeated_phase050_decode_context`，避免覆盖 Phase 040 和默认 12-case summary。Evidence Doctor 输入由当前 topic wrapper `script/generate_point_coding_evidence_manifest.py` 生成；如果出现 Error，先修复或降级，不用该 evidence 关闭阶段。当前 topic 尚无 `log/evidence_registry.json`，阶段结果用路径限定扫描记录 artifact tracking。

## 板卡复跑预算和决策桶

本阶段预算为 10-run context-only repeated board：

```bash
make collect_board_repeated run_board_repeated_evidence_doctor \
  POINT_CODING_REPEATED_RUNS=10 \
  POINT_CODING_REPEATED_DIR=log/board/repeated_phase050_decode_context \
  BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_context_*'
```

若 Evidence Doctor 无 Error 且各规模 median > 1，结论写 `production-shaped diagnostic weak-positive`。若出现 checksum mismatch、严格 A/B metadata Error 或高频退化，结论降级为 `unstable diagnostic`，不进入 PI1。长尾 warning 必须保留 min/median/max，并说明是否改变 decision bucket。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断），仍是 test-only。 |
| A/B boundary | Std side 使用真实 `PointCoding<PointXYZ>` 标量对象；RVV side 使用 test helper candidate 写入同形状 cloud range。 |
| 当前决策问题 | decode RVV 是否值得继续到更完整 octree-shaped context 或 PI1 前置审计。 |
| diagnostic 是否可外推到 production | unknown / no。它比 `decode_contiguous_*` 更接近 production，但仍不是真实 public entry 或 production dispatch。 |
| comparison-boundary / baseline mismatch 风险 | yes。Std side 是 production scalar object，RVV side 是 test helper；因此即使 positive，也只能说明 scout 值得继续。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no；弱正向只允许继续 test-only context 或请求用户授权 PI1 计划。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes。 |

## 完成条件和下一步

| 条件 | 判定 |
| --- | --- |
| context weak-positive | correctness / QEMU / asm 通过，board 10-run median 全部大于 1，Evidence Doctor 无 Error。 |
| unstable | 任一规模高频退化、doctor Error 或长尾改变 decision bucket。 |
| production integration | 本阶段不能直接进入；只有 context scout positive 且用户授权时，下一轮才写 PI1 production integration plan。 |

`next_phase_default`：若本阶段 positive，默认进入 `055-full-octree-context-scout` 或停在用户授权点请求 PI1；若本阶段 unstable，则保持 no-production，并把 decode 路线降级为 test-rvv diagnostic asset。
