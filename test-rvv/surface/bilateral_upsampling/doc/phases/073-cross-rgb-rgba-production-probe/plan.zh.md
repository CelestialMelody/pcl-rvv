# Phase 073 plan: cross RGB/RGBA production probe

## 阶段意图和边界

本阶段是 point-type expansion（点类型扩展）的有界生产探针：只验证
`PointXYZRGB -> PointXYZRGBA` 与 `PointXYZRGBA -> PointXYZRGB` 两个交叉 RGB/RGBA
输出组合能否安全进入当前 color-gather RVV family。phase 072 为了让生产 gate 与证据范围一致，
临时收窄到 same-type；本阶段只补这个缺口，不扩大到 `PointXYZ`、normal-like 点型、其它 layout
或 `Scalar=double`。

validated_scope：organized RGBD grid、`Scalar=float`、`RVVXYZAoSFloatLayout`、输入点型只读
`x/y/z/r/g/b`，输出点型只写 `x/y/z/r/g/b`，交叉组合的 public `process` 入口。

unvalidated_scope：其它 RGB 复合点型、`PointXYZ`、`PointXYZI`、normal-like 点型、自定义点型、
其它 layout、`Scalar=double`、真实 sensor 数据和更大规模。

phase_closeout_boundary：本阶段只能决定交叉 RGB/RGBA 是否进入当前 production gate；不能改变
same-type adopted 结论，也不能把结果外推成完整泛型点类型支持。

## 当前状态清单

| 项 | 当前状态 | 证据 |
| --- | --- | --- |
| same-type production | adopted / refreshed | phase 072 public `1.43x / 1.19x / 1.21x`，steady `1.29x / 1.21x / 1.28x`，Evidence Doctor `Errors=0` |
| cross RGB/RGBA | scalar fallback | phase 072 将 `kBilateralUpsamplingRVVCompatible` 收窄到 `std::is_same_v<PointInT, PointOutT>` |
| 输出语义 | 需验证 | 标量只复制 `r/g/b` 并写 `x/y/z`；`PointXYZRGBA` 的 alpha 不是本阶段 RVV 写回语义 |
| board availability | available | 当前会话用户确认板卡恢复；复现命令使用 `SSH_OPTS='-F /dev/null' RSYNC_SSH='ssh -F /dev/null'` |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public / production-detail |
| A/B boundary | public `BilateralUpsampling<PointInT, PointOutT>::process`，必要时补 production detail helper 归因 |
| 当前决策问题 | RVV-vs-scalar for cross RGB/RGBA gate expansion |
| diagnostic 是否可外推到 production | no；必须使用 public entry correctness 和 board compare |
| comparison-boundary / baseline mismatch 风险 | yes；若只用 helper 直调会绕过 public dispatch，因此不能 clean-adopt |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes；本阶段 probe 已限制在两个交叉组合，负向时保持 scalar fallback |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | no；当前决策是交叉组合 RVV-vs-scalar 是否比标量快，不是替换 adopted family |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| cross RGB/RGBA color-gather | organized RGBD grid | `PointXYZRGB -> PointXYZRGBA`、float、AoS layout | public `process` | new public-entry tests | new public / steady public bench labels | pending | color-gather helper with `vlse8/vluxei16/vfabs/vmflt/vfredusum` | pending | planned |
| cross RGB/RGBA color-gather | organized RGBD grid | `PointXYZRGBA -> PointXYZRGB`、float、AoS layout | public `process` | new public-entry tests | new public / steady public bench labels | pending | same as above | pending | planned |

## 实现和测试动作

1. 新增交叉点型 public-entry correctness helper 和两个 gtest，证明输出 `x/y/z/r/g/b` 与标量 reference 对齐。
2. 新增交叉点型 public / steady public bench labels，保持 same-type case 不变。
3. 如果 QEMU correctness 通过，将 production gate 从 same-type 收窄改为 RGB/RGBA exact-family gate，并保留其它点型 fallback。
4. 运行 `run_test_compare`、`dump_bench_rvv`、`run_bench_rvv BENCH_ARGS='1 0'`。
5. 板卡运行 `board_smoke SSH_OPTS='-F /dev/null' RSYNC_SSH='ssh -F /dev/null'`，生成当前二进制 public compare。
6. 为交叉 case 建立 phase 073 manifest 并运行 Evidence Doctor；根据结果更新 result、matrix、roadmap、evaluation、topic docs 和 Handoff。

## 板卡复跑预算和决策桶

本阶段先使用 single board smoke（`iterations=5`、`warmup=2`、`run_count=1`）判断方向：
`speedup >= 1.05x` 为 positive，`1.00x <= speedup < 1.05x` 为 weak-positive，
`0.98x <= speedup < 1.00x` 为 neutral，`speedup < 0.98x` 为 negative。若两个交叉 case
都 positive 且 Evidence Doctor 无 Error，可建议接入；若任一 case weak / neutral / negative，
保持 scalar fallback 或要求 repeated board 后再判断。

## 继续 / 停止条件

若 correctness、asm 或 board 任一关键证据失败，本阶段停止在 `attempted`，生产 gate 保持 same-type。
若交叉组合均正向且 Evidence Doctor 通过，进入 production gate expansion closeout，并等待用户确认是否把
交叉 RGB/RGBA 纳入 adopted production behavior。

## 文档更新清单

阶段完成后同步 `result.zh.md`、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、
topic-local evaluation、correctness/benchmark/optimization evidence、长期 `doc-rvv` 和 Handoff。
