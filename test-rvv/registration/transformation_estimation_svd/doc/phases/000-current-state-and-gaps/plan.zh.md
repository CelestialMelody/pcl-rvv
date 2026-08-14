# Phase 000 计划：当前状态和 dense ordered-cloud-pair 诊断 scaffold

## 阶段意图和边界

本阶段为 `registration/transformation_estimation_svd` 建立 RVV 诊断基础。目标是重建当前 SVD estimator 的标量路径，补齐 dense ordered-cloud-pair（稠密顺序点云对）`PointXYZ` / `float` 的 test-only fused accumulation（测试专用融合累加）candidate、correctness（正确性）测试、bench smoke（性能测试小型验证）和 topic-local 文档。

本阶段不修改 production（生产源码）。完成后最多输出 `diagnostic` 或 `partial-production-candidate` 前置证据；没有板卡 repeated bench 前不能进入 production-ready。

## 当前标量路径

| 步骤 | 当前源码语义 | RVV 诊断边界 |
| --- | --- | --- |
| public ordered-cloud-pair entry | 检查 source / target size，一致时构造两个 `ConstCloudIterator`。 | Phase 000 只覆盖这个 row source。 |
| `use_umeyama_ == true` | 默认路径，逐点装填两个 `3 x N` Eigen matrix，再调用 `pcl::umeyama(..., false)`。 | candidate 避免动态矩阵装填，但仍保留 3x3 SVD。 |
| `use_umeyama_ == false` | centroid、demean、correlation、3x3 SVD。 | 作为公式类比，不是默认构造参数路径。 |
| indices / correspondences overload | 通过 iterator 把不同 row source 统一到 helper。 | 本阶段不覆盖，写入 roadmap。 |
| 3x3 SVD tail | 每次 estimate 执行一次，Eigen 求解。 | 不手写 RVV。 |

## 假设与候选族

| candidate family | idea source | 假设 | 初始状态 |
| --- | --- | --- | --- |
| `fused_full_cloud_accum` | 默认 Umeyama 动态矩阵装填 | 大 N 时 fused accumulation 减少 matrix allocation / fill，可能抵消 Eigen SVD 固定成本。 | implemented in test-rvv scaffold |
| `public_umeyama_baseline` | 当前 production truth | 作为 semantic anchor 和 board A/B baseline。 | implemented in bench wrapper |
| `row_source_audit` | registration evidence rules | indexed / correspondences 不能继承 ordered-cloud-pair 结论。 | planned |

## 优化矩阵

矩阵主归属见 `doc/phases/optimization-matrix.zh.md`。Phase 000 只推进 `fused_full_cloud_accum` 的 correctness、QEMU log shape 和 asm 输入。

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| A1 | `include/tesvd.h`、`include/impl/tesvd_candidates.hpp` | 提供稳定聚合入口、fixtures、public baseline、fused scalar reference 和 RVV candidate。 |
| A2 | `src/test_tesvd.cpp` | 覆盖 public Umeyama semantic anchor、RVV candidate、small fallback、PointXYZI layout 和 determinant sign fix stress。 |
| A3 | `src/bench_tesvd.cpp` | 输出可解析 `Dataset:`、`Iterations:`、case `ms/iter`、`Total Time` 和 checksum。 |
| A4 | `Makefile`、`board.mk` | 接入公共 `rvv-topic.mk` 和 board runner。 |
| A5 | README、evaluation、doc suite、roadmap、matrix、phase result、Handoff | 回填 S0、证据边界、EvidenceDecision 和下一步。 |

## Evidence Doctor 和 registry 规则

`log/evidence_registry.json` 已初始化。Phase 000 若只运行 correctness，可在 result / Handoff 中写只覆盖 correctness 的 Evidence Doctor 边界。若运行 bench smoke，必须解释 QEMU timing 只作 log shape，并用 topic-local wrapper 生成 QEMU smoke manifest；正式板卡 repeated summary 和 board Evidence Doctor wrapper 推迟到 Phase 010。

## 板卡复跑预算和决策桶

本阶段不自动跑 repeated board。Phase 010 计划使用 5-run 起步，最多追加 1 轮同边界确认；decision bucket（决策桶）使用 `positive / weak_positive / neutral / negative / unstable`。

## 继续 / 停止条件

继续条件：

- correctness、asm 或 QEMU smoke 仍能在当前 topic-local 范围内补齐。
- Evidence Doctor wrapper、manifest 或 doc suite 仍缺低风险字段。
- 发现 row source audit 仍只触碰 test-rvv 资产。

停止条件：

- 需要修改 production 文件、public API 或进入 PI1。
- 工具链、QEMU 或依赖不可用，且失败命令已记录。
- Evidence Doctor Error 不能修复，或证据层之间矛盾。
- dirty isolation 无法保证只触碰当前 topic。

## 文档更新清单

- `test-rvv/registration/transformation_estimation_svd/README.zh.md`
- `test-rvv/registration/transformation_estimation_svd/doc/transformation_estimation_svd-evaluation.zh.md`
- `test-rvv/registration/transformation_estimation_svd/doc/optimization-roadmap.zh.md`
- `test-rvv/registration/transformation_estimation_svd/doc/phases/optimization-matrix.zh.md`
- `test-rvv/registration/transformation_estimation_svd/doc/phases/000-current-state-and-gaps/result.zh.md`

## roadmap 同步动作

本阶段结束时把 `fused_full_cloud_accum` 的 correctness、asm、board 和 Evidence Doctor 状态回填到 roadmap / matrix。若 QEMU、asm 或 board 未运行，应记录恢复命令和真实阻塞条件。
