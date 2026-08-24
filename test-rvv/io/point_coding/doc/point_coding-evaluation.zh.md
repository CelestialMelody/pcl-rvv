# point_coding 函数级评估

## 范围和目标源码

目标源码是 `io/include/pcl/compression/point_coding.h` 中的 `pcl::octree::PointCoding<PointT>`。当前 production patch（生产补丁）已经覆盖两条已采纳生产行为：Phase 060 的 `PointCoding<PointXYZ>::decodePoints`，以及 Phase 070 的 traits-gated PointXYZ-like decode path。接入后 production-direct（真实生产路径直连）板卡证据均为正向；用户已确认可以按 board 收益采纳。Phase 080 已补完整 `OctreePointCloudCompression` public entry（公开入口）审计，结果只有 near-threshold weak signal，不支持稳定 public entry 加速结论。

## 函数作用速览

| 函数 / 状态 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `setPrecision` | 设置坐标压缩分辨率。 | 写入 `pointCompressionResolution_`。 | 每个编码器状态配置。 | 非热点，保持标量。 |
| `setPointCount` | 为 diff vector 预留容量。 | 调用 `reserve(pointCount * 3)`。 | 避免编码时反复扩容。 | 非公式热点，保持标量。 |
| `initializeEncoding` | 清空 diff vector。 | 清空 `pointDiffDataVector_`。 | 每轮编码前执行。 | 保持标量。 |
| `initializeDecoding` | 重置 diff vector iterator。 | 设置 `pointDiffDataVectorIterator_`。 | 每轮解码前执行。 | 保持标量。 |
| `encodePoints` | 按 leaf 内 indices 读取点坐标，计算相对 reference point 的量化 diff，并写入 `x/y/z` 三个 byte。 | 输入 `Indices`、`referencePoint_arg`、`PointCloudConstPtr`；输出 `pointDiffDataVector_`。 | octree coding family 中最直接的坐标公式候选。 | 默认 gather + scalar same-chain quantize 局部正向；f64 exact quantize 正确但性能拒绝。 |
| `decodePoints` | 从 diff vector 顺序取三 byte，按 reference point 和 resolution 还原 `x/y/z`。 | 输入 `pointDiffDataVectorIterator_`、输出 cloud range。 | 解码侧局部公式。 | Phase 060 exact `PointXYZ` 生产证据与 Phase 070 traits-gated 代表点型证据都支持采纳。 |

## 标量路径

`encodePoints` 对每个 leaf 内 index 做三步：

1. 从 `inputCloud_arg` 按 index 取 `PointT`。
2. 对 `x/y/z` 分别计算 `(point.coord - referencePoint_arg[k]) / pointCompressionResolution_`，再 `static_cast<int>` 截断。
3. 将整数 clamp（截断）到 `[-127, 127]`，再转成 `unsigned char` 并以 `x,y,z` 顺序 `push_back` 到 `pointDiffDataVector_`。

这里的一个重要语义是：`PointT::x/y/z` 通常是 `float`，`referencePoint_arg` 是 `double*`，表达式会先走 double 运算，再除以 float resolution 提升后的 double。完整 f32 RVV 公式不能自然复刻这个链路，边界样本会出现 1 个量化单位差异。

`decodePoints` 对每个输出点顺序读取三 byte，按 `reference + (diff + 0.5) * resolution` 还原坐标并写回 `PointT::x/y/z`。当前 production 代码把 `char` 通过 `unsigned char` 解释，测试支撑也保留这个 byte 语义。

## 当前 RVV 诊断设计

