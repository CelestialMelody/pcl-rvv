# Phase 050 Plan: decode implementation-shape audit

## 阶段意图和边界

本阶段审计 phase 020 中 `ps_decode_points_leaf4096` 的 Evidence Doctor Error。目标不是把 `decodePoints` 直接纳入 production candidate（生产候选），而是在 topic-local test support（测试支撑）内验证一个 implementation-shape hypothesis（实现形态假设）：

> 当前 production-shaped decode（生产形态 decode）不稳定，可能与 RVV `vsse32` strided store（跨步存储）直接写回 `pcl::PointXYZRGBA` AoS（结构数组）有关。若先把 RVV 计算结果连续写入 scratch buffer（暂存缓冲区），再标量写回 AoS，可能更稳定或更容易解释。

本阶段允许修改：

- `test-rvv/io/color_coding/include/impl/color_coding_support.hpp`
- `test-rvv/io/color_coding/src/test_color_coding.cpp`
- `test-rvv/io/color_coding/src/bench_color_coding.cpp`
- `test-rvv/io/color_coding/script/generate_color_coding_evidence_manifest.py`
- `test-rvv/io/color_coding/Makefile`
- `test-rvv/io/color_coding/doc/**`
- `tmp/rvv-work-logs/io/color_coding/current-handoff/**`

本阶段禁止修改 `io/include/pcl/compression/color_coding.h`，不创建 `doc-rvv/io/color_coding-RVV.zh.md`，不改变 phase 030 PI1 encode/default production patch scope。

## 当前状态清单

| item | current state |
| --- | --- |
| current decision | `partial-production-candidate`：encode/default 可进入 PI1；decode excluded。 |
| blocking decode evidence | `ps_decode_points_leaf4096`：median 约 1.00x，min 0.82x，2/5 below 1，Doctor Error。 |
| production source | 未修改，继续只读。 |
| doc suite | phase 040 adopted。 |
| board availability | 用户声明板卡可用；本阶段如新增 bench evidence，必须跑 board repeated、manifest、Doctor、registry。 |

## Root Cause Investigation（根因调查）状态

| debugging step | evidence / hypothesis |
| --- | --- |
| read error | Doctor Error 是 `ba_degradation_frequency — ps_decode_points_leaf4096`，不是 correctness failure。 |
| reproduce | phase 020 5-run board summary 已复现：2/5 below 1。 |
| recent change | phase 020 新增 production-shaped point-vector decode helper；Error 只在 large PCL point-vector case 上阻塞 production candidate。 |
| current code shape | `decodePointsCandidatePointVector` 用 `vlse8` 读取 diff RGB stream，再用 `vsse32` 跨步写 AoS RGBA 字段。 |
| hypothesis | strided AoS store 可能引入不稳定或成本拐点；staged contiguous store 可以隔离这一变量。 |

## 本阶段候选族

| candidate family | code shape | expected evidence |
| --- | --- | --- |
| existing decode candidate | `vlse8` diff load + pack + `vsse32` AoS store | 保留 phase 020 baseline，继续作为 blocked candidate。 |
| staged-store decode candidate | `vlse8` diff load + pack + contiguous `vse32` scratch store + scalar AoS store | 若稳定正向，说明 direct `vsse32` store 可能是风险点；若仍不稳定，decode 候选继续排除。 |

## TDD 计划

1. RED：新增 gtest，调用尚不存在的 `decodePointsCandidatePointVectorStagedStore`，要求输出与 reference 一致。预期 RVV build 编译失败或符号缺失。
2. GREEN：在 `include/impl/color_coding_support.hpp` 新增 test-only staged-store helper，非 RVV 构建回退 reference。
3. 扩展 bench：新增 `ps_decode_points_staged_leaf257` / `ps_decode_points_staged_leaf4096` label。
4. 更新 manifest wrapper：把 staged label 标成 `decodePointsCandidatePointVectorStagedStore`，asm boundary 标成 staged contiguous store。

## 优化矩阵

| candidate family | row source policy | point type / layout | correctness | bench | board | asm | Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| existing production-shaped decode | contiguous output range | `pcl::PointXYZRGBA` AoS RGBA | existing pass | `ps_decode_points_leaf257/4096` | phase 020 Error on 4096 | candidate binary has RVV stores | Error=1 | rejected/deferred for production |
| staged-store production-shaped decode | contiguous output range | `pcl::PointXYZRGBA` AoS RGBA + scratch `uint32_t` | new gtest | new staged labels | phase 050 repeated required | expect `vse32` scratch and scalar AoS store | run Doctor | planned |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` and implementation-shape diagnostic。 |
| A/B boundary | production-shaped helper, not public overload and not production detail helper。 |
| 当前决策问题 | implementation-shape and RVV-vs-scalar diagnostic；不是 clean adoption。 |
| diagnostic 是否可外推到 production | unknown；只能说明 staged-store shape 是否值得后续 bounded production probe。 |
| comparison-boundary / baseline mismatch 风险 | yes；bench 不含 `OctreePointCloudCompression` public entry、entropy coder、真实 leaf distribution 和 production dispatch。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no for decode unless staged variant removes Doctor Error and user later expands production patch scope。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；本阶段最多生成 explicit probe candidate。 |

## 板卡复跑预算和决策桶

本阶段若 staged labels 通过 QEMU correctness 和 smoke，就运行一次 `run_board_color_coding_repeated`，run count 仍为 5，warmup 3，iterations 20。预算内不追加第二次复跑，除非脚本失败或输出缺失。

Decision bucket:

- `positive`: staged `ps_decode_points_staged_leaf4096` median >= 1.08x，min >= 1.0，Doctor 无 staged-specific Error。
- `weak_positive`: median > 1.03x，min >= 0.95，最多 Warning，不作为 production candidate clean pass。
- `neutral`: median 0.98x-1.03x 或接近阈值。
- `negative`: median < 0.98x 或多数 runs below 1。
- `unstable`: Error / high degradation frequency / long tail 无法解释。

## Evidence Doctor 和 registry 规则

本阶段新增 run label `board-color-coding-component-repeat-phase050`，doc refs 指向：

- `doc/phases/050-decode-implementation-shape-audit/result.zh.md`
- `doc/color_coding-evaluation.zh.md`

若 repeated target 覆盖 `component_repeat_5` summary / manifest / Doctor / registry，phase 020 数值降级为 historical evidence；evaluation、benchmark/evidence、optimization evidence、roadmap、matrix 和 Handoff 必须同步 current truth。

## 验证命令

```bash
make -C test-rvv/io/color_coding run_test_rvv
make -C test-rvv/io/color_coding run_test_compare
make -C test-rvv/io/color_coding run_bench_rvv BENCH_ARGS="--case-filter ps_decode_points_staged_leaf4096 --iterations 2 --warmup-iterations 1 --batch-repeats 2"
make -C test-rvv/io/color_coding dump_bench_rvv
make -C test-rvv/io/color_coding run_board_color_coding_repeated
make -C test-rvv/io/color_coding check_evidence_freshness
git diff --check -- test-rvv/io/color_coding tmp/rvv-work-logs/io/color_coding
```

QEMU bench 只用于 log-shape smoke（日志形状小测），不写性能结论。

## 继续 / 停止条件

- 如果 staged decode 仍触发 Error / unstable，decode 继续排除；下一步回到 PI2 encode/default production authorization boundary。
- 如果 staged decode 稳定正向，它只能成为 `bounded decode production probe candidate`；是否扩大 PI2 scope 到 decode 仍需要用户明确授权。
- 如果板卡、工具链或 registry 失败且无法修复，写 `turn_stop_deferred with stop_condition_hit`。
