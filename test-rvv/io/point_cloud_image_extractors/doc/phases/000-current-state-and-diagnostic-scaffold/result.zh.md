# Phase 000 Result: current-state-and-diagnostic-scaffold

## 实际执行范围

本阶段完成了 `PointCloudImageExtractor` 的首个 production-shaped diagnostic
（生产形态诊断）：测试专用 helper 复刻 RGB/RGBA unpack、`PointXYZI::intensity`
scaling 和 NaN 涂黑 post-pass。production 源码未修改。

已验证范围：

- `PointXYZRGB` / `PointXYZRGBA` 的 organized cloud RGB/RGBA 字段解包。
- `PointXYZI::intensity` 的 no-scaling、fixed-factor 和 full-range scaling。
- QEMU correctness（QEMU 正确性）、QEMU 小规模 bench 日志形状、反汇编和板卡 repeated board
  （重复板卡性能测试）。

未验证范围：

- normal field、label random / Glasbey、真实 production dispatch（生产分流）、PNG writer、
  `pcd2png` 端到端路径和泛型字段 offset gate（字段偏移验收条件）。

## 动作回填

| action | status | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED test | done | `make run_test_rvv` | 首次失败于缺少 `pcie.h`，测试入口能捕获候选层缺失。 |
| GREEN helper | done | `make run_test_compare` | Std/RVV 各 3 个 gtest 通过。 |
| QEMU bench smoke | done | `make run_bench_rvv BENCH_ARGS='--iterations 1 --warmup-iterations 1 --case-filter all'` | 日志和 checksum 可解析；QEMU timing 不作为性能证据。 |
| ASM | done | `make dump_bench_rvv` -> `build/asm/riscv/bench_pcie_rvv.asm` | 可见 `vlse32.v`、`vfmul`、`vfsub`、`vfncvt`、`vse16.v`。 |
| Board repeated | done | `make collect_board_repeated BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter all' PCIE_REPEATED_RUNS=5` | RGB 稳定正向，fixed-factor 弱正向，full-range 负向。 |
| Evidence Doctor | done | `make run_board_repeated_evidence_doctor` | `Errors=1, Warnings=0, Suggestions=0`；Error 指向 full-range scaling 5/5 退化。 |

## 诊断证据链

| case | repeated board 结果 | decision bucket | 证据边界 |
| --- | --- | --- | --- |
| `rgb_unpack_pointxyzrgb_640x480` | median 1.29x，min 1.27x，max 1.33x | positive | 支持 `rgb_u32_stride_unpack_v0` 继续进入更窄 production probe 或 store 形态 A/B。 |
| `rgb_unpack_pointxyzrgba_640x480` | median 1.31x，min 1.29x，max 1.32x | positive | 同上，但只覆盖 `PointXYZRGBA` 字段布局。 |
| `scaling_fixed_factor_intensity_640x480` | median 1.06x，min 1.05x，max 1.09x | weak-positive | 支持保留线索；不足以单独触发 production 接入。 |
| `scaling_full_range_intensity_640x480` | median 0.91x，min 0.88x，max 0.93x | negative | 当前 v0 诊断边界不支持 full-range production probe。 |

Evidence Doctor（证据体检）报告位于 `log/board/repeated_phase000/evidence_doctor.md`。
该报告的 Error 是证据角色错误风险，不表示 production 必然不可优化；它只说明
`scaling_float_stride_v0` 在当前 test helper A/B 边界下不能关闭 full-range 候选。

## Diagnostic 到 production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic`，不是 production direct。 |
| A/B boundary | `test_helper`；Std 侧和 RVV 侧都绕过真实 extractor dispatch。 |
| 当前决策问题 | `RVV-vs-scalar` 的诊断筛选。 |
| diagnostic 是否可外推到 production | RGB 正向只支持 bounded production probe；scaling v0 负向不能直接推出 no-production。 |
| comparison-boundary / baseline mismatch 风险 | 存在。bench helper 直接写 vector，production 还包含 `PCLImage` 构造、field metadata 和 extractor 对象状态。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | `scaling_float_stride_v0` 不允许；RGB v0 允许进入 PI1 或先做 store A/B。 |
| clean adoption 是否需要同一 production boundary 内 A/B | 需要。若出现 `rgb_segment_store_v1` 或 production patch，必须在同边界重新比较。 |

## Matrix 更新

- `rgb_u32_stride_unpack_v0`：`attempted -> partial-production-candidate`，仅限 diagnostic evidence。
- `scaling_float_stride_v0` full-range：`attempted -> rejected for full-range v0`。
- `scaling_float_stride_v0` fixed-factor：`attempted -> weak-positive / deferred`。
- 新增下一阶段：`scaling_reduction_v1`，尝试 RVV vector reduction（向量规约）替换 full-range 第一遍 lane scan。

## Continue / Stop Decision

`continue_stop_decision=continue`。停止条件未命中：板卡可用、dirty isolation 可控、production 未触碰，
且 roadmap 中仍有当前 topic 授权范围内的未阻塞动作。

`next_phase_default=010-scaling-full-range-reduction-ab`。
