# Phase 000: current-state and diagnostic staged-window

## 阶段意图和边界

本阶段建立 `surface/bilateral_upsampling` 的接入前诊断，验证 `performProcessing` 窗口查表累加是否存在可采纳的 RVV 信号。范围只包含 `test-rvv/surface/bilateral_upsampling` 下的测试资产和 topic-local 文档；不修改 production（生产源码）。

validated_scope（本阶段验证范围）：test-local `RgbPoint`、float depth、uint8 RGB、organized grid、`window_size=3/4/5`、dense 与 NaN holes 输入、`Scalar=float` 累加。

unvalidated_scope（未验证范围）：真实 `pcl::PointXYZRGB` / `pcl::PointXYZRGBA` 字段布局、`BilateralUpsampling::process` 公开入口、真实 projection matrix、production fallback、非 RVV 构建下的生产分流、工具 `tools/bilateral_upsampling.cpp` 的端到端路径。

## 当前状态清单

- 源码：`surface/include/pcl/surface/impl/bilateral_upsampling.hpp` 当前无 `__RVV10__` 分支。
- 筛选来源：`doc-rvv/library-screening/surface/surface-function-evaluation-queue.zh.md` 将该文件列为建议进入函数级评估。
- topic 资产：本阶段新建 `test-rvv/surface/bilateral_upsampling`。

## 候选族

| candidate | 假设 | 风险 |
| --- | --- | --- |
| staged-window-reduction | 查表后 `weight * z` 与 `weight` 规约可用 RVV 降低窗口累加成本。 | 每个中心像素的 staging 可能比规约收益更贵。 |

## 优化矩阵

见 `doc/phases/optimization-matrix.zh.md`。

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| scaffold | `Makefile`、`board.mk`、`include/`、`src/`、topic docs | 构建入口符合 shared Makefile。 |
| correctness | `make -C test-rvv/surface/bilateral_upsampling run_test_compare` | std / RVV 构建均通过。 |
| asm | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | 看到 `vsetvli`、`vle32.v`、`vfmul.vv`、`vfredusum.vs` 等候选指令。 |
| board | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | 板卡 test 通过并生成 compare summary。 |
| doctor | `python3 test-rvv/script/evidence_doctor.py --summary-md ...` | Errors / Warnings / Suggestions 已解释。 |

## 板卡复跑预算和决策桶

默认先跑一次 `board_smoke`。若 summary 方向接近 1.00x、长尾或 Evidence Doctor Warning 影响判断，最多追加一次同边界复跑。决策桶：`positive >= 1.10x`，`weak_positive 1.03x..1.10x`，`neutral 0.97x..1.03x`，`negative < 0.97x`，若两次跨桶则 `unstable`。

## 继续 / 停止条件

若板卡结果为 stable positive / weak_positive，默认停在用户确认点：建议是否进入 production integration loop。若结果 neutral / negative，默认不建议接入当前 staged 候选，并把 direct-load 或更少 staging 的候选留给 phase 010。若板卡不可达或 Evidence Doctor Error 未能消除，转为 blocked handoff。

## 文档更新清单

回填 `result.zh.md`、`doc/bilateral_upsampling-evaluation.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/phases/optimization-matrix.zh.md` 和最终 Handoff。没有 adopted production behavior 时不创建 `doc-rvv/surface/bilateral_upsampling-RVV.zh.md`。

