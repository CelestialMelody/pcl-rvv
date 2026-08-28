# Phase 000: normal-sphere count/select 诊断计划

## 阶段意图和边界

本阶段针对 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` 建立 production-shaped diagnostic（生产形态诊断）。目标是证明或反证 `countWithinDistance` / `selectWithinDistance` 的球心方向、球壳距离和 normal angle（法线夹角）组合是否适合 RVV。阶段不修改 production（生产源码），不创建 production 长期 `doc-rvv`，不把诊断结果写成 production direct（真实生产路径证据）。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| production | 三个公开入口都是标量循环；没有 `__RVV10__` 分流。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` |
| upstream smoke | `quadric_models` 有 NormalSphere RANSAC 冒烟测试。 | `test-rvv/sample_consensus/quadric_models/test_sample_consensus_quadric_models.cpp` |
| sibling evidence | sphere 已证明球壳 select/count；normal-plane 已证明 normal angle helper，但二者不能外推。 | `doc-rvv/library-screening/sample_consensus/sample_consensus-retained-candidate-rescreen.zh.md` |
| topic scaffold | 本阶段新建独立 `sac_model_normal_sphere` topic。 | `test-rvv/sample_consensus/sac_model_normal_sphere/` |
| board | 用户说明板卡可用；需要按 Makefile / board.mk 执行有界验证。 | prompt + `board.mk` |

## validated_scope / unvalidated_scope

| scope type | 内容 |
| --- | --- |
| validated_scope | 本阶段准备验证 `PointXYZ + Normal` 和 `PointXYZI + Normal` 的 direct indexed `indices_`，覆盖 `countWithinDistance` 和 `selectWithinDistance` 测试专用候选。 |
| unvalidated_scope | `PointXYZRGB/RGBA`、自定义 xyz 点型、非标准 normal layout、`PointNT` 非 `Normal`、`getDistancesToModel` 生产候选、production dispatch、`Scalar=double` 和非 indexed 入口。 |
| point_type_expansion_queue | Phase 010/020 后续按证据扩展 source 点型和 normal 点型；每个扩展都需要 correctness、fallback、asm、board 和 Evidence Doctor。 |
| phase_closeout_boundary | 本阶段只能关闭 test-only candidate 的诊断条目；不能关闭 production 接入或泛型模板入口。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic；测试专用 subclass 复刻 public entry 数据形态，但不修改真实 production dispatch。 |
| A/B boundary | public overload + test helper；bench 计时边界只包含 public 或 candidate 函数调用，不包含输入构造。 |
| 当前决策问题 | RVV-vs-scalar 候选筛选，以及是否值得进入 PI1 production integration plan（生产接入计划）。 |
| diagnostic 是否可外推到 production | 不能直接外推；只能说明相同输入、indices、点型和 normal layout 下公式与输出合同可行。 |
| comparison-boundary / baseline mismatch 风险 | 有。Std/RVV 是两个构建，candidate 不是 production dispatch；board summary 必须把 public baseline 和 candidate row 分开。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 correctness、球心退化边界、asm 归属和 Evidence Doctor 无 Error，且弱/负向不来自语义错配时，才允许窄范围 production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 是。若后续尝试 `vcompress` 或 dense double store，必须补同一 production boundary 内的 RVV-vs-RVV A/B。 |

## experience-migration audit

| sibling 经验维度 | sibling topic 里的机制 | 当前 topic 是否适用 | 状态 | 证据 / 理由 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| sphere shell mask | 基础 sphere 用 RVV 计算半径距离和 mask。 | 适用但不能外推。 | adopted | normal-sphere 共享 `n_dir.norm() - radius`，但还叠加 normal angle。 | 在 candidate 中复刻球壳距离并重测。 |
| normal-plane angle helper | `getAcuteAngle3DRVV_f32m2` 可计算锐角近似。 | 部分适用。 | attempted | normal-plane 第二向量是固定平面法线；normal-sphere 第二向量是每点 `n_dir`，需额外归一化。 | correctness 覆盖阈值附近和退化方向。 |
| mask + compress | sphere/normal-plane 已有 select 压缩输出正向。 | 暂缓。 | deferred | 首阶段先证明公式和 early mask；`vcompress` 是下一实现族。 | Phase 010 视 Phase 000 结果启动。 |
| full-RVV double store | line/circle/stick 在 getDistances 中正向。 | 暂缓。 | deferred | normal-sphere 有 `acos` 和双 sqrt，无 early continue；首阶段不扩大。 | 只有 count/select 正向后再审计。 |
| production boundary | mature sibling 都在 PI1-PI5 后才 adopted。 | 适用。 | adopted | 本阶段不改 production，避免诊断证据越界。 | 正向后另建 PI1。 |

## 优化矩阵

见 `../optimization-matrix.zh.md`。本阶段计划先关闭 `PointXYZ + Normal` 和 `PointXYZI + Normal` 的 count/select diagnostic 行。

## 实现和测试动作

| action | files | command / evidence | completion |
| --- | --- | --- | --- |
| 写入 phase plan 和 topic scaffold | `Makefile`、`board.mk`、`doc/`、`src/` | `git status --short --untracked-files=all -- test-rvv/sample_consensus/sac_model_normal_sphere` | 目录和计划可恢复。 |
| 写 RED correctness test | `src/test_sac_model_normal_sphere.cpp` | `make -C test-rvv/sample_consensus/sac_model_normal_sphere run_test_std` | 初次因候选聚合头缺失失败。 |
| 实现 test-only candidate | `include/test_sac_model_normal_sphere.h`、`include/bench_sac_model_normal_sphere.h`、`include/impl/sac_model_normal_sphere_access.hpp` | `run_test_compare` | Std/RVV correctness 通过。 |
| 反汇编归因 | `dump_bench_rvv` | `build/asm/riscv/bench_sac_model_normal_sphere_rvv.asm` | 可见候选 helper 的 `vfsqrt`、`vcompress` 或 mask/count 指令，若归属不足则降级。 |
| 板卡证据 | `board_smoke` / repeated run | board logs + summary | 只在目标硬件上解释性能；先 1-run，摇摆时最多 5-run。 |

## Evidence Doctor 和 registry 规则

benchmark、board summary、checksum 或 EvidenceDecision 前必须运行 `test-rvv/script/evidence_doctor.py` 或人工填写 Errors / Warnings / Suggestions。当前 topic 初建，registry 预期先为 `not_available`；生成 summary 后再补正式 manifest / registry target。

## 板卡复跑预算和决策桶

先运行 1 次 board smoke / compare。若 `count` 或 `select` speedup 在 `0.95x-1.05x`，或方向与 asm/correctness 矛盾，最多扩展到 5-run repeated。decision bucket（决策桶）：`positive >= 1.20x`、`weak-positive 1.05x-1.20x`、`neutral 0.95x-1.05x`、`negative < 0.95x`、多批方向不稳为 `unstable`。

## 继续 / 停止条件

板卡当前可用，所以需要板卡本身不是停止条件。合法停止条件是工具链或板卡实际不可达、Evidence Doctor Error 不能修复、dirty isolation 不安全，或下一步需要修改 production 源码而尚未进入 PI1。若 correctness / asm / board 仍未闭合且没有真实 blocker，本阶段继续推进。

## 文档更新清单

阶段结束前更新 `result.zh.md`、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、evaluation 和 Handoff。`doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md` 当前为 not_applicable，因为没有 adopted production behavior（已采用生产行为）。
