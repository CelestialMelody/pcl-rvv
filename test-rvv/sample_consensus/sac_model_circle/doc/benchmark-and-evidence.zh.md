# sac_model_circle benchmark and evidence

## 本文职责

本文说明 bench（性能测试）输出、板卡 repeated evidence（重复证据）、manifest（证据清单）、Evidence Doctor（证据体检）和 registry（证据登记表）的边界。QEMU timing（QEMU 计时）不作为性能结论。

## Bench 输出格式

`src/bench_sac_model_circle.cpp` 输出固定数据集说明、iteration / warmup、构建类型、checksum 和六个 timing row：

- `public selectWithinDistance`
- `public countWithinDistance`
- `public getDistancesToModel`
- `diagnostic select full-rvv error tail`
- `diagnostic candidate getDistancesToModel`
- `diagnostic full-rvv getDistancesToModel`

checksum 由 inliers、误差或 distances hash 组合而来，只用于发现路径输出异常，不是性能指标。Phase 060 的 `getDistancesToModel` 不要求 Std/RVV raw output hash 逐 bit 相同；正确性 gate 是 public / Standard / direct RVV gtest 的 `1e-6` 误差预算。

## CLI 参数和计时边界

| 参数 | 含义 |
| --- | --- |
| argv[1] | 点数，默认 65536。 |
| argv[2] | iteration 次数，默认 200。 |
| argv[3] | index mode（索引模式），`shuffled` 或 `identity`；只用于 Phase 040 历史 RVV-vs-RVV A/B。 |

计时边界只包含 public 或 candidate 入口调用和 checksum 输入生成，不包含点云、indices 和模型系数构造。warmup 固定为 5 次。

## 推荐 target

```bash
make -C test-rvv/sample_consensus/sac_model_circle dump_bench_rvv
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle collect_production_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_circle record_production_board_evidence_state
make -C test-rvv/sample_consensus/sac_model_circle production_evidence_status
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle collect_select_error_tail_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_circle record_select_error_tail_board_evidence_state
make -C test-rvv/sample_consensus/sac_model_circle select_error_tail_evidence_status
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle collect_select_error_tail_production_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_circle record_select_error_tail_production_board_evidence_state
make -C test-rvv/sample_consensus/sac_model_circle select_error_tail_production_evidence_status
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle collect_getdistances_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_circle record_getdistances_board_evidence_state
make -C test-rvv/sample_consensus/sac_model_circle getdistances_evidence_status
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle collect_getdistances_full_rvv_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_circle record_getdistances_full_rvv_board_evidence_state
make -C test-rvv/sample_consensus/sac_model_circle getdistances_full_rvv_evidence_status
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle collect_getdistances_production_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_circle record_getdistances_production_board_evidence_state
make -C test-rvv/sample_consensus/sac_model_circle getdistances_production_evidence_status
make -C test-rvv/sample_consensus/sac_model_circle identity_evidence_status
```

## 当前 board 证据