| 候选 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| encode indexed gather + scalar same-chain quantize | attempted / diagnostic-positive。RVV 侧使用 indexed gather 读取 `x/y/z`，随后把 lane（向量通道）落到临时数组，并调用同一个标量量化 helper。 | correctness、asm、board repeated summary 均已形成；当前 5-run 为 Errors=0，Warnings=6。 | 这只说明 gather 和 chunk 组织有局部收益线索；量化仍是标量 same-chain（同构链路）。`encode_indexed_16` 有 1/5 退化，tiny leaf 不能外推。 |
| full f32 RVV encode quantize | attempted / rejected for Phase 000。 | correctness 边界样本暴露量化差 1 的风险。 | 只有获得可证明误差合同后才可重开。 |
| f64 exact RVV encode quantize | attempted / rejected for Phase 020 default path。 | `make run_test_rvv POINT_CODING_ENABLE_F64_QUANTIZE=1` 通过；probe board 中 `encode_indexed_16` median 0.64x 且 5/5 退化。 | 正确但太重；保留显式 probe，不作为默认 candidate 或 production-shaped candidate。 |
| contiguous decode RVV | attempted / weak-positive diagnostic。RVV 侧使用 `vlse8` 跨步加载 diff byte，扩展到 float 后 `vsse32` 写回 AoS 字段。 | correctness、asm、board repeated summary 均已形成；Phase 040 decode-only 10-run 为 `Errors=0，Warnings=1`。 | 只覆盖连续输出段；当前 summary 正向但仍不是 decoder iterator production evidence。 |
| decode context RVV | attempted / weak-positive production-shaped diagnostic。Std side 使用真实 `PointCoding<PointXYZ>` object state；RVV side 使用 test helper 写同形状 cloud range。 | correctness、asm、Phase 050 board repeated summary 已形成；`Errors=0，Warnings=7`。 | 有 1/10 退化和 long-tail warning，不能单独作为 production adoption evidence（生产采纳证据）。 |
| decode multileaf context RVV | attempted / weak-positive production-shaped diagnostic。Std side 使用真实 `PointCoding<PointXYZ>` object state 按多 leaf sequence 多次调用 `decodePoints`；RVV side 使用 test helper 写同一 output cloud。 | correctness、asm、Phase 055 board repeated summary 已形成；`Errors=0，Warnings=3`。 | 多 leaf context 未吞掉所有收益，但小规模退化和长尾 warning 仍不能替代 production-direct 证据。 |
| public octree roundtrip feasibility | completed feasibility smoke。Std / RVV build 均调用真实 `OctreePointCloudCompression<PointXYZ>` public encode/decode 往返。 | `make run_test_compare` 已通过。 | Phase 056 只证明入口可构造；Phase 080 另补接入后的 public timing。 |
| production-direct decode RVV | adopted production behavior。Std / RVV build 均调用真实 `PointCoding<PointXYZ>::decodePoints`，RVV build 在 exact gate 下走 f64 vector decode。 | correctness、asm、Phase 060 board repeated summary 已形成；`Errors=0，Warnings=1`。 | Phase 060 的批准范围是 exact `PointXYZ` 和 point coder decode stream；Phase 070 另行批准 traits-gated representative point types。 |
| traits-gated production-direct decode RVV | adopted production behavior。Std / RVV build 均调用真实 `PointCoding<PointT>::decodePoints`，RVV build 在 traits gate 下对 `PointXYZI` / `PointXYZRGB` 走 f64 vector decode。 | correctness、asm、Phase 070 board repeated summary 已形成；`Errors=0，Warnings=2`。 | 只覆盖 layout-compatible 代表点型，不覆盖全部自定义点型或完整 public end-to-end。 |
| public octree decode / roundtrip timing | attempted / weak-near-threshold production-public evidence。Std / RVV build 均调用真实 `OctreePointCloudCompression<PointXYZ>` public entry。 | Phase 080 board repeated summary 已形成；`Errors=1，Warnings=2，Suggestions=4`。 | 只说明完整 public boundary 中收益被稀释；不推翻 Phase 060/070 direct patch，也不支持继续自动改 production。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `PointCoding::encodePoints` | production scalar helper | 当前真实标量坐标差分编码。 | octree compression 编排层。 | `pointDiffDataVector_`。 | production source truth（生产源码事实），未被修改。 | `io/include/pcl/compression/point_coding.h` |
| `PointCoding::decodePoints` | production dispatch | 当前真实坐标还原；RVV build 且 traits gate 命中时走 RVV helper，其它路径走 `decodePointsStd`。 | octree decompression 编排层。 | 输出 point cloud range。 | adopted production behavior。 | `io/include/pcl/compression/point_coding.h` |
| `include/point_coding.h` | test aggregator（测试聚合入口） | 给 test / bench 暴露稳定 include。 | `src/test_point_coding.cpp`、`src/bench_point_coding.cpp`。 | `include/impl/point_coding_support.hpp`。 | 测试支撑入口。 | `test-rvv/io/point_coding/include/point_coding.h` |
| `encodePointsScalar` / `decodePointsScalar` | diagnostic reference（诊断参考链路） | 复刻 production 标量公式。 | correctness test、bench Std build。 | checksum 和 candidate 对拍。 | correctness baseline（正确性基线）。 | `test-rvv/io/point_coding/include/impl/point_coding_support.hpp` |
| `encodePointsCandidate` / `decodePointsCandidate` | candidate helper（候选 helper） | Std build 回退标量，RVV build 走测试专用候选。 | correctness test、bench RVV build。 | board repeated summary、asm。 | component ablation candidate。 | `test-rvv/io/point_coding/include/impl/point_coding_support.hpp` |
| `src/test_point_coding.cpp` | correctness gate（正确性验收） | 对 encode、clamp、decode、traits gate 和 public smoke 做 Std/RVV 对拍。 | `make run_test_compare`。 | QEMU logs。 | correctness evidence。 | `test-rvv/io/point_coding/src/test_point_coding.cpp` |
| `src/bench_point_coding.cpp` | bench wrapper（性能测试包装） | 构造 synthetic leaf case 并输出 checksum / timing。 | QEMU smoke、board bench。 | analyze script、Evidence Doctor manifest。 | diagnostic board evidence。 | `test-rvv/io/point_coding/src/bench_point_coding.cpp` |
| `generate_point_coding_evidence_manifest.py` | analysis script（分析脚本） | 把 board log 转成 Evidence Doctor manifest。 | `make run_board_repeated_evidence_doctor`。 | 全局 Evidence Doctor。 | evidence validation input。 | `test-rvv/io/point_coding/script/generate_point_coding_evidence_manifest.py` |
| repeated board summary | evidence output summary（证据摘要） | 保存 5-run speedup 表。 | board repeated target。 | phase result、evaluation。 | board performance diagnostic。 | `test-rvv/io/point_coding/log/board/repeated_phase000/summary.md` |
| Evidence Doctor report | evidence output summary | 暴露异常信号和降级边界。 | manifest wrapper + global doctor。 | phase result、Handoff。 | warning / risk boundary。 | `test-rvv/io/point_coding/log/board/repeated_phase000/evidence_doctor.md` |
| decode-only repeated summary | evidence output summary | 保存 Phase 040 decode-only 10-run speedup 表。 | board repeated target with `decode_contiguous_*` filter。 | Phase 040 result、roadmap。 | decode stability diagnostic。 | `test-rvv/io/point_coding/log/board/repeated_phase040_decode/summary.md` |
| decode-context repeated summary | evidence output summary | 保存 Phase 050 production-shaped diagnostic 10-run speedup 表。 | board repeated target with `decode_context_*` filter。 | Phase 050 result、roadmap。 | production-shaped diagnostic board evidence。 | `test-rvv/io/point_coding/log/board/repeated_phase050_decode_context/summary.md` |
| decode-multileaf repeated summary | evidence output summary | 保存 Phase 055 production-shaped diagnostic 10-run speedup 表。 | board repeated target with `decode_multileaf_*` filter。 | Phase 055 result、roadmap。 | production-shaped diagnostic board evidence。 | `test-rvv/io/point_coding/log/board/repeated_phase055_decode_multileaf/summary.md` |
| `runPublicOctreeRoundtrip` | public API feasibility helper | 调用真实 `OctreePointCloudCompression<PointXYZ>` public encode/decode 往返并生成 smoke 摘要。 | `PointCodingPublicRoundtripFeasibility.RoundtripSmokeProducesFiniteOutput`。 | Phase 056 result、correctness tests。 | public entry feasibility；not RVV performance evidence。 | `test-rvv/io/point_coding/include/impl/point_coding_support.hpp` |
| `decode_production_direct_*` | production-direct bench cases | Std/RVV 两侧调用真实 `PointCoding<PointXYZ>::decodePoints`。 | board repeated target。 | Phase 060 summary / doctor。 | production-direct performance evidence。 | `test-rvv/io/point_coding/src/bench_point_coding.cpp` |
| `decode_production_direct_traits_*` | production-direct traits bench cases | Std/RVV 两侧调用真实 `PointCoding<PointT>::decodePoints`，PointT 为 `PointXYZI` / `PointXYZRGB`。 | board repeated target。 | Phase 070 summary / doctor。 | traits-gated production-direct performance evidence。 | `test-rvv/io/point_coding/src/bench_point_coding.cpp` |

