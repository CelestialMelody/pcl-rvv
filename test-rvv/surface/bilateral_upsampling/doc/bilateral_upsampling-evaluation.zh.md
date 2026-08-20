# surface/bilateral_upsampling 函数级 RVV 评估

## 入口与标量路径

公开入口是 `pcl::BilateralUpsampling<PointInT, PointOutT>::process`。它先执行 `initCompute`、检查 input cloud（输入点云）是否 organized（有序结构）、求 `projection_matrix_` 的逆矩阵，然后调用 `performProcessing`。

热点在 `performProcessing`：对每个 `(x, y)` 像素，按 `window_size_` 形成局部窗口，跳过 `z` 非有限的邻居点，用 `computeDistances` 预先生成的 depth / RGB 查表值计算 weight（权重），累加 `sum += weight * z` 和 `norm_sum += weight`。若 `norm_sum != 0`，用 `unprojection_matrix_ * [x*depth, y*depth, depth]` 写出 xyz；否则写 NaN。RGB 字段直接从中心点复制。

`computeDistances` 中的 `std::exp` 是 per-process 预计算，不是逐像素热点。当前 RVV 优先级在窗口内查表后的乘法、finite skip（有限值跳过）和规约。

## 函数级结论

当前 EvidenceDecision 是 `adopted production behavior`。用户已经确认保留 `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` 中的 color-gather RVV production patch；phase 073 又在用户“如果有收益，同意先接入”的授权下，把覆盖范围扩展到 RGB/RGBA exact family：`pcl::PointXYZRGB -> pcl::PointXYZRGB`、`pcl::PointXYZRGBA -> pcl::PointXYZRGBA`、`pcl::PointXYZRGB -> pcl::PointXYZRGBA`、`pcl::PointXYZRGBA -> pcl::PointXYZRGB`、`Scalar=float`、organized RGBD grid 和 `RVVXYZAoSFloatLayout`。其它模板实例继续走 fallback。

采纳理由不是早期 diagnostic speedup，而是 production direct 证据：phase 070 用户确认采纳时，真实 `BilateralUpsampling::process` public path 在板卡上为 `1.38x / 1.22x / 1.02x`，steady public 为 `1.34x / 1.24x / 1.06x`；Evidence Doctor 为 `Errors=0`、`Warnings=0`、`Suggestions=1`。Phase 071 补齐 infinity finite-mask correctness，phase 072 收窄 same-type gate。Phase 073 扩展交叉 RGB/RGBA 后，当前二进制刷新为 QEMU Std/RVV 13/13、asm 仍归属 color-gather helper，并包含 `vfabs/vmflt` 有限值过滤；板卡 public 为 `1.26x / 1.21x / 1.18x / 1.23x / 1.22x`，steady public 为 `1.31x / 1.21x / 1.25x / 1.21x / 1.21x`，Evidence Doctor 为 `Errors=0`、`Warnings=0`、`Suggestions=0`。

## Production patch scope

| 项 | 当前范围 | 说明 |
| --- | --- | --- |
| production 文件 | `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` | 新增 color-gather RVV helper，并调整 `performProcessing` dispatch 顺序。 |
| public entry | `BilateralUpsampling::process` -> `performProcessing` | bench 和 correctness 均通过真实公开入口覆盖。 |
| adopted helper | `bilateralUpsamplingPerformProcessingColorGatherRVV` | 当前主线收益来源。 |
| retained helper | `bilateralUpsamplingPerformProcessingRVV` | 旧 RVV helper 保留作 fallback / historical 对照，不是当前主线收益来源。 |
| build gate | `__RVV10__` | 非 RVV 构建不进入 RVV helper。 |
| layout gate | `kBilateralUpsamplingRgbPoint` + `RVVXYZAoSFloatLayout` | 只允许已验证的 RGB/RGBA exact-family AoS float layout。 |

## Covered path 与 fallback matrix

