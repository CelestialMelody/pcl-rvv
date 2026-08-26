# ROPS RVV topic

本目录保存 `features/include/pcl/features/impl/rops_estimation.hpp` 的 RVV（RISC-V Vector，可伸缩向量）函数级评估、测试资产和证据摘要。

## 当前结论

ROPS 当前已采纳一份有界 production patch（生产补丁）：`ROPSEstimation::rotateCloud()` 和
`ROPSEstimation::getDistributionMatrix()` 在 `__RVV10__`、dense cloud、足够规模、有效 bins / projection /
AABB extent，并且 `PointInT` 满足 `RVVXYZAoSFloatLayout` 的 xyz float AoS（结构数组）layout 时进入 RVV helper；其它情况回退原标量路径。

production-detail（生产私有 helper 直连）板卡数据为正向，用户已确认“板卡上的测试结果如果显示有收益即可采纳”。
正式长期文档位于 `doc-rvv/features/rops_estimation-RVV.zh.md`，其中的性能数据使用接入后的板卡 summary。

| scope | summary | decision |
| --- | --- | --- |
| `PointXYZ` production detail | `log/board/phase040_production_direct_repeated/summary.md`：median `1.700x`，min `1.690x`，0/5 退化 | adopted production behavior |
| `PointXYZI` traits expansion | `log/board/phase050_pointxyzi_production_detail_repeated/summary.md`：median `1.520x`，min `1.470x`，0/5 退化 | adopted representative typed scope |
| `PointNormal` traits expansion | `log/board/phase050_pointnormal_production_detail_repeated/summary.md`：median `1.590x`，min `1.570x`，0/5 退化 | adopted representative typed scope |

这些证据不覆盖完整 public `computeFeature()`、真实 mesh local surface、LRF、central moments、descriptor normalization、
`Scalar=double` 或所有自定义点型逐个性能。

## 阅读路径

| 读者问题 | 入口 |
| --- | --- |
| 当前 production patch 怎么工作 | `doc/rops_estimation-evaluation.zh.md`、`../../../doc-rvv/features/rops_estimation-RVV.zh.md` |
| 为什么从诊断进入生产接入 | `doc/rops_estimation-evaluation.zh.md`、`doc/phases/040-production-integration-plan/result.zh.md` |
| 每个 phase 的计划和结果 | `doc/phases/README.zh.md` |
| 哪些候选已采用、拒绝或暂缓 | `doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md` |
| 板卡 repeated 数据和 Evidence Doctor | `log/board/phase040_production_direct_repeated/summary.md`、`log/board/phase050_pointxyzi_production_detail_repeated/summary.md`、`log/board/phase050_pointnormal_production_detail_repeated/summary.md` |

## 常用命令

```bash
make -C test-rvv/features/rops_estimation run_test_compare
make -C test-rvv/features/rops_estimation dump_bench_rvv
make -C test-rvv/features/rops_estimation doctor_phase040_board_summary
make -C test-rvv/features/rops_estimation doctor_phase050_pointxyzi_board_summary
make -C test-rvv/features/rops_estimation doctor_phase050_pointnormal_board_summary
```

QEMU（仿真器）只用于 correctness（正确性）、路径命中和日志形状；性能结论只使用板卡 summary。raw `log/board/*/run-*`
日志默认不提交。summary / manifest / Evidence Doctor 因被 topic-local 文档和长期 `doc-rvv` 引用，可作为提交候选。
