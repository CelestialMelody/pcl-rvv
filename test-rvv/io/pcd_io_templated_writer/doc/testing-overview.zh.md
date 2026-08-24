# pcd_io_templated_writer 测试总览

## 本文职责

本文说明当前 topic 的 test（测试）、bench（性能测试）、QEMU smoke（QEMU 小型验证）、
board repeated（板卡重复采集）和 Evidence Doctor（证据体检）入口。每个 gtest 的输入与断言归
`correctness-tests.zh.md`；bench case（性能测试用例）和 output summary（输出摘要）的细节归
`benchmark-and-evidence.zh.md`。

## 运行入口分类

| 类别 | 当前入口 | 证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make run_test_compare` | Std/RVV build 的 helper-level correctness，包含 fallback path hit。 | 不证明真实 public entry（公开入口）命中 RVV。 |
| correctness alias（正确性细分入口） | not_applicable with evidence；当前只有一个 gtest binary | 当前测试数量较少，不需要拆 target。 | 不能当作 production direct。 |
| diagnostic bench（诊断 bench） | `bench_pcdtw --case-filter all`、`--case-filter compressed_*`、`--case-filter binary_*` | pack-only、pack+LZF 或 binary packed output 的 Std/RVV A/B。 | 不包含真实 file write、mmap 和 error path。 |
| QEMU smoke | `make run_qemu_smoke` | 构建、correctness、RVV bench log-shape（日志形状）。 | QEMU timing 不是性能证据。 |
| board smoke | `make run_board_pcdtw_smoke` | 单次板卡可运行和日志形状。 | 不替代 repeated performance。 |
| board repeated | `make run_board_pcdtw_repeated`、`make run_board_pcdtw_shaped_repeated`、`make run_board_pcdtw_binary_repeated`、`make run_board_pcdtw_production_compressed_repeated`、`make run_board_pcdtw_binary_tuple_segment_repeated`、`make run_board_pcdtw_production_binary_tuple_repeated` | 目标硬件 repeated summary、manifest、Doctor 和 registry。 | production compressed target 和 production binary tuple target 是当前 public overload 证据；Phase 080 binary production repeated 只作为历史负向证据。 |
| doctor / registry | `run_board_pcdtw_evidence_doctor`、`record_board_pcdtw_evidence_state`、shaped / binary 对应 target | 检查 summary / manifest 异常并登记 freshness。 | Doctor clean 不自动证明 production adopted。 |
| production direct | `PCDTemplatedWriterProductionDirect.*` | 覆盖真实 `PCDWriter::writeBinaryCompressed<PointT>` 和 `PCDWriter::writeBinary<PointT>` 写 PCD 文件、与标量 pack 对拍，并检查 RVV / fallback hook。 | binary tuple path 只覆盖 4 个连续 4-byte effective fields；indices overload 保持标量。 |

## 推荐测试流程

1. `make -C test-rvv/io/pcd_io_templated_writer run_test_compare`
2. `make -C test-rvv/io/pcd_io_templated_writer run_qemu_smoke BENCH_ARGS="--case-filter compressed_* --iterations 2 --warmup-iterations 1"`
3. `make -C test-rvv/io/pcd_io_templated_writer dump_bench_rvv`
4. 板卡可用时运行 `run_board_pcdtw_repeated`、`run_board_pcdtw_shaped_repeated`、`run_board_pcdtw_binary_repeated`
   或对应 production-public repeated target，例如 `run_board_pcdtw_production_compressed_repeated`、
   `run_board_pcdtw_production_binary_tuple_repeated`。
5. 通过 Make target 自动生成 summary、manifest、Evidence Doctor 和 `log/evidence_registry.json`。

## 输入数据总览

当前输入是 synthetic PointXYZRGB-like byte layout（合成 PointXYZRGB-like 字节布局）。
row source policy（行来源策略）是 contiguous PointCloud rows（连续点云行），不含 indices、
correspondences（对应关系索引）、mask（掩码）或 weight（权重）。

已覆盖布局：

- `point_step=16`：四个 4 字节字段，无尾部填充。
- `point_step=20`：四个 4 字节字段，尾部 4 字节 padding（填充）。
- `point_count=512`：小规模 smoke。

未覆盖布局：

- 非 4 字节字段。
- 多元素字段。
- 任意用户自定义 registered point type（注册点类型）；Phase 060 覆盖本 topic 定义的 compressed compact /
  tail-padding production-public 点型和 mixed-size fallback 点型。Phase 100 binary tuple path 覆盖本 topic
  定义的 compact / tail-padding production-public 点型、mixed-size fallback 和 indices unchanged。

## 覆盖矩阵

| path | correctness | bench | QEMU | board | production direct |
| --- | --- | --- | --- | --- | --- |
| pack-only component ablation | yes | `pointxyzrgb_*` | yes | repeated positive | no |
| pack+LZF production-shaped diagnostic | yes | `compressed_*` | yes | repeated positive | no |
| binary writer component ablation | yes | `binary_*` | yes | repeated positive；small outlier-scoped | no |
| production public entry | yes | `production_compressed_*` | QEMU smoke only | repeated positive | compressed writer adopted |
| binary field-outer production public entry | historical yes / removed | historical `production_binary_*` | historical QEMU smoke only | repeated negative；Doctor Errors=3 | rollback/no-production；historical target removed |
| binary tuple production public entry | yes | `production_binary_tuple_*` | QEMU smoke only | repeated positive；Doctor Errors=0 Warnings=1 | adopted production behavior |
| non-4-byte fallback | compressed / binary public fallback yes | not_applicable | QEMU correctness only | not_applicable | fallback covered；no performance claim |

## 可提交证据和默认排除项

可提交候选是被 README、evaluation 或 phase result 引用的 summary / manifest / doctor / registry。
raw logs（原始日志）、build output（二进制和反汇编 full dump）、`__pycache__` 和本机远端配置默认排除。
