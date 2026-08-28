# sac_model_line benchmark 与证据

## Bench 输入和计时边界

`src/bench_sac_model_line.cpp` 构造 65536 点 synthetic line-distance cloud。点云沿直线方向分布，并加入两个垂直方向的正弦 / 余弦偏移；`indices` 使用 adjacent-pair shuffle（相邻对调），保留 direct indexed row source（直接索引行来源）。

计时边界只包含入口调用本身，不包含点云、模型系数和 indices 构造。每个 build 固定 `Iterations: 200`，`Warmup Iterations: 5`。

## Bench label 字典

| label | 代码路径 | 证据角色 |
| --- | --- | --- |
| `public countWithinDistance` | 公开入口 `SampleConsensusModelLine::countWithinDistance`。 | Phase 040 后为 production direct；用于接入后 public Std/RVV 对比。 |
| `diagnostic candidate countWithinDistance` | 测试专用 `SampleConsensusModelLineDiagnostic::countWithinDistanceCandidate`。 | production-shaped diagnostic；Phase 040 前用于判断是否值得进入生产接入，当前已由接入后 public 证据闭合。 |
| `public selectWithinDistance` | 公开入口 `SampleConsensusModelLine::selectWithinDistance`。 | Phase 040 后为 production direct；用于接入后 public Std/RVV 对比。 |
| `diagnostic candidate selectWithinDistance` | 测试专用 `SampleConsensusModelLineDiagnostic::selectWithinDistanceCandidate`。 | production-shaped diagnostic；Phase 040 前用于判断 select 是否值得进入生产接入，当前已由接入后 public 证据闭合。 |
| `public getDistancesToModel` | 公开入口 `SampleConsensusModelLine::getDistancesToModel`。 | Phase 040 后为 production direct；Phase 050 后记录 direct double store 数据。 |
| `diagnostic candidate getDistancesToModel` | 测试专用 `SampleConsensusModelLineDiagnostic::getDistancesToModelCandidate`。 | production-shaped diagnostic；用于判断平方距离 RVV、标量 sqrt 和 dense store 形状是否值得继续。 |
| `diagnostic candidate getDistancesToModel vfsqrt` | 测试专用 `SampleConsensusModelLineDiagnostic::getDistancesToModelVFSqrtCandidate`。 | production-shaped diagnostic；Phase 040 前用于判断 RVV `vfsqrt` 形状是否值得进入生产接入，当前已由接入后 public 证据闭合。 |

## Historical Diagnostic Board Summary

下表是 Phase 000-030 的历史诊断证据。`production unchanged` 只描述这些诊断阶段当时的 public companion（公开入口陪跑）状态；当前 production 事实以 Phase 060 current production direct 数据为准。

| case | role | Std avg ms | RVV avg ms | B/A values | median / min / max | decision bucket |
| --- | --- | ---: | ---: | --- | --- | --- |
| public countWithinDistance | summary_only_unknown | 1.770945 | 1.770613 | not_applicable | about 1.00x companion | production unchanged |
| diagnostic candidate countWithinDistance | production_shaped_diagnostic | 1.770477 | 0.396724 | `4.4920x, 4.4350x, 4.4569x, 4.4390x, 4.4913x` | `4.4569x / 4.4350x / 4.4920x` | positive-stable |
| public selectWithinDistance | summary_only_unknown | 2.455160 | 2.448876 | not_applicable | about 1.00x companion | production unchanged |
| diagnostic candidate selectWithinDistance | production_shaped_diagnostic | 2.466660 | 0.795046 | `3.0647x, 3.1409x, 3.0622x, 3.1010x, 3.1434x` | `3.1010x / 3.0622x / 3.1434x` | positive-stable |
| public getDistancesToModel, Phase 020 companion | summary_only_unknown | 2.230201 | 2.267723 | not_applicable | about 1.00x companion | production unchanged |
| diagnostic candidate getDistancesToModel, scalar sqrt | production_shaped_diagnostic | 2.233099 | 2.374752 | `0.9289x, 0.9732x, 0.9356x, 0.9340x, 0.9300x` | `0.9340x / 0.9289x / 0.9732x` | negative |
| public getDistancesToModel, Phase 030 companion | summary_only_unknown | 2.272596 | 2.291348 | not_applicable | about 1.00x companion | production unchanged |
| diagnostic candidate getDistancesToModel vfsqrt | production_shaped_diagnostic | 2.266758 | 0.653100 | `3.3746x, 3.3924x, 3.5378x, 3.5283x, 3.5208x` | `3.5208x / 3.3746x / 3.5378x` | positive-stable |

## Production Direct Board Summary

