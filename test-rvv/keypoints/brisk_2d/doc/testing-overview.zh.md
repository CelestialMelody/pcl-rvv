# BRISK 2D 测试总览

## 入口分类

| target / 文件 | 类别 | 覆盖范围 | 不能证明什么 |
| --- | --- | --- | --- |
| `run_test_compare` | correctness aggregate（正确性汇总入口） | Std / RVV 两个构建分别运行 gtest，验证 `halfsample`、`twothirdsample`、`ScaleSpace` 派生层和 `BriskKeypoint2D::compute()` public smoke 可运行。 | 不证明板卡性能。 |
| `run_qemu_smoke` | QEMU smoke（QEMU 小型验证） | 运行 correctness，并检查 RVV bench binary 能输出日志。 | QEMU timing 不进入性能结论。 |
| `dump_bench_rvv` | asm attribution（反汇编归属） | 生成 RVV bench 的反汇编，确认 `vlse8`、`vzext`、`vdivu`、`vse8`、`vsse8` 等指令出现在 downsample helper 附近。 | 不证明性能收益。 |
| `board_smoke` | board smoke（板卡小型验证） | 在板卡跑 gtest 和单次 Std/RVV bench，确认二进制可运行、日志可解析。 | 单次 smoke 不是 repeated performance 证据。 |
| `collect_repeated_board_evidence` | board repeated（重复板卡采集） | 5-run 板卡采集，每轮包含 gtest、Std bench、RVV bench 和分析日志。 | 当前只覆盖 downsample helper / ScaleSpace 构造，不覆盖完整 `compute()` keypoint 输出。 |
| `collect_public_repeated_board_evidence` | board repeated public（公开入口重复板卡采集） | 5-run 板卡采集，新增 `brisk_public_compute_320x240`，用于判断 downsample helper 收益是否穿透完整公开入口。 | synthetic 输入不代表真实图像分布；不证明 AGAST/OAST 已加速。 |
| `run_repeated_board_evidence_doctor` | doctor alias（证据体检入口） | 生成 manifest 并运行 Evidence Doctor（证据体检）。 | Doctor 无 Error 不等于覆盖所有 BRISK 算法阶段。 |
| `record_repeated_board_evidence_state` / `repeated_evidence_status` | registry alias（证据登记入口） | 记录摘要证据 hash，并检查是否 fresh。 | raw logs 仍默认不提交。 |

## 覆盖矩阵

| 路径 | Std 构建 | RVV 构建 | QEMU | 反汇编 | 板卡 repeated | Evidence Doctor |
| --- | --- | --- | --- | --- | --- | --- |
| `Layer::halfsample()` | portable scalar / SSSE3 | RVV stride-load 2x2 平均 | pass | pass | weak-positive / near-threshold | 2 个 suggestion |
| `Layer::twothirdsample()` | portable scalar / SSSE3 | RVV stride-load 3x3 -> 2x2 加权 | pass | pass | weak-positive | no warning |
| `ScaleSpace::constructPyramid()` | 标量 helper 链 | RVV helper 链 | pass | 间接归属 | weak-positive | no warning |
| `BriskKeypoint2D::compute()` | public entry + 标量 detector | public entry + RVV downsample helper | pass | downsample 归属 | neutral / 1.017x | near-threshold suggestion |

## Target 粒度审计

当前 target 粒度足以支撑 adopted downsample helper 和 public-entry impact 审计：有 correctness aggregate、
QEMU smoke、board smoke、downsample repeated、public repeated、doctor / registry alias。`BriskKeypoint2D::compute()`
公开入口已经通过 synthetic `PointXYZRGBA` bench 验证为 near-neutral；该入口还包含 AGAST/OAST detector
（角点检测器）和 scale refinement（尺度细化），若继续追求完整 pipeline 收益，应另开 detector/profile phase。
