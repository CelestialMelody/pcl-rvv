# Phase 030: PointXYZRGB / PointXYZRGBA 点型扩展结果

## 阶段范围

| 项 | 内容 |
| --- | --- |
| validated_scope | `PointXYZRGB + Normal`、`PointXYZRGBA + Normal`，direct indexed source/normal，float xyz input，独立 `pcl::Normal` cloud。 |
| unvalidated_scope | 自定义 registered xyz 点型、source 自带 normal 字段的点型组合、其它 normal 点型、`Scalar=double`、非 indexed 入口、production dispatch（生产分流）。 |
| phase_closeout_boundary | 本阶段只关闭 RGB/RGBA source layout 的测试专用点型扩展，不关闭泛型模板入口或 production adoption（生产采纳）。 |

## RED / GREEN 回填

计划落盘前先运行过 RED：

```bash
make -C test-rvv/sample_consensus/sac_model_normal_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZRGB'
```

当时 bench CLI 返回 `Unsupported point type: PointXYZRGB`，exit code 为 2。该执行顺序早于 phase plan 落盘，属于 phase-order deviation（阶段顺序偏差），但它证明了本阶段要关闭的真实缺口：RGB/RGBA 点型没有进入测试专用 bench 入口。

GREEN 侧只修改 topic-local test-rvv 资产：

- `include/test_sac_model_normal_sphere.h` 和 `include/bench_sac_model_normal_sphere.h` 为 `PointXYZRGB` / `PointXYZRGBA` 填充颜色字段。颜色字段不参与 normal-sphere 距离公式，只用于证明 xyz 字段 offset 和 AoS stride（结构数组跨步）不会被附加字段破坏。
- `src/test_sac_model_normal_sphere.cpp` 新增 `PointXYZRGBAndRGBALayoutsMatchReference`，把 RGB/RGBA source cloud 与独立 `pcl::Normal` cloud 同标量参考对拍。
- `src/bench_sac_model_normal_sphere.cpp` 允许 `PointXYZRGB` / `PointXYZRGBA` CLI 参数。
- `Makefile` 和 `script/generate_normal_sphere_evidence_manifest.py` 增加 Phase 030 manifest、summary、Evidence Doctor（证据体检）和 registry（证据登记表）入口。

production 文件 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` 未修改。

## 执行结果

| 证据类型 | 命令 / 路径 | 结果 | 边界 |
| --- | --- | --- | --- |
| Python 语法 | `python3 -m py_compile test-rvv/sample_consensus/sac_model_normal_sphere/script/generate_normal_sphere_evidence_manifest.py` | 通过。 | 只证明 manifest generator 语法可解析。 |
| RED | `make -C test-rvv/sample_consensus/sac_model_normal_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZRGB'` | 失败，提示 unsupported point type，exit code 2。 | 证明 bench CLI 缺口，不证明 RVV 数值语义。 |
| QEMU correctness（QEMU 正确性） | `make -C test-rvv/sample_consensus/sac_model_normal_sphere run_test_compare` | Std/RVV 各 5 个 gtest 通过。 | QEMU 不证明真实性能。 |
| QEMU bench smoke（小型可运行性检查） | `make -C ... run_bench_rvv BENCH_ARGS='128 1 PointXYZRGB'` 和 `PointXYZRGBA` | 两个 Dataset 行均显示目标点型并返回 0。 | 只证明 RVV bench binary 可运行和日志形状，不写性能结论。 |
| 反汇编归属 | `make -C test-rvv/sample_consensus/sac_model_normal_sphere clean_bench_rvv dump_bench_rvv` | RVV bench binary 可重新生成反汇编；RGB/RGBA 复用同一 `RVVXYZAoSFloatLayout<PointT>` 和 `RVVNormalFloatLayout<PointNT>` 候选 helper 形态。 | 当前归属仍是测试专用 bench binary，不是 production helper。 |
| Board PointXYZRGB | `make -C test-rvv/sample_consensus/sac_model_normal_sphere board_smoke OUTPUT_DIR_BOARD=log/board-phase030-pointxyzrgb REMOTE_BOARD_OUTPUT_DIR=<REMOTE_BOARD_OUTPUT_DIR>/log/board-phase030-pointxyzrgb BENCH_ARGS='65536 200 PointXYZRGB'` | 板卡 gtest 5/5 通过；checksum `32012`。select `2.452x`，vcompress select `2.910x`，count `5.329x`，getDistances `4.518x`。 | 单次 board smoke，只支撑点型扩展初筛。 |
| Board PointXYZRGBA | `make -C test-rvv/sample_consensus/sac_model_normal_sphere board_smoke OUTPUT_DIR_BOARD=log/board-phase030-pointxyzrgba REMOTE_BOARD_OUTPUT_DIR=<REMOTE_BOARD_OUTPUT_DIR>/log/board-phase030-pointxyzrgba BENCH_ARGS='65536 200 PointXYZRGBA'` | 板卡 gtest 5/5 通过；checksum `32012`。select `2.324x`，vcompress select `2.975x`，count `5.424x`，getDistances `4.553x`。 | 代表 RGB/RGBA source layout，不代表所有自定义点型。 |
| Evidence Doctor | `doc/phases/030-rgb-rgba-point-type-expansion/board-evidence-doctor.md` | `Errors=0`、`Warnings=8`、`Suggestions=0`。 | 8 个 Warning 均为 `low_run_count`。 |
| Evidence registry | `make -C test-rvv/sample_consensus/sac_model_normal_sphere record_phase030_evidence_state && make -C ... evidence_status` | registry check 为 `fresh`。 | Phase 000-030 summary / manifest / doctor 均已登记。 |

