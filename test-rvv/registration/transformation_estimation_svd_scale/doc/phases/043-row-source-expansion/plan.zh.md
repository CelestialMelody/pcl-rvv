# Phase 043 计划：row-source expansion

## 阶段意图和边界

本阶段从已采纳的 ordered public scale RVV 主线继续推进 `row-source-expansion`（行来源扩展）：为 `TransformationEstimationSVDScale` 补 source-indexed、dual-indexed 和 correspondence 三类公开入口的 scale-aware fused accumulation（带尺度估计的融合累加）探针。目标是确认 scale 子类在 `use_umeyama_ == false` 时也能绕过父类 iterator + demean 动态矩阵路径，而不是误以为父类普通 SVD 的 RVV row-source helper 已经覆盖 scale。

本阶段验证范围冻结为 `PointXYZ -> PointXYZ`、`Scalar=float`、dense xyz AoS、非退化 source variance、`nr_points >= 16`。不证明更多泛型点型、`Scalar=double`、非 dense、非法 index / correspondence、全部自定义 xyz AoS 或 production final adoption。

## S0 偏好冻结

| 字段 | 冻结值 |
| --- | --- |
| `preferences_loaded` | defaults loaded；local override absent；prompt override 要求继续当前优化矩阵。 |
| `loaded_instruction_sources` | `AGENTS.md`、`.agents/config/defaults.yaml`、`rvv-workflow/SKILL.md`、`rvv-test/SKILL.md`、`rvv-test/references/optimization-phase-loop.zh.md`、`rvv-test/references/registration-topic-evidence.zh.md`、`rvv-documentation/SKILL.md`。 |
| `work_preferences` | production 注释克制；test-rvv / diagnostic 注释中文说明证据角色；evidence policy 为 summary-only。 |
| `dirty_isolation` | 只处理 `registration/include/pcl/registration/transformation_estimation_svd_scale.h`、`registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` 和 `test-rvv/registration/transformation_estimation_svd_scale/**`；不碰其它 topic 的 dirty work。 |
| `commit_preferences` | 不自动 commit；若后续提交，topic/生产源码和证据日志分开审查。 |

## 当前状态清单

| 对象 | 当前状态 | 证据 / 位置 |
| --- | --- | --- |
| ordered production scale | adopted-by-user。 | Phase 031；production helper 已覆盖 ordered public overload。 |
| matrix-local helper | production source 已接 `trace(R * H)`。 | Phase 042；当前仍是窄 helper probe。 |
| generic point type | 代表点型 QEMU/ASM/board 已闭合。 | Phase 040/041。 |
| row source | 矩阵中仍是 `phase_deferred + unblocked_after_adoption`。 | `doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`。 |
| 父类普通 SVD row source RVV | 已存在 source-indexed / dual-indexed / correspondence helper，但受 `use_umeyama` gate 限制。 | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp`。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `row-source-direct-fused-scale-accum` | 把 ordered scale 累加扩展到 indexed / correspondence，可以继续避免 scale 子类的 centroid、demean 和后段 rotated-cloud pass。 | gather / correspondence 载入成本可能吞掉收益；不能继承 ordered board 结论。 |
| `source-indexed-scale` | source 端 gather，target 端顺序 load，适合 indexed source + 等长 target。 | index range、byte offset 上限和 gather 成本需要 gate。 |
| `dual-indexed-scale` | source / target 两端都 gather，适合双索引公开入口。 | 双 gather 更贵，收益不确定。 |
| `correspondence-scale` | 从 correspondences 读取 query/match 后双 gather。 | correspondence 结构读取、非法项语义和 gather 成本需要独立证据。 |

## 本阶段优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `row-source-direct-fused-scale-accum` | source-indexed | `PointXYZ -> PointXYZ` / `float` / dense xyz AoS | 新增 public source-indexed 对拍和 fallback gate。 | 新增 `row-source-scale` case-filter。 | 本阶段优先补 QEMU smoke；board repeated 若 target 完成再登记。 | RVV bench asm 中需可见 indexed load / FMA / reduction。 | QEMU smoke doctor；board doctor 若采集。 | planned |
| `row-source-direct-fused-scale-accum` | dual-indexed | 同上 | 新增 public dual-indexed 对拍和 fallback gate。 | 同上。 | 同上。 | 同上。 | 同上。 | planned |
| `row-source-direct-fused-scale-accum` | correspondence | 同上 | 新增 public correspondence 对拍和 fallback gate。 | 同上。 | 同上。 | 同上。 | 同上。 | planned |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| production overload | 在 scale 子类声明并实现 source-indexed、dual-indexed、correspondence override。 | gate 失败时回父类；gate 命中时使用 scale 专用 RVV 累加。 |
| test support | 增加 indices / correspondences deterministic fixture、public wrapper 和 bench case。 | `run_test_compare` 能覆盖三类 row source。 |
| QEMU smoke | `ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/transformation_estimation_svd_scale run_bench_compare BENCH_ARGS="--case-filter row-source-scale --iterations 3 --warmup-iterations 1"` | 日志形状、checksum 和 `max_reference_error` 可解析。 |
| asm attribution | `make -C test-rvv/registration/transformation_estimation_svd_scale dump_bench_rvv` | bench 符号中出现 gather / FMA / reduction 相关指令。 |
| registry | `make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` | 没有 stale / unregistered 错误。 |

## Diagnostic-to-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | production-public probe（生产公开入口探针），因为会触碰真实 scale 子类公开入口。 |
| A/B boundary | public overload；Std 构建为标量父类路径，RVV 构建为 scale 子类 row-source RVV path。 |
| 当前决策问题 | RVV-vs-scalar for row source；不是与 ordered 主线互相替代的 RVV-family-selection。 |
| diagnostic 是否可外推到 production | QEMU smoke 不可外推性能；只有 board repeated 才能支撑性能。 |
| comparison-boundary / baseline mismatch 风险 | 有；每个 row source 的 gather 和 correspondence 成本不同，必须分 case 报告。 |
| weak / negative / unstable 时是否允许 bounded production probe | 本阶段已经是 bounded production probe；若板卡弱或负，保留源码探针等待用户判断，不写 clean adoption。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前 row source 还没有既有 scale RVV family；不需要 RVV-vs-RVV，但需要各 row source 自己的 public Std/RVV board evidence。 |

## 板卡复跑预算和完成条件

默认 board repeated budget 为 5 runs，沿用 `TESVD_SCALE_BOARD_REPEATED_RUNS`、20 次 iteration 和 5 次 warmup。若本轮只完成 QEMU correctness / smoke / ASM，则阶段结果必须写成 `production_probe_qemu_asm_pending_board`，并把 board repeated 保留为 unblocked next action，不能写成 adopted。

本阶段完成条件：

- correctness：Std/RVV `run_test_compare` 通过；
- QEMU smoke：`row-source-scale` case-filter 可解析且 Evidence Doctor 无 Error；
- asm：能归属到 row-source RVV load / FMA / reduction；
- board：若完成 5-run repeated 且按 row-source case 分桶，才能把 decision 提升到 board-supported positive / weak / negative；
- 文档：phase result、optimization matrix、roadmap、evaluation、testing overview、benchmark/evidence、test-support map 和长期 `doc-rvv` 同步当前真实范围。

## 继续 / 停止条件

如果三类 row source 只完成 QEMU/ASM，默认下一阶段继续 board repeated。若 board positive，再进入用户检查点，确认是否采纳 row-source 扩展为正式 production behavior。若某一类 row source 编译或语义失败，只关闭该 row source 的矩阵条目，不能影响其它 row source。