Phase 040 的 production direct（真实生产入口直连）结果来自接入后公开入口，而不是测试专用 diagnostic candidate。Phase 050 进一步替换 `getDistancesToModelRVV` 写回形态，并重跑接入后 public rows。Phase 060 又替换 `selectWithinDistanceRVV` 的 compressed double store（压缩 double 写回）形态。正式 `doc-rvv` 当前采用 Phase 060 表和对应 manifest 中的数据；Phase 040/050 表保留为 production baseline（生产基线）。Phase 070 的 identity-index strided load（恒等索引跨步加载）是同一 production boundary（生产边界）内的 RVV-vs-RVV strict A/B（RVV 实现族严格对比），结果用于拒绝该实现族，不替代当前生产性能表。

| case | role | Std avg ms | RVV avg ms | B/A values | median / min / max | decision bucket |
| --- | --- | ---: | ---: | --- | --- | --- |
| public countWithinDistance | production_direct | 1.783107 | 0.370203 | `4.8157x, 4.8178x, 4.7916x, 4.8321x, 4.8258x` | `4.8178x / 4.7916x / 4.8321x` | positive-stable |
| public selectWithinDistance | production_direct | 2.449200 | 0.796956 | `3.0800x, 3.0420x, 3.0744x, 3.1055x, 3.0640x` | `3.0744x / 3.0420x / 3.1055x` | positive-stable |
| public getDistancesToModel | production_direct | 2.227100 | 0.651839 | `3.4036x, 3.3409x, 3.3977x, 3.5229x, 3.4197x` | `3.4036x / 3.3409x / 3.5229x` | positive-stable |

## Phase 050 Production Direct Board Summary

Phase 050 的 production direct 结果来自 `getDistancesToModelRVV` 改成 `vfwcvt + vse64` 直接写 `std::vector<double>` 后的同一公开入口 repeated board。count/select 实现族当时没有改变；它们的 public rows 用于确认同一 binary 下仍保持 positive-stable。Phase 060 后，本表是历史 production baseline。

| case | role | Std avg ms | RVV avg ms | B/A values | median / min / max | decision bucket |
| --- | --- | ---: | ---: | --- | --- | --- |
| public countWithinDistance | production_direct | 1.777136 | 0.370890 | `4.7791x, 4.8269x, 4.8269x, 4.7463x, 4.7793x` | `4.7793x / 4.7463x / 4.8269x` | positive-stable |
| public selectWithinDistance | production_direct | 2.476793 | 0.791395 | `3.0786x, 3.2815x, 3.0318x, 3.1014x, 3.1593x` | `3.1014x / 3.0318x / 3.2815x` | positive-stable |
| public getDistancesToModel | production_direct | 2.280606 | 0.548416 | `4.2728x, 4.0365x, 4.2292x, 4.0338x, 4.2237x` | `4.2237x / 4.0338x / 4.2728x` | positive-stable |

## Phase 060 Current Production Direct Board Summary

Phase 060 的 production direct 结果来自 `selectWithinDistanceRVV` 改成 `vfwcvt + vse64`
直接写 `error_sqr_dists_` 后的同一公开入口 repeated board。`countWithinDistance` 和
`getDistancesToModel` 实现族没有改变；它们的 public rows 用于确认同一 binary 下仍保持
positive-stable。

| case | role | Std avg ms | RVV avg ms | B/A values | median / min / max | decision bucket |
| --- | --- | ---: | ---: | --- | --- | --- |
| public countWithinDistance | production_direct | 1.785273 | 0.373631 | `4.8068x, 4.8607x, 4.6009x, 4.7936x, 4.8351x` | `4.8068x / 4.6009x / 4.8607x` | positive-stable |
| public selectWithinDistance | production_direct | 2.464332 | 0.772475 | `3.2460x, 3.1463x, 2.9755x, 3.2513x, 3.3620x` | `3.2460x / 2.9755x / 3.3620x` | positive-stable |
| public getDistancesToModel | production_direct | 2.268423 | 0.555793 | `4.0460x, 4.0334x, 4.0348x, 4.2406x, 4.0557x` | `4.0460x / 4.0334x / 4.2406x` | positive-stable |

## Phase 070 Identity RVV-vs-RVV A/B Summary

Phase 070 比较的是当前 gather-only RVV baseline（只使用离散加载的 RVV 基线）和 identity-strided RVV candidate（恒等索引跨步加载候选）。`B/A` 表示 `gather-only RVV baseline / identity-strided RVV candidate`；大于 1 表示候选更快。identity 输入下 `countWithinDistance` 和 `getDistancesToModel` 只有弱正向，`selectWithinDistance` 退化；shuffled 控制组又出现退化频率 Error。因此 Phase 070 只作为拒绝证据，production 源码已回退到 Phase 060 gather-only load family。