| phase | evidence role | manifest | Evidence Doctor | 结果 |
| --- | --- | --- | --- | --- |
| 000 | production direct historical baseline | `doc/phases/000-circle-select-distance-production/production-repeated-evidence-manifest.json` | `doc/phases/000-circle-select-distance-production/production-repeated-evidence-doctor.md` | 第一版 select scalar error tail median 1.6702x，count median 1.4012x，Errors=0 / Warnings=0 / Suggestions=0。select 当前 truth 已由 Phase 090 取代。 |
| 020 | production-shaped diagnostic | `doc/phases/020-circle-getdistances-ablation/getdistances-repeated-evidence-manifest.json` | `doc/phases/020-circle-getdistances-ablation/getdistances-repeated-evidence-doctor.md` | getDistances candidate B/A median 0.6590x，Errors=1 / Warnings=1 / Suggestions=0。 |
| 050 | production-shaped diagnostic | `doc/phases/050-getdistances-vfsqrt-full-rvv/getdistances-full-rvv-repeated-evidence-manifest.json` | `doc/phases/050-getdistances-vfsqrt-full-rvv/getdistances-full-rvv-repeated-evidence-doctor.md` | full-RVV candidate B/A median 1.4745x，Errors=0 / Warnings=0 / Suggestions=0。 |
| 060 | production direct | `doc/phases/060-getdistances-production-probe/getdistances-production-repeated-evidence-manifest.json` | `doc/phases/060-getdistances-production-probe/getdistances-production-repeated-evidence-doctor.md` | 接入后 public getDistances B/A median 1.4737x，min/max 1.4687x / 1.4962x，Errors=0 / Warnings=0 / Suggestions=0；Phase 070 已采纳。 |
| 080 | strict A/B | `doc/phases/080-select-compressed-error-tail/select-error-tail-repeated-evidence-manifest.json` | `doc/phases/080-select-compressed-error-tail/select-error-tail-repeated-evidence-doctor.md` | select full-RVV error tail test-only candidate 相对旧 adopted RVV baseline median 1.6078x，min/max 1.5774x / 1.6138x，Errors=0 / Warnings=0 / Suggestions=0。 |
| 090 | production direct | `doc/phases/090-select-error-tail-production-probe/select-error-tail-production-repeated-evidence-manifest.json` | `doc/phases/090-select-error-tail-production-probe/select-error-tail-production-repeated-evidence-doctor.md` | 接入后 public select B/A median 2.5700x，min/max 2.5357x / 2.6770x，Errors=0 / Warnings=0 / Suggestions=0；当前已采纳。 |
| 040 | strict A/B | `doc/phases/040-production-closeout-and-identity-frontier/identity-repeated-evidence-manifest.json` | `doc/phases/040-production-closeout-and-identity-frontier/identity-repeated-evidence-doctor.md` | identity candidate select median 0.9914x，count median 0.9698x，Errors=2 / Warnings=0 / Suggestions=0。 |

Phase 020 的 Error 是 `ba_degradation_frequency`，说明当前 helper 形态稳定慢于 public row；Warning 是 `fewer_instructions_but_slower`，说明 RVV 指令更少不等于端到端更快。

Phase 040 的两个 Error 也是 `ba_degradation_frequency`。这批数据是历史 rejected probe（已拒绝探针）证据：候选二进制曾包含 identity strided-load 分支，但当前 production 源码已回到 gather-only RVV。`check_identity_strided_asm` 和 identity manifest regeneration（重新生成）target 默认受 `ALLOW_HISTORICAL_IDENTITY_PROBE=1` 保护，常规 closeout 只运行 `identity_evidence_status` 检查已有 summary evidence。

## ASM Attribution 口径

`dump_bench_rvv` 生成 `build/asm/riscv/bench_sac_model_circle_rvv.full.asm`。Phase 000 manifest 统计旧 `selectWithinDistanceRVV` 23 条 RVV 指令、`countWithinDistanceRVV` 15 条 RVV 指令；Phase 020 manifest 统计旧 `getDistancesToModelCandidateRVV` 13 条 RVV 指令。Phase 050 / 060 的 asm gate 明确检查 `vfsqrt.v`、`vfwcvt.f.f.v` 和 `vse64.v`，分别归属到 `getDistancesToModelFullRVV` 和 production `getDistancesToModelRVV`。Phase 080 / 090 的 asm gate 检查 `vcompress.vm`、`vfsqrt.v`、`vfwcvt.f.f.v` 和 `vse64.v`，Phase 090 归属到当前 production `selectWithinDistanceRVV`。Phase 040 的 `vlse32.v` asm gate 只属于历史 identity candidate，不属于当前 production 验证门禁。

## 提交边界

summary-only 策略下，可提交候选是 manifest、Evidence Doctor 摘要、phase result、evaluation、README、role docs 和 registry。raw board logs、QEMU logs、反汇编输出、二进制、`__pycache__` 和本机配置默认不提交。
