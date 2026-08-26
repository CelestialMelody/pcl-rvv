# PFHRGB RVV 函数级评估

## 范围

目标源码是 `features/include/pcl/features/impl/pfhrgb.hpp`，评估入口为
`pcl::PFHRGBEstimation<PointInT, PointNT, PointOutT>::computePointPFHRGBSignature`
以及上游 `computeFeature` public KSearch（公开 K 近邻搜索）路径。当前 production（生产源码）已接入
exact `pcl::PointXYZRGBNormal -> pcl::PFHRGBSignature250` 的 RVV 分流；其它模板实例保持标量 fallback（回退路径）。

## 函数作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | RVV 判断 |
| --- | --- | --- | --- |
| `computeFeature` | 对每个输入点搜索邻域并生成 `PFHRGBSignature250`。 | `indices_`、search method、normals、output cloud。 | 只可能通过下游 helper 间接受益；邻域搜索保持标量。 |
| `computePointPFHRGBSignature` | 对单个邻域所有有向点对计算几何和颜色 histogram。 | cloud、normals、neighbor indices、`nr_split`、`Eigen::VectorXf` histogram。 | O(k²) pair feature 和 RGB ratio 是当前 RVV 诊断主路径。 |
| `computeRGBPairFeatures` | 计算 Darboux frame（Darboux 坐标系）特征 `f1/f2/f3/f4` 和 RGB ratio `f5/f6/f7`。 | 两点 xyz、normal 和 rgb。 | 可做 caller-shaped batch（调用方形态批处理）诊断；文件自身不单独优化。 |
| histogram scatter | 将几何和颜色 bin 分别累加到 0-124 与 125-249。 | `pfhrgb_histogram` 输出。 | 当前保持标量顺序，避免 bin conflict 和非结合累加风险。 |

## 函数级结论

当前判断是 `adopted production behavior / exact-gated production`。接入后的 board repeated（板卡重复测试）
显示真实公开入口 `public_pfhrgb_k` 的 B/A 为 `1.28, 1.27, 1.28, 1.27, 1.27`，median `1.27x`，
0/5 低于 1，checksum 一致。它是 production-public（生产公开入口）证据，优先级高于接入前的
production-shaped diagnostic（生产形态诊断）。`public_pfhrgb_k_with_candidate_reuse` 仍作为实现形态背景：
median `1.24x`，0/5 低于 1。相反，helper-only `candidate_pfhrgb_pair_batch_rvv` 当前 rerun
median 为 `0.98x` 且触发 Evidence Doctor Error，因此不能把 fixed-neighborhood helper 写成独立
production value。

PFHRGB 与已采纳 PFH pair math 共享几何形态，但 PFHRGB 源码使用有向 pair 遍历、颜色 ratio 分支和
250-bin 双 histogram，不能把 PFH 的 production evidence（生产证据）直接外推。当前采纳只覆盖
exact `PointXYZRGBNormal`、`nr_split=5`、KSearch public boundary；泛型 RGB traits、其它点型组合和
非默认参数仍需独立 phase。

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `PFHRGBEstimation::computeFeature` | production public entry | 公开入口，负责 search、RVV workspace 复用和输出 descriptor。 | `Feature::compute()` | `computePointPFHRGBSignatureRVV` / Std fallback | production-public evidence | `features/include/pcl/features/impl/pfhrgb.hpp` |
| `computePointPFHRGBSignatureStd` | production Std helper | 标量逐邻域生成 250-bin histogram。 | public dispatch / fallback | `computeRGBPairFeatures` | scalar fallback source | `features/include/pcl/features/impl/pfhrgb.hpp` |
| `computePointPFHRGBSignatureRVV` | production RVV helper | exact 点型下批量计算 pair tuple 和 RGB ratio，histogram scatter 保持标量顺序。 | `computeFeature` / exact dispatch | RVV math helper + scalar binning | adopted RVV path | `features/include/pcl/features/impl/pfhrgb.hpp` |
| `pcl::computeRGBPairFeatures` | shared helper | 计算几何 pair tuple 和 RGB ratio。 | PFHRGB / PPFRGB callers | histogram binning | helper semantics source | `features/src/pfh.cpp` |
| `include/pfhrgb.h` | test support aggregator | topic-local 测试支撑聚合入口。 | test / bench source | internal helpers | correctness gate support | `test-rvv/features/pfhrgb/include/pfhrgb.h` |
| `computePublicPFHRGBWithPairBatchCandidate` | production-shaped diagnostic wrapper | KSearch 外层循环加非复用 candidate helper。 | test / bench source | `computePointPFHRGBSignaturePairBatchRVV` | public-shaped positive evidence | `test-rvv/features/pfhrgb/include/impl/pfhrgb_pair_batch_candidate.hpp` |
| `computePublicPFHRGBWithReusablePairBatchCandidate` | production-shaped diagnostic wrapper | KSearch 外层循环复用 `PairBatchWorkspace`。 | test / bench source | `computePointPFHRGBSignaturePairBatchRVVWithWorkspace` | preferred PI1 shape evidence | `test-rvv/features/pfhrgb/include/impl/pfhrgb_pair_batch_candidate.hpp` |
| `src/bench_pfhrgb.cpp` | bench wrapper | 输出五个 case label、timing 和 checksum。 | Make targets | manifest script | board performance summary source | `test-rvv/features/pfhrgb/src/bench_pfhrgb.cpp` |
| `generate_pfhrgb_evidence_manifest.py` | analysis script | 解析 board repeated compare summary 并补 metadata。 | `evidence_manifest_repeated` | Evidence Doctor | evidence manifest builder | `test-rvv/features/pfhrgb/script/generate_pfhrgb_evidence_manifest.py` |
| `evidence_manifest.json` | evidence output summary | 五轮 board repeated 证据清单。 | manifest script | Evidence Doctor / docs | current board truth | `test-rvv/features/pfhrgb/log/board/repeated/evidence_manifest.json` |
| `evidence_doctor.md` | Evidence Doctor output | 暴露 Error / Suggestion 和结论降级。 | manifest | phase result / Handoff | evidence quality gate | `test-rvv/features/pfhrgb/log/board/repeated/evidence_doctor.md` |
| `doc-rvv/features/pfhrgb-RVV.zh.md` | production long-term doc | 记录当前采用的 RVV 实现、证据链和未覆盖范围。 | production closeout | reviewer / 后续 worker | adopted production reference | `doc-rvv/features/pfhrgb-RVV.zh.md` |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| scalar reference | adopted_as_test_baseline | `run_test_compare` 通过 | 只作为测试 oracle，不是 production 实现。 |
| color pair batch RVV helper | attempted / negative-current-rerun | helper-only median `0.98x`，Doctor Error | 数值正确但不作为独立收益证据。 |
| public-with-candidate wrapper | production-shaped context | repeated board median `1.21x` | 接入后仅保留为 topic-local implementation-shape 背景。 |
| staging reuse wrapper | adopted shape input | repeated board median `1.24x`；QEMU smoke checksum 一致；ASM 有 RVV 指令。 | production 采用了 workspace 复用思路，但以真实 public 结果为采纳依据。 |
| component baseline | attempted / neutral-negative | `component_pfhrgb_signature` median `1.00x`，Doctor Warning | 不能作为收益结论，只作为背景。 |
| production public | adopted | `public_pfhrgb_k` median `1.27x`，0/5 低于 1，Doctor 无 case-specific Error。 | 只覆盖 exact 点型、`nr_split=5` 和当前 KSearch public boundary。 |

