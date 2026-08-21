# image_depth testing overview

## 本文职责

本文说明 `test-rvv/io/image_depth` 的测试入口、target 粒度和证据边界。单个 TEST 的输入和断言见 `correctness-tests.zh.md`；bench label、summary 和 Evidence Doctor（证据体检）见 `benchmark-and-evidence.zh.md`。

## 运行入口分类

| 类别 | target / 入口 | 当前状态 | 证明什么 | 不能证明什么 |
| --- | --- | --- | --- | --- |
| correctness aggregate（正确性汇总） | `make run_test_compare` | adopted | Std/RVV 两个 build 都通过 scalar-vs-candidate 对拍和 production-public path hit。 | 不证明板卡性能。 |
| correctness RVV | `make run_test_rvv` | adopted | RVV build 下 candidate helper 和 production public RVV path 可运行。 | 不覆盖 Std build。 |
| correctness Std | `make run_test_std` | adopted | 非 RVV build 的 fallback 行为。 | 不覆盖 RVV path selection。 |
| diagnostic bench | `src/bench_image_depth.cpp` + `--case-filter` | adopted | 生成可解析 label、checksum 和 timing。 | QEMU timing 不作性能结论。 |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS="--case-filter all --iterations 2 --warmup-iterations 1"` | adopted | RVV bench 可启动，日志形状可解析。 | 不证明目标硬件性能。 |
| asm attribution（反汇编归属） | `make dump_bench_rvv` | adopted | production-linked bench binary 中可见 `vle16`、`vlse16`、`vfmul`、`vfrdiv`、masked `vse32`。 | 不证明其它 PCL 构建配置。 |
| board smoke（板卡小型验证） | `run_board_depth_conversion_smoke` | adopted as smoke | 板卡能运行 all case，checksum / 日志形状可解析。 | 不替代 repeated performance。 |
| board repeated（板卡重复采集） | `record_board_repeated_evidence_state`、`record_board_downsample_evidence_state` | adopted | 生成 5-run summary、manifest、doctor 并登记 registry。 | 仍是 production-shaped diagnostic。 |
| production-public board repeated | `record_board_production_evidence_state` | adopted evidence | 生成 `prod_*` 5-run summary、manifest、doctor 并登记 registry。 | 当前 adopted production behavior 的性能依据；QEMU 不作为性能结论。 |
| doctor / registry | `run_board_*_evidence_doctor`、`make evidence_status` | adopted | 暴露 Error / Warning / Suggestion，检查 evidence freshness。 | 不替代 production direct 证据。 |
| historical guarded probe（历史保护探针） | 无 | not_applicable with evidence | 当前 topic 尚无曾接入又回滚的 production probe。 | 不应虚构 guard target。 |

## 输入数据总览

测试和 bench 使用 synthetic 16-bit depth image（合成 16-bit 深度图），包含三类 invalid pixel（无效像素）：`0`、`no_sample_value=2047`、`shadow_value=65535`。输出类型是 `float`，覆盖 tight row（紧凑行）、padded row（带行尾填充）和整数倍 downsample。

## 覆盖矩阵

| 路径 | correctness | QEMU smoke | asm | board repeated | production direct |
| --- | --- | --- | --- | --- | --- |
| contiguous depth meters | yes | yes | `vle16` / `vfmul` | yes | yes，median 1.42x |
| contiguous disparity | yes | yes | `vle16` / `vfrdiv` | yes | yes，median 1.80x |
| depth downsample | yes | yes | fallback, no production RVV asm | yes | fallback coverage，context median 0.99x |
| disparity downsample | yes | yes | `vlse16` / `vfrdiv` | yes | yes，median 1.34x |
| `fillDepthImageRaw()` | source audit only | no | no | no | no |
| OpenNI legacy | source audit only | no | no | no | no |

## 推荐流程

1. 先运行 `make run_test_compare`，确认 helper-level correctness。
2. 若修改 bench wrapper，运行 QEMU smoke，只看可运行和日志格式。
3. 若修改 RVV helper，运行 `make dump_bench_rvv` 并检查关键 RVV 指令。
4. 需要性能结论时在板卡上运行 repeated target，再生成 summary / manifest / Evidence Doctor。
5. 最后运行 `make evidence_status`，确认 registry 与文档引用 fresh。

## 当前结论边界

当前证据支撑 adopted production behavior，范围是 depth contiguous、disparity contiguous 和 disparity downsample。真实 `DepthImage` public entry（公开入口）上的 correctness、production-linked asm 和 production-public board repeated 已完成；depth downsample 保持标量 fallback。disparity 两个 adopted case 保留 variance warning，可选扩大 runs 只用于增强稳定性信心。
