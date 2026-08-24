# Integral Image Normal 测试总览

## 本文职责

本文记录当前 topic 的 test target（测试入口）、bench target（性能测试入口）、QEMU（仿真器）、board（板卡）和 Evidence Doctor（证据体检）边界。每个 TEST 的细节见 `correctness-tests.zh.md`；性能数值和证据提交边界见 `benchmark-and-evidence.zh.md`。

## 运行入口分类

| 类别 | 入口 | 当前状态 | 证明范围 | 不能证明什么 |
| --- | --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make run_test_compare` | adopted | Std / RVV 两个构建的 test-only map-prep / diff-buffer helper 与标量参考一致；public `compute()` distance map 与 reference 一致 | 其它点类型、indices output、其它 normal method 的性能 |
| correctness aliases（正确性细分入口） | `make run_test_std`、`make run_test_rvv` | adopted | 单独运行 Std 或 RVV 构建，便于定位失败侧 | 不提供跨构建性能结论 |
| diagnostic bench（诊断性能测试） | `make run_bench_std`、`make run_bench_rvv` | adopted | 生成 map-prep、diff-buffer 和 production-shaped profile 的日志形状和 checksum | QEMU timing 不作为性能证据 |
| QEMU smoke（QEMU 小型验证） | `run_test_compare`，必要时窄范围 `run_bench_rvv` | adopted | 构建、运行、日志字段和路径命中 | 不证明目标硬件性能 |
| board smoke（板卡小型验证） | `make board_smoke BENCH_ARGS='50'` | adopted | 板卡 unit test 可运行，单次 bench 输出可解析 | 单次结果不替代 repeated board |
| board repeated（板卡重复采集） | `run_board_integral_image_normal_repeated` 生成 5 个 run dir | adopted | 5-run decision bucket（决策桶）、checksum 稳定性和 Phase 050 production direct public `compute()` 性能 | PI5 采纳确认 |
| doctor / registry（证据体检 / 登记） | `make run_board_evidence_doctor`、`make evidence_status` | adopted | 生成 manifest、doctor，并检查登记文件 freshness（新鲜度） | registry 不替代 reviewer 审查 |
| historical probe（历史探针） | 无 | not_applicable with evidence | 当前没有回滚探针或废弃 production probe | 不适用 |

## 输入数据和覆盖矩阵

| 维度 | 当前覆盖 |
| --- | --- |
| row source（行来源） | organized image 顺序扫描，`width * height` 连续数据。 |
| point type / layout（点类型 / 布局） | map-prep 覆盖合成连续 `float z` buffer；diff-buffer 和 production-shaped profile 覆盖测试专用 `XYZPadPoint` 4-float stride。 |
| size（规模） | correctness 覆盖小图、tail 和无有效内圈；bench 覆盖 `320x240` 和 `641x481_tail`。 |
| invalid data（非法值） | correctness 覆盖 NaN 和 Inf。 |
| production direct（真实生产路径） | Phase 050 覆盖 `PointXYZ -> Normal`、organized full image、`AVERAGE_DEPTH_CHANGE` 的 public `compute()`。 |
| fallback（回退路径） | 非 RVV 构建和 `RVVXYZAoSFloatLayout<PointInT>` 不成立时走 Std helper；其它点型未做性能外推。 |

## 推荐验证顺序

```bash
make -C test-rvv/features/integral_image_normal run_test_compare
make -C test-rvv/features/integral_image_normal dump_test_rvv
make -C test-rvv/features/integral_image_normal run_board_integral_image_normal_repeated
make -C test-rvv/features/integral_image_normal run_board_evidence_doctor
make -C test-rvv/features/integral_image_normal record_board_evidence_state
make -C test-rvv/features/integral_image_normal evidence_status
```

`run_bench_compare` 在 QEMU 默认有 guard（保护门），只允许明确标记为 log-shape smoke 的窄范围调试；性能结论必须使用板卡 repeated summary。

## Target 粒度审计结论

当前 target 粒度足够支撑 Phase 050 PI5：correctness、QEMU smoke、asm、board repeated、manifest、doctor 和 registry check 都有入口。仍未单独拆出更多 production fallback aliases；当前 fallback 主要由模板 gate 和 Std/RVV 双构建 correctness 覆盖。若用户采纳当前 patch，长期 `doc-rvv` 应记录这些未覆盖范围。