## 验证结果

| 证据层 | 命令 / 路径 | 结果 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness | `make run_test_compare` | Std / RVV 各 11 个 test 通过，包含 Phase 050 object-state decode、Phase 055 multi-leaf decode、Phase 056 public roundtrip smoke、Phase 060 production-direct、Phase 070 traits-gated extra-field preservation 和 non-compatible fallback。 | 不证明 public entry 性能收益或完整 end-to-end 稀释情况。 |
| QEMU smoke | `make run_qemu_bench_smoke` | RVV bench 小 case 可运行并产生日志。 | QEMU timing 不作为性能结论。 |
| asm attribution（反汇编归属） | `make dump_bench_rvv`，`build/asm/riscv/bench_point_coding_rvv.asm` | 可见 encode 的 `vluxseg3ei32.v`，decode 的 `vlse8.v` / `vsse32.v`，traits-gated 紧凑 xyz store 由 `vssseg3e32.v` 归属。 | 只能说明候选路径含 RVV 指令，不能证明完整 public hot symbol。 |
| board correctness | `make run_board_test` | 板卡 RVV test 通过。 | 不覆盖生产入口。 |
| board performance | `make collect_board_repeated POINT_CODING_REPEATED_RUNS=5` | 当前默认路径 12-case summary 中 encode median 1.17x 到 2.14x；decode median 1.18x 到 1.29x。 | 只覆盖 synthetic component case，不覆盖真实 leaf distribution。 |
| decode stability board performance | `make collect_board_repeated run_board_repeated_evidence_doctor POINT_CODING_REPEATED_RUNS=10 POINT_CODING_REPEATED_DIR=log/board/repeated_phase040_decode BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_contiguous_*'` | Phase 040 decode-only median 为 1.15x 到 1.29x，Errors=0，Warnings=1。 | 不覆盖真实 decoder iterator 或完整 decompression pipeline。 |
| decode context board performance | `make collect_board_repeated run_board_repeated_evidence_doctor POINT_CODING_REPEATED_RUNS=10 POINT_CODING_REPEATED_DIR=log/board/repeated_phase050_decode_context BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_context_*'` | Phase 050 context median 为 1.21x 到 1.40x，Errors=0，Warnings=7。 | 不覆盖 production dispatch；1024/4096 退化 warning 未解释。 |
| decode multileaf board performance | `make collect_board_repeated run_board_repeated_evidence_doctor POINT_CODING_REPEATED_RUNS=10 POINT_CODING_REPEATED_DIR=log/board/repeated_phase055_decode_multileaf BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_multileaf_*'` | Phase 055 multileaf median 为 1.18x 到 1.28x，Errors=0，Warnings=3。 | 不覆盖 tree traversal、entropy decoding、stream input 或 production dispatch。 |
| production direct correctness | `make run_test_compare` | Phase 070 后 Std / RVV 各 11 个 gtest 通过，包含 double reference rounding 对抗样本和 non-compatible fallback。 | 不证明所有点型的 RVV gate 或完整 public end-to-end 性能。 |
| production direct board performance | `make collect_board_repeated run_board_repeated_evidence_doctor POINT_CODING_REPEATED_RUNS=10 POINT_CODING_REPEATED_DIR=log/board/repeated_phase060_production_decode BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_production_direct_*'` | Phase 060 production-direct median 为 1.22x / 1.23x / 1.18x / 1.16x，Errors=0，Warnings=1。 | 不覆盖完整 octree traversal 或 public stream timing。 |
| traits production direct correctness | `make run_test_compare` | Phase 070 后 Std / RVV 各 11 个 gtest 通过，包含 `PointXYZI` / `PointXYZRGB` extra-field preservation 和 double xyz fallback。 | 不证明全部自定义点型。 |
| traits production direct board performance | `make collect_board_repeated run_board_repeated_evidence_doctor POINT_CODING_REPEATED_RUNS=10 POINT_CODING_REPEATED_DIR=log/board/repeated_phase070_traits_decode BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_production_direct_traits_*'` | Phase 070 traits-direct median 为 `1.27x / 1.20x / 1.27x / 1.19x`，Errors=0，Warnings=2。 | 不覆盖所有自定义点型、非标准布局或完整 public stream timing。 |
| public octree board performance | `make collect_board_repeated run_board_repeated_evidence_doctor POINT_CODING_REPEATED_RUNS=10 POINT_CODING_REPEATED_DIR=log/board/repeated_phase080_public_octree BENCH_ARGS='--iterations 10 --warmup-iterations 2 --case-filter octree_*'` | Phase 080 public decode / roundtrip median 为 `1.01x / 1.01x / 1.01x / 1.02x`，Errors=1，Warnings=2，Suggestions=4。 | 不作为稳定 public-positive evidence；只说明完整公开链路稀释 direct helper 收益。 |
| Evidence Doctor | `make run_board_repeated_evidence_doctor` | 当前默认路径 Errors=0，Warnings=6，Suggestions=0；`encode_indexed_16` 有 1/5 退化和 long-tail。Phase 040 decode-only doctor 是 Errors=0，Warnings=1；Phase 050 decode context doctor 是 Errors=0，Warnings=7；Phase 055 multileaf doctor 是 Errors=0，Warnings=3；Phase 060 production-direct doctor 是 Errors=0，Warnings=1；Phase 070 traits doctor 是 Errors=0，Warnings=2；Phase 080 public doctor 是 Errors=1，Warnings=2，Suggestions=4。 | 旧 diagnostic warning 不能替代 production evidence；Phase 060 / 070 warning 已解释并保留 min/median/max 边界。Phase 080 Error/Warning 阻止把 public median 写成稳定收益。 |

