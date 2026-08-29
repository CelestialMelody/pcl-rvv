# Testing Overview

本 topic 的测试分三层：

| target | 层级 | 作用 |
| --- | --- | --- |
| `run_test_compare` | correctness aggregate | 标量参考与 RVV 候选对拍 |
| `run_qemu_smoke` | QEMU smoke | 只证明可运行和日志形状 |
| `run_upstream_test_compare` | production direct correctness | 直接跑 `test/recognition/test_recognition_cg.cpp`，验证 `recognize()` 入口 Std/RVV 一致 |
| `check_gc_rvv_asm` | asm gate | 证明候选路径命中 RVV gather / math 指令 |
| `board_repeated` | board repeated | 5-run repeated summary 和 Evidence Doctor |
| `record_evidence_state_repeated` | registry | 把 summary / manifest / doctor 登记到 evidence registry |
| `board_repeated_growth` | board repeated | 5-run growth-mode repeated summary 和 Evidence Doctor |
| `record_evidence_state_growth` | registry | 把 growth summary / manifest / doctor 登记到 evidence registry |

当前已有 production direct correctness；若后续补 production direct bench，再把它接到新的 board / phase 说明里。
