# Phase 000 结果：current state and component ablation

## 执行范围

本阶段按 `plan.zh.md` 验证 `PCDWriter::writeBinaryCompressed<PointT>` 压缩前置布局转换的
test-only component ablation（组件消融）。production（生产源码）未修改；证据只覆盖 synthetic
PointXYZRGB-like 4 字节字段布局，不覆盖泛型模板入口或 LZF / 文件写入边界。

## 计划动作回填

| action | status | evidence | 结论 |
| --- | --- | --- | --- |
| A1 failing correctness test | done | `make run_test_compare` 初次 RVV build 失败于 `PackRvv` 路径断言 | 测试能捕捉“候选未命中 RVV”的缺口。 |
| A2 test-only scalar / RVV candidate | done | `include/impl/pcdtw_support.hpp`，`make run_test_compare` | Std/RVV correctness 均通过；混合字段 fallback 保持标量。 |
| A3 component bench | done | `src/bench_pcdtw.cpp`，`make run_qemu_smoke` | QEMU 只确认 bench binary 和日志形状，不作为性能结论。 |
| A4 反汇编归属 | done | `build/asm/riscv/bench_pcdtw_rvv.asm` | 可见 `vlse32.v` 与 `vse32.v`，归属到 bench 内联候选范围。 |
| A5 板卡 repeated bench | done | `log/board/component_ablation_repeat_5/summary.md` | 5 次 repeated board 结果稳定 positive。 |
| A6 Evidence Doctor 和 registry | done | `log/board/evidence_doctor.md`，`log/evidence_registry.json` | Evidence Doctor：Errors=0，Warnings=0，Suggestions=0；registry fresh。 |
| A7 文档回填 | done | 本文、evaluation、roadmap、matrix | Phase 000 已回填；下一 phase 进入 production-shaped diagnostic。 |

## 证据摘要

| case | mean speedup | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `pointxyzrgb_4f_262k` | `1.4892x` | `1.4898x` | `1.4856x` | `1.4938x` | positive |
| `pointxyzrgb_4f_padding_262k` | `1.4466x` | `1.4445x` | `1.4417x` | `1.4555x` | positive |
| `pointxyzrgb_4f_small_512` | `1.5739x` | `1.5687x` | `1.5255x` | `1.6377x` | positive |

板卡 correctness（正确性）smoke：`make run_board_test fetch_board_logs` 通过，`log/board/run_test.log`
显示 3/3 tests passed。

## Diagnostic 到 production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | component ablation（组件消融） |
| A/B boundary | test helper / bench wrapper |
| 当前决策问题 | RVV-vs-scalar 是否值得进入 production-shaped diagnostic |
| diagnostic 是否可外推到 production | 不能直接外推；本阶段没有 LZF compression、header、mmap 或 file write。 |
| comparison-boundary / baseline mismatch 风险 | 存在；计时边界是 pack-only。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；当前结果为稳定 positive，但仍需 shaped diagnostic。 |
| clean adoption 是否需要 production boundary 内 RVV-vs-RVV detail A/B | 需要；当前阶段不能 clean adopt。 |

## Continue / Stop Decision

Phase 000 证据支持继续：layout conversion 组件在板卡上稳定 positive，Doctor clean，且 roadmap 中的
production-shaped compressed writer diagnostic 仍是当前 topic 授权范围内的未阻塞下一动作。

next_phase_default：`010-production-shaped-compressed-writer-diagnostic`。
