# Phase 055：full-octree-shaped decode context scout 计划

## 阶段意图和边界

本阶段只在 `test-rvv/io/point_coding/**` 内推进 test-only scout（测试专用侦察），不修改 `io/include/pcl/compression/point_coding.h`。目标是把 Phase 050 的单 leaf object-state decode 扩展到多 leaf sequence（多叶节点序列）：Std side 使用真实 `pcl::octree::PointCoding<PointXYZ>` 对象保存完整 diff vector、重置 iterator，并按 leaf point count 多次调用 `decodePoints`；RVV side 使用测试专用 helper 按相同 leaf sequence 写同一段 output cloud。

本阶段仍不证明真实 `OctreePointCloudCompression` public entry（公开入口）或 entropy decoding（熵解码）已经变快，也不覆盖 production dispatch（生产分流）。它只回答：在 point-count staging（点数暂存）、多 reference point（参考点）和多次 leaf decode 调用进入计时边界后，Phase 050 的 decode 正向信号是否被稀释或放大。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| production | `io/include/pcl/compression/point_coding.h` 未修改；真实 decode 入口仍是标量 `PointCoding::decodePoints`。 |
| correctness | `make run_test_compare` 已通过，Std/RVV 两侧各 4 个 gtest。 |
| QEMU / asm | `make run_qemu_bench_smoke` 和 `make dump_bench_rvv` 已通过；bench asm 可见 `vlse8.v`、`vsse32.v`、`vfcvt.f.xu.v`。 |
| board evidence | Phase 050 `decode_context_*` 10-run median 全部大于 1，但 Evidence Doctor 为 `Errors=0，Warnings=7`，1024/4096 各有 1/10 退化。 |
| docs / matrix | roadmap 默认恢复队列指向 `055-full-octree-context-scout`；production integration 仍被用户授权和 instability warning 阻塞。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness target | bench / board target | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| decode multi-leaf context RVV | multi-leaf point-count sequence | `PointXYZ` / float output / AoS / multiple begin offsets | production-shaped diagnostic helper | `make run_test_compare` 新增 multi-leaf gtest | `decode_multileaf_*`，10-run repeated board + Evidence Doctor | `vlse8.v` / `vsse32.v` | required | planned |

## 实现和测试动作

1. 在 `include/impl/point_coding_support.hpp` 增加 multi-leaf fixture、真实 `PointCoding` object-state reference 和 RVV candidate helper。helper 必须说明它只模拟 `deserializeTreeCallback` 中 point coder 的多 leaf 调用，不包含 tree traversal 或 entropy context。
2. 在 `src/test_point_coding.cpp` 增加 gtest，验证 sentinel padding、leaf begin/end offset 和多 reference 输出都与真实 `PointCoding` 对象一致。
3. 在 `src/bench_point_coding.cpp` 增加 `decode_multileaf_256/1024/4096/16384`。case 名中的规模表示总 decoded point count；每个 case 由多个 leaf size 组成，避免单一大 leaf 掩盖 per-leaf 调用开销。
4. 更新 topic-local manifest metadata、docs、optimization roadmap 和 matrix，保持 evidence role 为 `production_shaped_diagnostic`。
5. 运行 `make run_test_compare`、`make run_qemu_bench_smoke`、`make dump_bench_rvv`。本地验证通过后，在板卡上运行 10-run repeated board：

```bash
make collect_board_repeated run_board_repeated_evidence_doctor \
  POINT_CODING_REPEATED_RUNS=10 \
  POINT_CODING_REPEATED_DIR=log/board/repeated_phase055_decode_multileaf \
  BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_multileaf_*'
```

## Evidence Doctor 和 registry 规则

Evidence Doctor 输入来自 `script/generate_point_coding_evidence_manifest.py` 生成的 repeated manifest。若出现 checksum mismatch、strict A/B 缺 side、metadata 缺 evidence role 或 `B/A < 1` 高频退化，不能关闭本阶段；应修复、重跑或把证据降级为 unstable diagnostic。当前 topic 仍无 `log/evidence_registry.json`，本阶段 result 需要记录人工 freshness scan。

## 板卡复跑预算和决策桶

复跑预算为 10-run repeated board，不自动追加第二批。decision bucket：

- `positive diagnostic`：所有 case median 大于 1.20x，且无 `B/A < 1`。
- `weak-positive diagnostic`：median 大于 1，但存在 long-tail 或少量退化。
- `neutral / negative`：多数 case median 接近 1 或小于 1。
- `unstable`：median 正向但退化频率或 long-tail 足以阻止生产判断。

预算用完后若 bucket 仍摇摆，写成 unstable，不继续无限复跑。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic；仍是 test-only。 |
| A/B boundary | Std side 是真实 `PointCoding<PointXYZ>` 多 leaf decode object state；RVV side 是 test helper。 |
| 当前决策问题 | decode RVV 在多 leaf point coder context 中是否仍值得保留为后续 full octree / PI1 输入。 |
| diagnostic 是否可外推到 production | no。它不覆盖真实 tree traversal、entropy decoding、stream input 或 production dispatch。 |
| comparison-boundary / baseline mismatch 风险 | yes。Std/RVV 不是同一个 production detail helper，且 RVV helper 直接按 byte offset 走多 leaf。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 默认不允许；只有 multi-leaf context 稳定正向且用户明确授权 PI1 时才恢复。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes。当前无 production patch。 |

## 继续 / 停止条件

本阶段完成后，若 `decode_multileaf_*` 仍 weak-positive 且有 warning，则继续保持 no-production，并把下一步降级到 trace / full public context 可行性审计。若结果稳定 positive，也只能建议用户选择是否授权 PI1 production integration plan；worker 不自动修改 production。若板卡不可达、Evidence Doctor Error 无法修复或 dirty isolation 不安全，则停在 blocked handoff。
