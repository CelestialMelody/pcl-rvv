# Phase 050 结果：production public probe

## 执行范围

本阶段在用户授权 PI2 后修改了 `io/src/image_depth.cpp`，把 `DepthImage::fillDepthImage()` / `fillDisparityImage()` 的 float 输出公开入口接入 RVV（RISC-V Vector，可伸缩向量扩展）。接入范围按接入后生产证据收窄为：depth contiguous（深度连续路径）、depth padded contiguous（深度带 padding 连续路径）、disparity contiguous（视差连续路径）和 disparity downsample（视差下采样路径）。depth downsample（深度下采样路径）在 production-public（真实公开入口）证据中没有形成收益，已回退到标量路径，不作为当前 production RVV 采纳候选。

未修改 `fillDepthImageRaw()`、OpenNI legacy 文件或 public API。

## 计划动作回填

| 动作 | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| PI3 RED | done | `make run_test_rvv` 在 production patch 前失败 2 个 production-public path hit 断言 | 失败原因是真实 `DepthImage` 入口没有命中 RVV hook，RED 有效。 |
| PI2 production patch | done | `io/src/image_depth.cpp` | 抽出标量 helper；`__RVV10__` 下新增 contiguous depth/disparity 与 downsample disparity RVV helper；depth downsample 和非覆盖路径回退标量。 |
| PI3 GREEN correctness | done | `make run_test_compare` | Std/RVV 各 9 个 TEST 全通过；RVV build 覆盖 3 个 production RVV path hit、depth downsample 标量 fallback 和非整数 downsample 异常保持。 |
| QEMU bench smoke | done | `make run_bench_rvv BENCH_ARGS="--case-filter prod_depth_full_640x480 --iterations 1 --warmup-iterations 0"` | production-public bench label 可运行；QEMU 不作为性能结论。 |
| production-linked asm | done | `build/asm/riscv/bench_image_depth_rvv.full.asm`、`build/asm/riscv/bench_image_depth_rvv.asm` | 可见生产 helper / public entry 内联边界中的 `vle16.v`、`vlse16.v`、`vfcvt.f.xu.v`、`vfmul.vf`、`vfrdiv.vf`、masked `vse32.v`。 |
| PI4 board repeated | done | `log/board/repeated_production_public/summary.md` | 5-run production-public 板卡结果支持 3 条 RVV 候选；`prod_depth_downsample` 作为 fallback coverage（回退覆盖）保留上下文数据。 |
| Evidence Doctor / registry | done | `log/board/repeated_production_public/evidence_doctor.md`、`log/evidence_registry.json` | Doctor：Errors=0，Warnings=2，Suggestions=0；registry 已登记 production-public summary / manifest / doctor。 |

## production-public 板卡结果

| case | runs | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `prod_depth_full_640x480` | 5 | 1.42x | 1.31x | 1.43x | positive |
| `prod_depth_full_padded_640x480` | 5 | 1.20x | 1.14x | 1.25x | positive / weak-positive 边界 |
| `prod_depth_downsample_640x480_to_320x240` | 5 | 0.99x | 0.98x | 1.08x | fallback coverage；不作为 RVV 收益候选 |
| `prod_disparity_full_640x480` | 5 | 1.80x | 1.46x | 1.82x | positive with variance warning |
| `prod_disparity_downsample_640x480_to_320x240` | 5 | 1.34x | 1.13x | 1.39x | positive with variance warning |

`prod_depth_downsample` 在当前 production patch 中命中标量 fallback；summary 中 0.99x 的数值只作为 fallback context（回退上下文），用于证明“该入口没有 RVV 收益且不应采纳 depth downsample RVV”。topic-local manifest 把它标为 `production-fallback-coverage`，因此 Evidence Doctor 不再把它作为 RVV speedup candidate（RVV 加速候选）做退化频率判定。

## Evidence Doctor

| report | result | 处理 |
| --- | --- | --- |
| `log/board/repeated_production_public/evidence_doctor.md` | Errors=0，Warnings=2，Suggestions=0 | 无阻塞 Error；2 个 Warning 均在本 result、evaluation 和 matrix 中降级解释。 |

Warnings：

- `prod_disparity_downsample_640x480_to_320x240`：min/median/max 有长尾，保留 min/median/max，不剔除异常。
- `prod_disparity_full_640x480`：min/median/max 有长尾，但 min 仍为 1.46x，决策桶保持 positive。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | 当前新增证据为 `production-public`，真实调用 `DepthImage` public entry。 |
| A/B boundary | Std/RVV 两侧都链接同一份 `io/src/image_depth.cpp`；区别由 `USE_PCL_RVV10` / `__RVV10__` 控制。 |
| 当前决策问题 | 当前 production patch 是否值得采纳，以及用户确认后哪些路径成为 adopted production behavior。 |
| diagnostic 是否可外推到 production | 旧 diagnostic 只作为候选来源；本阶段最终数值来自 production-public board evidence。 |
| comparison-boundary / baseline mismatch 风险 | 已通过 production-public bench 降低；仍保留 synthetic wrapper 数据集和未记录 governor/freq/temperature metadata 风险。 |
| weak / negative / unstable 时是否允许 bounded production probe | 已完成 probe；`prod_depth_downsample` 证据不支持采纳，已从 production RVV 候选降级为标量 fallback。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 不需要；当前没有既有 adopted RVV family。 |

## optimization matrix 更新

| candidate family | decision |
| --- | --- |
| production contiguous depth RVV | adopted；production-public positive |
| production contiguous disparity RVV | adopted；production-public positive with variance warning |
| production downsample depth RVV | rejected for current production patch；public entry uses scalar fallback |
| production downsample disparity RVV | adopted；production-public positive with variance warning |
| `fillDepthImageRaw()` RVV | unchanged：profile-gated deferred |
| OpenNI legacy parity | unchanged：deferred，可作为后续 parity topic 复核 |

## Continue / Stop Decision

`continue_stop_decision = S11 closeout completed / adopted production behavior`。

用户已确认“有收益的优化均接入”。当前 production patch 已视为 adopted production behavior（已采用生产行为），并已创建正式长期文档 `doc-rvv/io/image_depth-RVV.zh.md`。该文档使用 `log/board/repeated_production_public/summary.md` 的接入后板卡数据。

最终采纳范围：

- 采纳 depth contiguous、disparity contiguous 和 disparity downsample RVV。
- depth downsample 保持标量 fallback，不继续作为当前生产 RVV 方向推进。
- disparity warning case 可选扩大 runs，但不是当前采纳前置条件。
- `fillDepthImageRaw()` 和 OpenNI legacy 保持 deferred，若要继续应另开 profile / parity topic。
