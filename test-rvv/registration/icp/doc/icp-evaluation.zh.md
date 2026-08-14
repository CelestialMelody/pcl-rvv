# ICP `transformCloud` 函数级评估

## 当前 EvidenceDecision

当前为 `production_direct_positive`：RVV path 已接入
`registration/include/pcl/registration/impl/icp.hpp` 的
`IterativeClosestPoint<PointSource, PointTarget, Scalar>::transformCloud`。接入边界是
`Scalar=float`、`input.size() >= 32`、PCL RVV AoS layout traits 通过，且运行期 `x/y/z`
与可选 `normal_x/y/z` field offset 和 traits offset 一致；否则调用
`pcl::registration::detail::transformCloudStandard` 标量 fallback。

板卡证据来自 Milkv-Jupiter 5-run repeated benchmark：
`test-rvv/registration/icp/log/board/transform_cloud_repeated/summary.md`。median speedup 为
`PointXYZ 64K=5.68x`、`PointXYZ 256K=5.30x`、`PointNormal 64K=3.76x`、
`PointNormal 256K=3.93x`。Evidence Doctor：
`test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_doctor.md`，
Errors=0，Warnings=2，Suggestions=0；warnings 均来自 `PointXYZ 64K` 的 long-tail / variance
和 group outlier，全部 5-run 仍大于 5.2x，decision bucket 保持 positive。

## 标量语义

`computeTransformation` 在初始 guess、每轮 ICP 迭代和最终输出阶段调用 `transformCloud`。目标函数按
`setInputSource()` 解析出的运行期 field offset，用 `memcpy` 从 `PointSource` 读取 XYZ。XYZ 任一分量
不是有限值时跳过整个点；否则执行 3x4 rigid transform（刚体变换）并写回 XYZ。若 source 有 normals，
函数再读取 `normal_x/y/z`，normal 任一分量非有限时只跳过 normal 写回，已经写出的 XYZ 保持变换后状态。
函数声明允许 input 和 output 是同一对象。

RVV 实现保持这些语义：不负责 `output = input`，只按原函数写回应该写回的字段。因此 caller 预置 output、
resize 后 output 或 in-place output 的行为仍由原调用边界决定。

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 证据角色 |
| --- | --- | --- | --- | --- |
| `IterativeClosestPoint::computeTransformation` | production public entry | ICP 主循环中决定何时调用 transformCloud。 | `Registration::align` | 入口调用链审计。 |
| `IterativeClosestPoint::transformCloud` | production dispatch entry | cast matrix，RVV 短路，失败后调用 Std fallback。 | ICP 主循环和最终输出变换 | RVV 接入点和 fallback 边界。 |
| `pcl::registration::detail::transformCloudStandard` | production Std fallback | 保留上游标量语义，按运行期 offset 变换 XYZ 和可选 normals。 | `transformCloud` | 非 RVV 构建、unsupported Scalar / layout / size fallback。 |
| `pcl::registration::detail::tryTransformCloudRVV` | production dispatch helper | 检查 size、`Scalar=float` 外层 gate、traits layout 和运行期 offset。 | `transformCloud` | fallback / gate 证据。 |
| `transformCloudXYZRVV` | production RVV helper | `PointT` AoS XYZ full-cloud masked transform。 | `tryTransformCloudRVV` | `PointXYZ` / generic XYZ path。 |
| `transformCloudXYZNormalRVV` | production RVV helper | XYZ 和 normal 两套 finite mask。 | `tryTransformCloudRVV` | `PointNormal` / generic normal path。 |
| `support::transformCloudStd` | test reference | 复刻 production 标量语义。 | gtest | correctness reference。 |
| `src/test_icp.cpp` | correctness tests | 覆盖 diagnostic helper 与 production direct。 | `run_test_compare` / board test | QEMU 和板卡正确性。 |
| `src/bench_icp.cpp` / `include/bench_icp.h` | production-direct bench wrapper | 通过 exposed ICP 调用 production `transformCloud`。 | board repeated collector | 性能证据。 |

