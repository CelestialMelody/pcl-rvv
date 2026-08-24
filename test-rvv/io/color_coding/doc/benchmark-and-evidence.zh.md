# color_coding Benchmark And Evidence

本文记录 bench CLI、case-filter、板卡 repeated evidence（重复采集证据）、Evidence Doctor 和 registry 边界。性能结论只来自板卡或目标硬件；QEMU timing（仿真器计时）只用于日志形状 smoke。

## Bench 输出格式

`src/bench_color_coding.cpp` 输出：

- dataset、iterations、warmup、batch repeats。
- 每个 label 的 `ms/iter`。
- `Total Time` 和 FNV-1a 风格 checksum。

Std build 运行 reference helper；RVV build 在 `__RVV10__` 下运行 candidate helper。checksum 用于确认同 case 的输出稳定，不替代 gtest correctness。

## CLI 参数

| 参数 | 默认值 | 作用 |
| --- | ---: | --- |
| `--case-filter <label|all|production>` | 全部 case | 只跑指定 bench label、全部 label或真实 production direct 的 `prod_*` labels。 |
| `--iterations <n>` | 20 | 计时迭代次数。 |
| `--warmup-iterations <n>` | 3 | 预热次数。 |
| `--batch-repeats <n>` | 64 | 每次迭代内重复调用次数，用于放大短 helper 的计时边界。 |

## Case-filter 字典

| label family | labels | point type / layout | 计时边界 | 证据角色 |
| --- | --- | --- | --- | --- |
| `encode_average_leaf*` | 31 / 257 / 1024 / 4096 | `ColorPoint` RGBA | indexed gather、RGB sum、vector reduction / scalar sum、average bytes | component_ablation |
| `encode_points_leaf*` | 31 / 257 / 1024 / 4096 | `ColorPoint` RGBA | average pass + scalar diff push + checksum | component_ablation |
| `decode_points_leaf*` | 31 / 257 / 1024 / 4096 | `ColorPoint` RGBA | reference encode setup、decode output store、checksum | component_ablation |
| `set_default_color_*` | 4096 / 16384 | `ColorPoint` RGBA | output allocation、default color helper、checksum | component_ablation |
| `ps_encode_average_leaf*` | 257 / 4096 | `pcl::PointXYZRGBA` RGBA field | production-shaped indexed gather and reduction | production_shaped_diagnostic |
| `ps_encode_points_leaf*` | 257 / 4096 | `pcl::PointXYZRGBA` RGBA field | production-shaped average pass + scalar diff push | production_shaped_diagnostic |
| `ps_decode_points_leaf*` | 257 / 4096 | `pcl::PointXYZRGBA` RGBA field | production-shaped decode output store | production_shaped_diagnostic |
| `ps_decode_points_staged_leaf*` | 257 / 4096 | `pcl::PointXYZRGBA` RGBA field + scratch `uint32_t` | production-shaped staged scratch store + scalar AoS writeback | production_shaped_implementation_shape_diagnostic |
| `ps_set_default_color_4096` | 4096 | `pcl::PointXYZRGBA` RGBA field | production-shaped default store | production_shaped_diagnostic |
| historical `prod_set_default_color_4096` | 4096 | `pcl::PointXYZRGBA` public `ColorCoding` | phase 070 real `setDefaultColor` public method | historical production_public |

`ps_` 表示 production-shaped diagnostic（生产形态诊断）：它使用真实 PCL 点类型布局，但仍不经过 production public entry。`prod_` 表示 historical production direct（历史真实生产路径证据）：phase 080 完整回滚后，当前 `--case-filter production` 不再选择任何 production case。

## 推荐 target

| 目标 | 命令 | 输出 | 何时使用 |
| --- | --- | --- | --- |
| QEMU smoke | `make -C test-rvv/io/color_coding run_qemu_smoke` | QEMU test logs 和 RVV bench log | 构建 / 输出形状检查 |
| 窄 bench smoke | `make -C test-rvv/io/color_coding run_bench_rvv BENCH_ARGS="--case-filter ps_encode_points_leaf257 --iterations 2 --warmup-iterations 1 --batch-repeats 2"` | `log/qemu/run_bench_rvv.log` | 快速确认新 label 可跑 |
| board smoke | `make -C test-rvv/io/color_coding run_board_color_coding_smoke` | 单次 board compare logs | 板卡路径可用性 |
| board repeated | `make -C test-rvv/io/color_coding run_board_color_coding_repeated` | summary / manifest / doctor / registry | 刷新性能证据 truth |
| production repeated | `make -C test-rvv/io/color_coding run_board_color_coding_production_repeated` | skip message after phase 080 | phase 080 后没有 current production RVV case；不再刷新 historical evidence |
| freshness | `make -C test-rvv/io/color_coding check_evidence_freshness` | registry check output | closeout / 文档同步前 |
| asm | `make -C test-rvv/io/color_coding dump_bench_rvv` | `build/asm/riscv/bench_color_coding_rvv.asm` | 确认 RVV 指令归属 |

