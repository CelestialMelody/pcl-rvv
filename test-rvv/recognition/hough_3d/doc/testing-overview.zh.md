# Testing Overview

## 入口分类

| target | 主测试类型 | 说明 |
| --- | --- | --- |
| `run_test_compare` | 正确性测试 | std / rvv 两个 build 对拍，先确认 candidate 的语义没有跑偏。 |
| `run_qemu_smoke` | QEMU 冒烟 | 只看构建、路径和日志形状，不写性能结论。 |
| `dump_bench_rvv` | 反汇编检查 | 先看 RVV 指令是否落在当前 helper。 |
| `board_repeated` | 板卡重复测试 | 目标硬件上的 repeated summary，才可写性能结论。 |
| `board_production_repeated` | 生产直连板卡测试 | 真实 `houghVoting()` 入口的 repeated summary，用来判断当前 patch 是否值得采纳。 |
| `evidence_doctor_production_repeated` | 证据体检 | 生产直连 summary 的 manifest / doctor 检查。 |
| `record_evidence_state_production_repeated` | 证据登记 | 把 phase 010 的 production direct 证据写入 registry。 |

## 目前覆盖

- `houghVoting()` 的 vote generation 诊断。
- `houghVoting()` 的 production direct probe。
- `HoughSpace3D::vote()` / `voteInt()` / `findMaxima()` 暂时不进本轮主线。
