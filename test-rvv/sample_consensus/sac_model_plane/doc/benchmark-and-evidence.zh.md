# sac_model_plane benchmark 与证据

## 本文职责

本文记录 `bench_sac_model_plane` 的输入、计时边界、checksum（校验和）、board evidence
（板卡证据）、manifest（证据清单）和 Evidence Doctor（证据体检）边界。

## Bench 输出格式

`src/bench_sac_model_plane.cpp` 输出 dataset、iterations、warmup、build、checksum，以及三条公开入口的
`ms/iter`。参数为：

```text
bench_sac_model_plane <points> <iterations> [identity|shuffled]
```

默认是 `65536 200 shuffled`，warmup 固定为 5。`identity` 表示 `indices[i] == i`；
`shuffled` 每 4 个元素交换一对索引，用来证明 fallback gather（回退离散加载）没有退化。

## 计时边界

计时只包含公开入口调用本身；点云、indices、模型系数和对象构造不计入。`selectWithinDistance`
计入 inliers 和 `error_sqr_dists_` 写回；`getDistancesToModel` 计入 dense distance store
（密集距离写回）；`countWithinDistance` 只计入 mask count（掩码计数）。

## Phase 000 基础 board 证据

Phase 000 使用 `PointXYZ`、65536 点、direct indexed `indices_`、200 iterations 和 5 warmup。
Std/RVV checksum 均为 `65740`。

| public entry | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | --- |
| `selectWithinDistance` | 3.1965x | 3.1746x | 3.2748x | positive |
| `countWithinDistance` | 1.6678x | 1.6662x | 1.6861x | positive |
| `getDistancesToModel` | 2.3695x | 2.1138x | 2.3943x | positive |

Evidence Doctor 报告路径是
`doc/phases/000-base-plane-select-distance-production/evidence-doctor.md`，结果为
Errors=0、Warnings=0、Suggestions=6。

## Phase 010 identity / shuffled board 证据

Phase 010 使用 production-public（真实公开入口）Std/RVV 对比，分别采集 identity 和 shuffled
5-run repeated board。

| index mode | entry | median | min | max | decision |
| --- | --- | ---: | ---: | ---: | --- |
| identity | `selectWithinDistance` | 3.3896x | 3.3449x | 3.4529x | adopted for identity strided load |
| identity | `countWithinDistance` | 2.1969x | 2.1822x | 2.2002x | adopted for identity strided load |
| identity | `getDistancesToModel` | 2.1547x | 2.0321x | 2.6258x | not adopted; current code remains gather-only |
| shuffled | `selectWithinDistance` | 3.1895x | 3.1471x | 3.2180x | gather fallback retained |
| shuffled | `countWithinDistance` | 1.6663x | 1.6618x | 1.6761x | gather fallback retained |
| shuffled | `getDistancesToModel` | 2.3126x | 2.1626x | 2.4473x | Phase 000 gather path retained |

Evidence Doctor：

- identity report：`log/board/phase-010/identity-stride-select-count/repeated/evidence_doctor.md`，
  Errors=0、Warnings=1、Suggestions=6。
- shuffled report：`log/board/phase-010/shuffled-stride-select-count/repeated/evidence_doctor.md`，
  Errors=0、Warnings=0、Suggestions=6。
- 可提交摘要：`doc/phases/010-identity-index-strided-load/evidence-doctor.md`。

identity report 的 Warning 属于未采纳的 `getDistancesToModel` identity 分支；select/count
窄采纳不因此降级。Suggestions 是缺少 taskset、governor、freq、temperature 和 binary hash
（等价二进制身份），当前不阻塞 positive 桶。

## 复现命令

```bash
make -C test-rvv/sample_consensus/sac_model_plane run_test_compare
make -B -C test-rvv/sample_consensus/sac_model_plane dump_bench_rvv
make -C test-rvv/sample_consensus/sac_model_plane generate_phase_010_identity_evidence_manifest
make -C test-rvv/sample_consensus/sac_model_plane generate_phase_010_shuffled_evidence_manifest
python3 test-rvv/script/evidence_doctor.py --manifest test-rvv/sample_consensus/sac_model_plane/log/board/phase-010/identity-stride-select-count/repeated/evidence_manifest.json --output test-rvv/sample_consensus/sac_model_plane/log/board/phase-010/identity-stride-select-count/repeated/evidence_doctor.md --fail-on never
python3 test-rvv/script/evidence_doctor.py --manifest test-rvv/sample_consensus/sac_model_plane/log/board/phase-010/shuffled-stride-select-count/repeated/evidence_manifest.json --output test-rvv/sample_consensus/sac_model_plane/log/board/phase-010/shuffled-stride-select-count/repeated/evidence_doctor.md --fail-on never
```

Raw logs 默认不提交；需要提交日志时先走 sanitize（脱敏）检查。