| 范围 | 当前状态 | 证据 / 回退 |
| --- | --- | --- |
| `PointXYZRGB -> PointXYZRGB`、organized grid、`Scalar=float` | adopted / refreshed | public holes correctness、infinity correctness、phase 072 public / steady board speedup、asm、Evidence Doctor |
| `PointXYZRGBA -> PointXYZRGBA`、organized grid、`Scalar=float` | adopted / refreshed | public dense correctness、public / steady board speedup、asm、Evidence Doctor |
| `PointXYZRGB -> PointXYZRGBA` / `PointXYZRGBA -> PointXYZRGB` | adopted / refreshed | phase 073 public correctness、public / steady board speedup、asm、Evidence Doctor；alpha 不作为本阶段写回语义 |
| 非 RVV 构建 | scalar fallback | `__RVV10__` 外不编译 RVV helper |
| 非 RGB/RGBA 点型 | scalar fallback | 不满足 point type gate |
| 非 AoS float layout | scalar fallback | 不满足 layout gate |
| `Scalar=double` | not covered / scalar fallback | 当前 evidence 只覆盖 float production path |
| 非 organized input、非法窗口、空输入 | scalar / original guard behavior | 维持原 public semantics |
| indices / correspondences | not applicable with evidence | `BilateralUpsampling` 当前路径按 organized image grid，不是 registration row-source topic |

## Production direct tests

`src/test_bilateral_upsampling.cpp` 中保留五个真实 public-entry tests：`PointXYZRGBMatchesReferenceWithNanHoles` 覆盖 RGB + holes，`PointXYZRGBAMatchesReferenceDense` 覆盖 RGBA + dense，`PointXYZRGBSkipsInfiniteDepth` 覆盖 production color-gather finite mask 的 infinity skip，另外两个 cross tests 覆盖 `PointXYZRGB -> PointXYZRGBA` holes 和 `PointXYZRGBA -> PointXYZRGB` dense。加上 diagnostic reference tests，`run_test_compare` 中 Std / RVV 均为 13/13 passed。详细 TEST 字典见 `doc/correctness-tests.zh.md`。

QEMU 证据路径是 `log/qemu/run_test_std.log` 和 `log/qemu/run_test_rvv.log`。QEMU 只证明 correctness、日志形状和路径可运行，不作为性能结论。

## Production asm

`dump_bench_rvv` 生成的 `build/asm/riscv/bench_bilateral_upsampling_rvv.asm` 能看到 color-gather helper 相关的 `vlse8.v`、`vzext.vf2`、`vmaxu.vv`、`vminu.vv`、`vluxei16.v`、`vfredusum.vs`，以及 phase 071 finite mask 的 `vfabs.v` / `vmflt.vf`。这说明当前 production helper 的 RGB 读取、色差计算、RGB 查表 gather、有限值过滤和规约都实际落到 RVV 指令上。

## Production board bench

| production public case | Std avg | RVV avg | speedup | decision |
| --- | ---: | ---: | ---: | --- |
| `PointXYZRGB 80x60 w3 dense` | 6.1123 ms | 4.8681 ms | 1.26x | positive |
| `PointXYZRGB 120x90 w4 holes` | 25.7178 ms | 21.2556 ms | 1.21x | positive |
| `PointXYZRGBA 180x120 w5 dense` | 75.3833 ms | 63.6616 ms | 1.18x | positive |
| `PointXYZRGB -> PointXYZRGBA 120x90 w4 holes` | 26.3132 ms | 21.3591 ms | 1.23x | positive |
| `PointXYZRGBA -> PointXYZRGB 120x90 w4 dense` | 26.0464 ms | 21.3000 ms | 1.22x | positive |

| steady public case | Std avg | RVV avg | speedup | decision |
| --- | ---: | ---: | ---: | --- |
| `PointXYZRGB 80x60 w3 dense` | 6.6024 ms | 5.0495 ms | 1.31x | positive |
| `PointXYZRGB 120x90 w4 holes` | 26.0561 ms | 21.5517 ms | 1.21x | positive |
| `PointXYZRGBA 180x120 w5 dense` | 77.4845 ms | 62.1629 ms | 1.25x | positive |
| `PointXYZRGB -> PointXYZRGBA 120x90 w4 holes` | 26.0141 ms | 21.5678 ms | 1.21x | positive |
| `PointXYZRGBA -> PointXYZRGB 120x90 w4 dense` | 26.1494 ms | 21.5923 ms | 1.21x | positive |

