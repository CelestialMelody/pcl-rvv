# color_coding 函数级评估

## S2 函数级评估

`ColorCoding<PointT>` 是 octree compression（八叉树压缩）中的颜色编码组件。`OctreePointCloudCompression` 在遍历 leaf（叶节点）时调用它，把点的 RGBA 颜色字段编码成 average color（平均颜色）和 differential color（差分颜色）两组 byte stream（字节流）。

当前源码入口在 `io/include/pcl/compression/color_coding.h`：

| function | scalar path | RVV opportunity | current boundary |
| --- | --- | --- | --- |
| `encodeAverageOfPoints` | 按 `indices` gather 点颜色，累加 R/G/B，除以 leaf size，右移 bit reduction 后写入 average vector | indexed gather + vector reduction | leaf size 可能短；只输出 3 bytes |
| `encodePoints` | 第一遍求平均；第二遍逐点计算 `(avg ^ color) >> reduction` 并 push diff bytes；最后写 average bytes | indexed gather、RGB 分量拆分、XOR diff | 两遍 gather 和 `push_back` 成本可能吞掉收益 |
| `decodePoints` | 读取 average / diff byte stream，连续写回 `[begin,end)` 点颜色字段 | contiguous store 更适合 RVV | 只覆盖连续 output 段，不覆盖 entropy decode |
| `setDefaultColor` | 连续写默认白色 `0x00ffffff` | contiguous store | 简单 fill，收益可能小 |

初步判断：进入 diagnostic（诊断）/ component ablation（组件消融），不直接修改 production。第一问是 color coder 的 byte average / encode / decode 是否在隔离组件里正向，且整数舍入、RGBA offset 和 bit reduction 语义可控。

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `ColorCoding::encodeAverageOfPoints` | production scalar helper | 生成每个 leaf 的平均颜色 | octree compression traversal | entropy color stream | production boundary reference | `io/include/pcl/compression/color_coding.h` |
| `ColorCoding::encodePoints` | production scalar helper | 生成 average + differential color stream | octree compression traversal | entropy color stream | production boundary reference | `io/include/pcl/compression/color_coding.h` |
| `ColorCoding::decodePoints` | production scalar helper | 从 average/diff stream 恢复输出点颜色 | octree decompression traversal | output cloud | production boundary reference | `io/include/pcl/compression/color_coding.h` |
| `test-rvv/io/color_coding/include/color_coding.h` | test support aggregator | 测试专用聚合入口 | gtest / bench | internal support header | diagnostic support | `test-rvv/io/color_coding/include/color_coding.h` |
| `test-rvv/io/color_coding/src/test_color_coding.cpp` | correctness gate | 手算期望值，对拍 reference 与 candidate | `make run_test_compare` | QEMU run logs | correctness evidence | `test-rvv/io/color_coding/src/test_color_coding.cpp` |
| `test-rvv/io/color_coding/src/bench_color_coding.cpp` | bench wrapper | 隔离 encode/decode/default component timing | board bench target | summary / Evidence Doctor | board performance input | `test-rvv/io/color_coding/src/bench_color_coding.cpp` |
| `doc/testing-overview.zh.md` | topic-local doc role | target 粒度审计、运行入口和覆盖矩阵 | README / reviewer | correctness / bench / evidence docs | reviewability evidence | `test-rvv/io/color_coding/doc/testing-overview.zh.md` |
| `doc/correctness-tests.zh.md` | topic-local doc role | gtest 字典和正确性证明边界 | testing overview | test source / helper | correctness explanation | `test-rvv/io/color_coding/doc/correctness-tests.zh.md` |
| `doc/benchmark-and-evidence.zh.md` | topic-local doc role | bench label、board repeated、Doctor 和 registry | testing overview | summary / manifest / Doctor / registry | evidence explanation | `test-rvv/io/color_coding/doc/benchmark-and-evidence.zh.md` |
| `doc/optimization-evidence.zh.md` | topic-local doc role | candidate family 到证据和 decision 的映射 | evaluation / roadmap | matrix / phase result | decision index | `test-rvv/io/color_coding/doc/optimization-evidence.zh.md` |
| `doc/test-support-code-map.zh.md` | topic-local doc role | 聚合头、internal helper、test / bench / script / output 定位 | README / reviewer | topic test assets | code map | `test-rvv/io/color_coding/doc/test-support-code-map.zh.md` |

