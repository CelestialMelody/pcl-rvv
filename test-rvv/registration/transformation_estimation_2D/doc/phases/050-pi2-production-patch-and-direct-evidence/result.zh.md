# Phase 050 Result: pi2-production-patch-and-direct-evidence

## 当前结论

本阶段完成了窄范围 production integration loop（生产接入闭环）：

- 入口：ordered-cloud-pair public overload。
- 点型 / Scalar：exact `PointXYZ -> PointXYZ`、`Scalar=float`。
- gate：两侧 size 相等、dense、finite、点数不少于 16。
- fallback：其它点型、`Scalar=double`、indices、correspondences、小规模、非 dense 或非有限输入继续走原标量 iterator 路径。

最新真实板卡证据支持保留当前 production patch，当前状态为：

```text
production-candidate-supported / user-review-pending
```

这不是对其它 row source 或泛型点型的批准，也不是自动提交授权。

## 执行动作回填

| id | 状态 | 产物 / 命令 | 结论 |
| --- | --- | --- | --- |
| PI2-A1 | done / retained | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` | 当前 patch 保留；RVV gate 失败回退标量。 |
| PI3-A1 | done | `record_qemu_correctness_state` | 当前 Std / RVV 各 16/16 通过。 |
| PI3-A2 | done | `src/test_te2d.cpp` | 小规模、非 dense、非有限、`Scalar=double`、其它点型和三类非 ordered row source 的标量边界仍受保护。 |
| PI4-A1 | done | `record_qemu_production_public_state` | QEMU Doctor 0/0/0；只证明路径和日志形状。 |
| PI4-A2 | done | `generate_production_public_asm_attribution_summary` | public overload / `runPublicCase` 内联边界可归属关键 RVV 指令。 |
| PI4-A3 | done / refreshed | `run_board_bench_ordered_cloud_pair_public_repeated` | 重新编译当前 patch、部署到 `Milkv-Jupiter`、SSH 远端执行 5 runs。 |
| PI5-A1 | done | 本 result、matrix、roadmap、evaluation、README、Handoff | 最新 board evidence 支持窄范围 candidate；等待用户 review/adoption。 |

## Production-public Board 证据

本结果来自真实板卡，不是 QEMU：

| case | median B/A | min | max | B/A<1 | bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `public 2D ordered-cloud-pair 4K` | 4.222x | 4.110x | 4.231x | 0/5 | `positive` |
| `public 2D ordered-cloud-pair 64K` | 5.310x | 5.199x | 5.362x | 0/5 | `positive` |
| `public 2D ordered-cloud-pair 256K` | 4.947x | 4.864x | 5.068x | 0/5 | `positive` |

证据路径：

```text
test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/evidence_manifest.json
test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/evidence_doctor.md
```

manifest 明确记录 `device=Milkv-Jupiter`、5 runs、每 run 20 iterations、5 warmup，以及 Std/RVV binary hash。Make target 的流程是本地交叉编译、rsync 到板卡、SSH 运行 `run_bench_compare`、rsync 日志回本机、本地生成 summary/manifest/Doctor。

## QEMU、ASM 和 Evidence Doctor

- correctness：Std / RVV 各 16/16。
- QEMU production-public Doctor：Errors=0、Warnings=0、Suggestions=0。
- asm：production public boundary 能看到 `vlsseg3e32.v`、`vfmacc`、`vfredosum`、`vfsub`、`vfadd`。
- board production-public Doctor：Errors=0、Warnings=0、Suggestions=0。

QEMU 和 asm 只能证明正确性、路径命中和指令归属；真实性能结论只来自上面的 `Milkv-Jupiter` board repeated。

## Evidence Registry

最新 board run 由 `record_board_ordered_cloud_pair_public_state` 登记为：

```text
backend=board
evidence_role=post_production_performance
run_label=board-te2d-production-public-repeated-pi4
```

row-source board run 另行登记为 `pre_production_diagnostic`，不可混入 production evidence。

## Optimization Matrix 更新

| candidate family | decision | next action |
| --- | --- | --- |
| exact ordered-cloud-pair production dispatch | production-candidate-supported / user-review-pending | 审阅 diff、fallback 和文档；用户确认后再决定提交或继续 |
| source-indexed / dual-indexed / correspondence | diagnostic-only | 读取 Phase 030 board Doctor，设计新的 gather/staging candidate |
| generic point types / `Scalar=double` | deferred | 独立 traits、数值预算和板卡证据 |

## Continue / Stop Decision

`continue_stop_decision`：Phase 050 的 production direct evidence 已闭合；当前没有理由自动回滚 patch，也没有提交授权。

`stop_condition_hit`：下一步涉及用户对 production diff 的审阅/采用判断，或扩大到新的 row source / 点型 / Scalar，需要新的 PI1。

`next_phase_default`：`060-production-candidate-review-and-row-source-boundaries`。