## 测试与证据

| 证据类型 | 当前入口 | 当前结果 | 能证明 | 不能证明 |
| --- | --- | --- | --- | --- |
| QEMU correctness | `make -C test-rvv/registration/icp run_test_compare record_qemu_correctness_state` | Std/RVV 构建各 12 tests passed。 | 标量 / RVV production direct 对拍、finite gate、fallback gate、in-place、泛型 layout gate。 | 性能结论。 |
| board correctness | `make -C test-rvv/registration/icp run_board_test fetch_board_logs` | 板卡 RVV 构建 12 tests passed。 | 目标硬件上 production direct correctness 可运行。 | 稳定性能分布。 |
| board performance | `make -C test-rvv/registration/icp collect_board_transform_cloud_repeated run_board_evidence_doctor record_board_evidence_state` | 5-run repeated，median 3.76x 至 5.68x。 | 目标硬件 production `transformCloud` full-cloud 路径稳定正向。 | ICP 端到端整体加速比、nearest-neighbor 或 SVD 成本。 |
| disassembly | `make -C test-rvv/registration/icp dump_bench_rvv` | production `transformCloud` 符号内有 RVV 指令簇，Std fallback 有独立 `transformCloudStandard` 符号。 | bench 确实调用 production RVV hot path，fallback 边界可归因。 | 每条指令的周期归因。 |
| QEMU bench smoke | guarded historical only | 历史 raw logs 仍在工作区。 | 历史日志形状。 | 性能排序、采纳审计或 EvidenceDecision。 |

## 当前证据路径

| 路径 | 摘要 |
| --- | --- |
| `test-rvv/registration/icp/log/qemu/run_test_std.log` | QEMU 标量构建 gtest：12 tests passed。 |
| `test-rvv/registration/icp/log/qemu/run_test_rvv.log` | QEMU RVV 构建 gtest：12 tests passed。 |
| `test-rvv/registration/icp/log/board/run_test.log` | 板卡 RVV 构建 gtest：12 tests passed。 |
| `test-rvv/registration/icp/log/board/transform_cloud_repeated/summary.md` | Milkv-Jupiter 5-run production direct repeated summary。 |
| `test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_manifest.json` | `evidence_role=production_direct`，包含 production boundary 和 asm boundary。 |
| `test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_doctor.md` | Errors=0，Warnings=2，Suggestions=0。 |
| `test-rvv/registration/icp/doc/asm-attribution.zh.md` | production symbol 反汇编归因。 |
| `test-rvv/registration/icp/log/evidence_registry.json` | 当前证据登记文件。 |

历史 QEMU bench 文件 `test-rvv/registration/icp/log/qemu/run_bench_std.log`、
`test-rvv/registration/icp/log/qemu/run_bench_rvv.log`、
`test-rvv/registration/icp/log/qemu/analyze_bench_compare.log`、
`test-rvv/registration/icp/log/qemu/evidence_manifest.json` 和
`test-rvv/registration/icp/log/qemu/evidence_doctor.md` 只保留为 `qemu_smoke_only` 历史证据。
公共 Makefile 已默认禁止 QEMU `run_bench_compare`，除非显式 `ALLOW_QEMU_BENCH_COMPARE=1`。

## 风险与边界

- `PointXYZ 64K` repeated speedup 的 min/median/max 为 5.29x/5.68x/6.50x，Doctor 报
  long-tail 和 group outlier warning；因所有 run 均明显正向，当前结论不降级，但文档保留该波动。
- `PointXYZI` 和 `PointXYZINormal` 有 production direct correctness test；性能 repeated 代表点型仍是
  `PointXYZ` 和 `PointNormal`。
- `IterativeClosestPointWithNormals` 不走本函数，另属 `pcl::transformPointCloudWithNormals` 路径。
- 当前 bench 边界是 `transformCloud` full-cloud microbench，不声称 ICP 端到端整体加速同等幅度。