## 证据需求

| evidence | required for phase 000 | boundary |
| --- | --- | --- |
| correctness | yes | QEMU / local run can prove same-chain semantics |
| asm attribution（反汇编归属） | yes before performance decision | 只证明 candidate binary has RVV instructions |
| board performance（板卡性能） | yes for speed conclusion | 只证明 component helper，不能证明 full compression |
| Evidence Doctor（证据体检） | yes before EvidenceDecision | benchmark / board summary must be checked |

## 当前诊断证据链

当前 evidence role（证据角色）是 `mixed_component_production_shaped_and_decode_shape_diagnostic`：同一批板卡 repeated summary 同时包含 component ablation（组件消融）、production-shaped diagnostic（生产形态诊断）和 phase 050 的 decode implementation-shape diagnostic（decode 实现形态诊断）。A/B boundary（对比边界）仍是 test helper / production-shaped helper，不是 public overload（公开入口重载）或 production detail helper（生产内部 helper）。板卡 repeated summary 位于 `test-rvv/io/color_coding/log/board/component_repeat_5/summary.md`，Evidence Doctor 位于 `test-rvv/io/color_coding/log/board/component_repeat_5/evidence_doctor.md`。

phase 050 rerun（复跑）取代 phase 020 的同路径板卡数字成为 diagnostic current truth（诊断当前事实）。当前 Evidence Doctor 为 `Errors=2, Warnings=11, Suggestions=6`。两个 Error 都来自 staged-store decode 候选：`ps_decode_points_staged_leaf257` 和 `ps_decode_points_staged_leaf4096`。直接 `ps_decode_points_leaf4096` 现在降级为 Warning，但 median 只有 1.0409x、min 为 0.9116x，仍不能进入 production candidate。encode average、encode points 和 default 的生产形态证据曾保持正向，但 phase 060 / 070 证明这些 diagnostic positive 不能外推成 production adoption（生产采纳）。

| component | current board signal | Evidence Doctor handling | production meaning |
| --- | --- | --- | --- |
| `encodeAverageOfPoints` | leaf 31/257/1024/4096 median 约 1.5865x / 2.6640x / 1.9824x / 1.8934x | group_outlier warnings；必须按 case 单独报告 | 只支持 production-shaped precheck，不支持直接 production patch |
| `encodePoints` | leaf 31/257/1024/4096 median 约 1.2059x / 1.2826x / 1.2313x / 1.2411x | no case-specific Error | 支持下一阶段验证真实 PCL 点类型和 caller shape |
| `decodePoints` | leaf 31/257/1024/4096 median 约 1.0268x / 1.0596x / 1.0481x / 1.0435x | 多个 near-threshold suggestion | decode 收益较弱，必须与 default / encode 分开批准 |
| `setDefaultColor` | 4096/16384 median 约 1.2294x / 1.2470x | 16384 long-tail warning | 支持下一阶段生产形态预检 |
| `ps_encodeAverageOfPoints` | `PointXYZRGBA` leaf 257/4096 median 约 2.5073x / 1.9590x | group_outlier warning；必须单独报告 | 支持 PI1 计划，不支持直接采纳 |
| `ps_encodePoints` | `PointXYZRGBA` leaf 257/4096 median 约 1.3050x / 1.1671x | no encode-specific Error | 可进入 PI1 计划，但需要规模 / fallback gate |
| `ps_decodePoints` | `PointXYZRGBA` leaf257 median 1.0499x；leaf4096 median 1.0409x、min 0.9116x | Warning + near-threshold suggestion | 不进入 production candidate；保持标量 |
| `ps_decodePoints staged-store` | `PointXYZRGBA` leaf257 median 0.9918x；leaf4096 mean 0.9758x、min 0.8884x | staged-specific Errors=2 | staged-store implementation-shape hypothesis rejected |
| `ps_setDefaultColor` | `PointXYZRGBA` 4096 median 1.1655x | no default-specific Error | 支持 PI1 计划 |