## 测试和 bench 主归属

| 文档 | 主职责 |
| --- | --- |
| `doc/testing-overview.zh.md` | Make target、case-filter、QEMU / board / Doctor 的入口分类。 |
| `doc/correctness-tests.zh.md` | 每个 gtest 的输入、断言和证明范围。 |
| `doc/benchmark-and-evidence.zh.md` | bench case、board repeated、Evidence Doctor、ASM 和提交边界。 |
| `doc/optimization-evidence.zh.md` | candidate family 到测试、bench、Doctor 和 decision 的索引。 |
| `doc/test-support-code-map.zh.md` | test support helper、bench wrapper、script 和 production 对照关系。 |

## 诊断证据链

| 层级 | 当前证据 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| local correctness | Std/RVV gtest 6/6 通过。 | reference、candidate、public-shaped wrapper、reusable workspace、production exact path 和非 exact source fallback 数值一致。 | 不证明泛型 RGB traits 或所有 fallback 原因。 |
| QEMU | QEMU test / bench smoke 可运行。 | 构建、日志形状和 checksum。 | 不证明性能。 |
| ASM | `check_production_rvv_symbol` 通过。 | RVV bench binary 中存在 production RVV helper 符号和向量指令。 | 不证明其它模板实例命中 RVV。 |
| board | current manifest 5-run repeated。 | 真实公开入口 `public_pfhrgb_k` 稳定正向，median `1.27x`。 | 不证明未覆盖点型、radius search 或真实业务数据集。 |
| Evidence Doctor | Errors=1 / Warnings=1 / Suggestions=6。 | helper-only 收益必须降级；production-public case 无 Error。 | 不消除环境 metadata 缺失风险。 |

## 生产接入判断

当前 production 接入可采纳。用户本轮已说明“接入后板卡有收益即可采纳”，而接入后的 `public_pfhrgb_k`
5-run repeated board 为 positive，因此本评估将它记录为 adopted production behavior。正式长期文档为
`doc-rvv/features/pfhrgb-RVV.zh.md`。

## 遗留风险和下一步

| 风险 / 缺口 | 当前状态 | 下一步 |
| --- | --- | --- |
| fallback gate 扩展 | 已覆盖非 exact source fallback；小规模和非默认 `nr_split` 通过 helper fallback 间接覆盖，仍缺独立 gtest。 | 后续扩展 phase 可补更细 fallback matrix。 |
| 泛型点类型 | 未验证。 | exact production positive 后读取 generic point type strategy（泛型点类型策略）并单独排队。 |
| helper-only 退化 | 本轮 Doctor Error 已降级。 | 不把 helper-only speedup 写进 PI1 决策；仅作为实现组件。 |
