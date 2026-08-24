# Phase 000 结果：scale-aware fused accumulation 诊断

## EvidenceDecision

`partial-production-candidate`。

Phase 000 完成时停在用户确认点。`direct-fused-scale-accum` 在 ordered-cloud-pair、dense、`Scalar=float`、`PointXYZ -> PointXYZ`、4K/64K/256K 范围内是 positive diagnostic（正向诊断）。本阶段没有修改 production 源码；当前恢复目标已经授权先做 doc-suite role parity，再进入 production integration loop（生产接入闭环）的 PI1 计划阶段。

## 已验证范围

| 维度 | 结果 |
| --- | --- |
| entry | test-only diagnostic helper；不接 production public overload。 |
| row source | ordered-cloud-pair。 |
| point type / scalar | `PointXYZ -> PointXYZ` / `float`。 |
| input shape | dense synthetic similarity transform corpus。 |
| sizes | 4K、64K、256K。 |

## 证据摘要

| gate | result | notes |
| --- | --- | --- |
| QEMU correctness | historical passed | Phase 000 完成时 Std/RVV 各 4 个 gtest 全通过；后续 Phase 010 曾扩展为 6 个 gtest，Phase 020 当前可覆盖日志已扩展为 7 个 gtest。 |
| QEMU smoke Evidence Doctor | passed | `Errors=0`、`Warnings=0`；QEMU timing 不作为性能结论。 |
| board correctness smoke | historical passed | Phase 000 当时板卡 RVV test binary 4 个 gtest 全通过；Phase 010 曾以 QEMU 6-test rerun 和 production direct board evidence 为主，Phase 020 当前 correctness 以 QEMU 7-test rerun 为主。 |
| board repeated diagnostic | positive | 5 runs、20 iterations、5 warmup；4K/64K/256K median B/A = `2.783x` / `2.958x` / `2.937x`。 |
| board Evidence Doctor | acceptable | `Errors=0`、`Warnings=1`；warning 为 4K long-tail，4K min B/A 仍为 `2.513x`。 |
| asm shape | passed | RVV bench asm 可见 `vlseg*` / `vfmacc.vv` / `vfredosum.vs` / `vsetvli`。 |
| registry | fresh with later overwrite note | Phase 000 board repeated 和 QEMU smoke 均已登记；QEMU correctness 的可覆盖 log path 后由 Phase 010 6-test rerun、再由 Phase 020 7-test rerun 刷新并重新登记。 |

## 数值一致性

RVV reduction tree（规约树）与 scalar ordered accumulation 不 bit-identical，所以 1e-6 量化 matrix checksum 在 Std/RVV 日志中不同。当前 correctness gate 不使用 checksum 等值，而使用 gtest 和 bench 输出的 `max_reference_error`：

| size | max RVV reference error | tolerance |
| --- | ---: | ---: |
| 4K | `1.490e-06` | `2e-3` |
| 64K | `1.001e-05` | `2e-3` |
| 256K | `6.974e-05` | `2e-3` |

## 不能外推的范围

本结果不能证明 source-indexed、dual-indexed、correspondence、泛型 xyz AoS、`Scalar=double`、non-dense / NaN / Inf 输入或真实 production dispatch。若进入 production integration loop，需要重新补 public ordered overload correctness、fallback gate、asm attribution、board production repeated 和 Evidence Doctor。

## 下一步

Phase 000 当时建议进入 `010-production-integration-plan`，停止原因是继续会触碰 production 文件。当前恢复目标已授权进入 PI1；PI2 之后仍必须按 PI 计划冻结范围、补 public production direct 证据，并在 PI5 停到用户检查点。

- `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp`
- 必要时的 declaration / support header 和 production direct tests
