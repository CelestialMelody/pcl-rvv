# surface/bilateral_upsampling RVV 主题导航

本目录是 `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` 的 RVV 生产探针主题。当前已经完成 production public（公开生产入口）探针、steady-state public shell 消融、helper-only 消融、mask/chunk 组件消融、color-gather 生产探针，以及 cross RGB/RGBA production probe；RGB/RGBA exact-family 板卡采用证据已转正，并已被用户确认保留 / 允许有收益先接入，进入 adopted 语义。

## 先读哪份

1. `doc/bilateral_upsampling-evaluation.zh.md`：函数级评估、production closeout 和最终 EvidenceDecision。
2. `doc/testing-overview.zh.md`：测试入口、target 粒度和 QEMU / board 证据边界。
3. `doc/correctness-tests.zh.md`：每个 gtest 的输入、路径、断言和不能证明的范围。
4. `doc/benchmark-and-evidence.zh.md`：bench label、Std/RVV 结果、manifest、Evidence Doctor 和提交边界。
5. `doc/optimization-evidence.zh.md`：各优化方式为什么 adopted、rejected、attempted 或 deferred。
6. `doc/test-support-code-map.zh.md`：测试支撑源码地图。
7. `doc/phases/073-cross-rgb-rgba-production-probe/result.zh.md`：当前 production-public 正向 truth、交叉 RGB/RGBA 扩展和 adopted 结论。
8. `doc/phases/070-production-detail-color-gather-production-probe/result.zh.md`：color-gather family 进入 production 的历史采纳证据。
9. `doc/phases/README.zh.md`：阶段索引和当前恢复动作。

## 文档角色

| role | 当前文件 | 说明 |
| --- | --- | --- |
| topic_navigation | `README.zh.md` | 当前结论、阅读路径、常用命令和证据白名单。 |
| testing_overview | `doc/testing-overview.zh.md` | target 粒度、运行入口分类和覆盖矩阵。 |
| correctness_tests | `doc/correctness-tests.zh.md` | 13 个 gtest 的输入、断言和证明边界。 |
| benchmark_and_evidence | `doc/benchmark-and-evidence.zh.md` | bench label、板卡结果、QEMU / board 边界和 Evidence Doctor。 |
| optimization_evidence | `doc/optimization-evidence.zh.md` | 候选族证据索引和 adopted / rejected 取舍。 |
| optimization_roadmap | `doc/optimization-roadmap.zh.md` | 后续点型、layout、`Scalar` 扩展和恢复条件。 |
| test_support_code_map | `doc/test-support-code-map.zh.md` | test support、bench harness、production helper 和输出的调用关系。 |
| phase suite | `doc/phases/` | 每阶段 plan/result、optimization matrix 和 Evidence Doctor。 |
| production_topic_doc | `doc-rvv/surface/bilateral_upsampling-RVV.zh.md` | adopted production behavior 的长期说明和板卡结果摘要。 |

## 常用命令

```bash
make -C test-rvv/surface/bilateral_upsampling run_test_compare
make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv
make -C test-rvv/surface/bilateral_upsampling board_smoke
```

`run_bench_compare` 默认由 shared Makefile 阻止在 QEMU 上运行；性能结论只采信板卡或目标硬件结果。当前 adopted 结论的性能证据来自 phase 073 `board_smoke` 和 `analyze_bench_compare.log`；phase 071 修复 finite mask、phase 072 收窄 same-type gate、phase 073 扩展 cross RGB/RGBA 后，当前二进制已刷新板卡，production public 为 `1.23x/1.09x/1.13x/1.10x/1.10x`，steady public 为 `1.21x/1.09x/1.16x/1.09x/1.10x`。

当前可提交证据默认引用 summary、manifest、doctor、QEMU correctness log 和 asm；本地 build 输出、raw board log、私有板卡地址和聊天记录默认排除。