## 板卡摘要

| case | point type | baseline ms | candidate ms | B/A speedup | bucket | evidence role |
| --- | --- | ---: | ---: | ---: | --- | --- |
| `diagnostic candidate selectWithinDistance` | `PointXYZRGB + Normal` | 9.7830 | 3.9895 | 2.452x | positive | production-shaped diagnostic |
| `diagnostic candidate vcompress selectWithinDistance` | `PointXYZRGB + Normal` | 9.7836 | 3.3626 | 2.910x | positive | component ablation |
| `diagnostic candidate countWithinDistance` | `PointXYZRGB + Normal` | 14.6652 | 2.7522 | 5.329x | positive | production-shaped diagnostic |
| `diagnostic candidate getDistancesToModel` | `PointXYZRGB + Normal` | 14.2316 | 3.1502 | 4.518x | positive | production-shaped diagnostic |
| `diagnostic candidate selectWithinDistance` | `PointXYZRGBA + Normal` | 9.7423 | 4.1918 | 2.324x | positive | production-shaped diagnostic |
| `diagnostic candidate vcompress selectWithinDistance` | `PointXYZRGBA + Normal` | 9.7479 | 3.2767 | 2.975x | positive | component ablation |
| `diagnostic candidate countWithinDistance` | `PointXYZRGBA + Normal` | 14.6551 | 2.7018 | 5.424x | positive | production-shaped diagnostic |
| `diagnostic candidate getDistancesToModel` | `PointXYZRGBA + Normal` | 14.2459 | 3.1290 | 4.553x | positive | production-shaped diagnostic |

同一日志中的 public overload（公开入口）Std/RVV speedup 约为 `0.983x-1.010x`。公开入口当前没有 production RVV dispatch，因此这些 public 行只是 mixed-boundary cross-check（混合边界交叉检查），不能作为 production performance（生产性能）或 production direct（真实生产路径证据）。

## Evidence Doctor 解释

本阶段修正过一次 evidence label（证据标签）误触发：`rgb-rgba-board-smoke` 中的 `a-b` 子串会被 Evidence Doctor 误识别为 A/B（严格对比）语义。当前 run label 改为 `normal-sphere-phase030-rgb-rgba-smoke` 后，`role_name_mismatch` warning 已消失。

剩余 8 个 Warning 都是 `low_run_count`：RGB/RGBA 每个候选只采集一次 board smoke，不能判断异常频率、温度 / governor / freq 或 repeated-board（重复板卡测试）稳定性。由于所有候选 speedup 都远高于 `1.20x` positive 阈值，没有触发本阶段自动复跑；若进入 production integration loop（生产接入闭环），PI4 必须在真实 production boundary（生产边界）内重跑。

## Diagnostic 到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic；`vcompress` 行是 component ablation（组件消融）。 |
| A/B boundary | board Std build fallback vs board RVV build test-only candidate；`vcompress` 行为同一日志中的写回形态诊断，不是 production RVV-vs-RVV 结论。 |
| 当前决策问题 | RGB/RGBA source layout 是否可加入 PI1 候选讨论。 |
| 是否可外推到 production | 不能直接外推。production 源码未改，缺少 production dispatch、fallback、production direct correctness、production asm 和 repeated board。 |
| comparison-boundary / baseline mismatch 风险 | 存在。测试专用派生类和 public overload 边界不同；颜色字段只是布局负载，不参与距离公式。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段结果为 positive；若后续 repeated board 降为弱 / 中性，也只能降级 diagnostic，不得直接推出 no-production。 |
| clean adoption 是否需要同一 production boundary 内证据 | 需要 PI1-PI5，并在 PI5 后等待用户明确确认。 |

## 阶段结论

Phase 030 决策为 `point-type-expansion-positive-diagnostic`。在测试专用 direct indexed source/normal 边界内，`PointXYZRGB + Normal` 和 `PointXYZRGBA + Normal` 均可复用当前 RVV source xyz layout gate，三入口候选和 `vcompress` select 写回都保持正向板卡信号。

本结论不改变 production 状态。`countWithinDistance`、`selectWithinDistance` 和 `getDistancesToModel` 仍只能作为 PI1 候选范围讨论；真实接入需要用户明确授权进入 production integration loop。

## 继续 / 停止判断

当前 phase 已闭合，registry 为 fresh，未命中板卡不可用、Evidence Doctor Error、证据矛盾或 dirty isolation 不安全。由于本 topic 已有多阶段、board summary、Evidence Doctor 和复杂测试支撑结构，workflow 规则要求补齐 topic-local doc suite（主题本地文档套件）后才能进入 `ready_for_review`。下一阶段默认入口是 `040-structure-parity-doc-suite`，只触碰当前 topic 的文档套件和引用，不修改 production 源码。
