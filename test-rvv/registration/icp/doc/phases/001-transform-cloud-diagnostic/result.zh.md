# Phase 001 result：transformCloud 诊断候选

## 结论

本 phase 已完成 `IterativeClosestPoint::transformCloud` 的 test-only RVV diagnostic candidate（测试专用
RVV 诊断候选）。当前 EvidenceDecision 仍为 `diagnostic`：生产源码
`registration/include/pcl/registration/impl/icp.hpp` 未修改，没有 production direct（真实生产路径证据），
也没有目标硬件 repeated benchmark，因此不能升级为 production performance 结论。

## 动作矩阵

| action | 结果 |
| --- | --- |
| scaffold | done：建立 `test-rvv/registration/icp` topic 目录、Makefile、README、evaluation、roadmap 和 phase 文档。 |
| scalar reference | done：`transformCloudStd` 按运行期 offset、`memcpy`、XYZ finite gate、normal finite gate 和 in-place 语义复刻 production。 |
| RVV candidate | done：`transformCloudCandidate` 在 `__RVV10__`、`input.size() >= 32` 且 layout 为标准 `PointXYZ` / `PointNormal` 时进入 RVV，否则 fallback。 |
| correctness tests | done：Std/RVV 构建各 5 tests passed，覆盖 PointXYZ、PointNormal、normal 非有限值、in-place、小规模 fallback 和 offset fallback。 |
| bench smoke | done for QEMU：4 个 case 输出 Std/RVV 对比；QEMU timing 只作为日志形状和路径证据。 |
| asm attribution | partial：bench RVV binary 中观察到 `vsetvli e32,m2`、`vfmacc.vf`、`vmand.mm`、masked `vsse32.v`；production 符号归属未做。 |
| Evidence Doctor | done：Errors=0，Warnings=4；warning 均为单次 QEMU smoke 的 `low_run_count`。 |
| evidence registry | done：当前 QEMU 证据文件已记录，并由 README、evaluation 和长期 topic doc 显式引用。 |
| board performance | missing：没有目标硬件 repeated summary。 |
| production decision | no production change：保持诊断状态，等待板卡 repeated bench 再决定是否进入 PI1。 |

## 证据摘要

Correctness（正确性）：

| 命令 | 摘要 | 证据路径 |
| --- | --- | --- |
| `make -C test-rvv/registration/icp run_test_std` | 5 tests passed。 | `test-rvv/registration/icp/log/qemu/run_test_std.log` |
| `make -C test-rvv/registration/icp run_test_rvv` | 5 tests passed。 | `test-rvv/registration/icp/log/qemu/run_test_rvv.log` |

QEMU bench smoke（不是性能结论）：

| case | Std avg | RVV avg | smoke ratio |
| --- | --- | --- | --- |
| `icp transform-cloud xyz 64K` | 16.0625 ms | 5.8179 ms | 2.76x |
| `icp transform-cloud xyz 256K` | 64.6466 ms | 23.0901 ms | 2.80x |
| `icp transform-cloud xyz-normal 64K` | 31.4550 ms | 12.7155 ms | 2.47x |
| `icp transform-cloud xyz-normal 256K` | 126.3915 ms | 47.5241 ms | 2.66x |

Bench 原始和汇总路径：

| 路径 | 角色 |
| --- | --- |
| `test-rvv/registration/icp/log/qemu/run_bench_std.log` | 标量 bench 原始 QEMU 日志。 |
| `test-rvv/registration/icp/log/qemu/run_bench_rvv.log` | RVV bench 原始 QEMU 日志。 |
| `test-rvv/registration/icp/log/qemu/analyze_bench_compare.log` | Std/RVV smoke 汇总。 |

Evidence Doctor / registry：

| 路径 | 摘要 |
| --- | --- |
| `test-rvv/registration/icp/log/qemu/evidence_manifest.json` | 4 个 diagnostic comparison，`run_count=1`。 |
| `test-rvv/registration/icp/log/qemu/evidence_doctor.md` | Errors=0，Warnings=4，均为 `low_run_count`。 |
| `test-rvv/registration/icp/log/evidence_registry.json` | 当前 QEMU 证据登记。 |

## 反汇编观察

`make -C test-rvv/registration/icp dump_bench_rvv` 生成 bench RVV binary 反汇编。当前观察到的候选相关指令簇包括
`vsetvli ... e32,m2`、`vfmacc.vf`、`vmand.mm` 和 masked `vsse32.v ... v0.t`，主要出现在
`test-rvv/registration/icp/build/asm/riscv/bench_icp_rvv.full.asm` 的 PointXYZ / PointNormal candidate 区域。
这只能说明 test-only bench candidate 的 RVV lowering（向量化落地）存在，不能说明 production `icp.hpp` 已接入。

## 未闭合项

- 目标硬件 repeated benchmark 缺失；正式 performance evidence 至少需要 5-run，接近阈值时扩大到 20-run / 50-run。
- production direct 缺失；`registration/include/pcl/registration/impl/icp.hpp` 没有修改。
- production fallback tests、`Scalar=double` gate、泛型点类型 layout gate 和 production 符号反汇编归属尚未进入本 phase。

## 下一步默认动作

若板卡可用，先运行同一边界的 repeated board bench，并保留 correctness、manifest、doctor 和 registry 链路。
若 repeated board 对 `PointXYZ` 或 `PointNormal` 主分支稳定正向，再开启 PI1 production integration plan。