| input mode | entry | median / min / max | degradation count | Evidence Doctor |
| --- | --- | --- | ---: | --- |
| identity | `countWithinDistance` | `1.0084x / 1.0063x / 1.0214x` | 0/5 | identity doctor Errors=1 / Warnings=1 / Suggestions=2 |
| identity | `selectWithinDistance` | `0.9967x / 0.9719x / 1.0020x` | 4/5 | identity doctor Errors=1 / Warnings=1 / Suggestions=2 |
| identity | `getDistancesToModel` | `1.0132x / 0.9986x / 1.0218x` | 1/5 | identity doctor Errors=1 / Warnings=1 / Suggestions=2 |
| shuffled control | `countWithinDistance` | `1.0037x / 0.9934x / 1.0136x` | 2/5 | shuffled doctor Errors=3 / Warnings=0 / Suggestions=2 |
| shuffled control | `selectWithinDistance` | `1.0077x / 0.9836x / 1.0213x` | 2/5 | shuffled doctor Errors=3 / Warnings=0 / Suggestions=2 |
| shuffled control | `getDistancesToModel` | `0.9959x / 0.9916x / 1.0133x` | 3/5 | shuffled doctor Errors=3 / Warnings=0 / Suggestions=2 |

Evidence paths:

- `test-rvv/sample_consensus/sac_model_line/doc/phases/000-line-count-diagnostic/repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/000-line-count-diagnostic/repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/000-line-count-diagnostic/repeated-evidence-doctor.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/010-line-select-diagnostic/repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/010-line-select-diagnostic/repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/010-line-select-diagnostic/repeated-evidence-doctor.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/020-line-get-distances-diagnostic/repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/020-line-get-distances-diagnostic/repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/020-line-get-distances-diagnostic/repeated-evidence-doctor.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/030-line-get-distances-vfsqrt-diagnostic/repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/030-line-get-distances-vfsqrt-diagnostic/repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/030-line-get-distances-vfsqrt-diagnostic/repeated-evidence-doctor.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/040-line-production-integration/production-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/040-line-production-integration/production-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/040-line-production-integration/production-repeated-evidence-doctor.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-doctor.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-doctor.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-repeated-evidence-doctor.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-shuffled-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-shuffled-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-shuffled-repeated-evidence-doctor.json`
- raw repeated board logs under `log/board/repeated-20260828-phase000/run-*/` are local-only by default.
- raw repeated board logs under `log/board/repeated-20260828-phase010/run-*/` are local-only by default.
- raw repeated board logs under `log/board/repeated-20260828-phase020/run-*/` are local-only by default.
- raw repeated board logs under `log/board/repeated-20260828-phase030/run-*/` are local-only by default.

## Evidence Doctor

`make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_evidence_state`、`record_repeated_board_select_evidence_state`、`record_repeated_board_get_distances_evidence_state`、`record_repeated_board_get_distances_vfsqrt_evidence_state`、`record_repeated_board_production_evidence_state`、`record_repeated_board_production_vse64_evidence_state`、`record_repeated_board_production_select_vse64_evidence_state`、`record_identity_board_evidence_state` 和 `record_identity_shuffled_board_evidence_state` 生成 manifest、运行 Evidence Doctor，并登记 `log/evidence_registry.json`。当前 count、select、getDistances vfsqrt 诊断 doctor、Phase 040 production baseline doctor、Phase 050 getDistances direct store doctor 和 Phase 060 current production doctor 结果均为 Errors=0，Warnings=0，Suggestions=0。Phase 020 scalar-sqrt 形状 doctor 结果为 Errors=1，原因是 5/5 B/A 都小于 1；该 Error 已在 Phase 020 result 中降级为当前 diagnostic boundary 的拒绝证据。Phase 070 identity / shuffled doctor 的 Error 来自同边界 RVV-vs-RVV A/B 退化频率，用于拒绝 identity-index strided load 候选。

Phase 010 的早期 checksum mismatch（校验和不一致）已降级为 evidence checksum policy 问题，而不是 correctness 失败。bench checksum 现在覆盖 inlier 序列和 error vector size；getDistances checksum 只记录 dense distance vector size，精确浮点距离值由 gtest 的 `1e-6` / `2e-6` tolerance（误差容忍度）负责检查。

脚本没有记录 taskset、governor、freq、temperature 和 binary hash；这些缺口不改变本阶段 positive-stable bucket，但后续提交或长期审查时仍可补更完整环境 metadata。board run 中出现 `script/rvv-board-run.mk` clock skew warning（远端文件时间戳告警），测试和 bench 命令仍返回 0；该告警作为环境风险保留。

## Evidence Registry

`log/evidence_registry.json` 登记了 manifest、doctor Markdown 和 doctor JSON。`repeated_evidence_status`、`repeated_select_evidence_status`、`repeated_get_distances_evidence_status`、`repeated_get_distances_vfsqrt_evidence_status`、`repeated_production_evidence_status`、`repeated_production_vse64_evidence_status`、`repeated_production_select_vse64_evidence_status`、`identity_evidence_status` 和 `identity_shuffled_evidence_status` 要求这些路径被 phase result 或 topic-local docs 引用；本文件和各 phase result 是当前引用来源。

## 提交边界

summary-only 是默认策略。phase docs、evaluation、role docs、manifest 和 doctor report 可作为 topic evidence summary 进入 review；raw `log/board/**`、`log/qemu/**`、`build/**` 和本机配置默认不提交。