当前板卡 summary 为 `log/board/analyze_bench_compare.log`。phase 073 manifest 为 `doc/phases/073-cross-rgb-rgba-production-probe/evidence_manifest.json`，Evidence Doctor 为同目录 `evidence-doctor.md`，结果是 `Errors=0`、`Warnings=0`、`Suggestions=0`。Phase 070 的 `PointXYZRGBA 180x120 w5 dense` public case 曾只有 `1.02x`，phase 073 当前二进制刷新后仍为正向。

Phase 072 的 `board_smoke` 通过 `SSH_OPTS='-F /dev/null' RSYNC_SSH='ssh -F /dev/null'` 绕开本机 SSH config 后完成；默认 SSH config 曾导致板卡连接超时，但板卡本身可达。

## Decision delta

| candidate | 历史证据 | 当前决策 |
| --- | --- | --- |
| `staged-window-reduction` | phase 000 为 `0.90x / 0.91x / 0.90x`，Evidence Doctor `Errors=3` | rejected，不接 production |
| `column-stride-depth-direct` diagnostic | phase 010 为 `1.04x / 1.16x / 1.28x`，但仅是 diagnostic | historical precursor，只说明 direct load 值得探索 |
| old production exact-gate helper | public / steady / helper-only / nan-mask 证据没有稳定正向 | rejected / superseded |
| `production detail color-gather` | phase 060 bench-local precursor 为 `1.31x / 1.28x / 1.07x` | accepted precursor，进入 production probe |
| color-gather production helper | phase 070 public / steady production direct 全部超过 1.0x，doctor 无 error/warning | adopted |
| finite-mask correctness refresh | phase 071 QEMU Std/RVV 11/11 passed，asm 可见 `vfabs/vmflt`；phase 073 已刷新 board | adopted correctness refresh |
| same-type gate evidence alignment | phase 072 收窄 production RVV gate 到 same-type RGB/RGBA，QEMU/asm/board/Evidence Doctor 均刷新 | historical scope alignment |
| cross RGB/RGBA production probe | phase 073 cross public 为 `1.23x / 1.22x`，steady 为 `1.21x / 1.21x`，QEMU Std/RVV 13/13 | adopted scope expansion / refreshed |

## Doc suite audit