## 当前 board evidence

当前 production registered run label 是 historical `board-color-coding-production-repeat-phase070`：

| artifact | path | role |
| --- | --- | --- |
| production summary | `log/board/production_repeat_5/summary.md` | default-only `prod_set_default_color_4096` 的 mean / median / min / max 和 Std/RVV mean ms。 |
| production manifest | `log/board/production_repeat_5/evidence_manifest.json` | Doctor 输入，`evidence_role=production_public`。 |
| production Evidence Doctor | `log/board/production_repeat_5/evidence_doctor.md` | Errors=1, Warnings=0, Suggestions=1。 |
| registry | `log/evidence_registry.json` | 登记 phase070 production evidence 和历史 phase050 diagnostic evidence。 |

Phase 080 完整回滚后没有 current production RVV case；`run_board_color_coding_production_repeated` 只输出 skip 信息，不覆盖 historical evidence。

历史 diagnostic run label 是 `board-color-coding-component-repeat-phase050`：

| artifact | path | role |
| --- | --- | --- |
| summary | `log/board/component_repeat_5/summary.md` | 23 个 case 的 mean / median / min / max 和 Std/RVV mean ms。 |
| manifest | `log/board/component_repeat_5/evidence_manifest.json` | Doctor 输入，包含 evidence role、wrapper、row source、checksum 和 B/A values。 |
| Evidence Doctor | `log/board/component_repeat_5/evidence_doctor.md` | Errors=2, Warnings=11, Suggestions=6。 |
| registry | `log/evidence_registry.json` | 登记当前 summary / manifest / doctor 的 sha256、run label 和 doc refs。 |

Run budget 为 5 次 repeated board compare，iterations=20，warmup=3。当前没有记录 taskset、governor、频率、温度和 binary hash；Doctor 把这些作为 metadata 风险处理。

## Phase 070 production direct result

| case | median | Doctor handling | production meaning |
| --- | ---: | --- | --- |
| `prod_set_default_color_4096` | 1.0035x | Error，2/5 below 1；near-threshold suggestion | default-only 不支持采纳；phase 080 已完整回滚。 |

## Evidence Doctor 处理

Phase 070 production Doctor：

| severity | case | phase 070 文档处理 |
| --- | --- | --- |
| Error | `prod_set_default_color_4096` | 2/5 below 1，不能写成稳定加速；不建议采纳。 |
| Suggestion | `prod_set_default_color_4096` | near-threshold，收益太弱，不建议为了当前实现承担 production patch。 |

| severity | case | phase 050 文档处理 |
| --- | --- | --- |
| Error | `ps_decode_points_staged_leaf257` 4/5 低于 1；`ps_decode_points_staged_leaf4096` 2/5 低于 1 | staged-store decode 被拒绝，不进入 production candidate，也不作为 PI2 扩围理由。 |
| Warning | `ps_decode_points_leaf4096` 1/5 低于 1，min 0.9116x；encode average group_outlier；default 16384 长尾 | 直接 decode 保持 weak / unstable diagnostic；encode/default 按 case 单独报告，只写 partial-production-candidate。 |
| Suggestion | `decode_points_*` 与 `ps_decode_points_*` 多个 near-threshold case | decode 不因 median 略大于 1 而进入生产候选；后续需要新实现族或 profile。 |

## ASM Attribution 口径

`dump_bench_rvv` 生成的 RVV bench 反汇编用于证明 diagnostic candidate binary 含预期 RVV 指令。phase 050 已确认 bench binary 仍包含 encode 的 `vluxei32` / `vredsum`、default/direct decode 的 `vsse32` 以及 staged scratch path 相关 `vse32` 类存储。该 asm 归属仍是 bench / diagnostic boundary，不是 production helper 符号归属。

## 提交边界

默认可提交：topic 源码 / 文档、summary evidence、manifest、Doctor、registry。默认排除：`build/`、`run*/` raw logs、板卡私有路径、本机配置和临时输出。用户若要求提交 raw logs，需先单独脱敏并拆分 evidence commit。
