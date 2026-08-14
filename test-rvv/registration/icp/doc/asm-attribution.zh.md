# ICP transformCloud 反汇编归因

## 当前结论

`make -C test-rvv/registration/icp dump_bench_rvv` 生成的
`test-rvv/registration/icp/build/asm/riscv/bench_icp_rvv.full.asm` 中，RVV 指令簇归属于真实
production symbol（生产符号），不是 test-only helper。当前入口结构是
`IterativeClosestPoint::transformCloud` 先尝试 RVV，失败后 `jal` 到
`pcl::registration::detail::transformCloudStandard` 标量 fallback；`nm -C` 可见独立 Std fallback 符号。

| 点型 | production symbol | bench 调用边界 | 观察到的 RVV 指令 |
| --- | --- | --- | --- |
| `PointXYZ` | `pcl::IterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ, float>::transformCloud(...)` | `benchPointXYZ` lambda `jal` 到该符号 | `vsetvli e32,m2`、`vlsseg3e32.v`、`vfmacc.vf`、`vmfeq.vv`、`vmflt.vf`、`vmand.mm`、masked `vsse32.v` |
| `PointNormal` | `pcl::IterativeClosestPoint<pcl::PointNormal, pcl::PointNormal, float>::transformCloud(...)` | `benchPointNormal` lambda `jal` 到该符号 | XYZ 与 normal 两段 `vsetvli e32,m2`、`vlsseg3e32.v`、`vfmacc.vf`、`vmand.mm`、masked `vsse32.v` |

`nm -C test-rvv/registration/icp/build/riscv/bench_icp_rvv` 同时能看到这两个
`IterativeClosestPoint::transformCloud` 符号，以及 `PointXYZ` / `PointNormal` 的
`pcl::registration::detail::transformCloudStandard` fallback 符号；bench lambda 入口只负责调用
production helper 和 checksum。

## 证据边界

- 反汇编只证明 RVV build 的 production `transformCloud` hot path 中存在目标 RVV 指令簇，并证明
  fallback 已抽成可归因的 `transformCloudStandard` 符号。
- 性能结论仍以 `test-rvv/registration/icp/log/board/transform_cloud_repeated/summary.md` 为准。
- Correctness（正确性）以 QEMU / board gtest 的 production-direct 对拍为准，不以 bench checksum 作数值等价证明。
