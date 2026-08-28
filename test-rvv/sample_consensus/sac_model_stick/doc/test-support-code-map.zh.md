# sac_model_stick test-support code map

## 本文职责

本文定位当前 topic 的测试支撑代码、helper（辅助函数）、bench harness（性能测试框架）、script（脚本）和 evidence output（证据输出）。它不承担性能结论；性能和 Evidence Doctor（证据体检）见 `doc/benchmark-and-evidence.zh.md`。

## 总调用图

```text
production public entries
  SampleConsensusModelStick::countWithinDistance
  SampleConsensusModelStick::selectWithinDistance
  SampleConsensusModelStick::getDistancesToModel
        |
        +-- production RVV helpers under __RVV10__
        |     countWithinDistanceRVV
        |     selectWithinDistanceRVV
        |     getDistancesToModelRVV
        |
        +-- Standard fallback helpers
              countWithinDistanceStandard
              selectWithinDistanceStandard
              getDistancesToModelStandard

test-only derived model
  SampleConsensusModelStickDiagnostic
        |
        +-- countWithinDistanceCandidate
        +-- selectWithinDistanceCandidate
        +-- getDistancesToModelCandidate
        |
        +-- src/test_sac_model_stick.cpp correctness gates
        +-- src/bench_sac_model_stick.cpp timing and checksum
                 |
                 +-- script/generate_stick_board_evidence_manifest.py
                         |
                         +-- test-rvv/script/evidence_doctor.py
                         +-- test-rvv/script/evidence_registry.py
```

## 稳定入口