## 诊断证据链

本轮证据支持：`PointCoding` 的 leaf-level encode gather 和 decode byte-to-float helper 存在局部 RVV 收益线索；Phase 060 进一步证明 exact `PointXYZ` production `decodePoints` 在同 production boundary 下有板卡收益；Phase 070 再证明 traits-gated PointXYZ-like decode 在代表点型上仍有板卡收益，并且额外字段保持正确。

本轮证据不支持：声明泛型 `PointCoding<PointT>` 已经对所有点型优化，或声明 `OctreePointCloudCompression` 完整 public entry 已经有稳定生产收益。Phase 080 已把公开 decode / roundtrip 纳入接入后板卡审计，但 median 只有 `1.01x..1.02x` 且有退化频率 Error / Warning；真实文件流、其它 compression profile、真实 leaf size 分布和所有点型仍未覆盖。

## 生产接入判断

当前 EvidenceDecision 是 `adopted production behavior`，范围为 exact `PointXYZ` decode production patch 加上 Phase 070 traits-gated PointXYZ-like decode patch。正式长期文档为 `doc-rvv/io/point_coding-RVV.zh.md`。

保留限制：

- encode 的 full f32 RVV 量化存在 double 语义风险；f64 exact 量化已证明正确但性能不可采纳。
- `PointXYZ` exact gate 不能外推为所有点型结论。
- Phase 080 已测 public entry 稀释，结果为 weak / near-threshold；真实文件流、其它 compression profile 和真实 leaf distribution 仍未覆盖。
- traits-gated PointXYZ-like decode 仍只覆盖 layout-compatible 代表点型，后续若继续需要独立 phase 证明其它点型的 layout、fallback、asm、board 和 Evidence Doctor。

当前 exact `PointXYZ` / traits-gated representative decode closeout 已完成，Phase 080 也已完成 public boundary audit。当前不建议在本 topic 内继续自动扩大 production patch；重新推进需要真实 workload / profile 指向 point coder decode 是公开链路主成本，或有明确的新点型 / profile 需求。当前 adopted patch 可等待 review / commit phase（提交阶段）授权。