这条诊断证据链不能替代 production evidence（生产证据）。phase 060 已补齐真实 `ColorCoding<pcl::PointXYZRGBA>` public method 的 production direct evidence，结果显示 phase 050 的 production-shaped positive 不能直接外推到生产公开方法边界。

## 当前生产证据链

当前 production evidence role（生产证据角色）是 `production_public`，A/B boundary（对比边界）是 `public_overload`。板卡 repeated summary 位于 `test-rvv/io/color_coding/log/board/production_repeat_5/summary.md`，Evidence Doctor 位于 `test-rvv/io/color_coding/log/board/production_repeat_5/evidence_doctor.md`。

| production public case | current board signal | Evidence Doctor handling | production meaning |
| --- | --- | --- | --- |
| `prod_encode_average_leaf257/4096` | historical phase 060 negative；phase 070 已从 production filter 移除 | historical Error | 已从当前 production patch 回滚；不建议继续。 |
| `prod_encode_points_leaf257/4096` | historical phase 060 weak / unstable；phase 070 已从 production filter 移除 | historical Warning + Suggestions | 已从当前 production patch 回滚；不建议继续。 |
| `prod_set_default_color_4096` | phase 070 median 1.0035x，min 0.9945x，mean 1.0054x | Error：2/5 below 1；Suggestion：near-threshold | 不支持采纳 default RVV；phase 080 已完整回滚。 |

当前生产接入判断：`no production RVV adopted`。Phase 080 已完整回滚剩余 default RVV；当前没有建议保留的 production RVV 优化；`decodePoints` 保持标量。

## doc_suite_role_inventory

| role | status | path / evidence |
| --- | --- | --- |
| topic_navigation | standalone | `README.zh.md` |
| testing_overview | standalone | `doc/testing-overview.zh.md` |
| correctness_tests | standalone | `doc/correctness-tests.zh.md` |
| benchmark_and_evidence | standalone | `doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | standalone | `doc/optimization-evidence.zh.md` |
| optimization_roadmap | standalone | `doc/optimization-roadmap.zh.md` |
| test_support_code_map | standalone | `doc/test-support-code-map.zh.md` |
| phase_index | standalone | `doc/phases/README.zh.md` |
| evaluation_diagnostic | standalone | `doc/color_coding-evaluation.zh.md` |
| evaluation_production | standalone in phase 080 | 完整回滚和 no-adoption closeout 已完成。 |
| production_topic_doc | not_applicable with evidence | `doc-rvv/io/color_coding-RVV.zh.md` 不创建；当前没有 adopted production behavior。 |

## 文档归属

S2 评估、candidate 取舍和诊断证据链主归属在本文件与 phase result。phase 050 诊断 bench 统计和 Doctor 细节主归属在 `log/board/component_repeat_5/summary.md` 与 `log/board/component_repeat_5/evidence_doctor.md`；phase 070 production evidence 主归属在 `log/board/production_repeat_5/summary.md`、`log/board/production_repeat_5/evidence_doctor.md` 和 `doc/phases/070-pi5-partial-rollback-default-only/result.zh.md`；phase 080 完整回滚主归属在 `doc/phases/080-full-rollback-no-adoption-closeout/result.zh.md`。

production 长期主题文档 `doc-rvv/io/color_coding-RVV.zh.md` 当前 `not_applicable`，因为没有 adopted production behavior（已采用生产行为）。phase 070 的 default-only 证据不支持 clean adoption（干净采纳）。
