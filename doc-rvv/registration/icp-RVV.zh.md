# ICP transformCloud RVV 主题文档

## 当前状态

`registration/include/pcl/registration/impl/icp.hpp` 中
`IterativeClosestPoint::transformCloud` 已接入 RVV production path。当前 EvidenceDecision 为
`production_direct_positive`：QEMU / 板卡 correctness 通过，Milkv-Jupiter production direct
5-run repeated benchmark 稳定正向，反汇编归因指向 production `transformCloud` 符号。

## 生产分流边界

RVV path 只在以下条件全部满足时启用：

- `__RVV10__` build。
- `Scalar=float`；`Scalar=double` 回退标量。
- `input.size() >= 32`。
- `PointSource` 通过 `pcl::rvv::kRVVXYZAoSPointCompatible` 或
  `pcl::rvv::kRVVXYZNormalPointCompatible`。
- 运行期 `x/y/z` 与可选 `normal_x/y/z` offsets 和 traits layout offsets 完全一致。

其它情况调用 `pcl::registration::detail::transformCloudStandard`。该 helper 保留上游标量语义；
production 入口只保留 matrix cast、RVV 短路和 Std fallback 调用。RVV helper 只写原标量路径会写的字段，
不额外做 `output = input`，以保持 caller 预置 output 和 in-place 语义。

## 正确性与高效性证据链

| 证据 | 当前状态 | 说明 |
| --- | --- | --- |
| QEMU correctness | done | `test-rvv/registration/icp/log/qemu/run_test_std.log` 和 `test-rvv/registration/icp/log/qemu/run_test_rvv.log` 均为 12 tests passed。 |
| board correctness | done | `test-rvv/registration/icp/log/board/run_test.log` 为 12 tests passed。 |
| board performance | done | `test-rvv/registration/icp/log/board/transform_cloud_repeated/summary.md`，Milkv-Jupiter 5-run production direct。 |
| Evidence Doctor | done | `test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_doctor.md`：Errors=0，Warnings=2，Suggestions=0。 |
| manifest | done | `test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_manifest.json`：`evidence_role=production_direct`。 |
| asm attribution | done | `test-rvv/registration/icp/doc/asm-attribution.zh.md`：RVV 指令簇位于 production `transformCloud` 符号内。 |
| evidence registry | done | `test-rvv/registration/icp/log/evidence_registry.json` 记录当前 production direct evidence。 |

Production direct board median：

| case | median speedup |
| --- | ---: |
| `icp transform-cloud xyz 64K` | 5.68x |
| `icp transform-cloud xyz 256K` | 5.30x |
| `icp transform-cloud xyz-normal 64K` | 3.76x |
| `icp transform-cloud xyz-normal 256K` | 3.93x |

Doctor warnings 均来自 `PointXYZ 64K`：long-tail / variance 和 group outlier；min=5.29x，
median=5.68x，max=6.50x。由于全部 run 均明显正向，当前不扩大复跑预算，但保留该风险说明。

## QEMU bench compare 策略

QEMU 只用于 correctness、build 和日志形状，不用于性能结论。公共 Makefile 已默认禁止
QEMU `run_bench_compare`；只有为了历史/窄范围 smoke，且显式设置 `ALLOW_QEMU_BENCH_COMPARE=1`
并写明 `qemu_smoke_only`，才可运行。历史 QEMU bench 文件
`test-rvv/registration/icp/log/qemu/run_bench_std.log`、
`test-rvv/registration/icp/log/qemu/run_bench_rvv.log`、
`test-rvv/registration/icp/log/qemu/analyze_bench_compare.log`、
`test-rvv/registration/icp/log/qemu/evidence_manifest.json` 和
`test-rvv/registration/icp/log/qemu/evidence_doctor.md` 不进入性能排序或 EvidenceDecision。

## 不覆盖范围

- `IterativeClosestPointWithNormals` 不走本函数，它直接调用 `pcl::transformPointCloudWithNormals`。
- indices / correspondences 不属于 `transformCloud` 自身 row source；本函数只扫描已 materialized input cloud。
- 当前 microbench 不声称 ICP 端到端整体加速同等幅度，nearest-neighbor search 和 SVD 求解仍是独立成本。