| 入口 | 路径 | 职责 |
| --- | --- | --- |
| topic Makefile | `Makefile` | 定义 test/bench binary、topic aliases、board repeated、manifest、doctor 和 registry target。 |
| board config | `board.mk` | 定义远端 binary 名称和 shared board runner。 |
| production source | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` | 三条 public entry、Standard fallback helper 和 RVV helper。 |
| test source | `src/test_sac_model_stick.cpp` | 11 个 correctness gate。 |
| bench source | `src/bench_sac_model_stick.cpp` | 统一输出 count/select/getDistances 三条 public entry 计时行。 |
| diagnostic helper | `include/impl/sac_model_stick_diagnostic.hpp` | 测试专用 candidate 和 scalar fallback。 |
| manifest script | `script/generate_stick_board_evidence_manifest.py` | 将 repeated board logs 转成 Evidence Doctor manifest。 |

当前没有独立聚合头文件；`include/impl/sac_model_stick_diagnostic.hpp` 被 test 和 bench 直接包含。该形态仍在当前配置的 `include/impl` 内部 helper 目录下，没有旧 `test_support/` 目录或 compatibility alias（兼容别名）。

## Fixtures 与输入构造

| 符号 | 位置 | 用途 |
| --- | --- | --- |
| `stickCoefficients` | test / bench source | 统一七维 stick 系数。count/select 把 3-5 当第二端点；getDistances candidate 必须按 public 语义把 3-5 当方向。 |
| `makeStickDistanceCloud` | test source | 用短 radial offset 列表构造可手工理解的 correctness 输入。 |
| `makeBenchCloud` | bench source | 构造 65536 点默认 bench 输入，混合内圈、外圈和远外圈。 |
| `indices` shuffle | test / bench source | 证明 direct indexed row source（直接索引行来源）和输出顺序。 |

## Candidate / Diagnostic Helper

| helper | fallback | RVV 条件 | 当前状态 |
| --- | --- | --- | --- |
| `countWithinDistanceCandidate` | 非 `__RVV10__` 或布局不匹配时走 scalar candidate。 | `RVVXYZAoSFloatLayout<PointT>` 且输入字节 offset 可用 u32。 | Phase 000 completed，partial-production-candidate。 |
| `selectWithinDistanceCandidate` | 非 RVV、布局不匹配或 `pcl::index_t` 非 32-bit signed 时走 scalar candidate。 | xyz AoS + 32-bit signed index。 | Phase 020 completed，partial-production-candidate。 |
| `getDistancesToModelCandidate` | 非 RVV 或布局不匹配时走 scalar candidate。 | `RVVXYZAoSFloatLayout<PointT>` 且输入字节 offset 可用 u32。 | Phase 040 completed，partial-production-candidate。 |

这些 helper 只在 `test-rvv` 中存在，不能被写成 production helper。

## Production Helper

| helper | fallback | RVV 条件 | 当前状态 |
| --- | --- | --- | --- |
| `countWithinDistanceRVV` | public entry 或 helper 内部条件不满足时走 `countWithinDistanceStandard`。 | `RVVXYZAoSFloatLayout<PointT>`、32-bit signed `pcl::index_t`、输入点云规模可用 u32 byte offset 表达。 | Phase 080 production-adopted。 |
| `selectWithinDistanceRVV` | public entry 或 helper 内部条件不满足时走 `selectWithinDistanceStandard`。 | 同上；同时保持 `inliers` 顺序和 `error_sqr_dists_` 写回。 | Phase 080 production-adopted。 |
| `getDistancesToModelRVV` | public entry 或 helper 内部条件不满足时走 `getDistancesToModelStandard`。 | 同上；保留 direction coefficient（方向系数）和 `radius_max_` penalty；Phase 100 后用 RVV mask / merge 和 `vse64.v` 直接写 double distances。 | Phase 100 production-adopted。 |

## Bench Harness 与 Case Registry

bench harness 用 `runTimed` 只计时入口调用，setup（点云、indices、系数构造）不在计时边界内。case registry 目前写在 `script/generate_stick_board_evidence_manifest.py` 的 `ITEM_METADATA` 中，字段包括 evidence role（证据角色）、boundary（边界）、wrapper（包装入口）、timer boundary（计时边界）、mask、reduction（规约）和 asm symbol（反汇编符号）。

## Scripts 与 Evidence Output

| 输出 | 生成方式 | 提交边界 |
| --- | --- | --- |
| repeated manifest | `generate_board_evidence_manifest` / `generate_production_board_evidence_manifest` / `generate_vector_writeback_board_evidence_manifest` | summary-only，可提交候选。 |
| Evidence Doctor Markdown / JSON | `run_repeated_board_evidence_doctor` / `run_production_board_evidence_doctor` / `run_vector_writeback_board_evidence_doctor` | summary-only，可提交候选。 |
| registry | `record_repeated_board_evidence_state` / `record_production_board_evidence_state` / `record_vector_writeback_board_evidence_state` | 可提交候选，用于 freshness 检查。 |
| raw board logs | `collect_repeated_board_evidence` / `collect_production_repeated_board_evidence` / `collect_vector_writeback_board_evidence` | local-only，默认不提交。 |
| asm full dump | `dump_bench_rvv` | build output，默认不提交；文档只引用符号和关键指令。 |

## Production 与 Test Support 边界

production 源码当前已有 stick RVV dispatch。test-only derived model 继续保留，用来回归 Phase 000 / 020 / 040 的 diagnostic candidate（诊断候选）语义；采纳结论以 Phase 080 和 Phase 100 的 public production direct 证据为准。测试专用 helper 不参与 runtime dispatch（运行时分流），不能替代 production helper 的 asm 或 board evidence。

## 拆分审计

当前 source layout（源码布局）为 `src/`、`include/impl/`、`script/`，符合 `.agents/config/defaults.yaml` 中的 topic-local source 和 internal helper 方向。没有旧 `test_support/` 目录、root-level 超长 `.cpp`、legacy pointer（旧路径指针）或 compatibility alias。后续如果进入 `090-stick-point-type-expansion`，可能需要把 `include/impl/sac_model_stick_diagnostic.hpp` 再按 reference / candidate / assertion 职责拆分；当前 Phase 080 不把它作为阻塞。