| area | current shape scan | quality bar / optional calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| topic_navigation | `README.zh.md` 有阅读路径、命令和证据边界 | topic-navigation template 要求当前结论、证据白名单、production doc 适用性和未闭合恢复入口 | adopted / refreshed | phase 072 same-type gate 和 board freshness 已写入入口说明 | 后续扩展时同步 |
| testing_overview | `doc/testing-overview.zh.md` 有 target 粒度审计和覆盖矩阵 | testing-overview template 要求每类 target 说明证明范围和不能证明范围 | adopted / refreshed | 已写清 RGB/RGBA exact family 覆盖和其它点型 fallback | 后续扩展时同步 |
| correctness_tests | `doc/correctness-tests.zh.md` 列出 13 个 TEST 的输入、路径和边界 | correctness role 要求 TEST 字典不能外推到未测点型 | adopted / refreshed | `runProductionPublicEntry<PointInT, PointOutT>` 覆盖 same-type 与 cross RGB/RGBA 样本 | 后续其它点型扩展时同步 |
| benchmark_and_evidence | `doc/benchmark-and-evidence.zh.md` 有 CLI、case label、board 数字和提交边界 | benchmark/evidence template 要求 current board evidence、manifest、doctor、freshness 和提交边界 | adopted / refreshed | phase 073 已刷新当前二进制 board summary 和 doctor | 保持 phase 070/072 为历史证据，phase 073 为当前 freshness |
| optimization_evidence | `doc/optimization-evidence.zh.md` 有 candidate family 总表 | optimization evidence template 要求每个 candidate 的代码、bench、board、asm、doctor 和边界一致 | adopted / refreshed | color-gather 边界已写成 RGB/RGBA exact family，并指向 phase 073 | 后续扩展时同步 |
| optimization_roadmap | `doc/optimization-roadmap.zh.md` 有候选搜索空间和恢复条件 | roadmap role 记录未来扩展，不替代当前证据 | adopted / refreshed | 其它点型、layout、`Scalar` 仍需另开 phase | 后续扩展时同步 |
| test_support_code_map | `doc/test-support-code-map.zh.md` 有调用图、helper、bench harness 和 output | code-map template 要求 test support 与 production 对照清楚 | adopted / refreshed | 已列出 phase 073 当前 manifest / doctor | 后续加脚本或 case-filter 时同步 |
| phase suite | `doc/phases/` 有 plan/result、optimization matrix 和 doctor | phase suite 要求每次范围修正都有 phase 记录和继续/停止判断 | adopted / refreshed | phase 073 已补 cross RGB/RGBA production probe，并重跑 QEMU/asm/board | 后续扩展另开 phase |
| production_topic_doc | `doc-rvv/surface/bilateral_upsampling-RVV.zh.md` 有长期生产说明和板卡表 | production topic doc 要解释当前源码行为、证据链、fallback 和未覆盖边界 | adopted / refreshed | 覆盖范围已改成 RGB/RGBA exact family，避免外推其它点型 | 作为长期入口 |

## Traceability Map

| 对象 | 角色 | 路径 |
| --- | --- | --- |
| production public entry / dispatch | adopted production behavior | `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` |
| production correctness | public-entry tests | `src/test_bilateral_upsampling.cpp`、`doc/correctness-tests.zh.md` |
| bench cases | public / steady / helper / component evidence | `src/bench_bilateral_upsampling.cpp`、`doc/benchmark-and-evidence.zh.md` |
| candidate decision | adopted / rejected / attempted index | `doc/optimization-evidence.zh.md`、`doc/phases/optimization-matrix.zh.md` |
| phase 070 board truth | historical adoption summary | `doc/phases/070-production-detail-color-gather-production-probe/result.zh.md`、`doc/phases/070-production-detail-color-gather-production-probe/evidence_manifest.json` |
| phase 071 finite-mask refresh | correctness refresh，board freshness 由 phase 073 覆盖 | `doc/phases/071-production-finite-mask-correctness-refresh/result.zh.md` |
| phase 072 same-type alignment | historical same-type scope alignment | `doc/phases/072-production-same-type-gate-alignment/result.zh.md` |
| phase 073 board freshness | current production direct summary | `log/board/analyze_bench_compare.log`、`doc/phases/073-cross-rgb-rgba-production-probe/result.zh.md` |
| Evidence Doctor | current manifest health | `doc/phases/073-cross-rgb-rgba-production-probe/evidence-doctor.md` |
| production long-term doc | maintenance-facing implementation facts | `doc-rvv/surface/bilateral_upsampling-RVV.zh.md` |

## 遗留风险与恢复条件

当前最大风险曾是大 RGBA public case 在 phase 070 只有 `1.02x`，可能受测量波动、输入分布或二进制差异影响。Phase 073 当前二进制刷新后该 case 仍为正向，Evidence Doctor 无 finding，因此 near-threshold 风险已降级为历史风险。由于当前仍是单次 board smoke，不是 repeated suite；若 reviewer 要求更稳，可以另开 bounded rerun phase，而不是把 QEMU timing 写成性能证据。

其它点类型、`Scalar=double`、其它 layout、真实 sensor 数据或更大规模都不在当前证据范围内。后续扩展时必须重新做 production direct correctness、asm、board 和 Evidence Doctor，并同步 `doc-rvv`、evaluation、optimization evidence 和 Handoff。
