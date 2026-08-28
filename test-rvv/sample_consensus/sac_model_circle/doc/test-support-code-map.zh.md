# sac_model_circle test support code map

## 本文职责

本文帮助 reviewer 从文档定位到测试支撑代码、bench wrapper（性能测试包装）、manifest script（证据清单脚本）和 production 对照。它不承担性能结论。

## 总调用图

```text
production public entry
  selectWithinDistance / countWithinDistance / getDistancesToModel
      |
      +-- production Standard / RVV helper
      |
      +-- src/test_sac_model_circle.cpp correctness gtest
      |
      +-- src/bench_sac_model_circle.cpp timing rows
              |
              +-- script/generate_circle_board_evidence_manifest.py
                      |
                      +-- Evidence Doctor + evidence_registry.json
```

## 稳定入口

| 入口 | 位置 | 作用 |
| --- | --- | --- |
| test executable | `test_sac_model_circle_std`、`test_sac_model_circle_rvv` | correctness aggregate 和细分 gtest filter。 |
| bench executable | `bench_sac_model_circle_std`、`bench_sac_model_circle_rvv` | board repeated timing input。 |
| Makefile | `Makefile` | 本地 / QEMU / board / doctor / registry target 汇总。 |
| board config | `board.mk` | 板卡远端 binary 名称和部署变量，私有值不写入文档。 |

## Fixtures 与输入构造

`makeCircleDispatchCloud` 构造 10 点固定输入，覆盖 shell 内、shell 外、圆上点和非对称点。bench 构造 65536 点合成圆 shell cloud，并对相邻 pairs 做局部乱序，模拟 direct indexed `indices_`，而不是纯 identity load。

## 标量 Reference

`selectWithinDistanceStandard`、`countWithinDistanceStandard` 和 `getDistancesToModelStandard` 是 production 源码中的标量 reference（参考链路）。test-only `getDistancesToModelCandidate` / `getDistancesToModelFullRVVCandidate` 使用 public 或 Standard 路径作为 reference；Phase 060 后可直接调用 `getDistancesToModelRVV` 做 production correctness。

## Candidate / Diagnostic Helper

| helper | 层级 | 当前状态 |
| --- | --- | --- |
| `selectWithinDistanceRVV` | production RVV helper | adopted production behavior（已采用生产行为），Phase 000 接入后板卡数据为正向。 |
| `countWithinDistanceRVV` | production RVV helper | adopted production behavior，Phase 000 接入后板卡数据为正向。 |
| `getDistancesToModelCandidateRVV` | test-only production-shaped diagnostic | Phase 020 rejected with diagnostic evidence，不进入 production probe。 |
| `getDistancesToModelFullRVV` | test-only production-shaped diagnostic | Phase 050 positive；已推进到 Phase 060 production probe。 |
| `getDistancesToModelRVV` | production RVV helper | Phase 060 接入后 positive-stable；Phase 070 已采纳。 |

## Bench Harness 与 Case Registry

`src/bench_sac_model_circle.cpp` 固定输出 public select/count/getDistances、旧 diagnostic candidate 和 full-RVV candidate row，并可用 `identity` index mode（索引模式）复放 Phase 040 历史探针。`script/generate_circle_board_evidence_manifest.py --mode production` 只采集 select/count production rows；`--mode getdistances` 采集 Phase 020 旧 candidate；`--mode getdistances-full-rvv` 采集 Phase 050 full-RVV diagnostic；`--mode getdistances-production` 采集 Phase 060 接入后 public Std/RVV。这个拆分避免把诊断、production direct 和历史 identity probe 混在同一个 EvidenceDecision 中。

## Scripts 与 Evidence Output

| 对象 | 位置 | 说明 |
| --- | --- | --- |
| manifest generator | `script/generate_circle_board_evidence_manifest.py` | topic-local，因为它知道 circle case label、helper 名和输出行。 |
| Evidence Doctor | `../../script/evidence_doctor.py` | 通用检查脚本，由 Makefile target 调用。 |
| registry tool | `../../script/evidence_registry.py` | 通用登记脚本，记录 Phase 000 / 020 / 050 / 060 summary evidence。 |
| evidence output | `doc/phases/*/*manifest.json`、`*doctor.md`、`*doctor.json` | summary-only 提交候选；Phase 040 identity 输出只作为 rejected family evidence，Phase 060 输出是 getDistances adopted production evidence。 |

## Production 与 Test Support 边界

production 源码当前包含 select/count 的 gather-only RVV helper，以及 Phase 060 新增、Phase 070 采纳的 `getDistancesToModelRVV` production helper。`getDistancesToModelCandidateRVV` 和 `getDistancesToModelFullRVV` 只在 test support 中存在，不能被文档写成 production behavior。identity strided-load 分支已在 Phase 040 负向后撤回，当前只保留受保护的历史 replay target。

## 拆分审计

当前测试和 bench 已位于配置解析出的 `src/` 下，测试支撑聚合头在 `include/`，内部候选 helper 在 `include/impl/`。没有旧 `test_support/` 目录，也没有单个超过 800 行的 C++ helper 文件。脚本职责集中在 manifest 解析和 case label 映射，当前不需要拆成多个脚本。若未来新增更多点型 expansion 或 RVV-vs-RVV family A/B，再评估进一步拆分。
