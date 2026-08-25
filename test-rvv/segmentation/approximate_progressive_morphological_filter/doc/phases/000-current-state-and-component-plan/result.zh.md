# 000 current-state-and-component-plan 结果

## 实际执行范围

本阶段按计划只创建 `test-rvv/segmentation/approximate_progressive_morphological_filter/` 下的测试资产、bench（性能测试）资产、topic-local 阶段文档和证据 manifest（证据清单）生成脚本。`segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp` 生产源码未修改；当前证据角色是 diagnostic（诊断），不能替代 production direct（真实生产路径）证据。

## 计划动作回填

| action | status | 命令 / 证据路径 | 结论 |
| --- | --- | --- | --- |
| 写 RED 测试 | done | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_rvv`，首次因缺少 `apmf.h` 失败 | 测试先于 helper 存在，失败原因命中缺少候选入口 |
| 实现 test-only helpers | done | `include/apmf.h`、`include/impl/apmf_components.hpp` | 建立 z-min、window open 和 tail-compress 的 Std / RVV 同构组件 helper |
| 写 component bench | done | `src/bench_apmf.cpp` | 输出 `Dataset:`、`Iterations:`、case label 和 checksum，可被共享 compare 脚本解析 |
| 接入 Makefile / board.mk | done | `Makefile`、`board.mk` | QEMU correctness、bench build、board smoke、repeated board 和 Evidence Doctor target 均可运行 |
| 补 topic-local 文档 | partial | 本文件、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md` | 阶段文档和 roadmap 已建立；evaluation 与完整 doc suite 仍是后续结构成熟度动作 |

## 正确性和路径证据

| 证据层 | 命令 / 路径 | 结果 | 边界 |
| --- | --- | --- | --- |
| RED gate（会失败的验收条件） | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_rvv` | 首次因 `apmf.h` 缺失失败 | 证明测试能捕捉候选入口缺失 |
| QEMU correctness（QEMU 正确性） | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_compare` | Std / RVV 均通过 4 个 component 测试 | 只证明同构组件正确性，不证明真实性能 |
| QEMU bench smoke（QEMU 小型冒烟） | `ALLOW_QEMU_BENCH_COMPARE=1 BENCH_ARGS='--size 4096 --grid-rows 24 --grid-cols 24 --half 2 --iterations 2 --warmup 1' make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_bench_compare` | 输出形状和 checksum 可解析 | 仅用于 build / log-shape（日志形状）检查，不作为性能结论 |
| asm attribution（反汇编归属） | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter dump_bench_rvv`，`build/asm/riscv/bench_apmf_rvv.asm` | 观察到 `vfcvt.rtz.x.f.v`、`vcompress.vm`、`vcpop.m`、`vluxseg3ei32.v`、`vle32.v`、`vse32.v` | 指令存在并归属到诊断二进制，尚非 production hot symbol |

## 板卡性能证据

本阶段 board（板卡）预算为 5 run；输入为 `--size 262144 --grid-rows 160 --grid-cols 160 --half 4 --iterations 8 --warmup 2`，目标硬件 label 为 `Milkv-Jupiter`。summary-only 证据路径：

- `log/board/repeated/summary.md`
- `log/board/repeated/evidence_manifest.json`
- `log/board/repeated/evidence_doctor.md`

| case | run count | median speedup | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `apmf grid z-min dense component` | 5 | 2.27x | 2.22x | 2.28x | positive |
| `apmf grid z-min non-dense component` | 5 | 2.45x | 2.44x | 2.46x | positive |
| `apmf tail compress component` | 5 | 2.32x | 2.29x | 2.33x | positive |
| `apmf window open component` | 5 | 1.02x | 1.01x | 1.02x | neutral |

## Evidence Doctor 结果

`make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_board_evidence_doctor` 生成的 report 为 `log/board/repeated/evidence_doctor.md`。

| severity | count | finding | 处理 |
| --- | ---: | --- | --- |
| Error | 0 | 无 | 不阻塞本阶段 diagnostic 结论 |
| Warning | 1 | `group_outlier`：`apmf window open component` 相比同组 median 偏离明显 | window-open 不继承 z-min / tail-compress 收益，矩阵单独标为 neutral |
| Suggestion | 1 | `near_threshold_ba`：window-open median=1.02x 接近 1.0 阈值 | 后续 production-shaped diagnostic 必须按完整链路重测，不能把该组件写成稳定加速 |

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `diagnostic`，component-only ablation（组件消融） |
| A/B boundary | `test helper`；Std / RVV 分别由 `bench_apmf_std` 和 `bench_apmf_rvv` 执行 |
| 当前决策问题 | `RVV-vs-scalar` 与 `implementation-shape` |
| diagnostic 是否可外推到 production | 不可直接外推。production 入口还包含 `initCompute`、window size 生成、`getMinMax3D`、OpenMP 外层、`copyPointCloud`、多轮 `A.swap(Zf)` 和真实 `ground` 状态更新 |
| comparison-boundary / baseline mismatch 风险 | 有。component case 分段计时，baseline 和 candidate 不是 public overload，也未覆盖完整对象状态 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许有界探针的条件是先完成 production-shaped full diagnostic（生产形态完整诊断）并证明组合链路仍有稳定正向信号；window-open neutral 不能单独支持生产接入 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。当前只有 Std/RVV component 对比，不能 clean-adopt |

## 优化矩阵更新

| candidate family | decision | 理由 | unblocked next action |
| --- | --- | --- | --- |
| `z-min-index-staging` | attempted-positive | QEMU correctness 通过，反汇编命中 RVV 指令，5-run board median 2.27x / 2.45x | 合入 production-shaped full diagnostic，检查完整链路是否仍正向 |
| `window-open-row-reduction` | attempted-neutral | QEMU correctness 通过，但 5-run board median 1.02x，Evidence Doctor 报 `group_outlier` 和 `near_threshold_ba` | 在完整链路中保留为候选但单独解释，必要时尝试保持标量 window-open 的 hybrid 方案 |
| `tail-compress-indices` | attempted-positive | QEMU correctness 通过，反汇编命中 `vcompress` / gather 相关指令，5-run board median 2.32x | 合入 production-shaped full diagnostic，验证真实 ground 更新和保序输出 |

## 阶段反思

组件层证据说明 z-min 和 tail-compress 值得继续；window-open 目前更像收益中性的实现形态诊断。下一阶段不应直接改 production，而应构造 production-shaped full diagnostic：同一 helper 内完成初始 z-min、按多个 half-size 连续执行 open、`A.swap(Zf)`，并用 tail-compress 更新 `ground`。该阶段能回答组件正向是否在完整链路中被 window-open、copy / grid 状态和多轮循环稀释。

## Continue / Stop Decision

current_decision：`diagnostic / continue`。没有命中停止条件；板卡可用且当前 topic 仍有授权内的高优先级未阻塞动作。默认下一阶段为 `010-production-shaped-full-diagnostic`。

stop_condition_hit：none。

next_phase_default：`010-production-shaped-full-diagnostic`。
