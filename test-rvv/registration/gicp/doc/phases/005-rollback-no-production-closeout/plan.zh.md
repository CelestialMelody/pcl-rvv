# Phase 005 Plan: rollback no-production closeout

## 阶段意图和边界

本阶段按用户确认执行 no-production（不接入生产）收口：回退
`registration/include/pcl/registration/gicp.h` 和
`registration/include/pcl/registration/impl/gicp.hpp` 中的 GICP RVV 生产补丁，删除不再适用的
`doc-rvv/registration/gicp-RVV.zh.md`，保留 `test-rvv/registration/gicp` 的诊断、bench
和 phase 证据。

本阶段不新增 RVV 实现，不扩大到其它点型、`Scalar`、row source 或其它 registration topic。

## 当前状态

Phase 003 clean adoption 的 production-public 证据为弱正向：1024 点 median `1.080x`，
4096 点 median `1.058x`。Phase 004 的 `vluxei32` gather width 微调为 1024 点 median
`1.073x`，没有改善。用户已确认按“收益偏低则回退并结束 topic”的建议执行。

## 动作

| action | artifact | completion |
| --- | --- | --- |
| 回退生产源码 | `registration/include/pcl/registration/gicp.h`、`registration/include/pcl/registration/impl/gicp.hpp` | GICP 生产源码对基线应为零 diff |
| 删除长期生产文档 | `doc-rvv/registration/gicp-RVV.zh.md` | no-production 下不保留 adopted production behavior 文档 |
| 同步 topic-local 文档 | `README.zh.md`、evaluation、benchmark、matrix、roadmap、phase index | 当前结论写为 no-production |
| 刷新证据检查入口 | `Makefile`、`log/evidence_registry.json` | `evidence_status` 不再依赖已删除的 doc-rvv |

## 测试和验证

| evidence | command | completion |
| --- | --- | --- |
| QEMU correctness | `make -C test-rvv/registration/gicp record_qemu_correctness_state` | Std/RVV 10 tests pass，并登记 registry |
| evidence registry | `make -C test-rvv/registration/gicp evidence_status` | fresh |
| diff hygiene | `git diff --check -- <GICP paths>` | 通过 |
| production rollback | `git diff -- registration/include/pcl/registration/gicp.h registration/include/pcl/registration/impl/gicp.hpp` | 无输出 |

## 完成条件

本阶段完成后，GICP topic 可作为 no-production closeout 提交。提交边界只包含
`test-rvv/registration/gicp` 的测试/文档/summary 证据和删除的长期 GICP `doc-rvv` 文档；
不得混入其它 registration topic。
