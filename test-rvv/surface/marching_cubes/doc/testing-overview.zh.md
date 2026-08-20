# 测试总览

## 入口分类

| 入口 | 作用 | 证据角色 |
| --- | --- | --- |
| `run_test_compare` | Std/RVV gtest 对拍 | 正确性 |
| `dump_bench_rvv` | 生成 RVV bench 反汇编 | 指令归属 |
| `run_board_edge_interpolation_evidence_doctor` | edge repeated summary + doctor | 诊断证据 |
| `run_board_prepass_evidence_doctor` | prepass repeated summary + doctor | 诊断证据 |
| `run_board_production_direct_evidence_doctor` | production-direct repeated summary + doctor | production evidence |
| `run_board_generic_repeated_evidence_doctor` | generic representative repeated summary + doctor | production evidence |

## 当前覆盖

- 正确性：6 个 gtest，覆盖 dense sphere、wave、sparse sphere、production direct、generic 代表点型和 fallback 回退。
- bench：edge candidate、prepass candidate、production direct 和 generic 代表点型共用同一 bench harness。
- board：edge repeated、prepass repeated、historical `PointNormal` production repeated 和 generic representative repeated 都已有 summary / doctor。

## 边界说明

- QEMU 只证明可运行、日志形状和 checksum 一致。
- 板卡结果才用于性能判断。
- 诊断 helper 不等于 production public path。
- Generic representative repeated 只证明 `RVVXYZAoSFloatLayout<PointNT>` gate 下的 synthetic public path；真实 Hoppe / RBF voxelization 分布仍不由本证据外推。
