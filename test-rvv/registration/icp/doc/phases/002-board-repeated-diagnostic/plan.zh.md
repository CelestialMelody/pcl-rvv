# Phase 002：transformCloud 板卡重复诊断

## 目标

在板卡上采集 `IterativeClosestPoint::transformCloud` test-only RVV candidate 的 repeated benchmark
（重复性能测试）证据，替代 phase 001 的 QEMU bench smoke。QEMU bench compare 不再作为默认动作；
本 phase 的性能判断只引用 `log/board/transform_cloud_repeated/summary.md`。

## 计划动作

| action | 完成判据 | 状态 |
| --- | --- | --- |
| board harness | `board.mk` 能部署 `bench_icp_std`、`bench_icp_rvv` 和 `test_icp_rvv` 到板卡。 | planned |
| board correctness | `make run_board_test fetch_board_logs` 通过，并保留 `log/board/run_test.log`。 | planned |
| repeated bench | `collect_board_transform_cloud_repeated` 完成 5-run board compare，summary 写入 `log/board/transform_cloud_repeated/summary.md`。 | planned |
| Evidence Doctor | board summary 生成 manifest / doctor，Errors 必须为 0 或降级结论。 | planned |
| evidence registry | board evidence 被 registry 记录，README / evaluation / topic doc 显式引用。 | planned |
| adoption audit | 若 repeated board 稳定正向，写明能否进入 production integration；否则停止在 diagnostic。 | planned |

## 证据边界

- 本 phase 仍是 test-only diagnostic，不是 production direct。
- repeated board speedup 只能说明当前 `bench_icp` wrapper 中的 full-cloud transformCloud candidate。
- 生产接入还需要 `registration/include/pcl/registration/impl/icp.hpp` 的 direct tests、fallback gate、
  `Scalar=double` 回退、泛型点类型 layout gate 和 production asm attribution。

## 复跑预算

默认先采集 5-run。若某个 case 接近 1.0、跨越方向或 Evidence Doctor 给出高风险 warning，最多自动追加一次
同边界 5-run 确认；仍摇摆则标成 `unstable`，不进入 production。
