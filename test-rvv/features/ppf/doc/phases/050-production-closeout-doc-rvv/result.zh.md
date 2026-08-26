# Phase 050 Production Closeout Doc-RVV Result

## 当前结论

本阶段完成 PI5 后的 S11 production closeout（生产收尾）。用户确认“板卡上的测试结果如果显示有收益即可采纳”，
因此 Phase 040 的 exact `PointXYZ + Normal + PPFSignature` production-public positive 证据已升级为
adopted production behavior（已采用生产行为）。

正式 production 长期文档已创建：`doc-rvv/features/ppf-RVV.zh.md`。该文档只使用接入 production 后的
Phase 040 board repeated 数据作为性能采纳依据；Phase 030 diagnostic speedup 只保留为进入生产探针的历史理由。

## 计划执行回填

| action | status | evidence / path | result |
| --- | --- | --- | --- |
| 创建 production 长期主题文档 | done | `doc-rvv/features/ppf-RVV.zh.md` | 覆盖当前状态、函数语义、采用方式、fallback 矩阵、Traceability Map、数值算例、bench 与证据、正确性与高效性证据链和后续方向。 |
| 更新 evaluation | done | `test-rvv/features/ppf/doc/ppf-evaluation.zh.md` | production decision 从 PI5 pending 改为 adopted production behavior，并引用正式 `doc-rvv`。 |
| 更新 roadmap / matrix | done | `doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` | `production alpha_m batch RVV` 改为 adopted；后续泛型点型、证据增强和 direct-AoS revisit 标为 separate phase。 |
| 更新 topic navigation / phase index | done | `README.zh.md`、`doc/phases/README.zh.md` | 默认恢复动作改为 ready for review，并增加 Phase 050 入口。 |
| 更新 Phase 040 历史结果 | done | `040-production-alpha-m-rvv-integration/result.zh.md` | 保留当时 PI5 停点，同时说明已由 Phase 050 采纳。 |
| 更新筛选清单 | done | `doc-rvv/library-screening/features/features-retained-candidate-rescreen.zh.md` | PPF 从未启动改为已完成 / adopted production behavior。 |

## Production Doc Closeout Gate

| area | current status | action |
| --- | --- | --- |
| 当前状态 | adopted exact `PointXYZ + Normal + PPFSignature` production path | 已写入 `doc-rvv/features/ppf-RVV.zh.md#当前状态` |
| 稳定证据索引 | Phase 040 production-public board repeated + Doctor `0E/0W/2S` | 已写入 Bench 与证据、正确性与高效性证据链 |
| 函数语义 | public `compute()` / `computeFeature`、`indices_ x input_`、`computePairFeatures`、`alpha_m` | 已写入函数语义和标量路径 |
| 当前采用方式 | exact gate 下 `alpha_m` closed-form RVV batch，`f1..f4` 标量 | 已写入当前采用的优化方式和 VL chunk 流程 |
| 范围决策表 | exact gate adopted；generic traits、double、其它 row source deferred | 已写入 fallback 矩阵、closeout 和后续方向 |
| 标量 / RVV 差异 | RVV 只替换 `alpha_m` 后段；输出顺序和 failure 语义不变 | 已写入标量路径与 RVV 路径差异 |
| Traceability Map | production、test、bench、script、phase、evaluation、evidence summary 可定位 | 已写入正式 `doc-rvv` 和 evaluation |
| 数值算例 | parallel-to-x lane 示例 | 已写入正式 `doc-rvv` |
| Bench 与证据 | board repeated 5-run、mean Std/RVV、QEMU 边界、Doctor suggestions | 已写入正式 `doc-rvv` |
| fallback 矩阵 | 非 RVV、非 exact 点型、identity、helper failure、其它 row source | 已写入正式 `doc-rvv` |
| 遗留风险 | 泛型点型、证据增强、direct-AoS pair-feature revisit | 已写入后续方向 |

## 正确性与高效性证据链

| 层级 | 当前证据 | 结论边界 |
| --- | --- | --- |
| correctness | `make -C test-rvv/features/ppf run_test_compare` fresh：Std 5/5，RVV 6/6。 | exact covered public path 与 reference 对拍通过。 |
| production direct | `PPFProductionDirect.RVVAlphaMPathHitsPublicComputeForExactTypes`。 | RVV 构建下 public `compute()` 命中 production RVV helper。 |
| QEMU smoke | `public_ppf_compute` 小规模 QEMU bench 可运行。 | 只证明日志形状，不作为性能结论。 |
| asm | production helper 符号范围含 `vsetvli`、`vle32`、`vfdiv`、`vfsqrt`、`vmerge`、`vfnmsac`、`vfmacc`、`vse32`。 | 证明 RVV 指令归属到 production helper。 |
| board performance | `public_ppf_compute` 5-run speedup `1.35, 1.35, 1.37, 1.39, 1.38`。 | 目标板卡、当前规模和 exact gate 下为 positive。 |
| Evidence Doctor | `Errors=0, Warnings=0, Suggestions=2`。 | suggestions 不阻塞采纳；严格归档时可补 metadata。 |

## Continue / Stop Decision

`continue_stop_decision`：stop at adopted production closeout / ready for review。

`stop_condition_hit`：当前 exact production boundary 已完成生产接入、生产直连测试、asm、板卡 repeated、
Evidence Doctor、正式 `doc-rvv` 和筛选状态同步。继续优化需要扩大到新的 phase scope（阶段范围），例如
泛型点型 traits、其它 row source、证据增强或 direct-AoS pair-feature revisit；这些都不能在当前 closeout
中直接继续。

默认下一步：review 当前 production patch、topic test assets 和正式 `doc-rvv`。如果用户要求继续扩大覆盖，
优先新建 point-type expansion phase；如果用户要求更严格证据归档，新建 evidence hardening phase。
