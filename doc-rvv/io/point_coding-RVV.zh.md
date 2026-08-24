# point_coding RVV 已采纳生产说明

## 当前状态

`io/include/pcl/compression/point_coding.h` 已接入一条窄范围 RVV production patch（生产补丁）：
`pcl::octree::PointCoding<PointT>::decodePoints` 在 RVV 构建下命中兼容 xyz AoS 点型时，使用 RVV decode helper；其它模板实例和非 RVV 构建继续走原标量路径。

当前已采纳的 production behavior（生产行为）分两段：

| phase | 代表点型 | board summary | decision |
| --- | --- | --- | --- |
| Phase 060 | exact `PointXYZ` | 10-run median `1.22x / 1.23x / 1.18x / 1.16x`，min 全部大于 `1.0x` | adopted |
| Phase 070 | traits-gated `PointXYZI` / `PointXYZRGB` | 10-run median `1.27x / 1.20x / 1.27x / 1.19x`，min 全部大于 `1.0x` | adopted |

Evidence Doctor（证据体检）分别为：

- Phase 060：`Errors=0, Warnings=1, Suggestions=0`
- Phase 070：`Errors=0, Warnings=2, Suggestions=0`

接入后又执行了 Phase 080 public boundary audit（公开入口边界审计）。`OctreePointCloudCompression<PointXYZ>`
公开 decode / roundtrip 10-run median 只有 `1.01x / 1.01x / 1.01x / 1.02x`，Evidence Doctor 为
`Errors=1, Warnings=2, Suggestions=4`。这不推翻 Phase 060/070 的 production-direct 采纳，但说明完整公开链路会明显稀释 point coder decode 的局部收益，不能写成稳定公开入口加速。

本文只记录已采纳生产行为和接入后证据链。Phase 040/050/055 的 diagnostic（诊断）与 production-shaped diagnostic（生产形态诊断）数据只作为历史进入生产接入闭环的依据，不作为本文的最终性能数字来源。

## 覆盖范围

| item | current boundary |
| --- | --- |
| production entry | `pcl::octree::PointCoding<PointT>::decodePoints` |
| optimized operation | 从 `pointDiffDataVector_` 顺序读取 `x/y/z` diff byte，并写回输出 cloud 的 `x/y/z` |
| point type | exact `pcl::PointXYZ`，以及 traits-gated 代表点型 `pcl::PointXYZI` / `pcl::PointXYZRGB` |
| scalar semantics | `referencePoint_arg` 是 `double*`，`pointCompressionResolution_` 提升到 double 参与公式，最后写回 float |
| input layout | interleaved diff byte stream，顺序为 `x, y, z, x, y, z, ...` |
| output layout | AoS（结构数组）点型，以 traits offset / stride 写 `x/y/z` 字段 |
| RVV compile gate | `__RVV10__` |
| fallback | 非 RVV 构建、非 traits-compatible 模板实例和 encode path 都走标量 helper |

不覆盖泛型 `PointT` 的所有可能布局、indices / correspondences 路径、entropy coding（熵编码）或 encode path。
完整 `OctreePointCloudCompression` public end-to-end timing（公开入口端到端计时）已在 Phase 080 作为接入后审计测量；
结果为 weak / near-threshold，不属于当前采纳范围。

## 函数语义

`PointCoding` 是 octree compression（八叉树压缩）里的坐标差分编码器。编码时，`encodePoints`
按 leaf 内 indices 读取输入 cloud 中的点，把每个点相对 `referencePoint_arg` 的 `x/y/z` 差值量化成三个 byte，
写入 `pointDiffDataVector_`。本次不优化 encode，因为完整 f32 RVV 量化在整数边界附近不能自然复刻 double
reference 语义；f64 exact 量化 correctness（正确性）成立，但板卡上成本过高。

解码时，`decodePoints` 从 `pointDiffDataVectorIterator_` 连续取三个 byte，按下面公式还原坐标并写回输出 cloud：

```text
point.x = float(reference[0] + (diffX + 0.5) * resolution)
point.y = float(reference[1] + (diffY + 0.5) * resolution)
point.z = float(reference[2] + (diffZ + 0.5) * resolution)
```

这里 `reference[k]` 是 double，`resolution` 在表达式中按 double 参与运算。RVV patch 保留这条语义链：
先把 unsigned diff byte 扩展到 double 向量，完成 `+0.5`、乘 resolution 和加 reference，最后窄化成 float 写回。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 / 边界 |
| --- | --- | --- | --- |
| dispatch（分流逻辑） | adopted | `decodePoints` 先计算 point count；RVV 构建且 `PointT` 兼容 xyz AoS traits 时调用 `decodePointsRVV`，否则调用 `decodePointsStd`。 | production diff、Std/RVV build correctness、traits gate 审计。 |
| Std helper | adopted | 原标量循环抽到 `decodePointsStd`，让 fallback 边界可审查。 | Std/RVV correctness。 |
| RVV diff byte load | adopted | diff stream 是 `x/y/z` 交错 byte，使用 `vlse8.v` 按 3-byte stride 分别加载三个坐标分量。 | asm attribution。 |
| f64 vector arithmetic（双精度向量运算） | adopted | 保持原 double reference rounding（双精度参考舍入）语义，避免 f32 quick path 的边界差异。 | `DecodePointXYZKeepsDoubleReferenceRounding`。 |
| AoS strided / segmented stores | adopted | 输出是 compatible AoS 点型，使用 `vsse32.v` 或 `vssseg3e32.v` 按 traits offset / stride 写 `x/y/z`。 | asm attribution、production-direct board。 |
| exact PointXYZ gate | adopted | exact `PointXYZ` 已完成 production-direct board repeated。 | Phase 060 summary。 |
| generic PointXYZ-like traits gate | adopted | `PointXYZI` / `PointXYZRGB` 代表点型板卡收益为正，额外字段保持。 | Phase 070 summary。 |
| encode RVV production | rejected / scalar-only | f32 量化有语义风险，f64 exact 量化太重；默认不接 production。 | Phase 020/030 topic-local result。 |

## VL chunk 内部流程

每个 VL chunk（可变向量长度分块）执行以下步骤：

1. 从 diff byte stream 分别用 3-byte stride 加载 `diffX`、`diffY`、`diffZ`。
2. 将 unsigned byte 扩展到 32-bit unsigned，再转换为 double 向量。
3. 对每个分量执行 `(diff + 0.5) * resolution + reference[k]`。
4. 将 double 向量窄化为 float。
5. 对输出 cloud 的 `x/y/z` 字段分别用 traits offset / stride 写回。
6. chunk 完成后推进 `pointDiffDataVectorIterator_ += pointCount * 3`，保持原 iterator 消费语义。

RVV 路径只改变循环组织和 load/store 方式，不改变 diff byte 的解释、reference/resolution 公式、输出顺序或 iterator 前进距离。

## Fallback 矩阵

| 条件 | 行为 | 语义保持证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | RVV helper 不编译，`decodePoints` 调用 `decodePointsStd`。 | Std/RVV correctness。 |
| `PointT` 不是 traits-compatible xyz AoS 点型 | 即使是 RVV build，也调用 `decodePointsStd`。 | `DecodeDoubleXYZFallsBackToScalarSemantics` 覆盖已注册但非 single-float xyz 的 fallback。 |
| `pointCount == 0` | RVV helper 直接返回，不消费 diff byte。 | source gate 与标量空循环等价。 |
| encode path | 保持原标量 `encodePoints`。 | Phase 020/030 语义与性能审计。 |
| 泛型 xyz / 自定义布局 | 当前不尝试 RVV，仍由模板标量路径编译和执行。 | exact gate 与 traits gate 设计。 |
| 完整 octree public stream | 本 patch 不改变 tree traversal、entropy coding 或 stream I/O。 | Phase 080 public boundary 10-run median `1.01x..1.02x` 且 Doctor `Errors=1，Warnings=2`；不作为稳定 public-positive 证据。 |

## 范围决策表

| 范围 | 状态 | 当前证据 | 不能外推到哪里 |
| --- | --- | --- | --- |
| exact `PointCoding<PointXYZ>::decodePoints` | adopted | Std/RVV 各 11 个 gtest 通过；production-direct board median `1.22x / 1.23x / 1.18x / 1.16x`；Doctor `Errors=0, Warnings=1`。 | 不外推到所有点型、完整 octree public timing 或其它目标硬件。 |
| traits-gated `PointXYZI` / `PointXYZRGB` decode | adopted | extra-field preservation gtests；production-direct board median `1.27x / 1.20x / 1.27x / 1.19x`；Doctor `Errors=0, Warnings=2`。 | 不说明所有自定义点型、非标准布局或其它硬件都成立。 |
| non-RVV build | scalar-only fallback | Std build correctness 通过。 | 不提供 RVV 性能结论。 |
| encode path | rejected for production in current topic | f32 语义风险；f64 exact probe 性能拒绝。 | 后续必须先有输入域 / 误差合同或新候选证据。 |
| full public octree end-to-end | attempted / weak-near-threshold | Phase 080 public decode / roundtrip 10-run median `1.01x / 1.01x / 1.01x / 1.02x`；Doctor `Errors=1，Warnings=2，Suggestions=4`。 | 不推翻 direct helper 采纳；不能写成稳定公开入口加速，也不能外推到真实文件流、其它 profile 或其它点型。 |

## 标量路径与 RVV 路径差异

| 阶段 | 标量路径 | RVV 路径 | 保留边界 |
| --- | --- | --- | --- |
| 入口与范围检查 | `decodePoints` 断言 begin/end 后逐点循环。 | 同一入口，先计算 `pointCount`，traits gate 命中才进入 RVV。 | public API 不变。 |
| diff byte 消费 | 每点连续取 `diffX/diffY/diffZ`。 | 三次 strided load 形成 X/Y/Z 向量。 | 仍消费同一 byte stream。 |
| 坐标公式 | double reference + resolution，最后 cast float。 | f64 vector arithmetic 后窄化 float。 | 避免 f32 近边界语义差异。 |
| 输出写回 | 对每个点型写 `x/y/z`。 | 对 compatible AoS 字段用 strided / segmented store。 | 只批准 compatible point type。 |
| iterator 更新 | 循环中每点推进 3 个 byte。 | helper 末尾一次推进 `pointCount * 3`。 | 总消费量相同。 |

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `PointCoding::decodePoints` | production dispatch | 解码点坐标；RVV 构建且 traits-compatible 时分流到 RVV。 | octree decompression 编排层。 | `decodePointsStd` / `decodePointsRVV`。 | adopted production behavior | `io/include/pcl/compression/point_coding.h` |
| `PointCoding::decodePointsStd` | production Std helper | 保留原标量循环和 fallback。 | `decodePoints`。 | 输出 cloud。 | scalar reference / fallback | `io/include/pcl/compression/point_coding.h` |
| `PointCoding::decodeDiffBytesToFloat` | production RVV lane helper | 把 diff byte 按 double reference 公式转换成 float vector。 | `decodePointsRVV`。 | strided / segmented store。 | numeric semantics | `io/include/pcl/compression/point_coding.h` |
| `PointCoding::decodePointsRVV` | production RVV helper | VL chunk load、f64 arithmetic、AoS strided/segmented store，并推进 iterator。 | `decodePoints` traits gate。 | output cloud。 | adopted RVV path | `io/include/pcl/compression/point_coding.h` |
| `test_point_coding` | correctness gate | 覆盖 component、production-shaped、production-direct rounding/fallback、traits preservation 和 public roundtrip smoke。 | `make run_test_compare`。 | gtest output。 | correctness / fallback evidence | `test-rvv/io/point_coding/src/test_point_coding.cpp` |
| `bench_point_coding` | bench wrapper | `decode_production_direct_*` 两侧调用真实 production `decodePoints`；`octree_*_public_*` 调用真实 `OctreePointCloudCompression<PointXYZ>` public entry。 | QEMU smoke / board repeated。 | summary / manifest。 | production-direct / production-public performance | `test-rvv/io/point_coding/src/bench_point_coding.cpp` |
| manifest generator | analysis script | 为 production-direct 和 production-public summary 生成 Evidence Doctor manifest。 | `make run_board_repeated_evidence_doctor`。 | Evidence Doctor。 | doctor input | `test-rvv/io/point_coding/script/generate_point_coding_evidence_manifest.py` |
| production repeated summary | evidence output summary | 10-run production-direct 板卡结果。 | board logs。 | Evidence Doctor、本文。 | board performance | `test-rvv/io/point_coding/log/board/repeated_phase060_production_decode/summary.md` |
| traits repeated summary | evidence output summary | 10-run traits-gated production-direct 板卡结果。 | board logs。 | Evidence Doctor、本文。 | board performance | `test-rvv/io/point_coding/log/board/repeated_phase070_traits_decode/summary.md` |
| public repeated summary | evidence output summary | 10-run public decode / roundtrip 板卡结果。 | board logs。 | Evidence Doctor、本文。 | public boundary audit | `test-rvv/io/point_coding/log/board/repeated_phase080_public_octree/summary.md` |
| Phase 060 / 070 / 080 result | documentation section | PI1-PI5、traits-gated 执行事实、public boundary audit、EvidenceDecision 和 warning 处理。 | production integration loop / public audit。 | 本文、Handoff。 | recovery pointer | `test-rvv/io/point_coding/doc/phases/060-production-integration-plan/result.zh.md`、`070-pointxyz-like-traits-expansion/result.zh.md`、`080-public-octree-end-to-end/result.zh.md` |
| topic-local evaluation | documentation section | 函数级评估、证据边界和未覆盖范围。 | topic closeout。 | 本文、筛选状态。 | decision audit | `test-rvv/io/point_coding/doc/point_coding-evaluation.zh.md` |

## Bench 与证据

production-direct benchmark 用 synthetic diff byte 初始化真实 `PointCoding<PointT>` object，并让 Std/RVV 两侧都调用
`PointCoding<PointT>::decodePoints`。speedup 口径是 `Std time / RVV time`，checksum（校验和）在 Std/RVV 间一致。

| case | 数据 | 计时边界 | 结论 |
| --- | --- | --- | --- |
| `decode_production_direct_256/1024/4096/16384` | 256 / 1024 / 4096 / 16384 个 `PointXYZ` 输出点。 | 真实 production `decodePoints` | Phase 060 exact `PointXYZ` adopted。 |
| `decode_production_direct_traits_xyzi_1024/4096` | 1024 / 4096 个 `PointXYZI` 输出点。 | 真实 production `decodePoints` | Phase 070 traits-gated extra-field preservation adopted。 |
| `decode_production_direct_traits_xyzrgb_1024/4096` | 1024 / 4096 个 `PointXYZRGB` 输出点。 | 真实 production `decodePoints` | Phase 070 traits-gated color preservation adopted。 |
| `octree_decode_public_256/1024` | 真实 public encoder 生成压缩流，计时真实 public decode。 | `OctreePointCloudCompression<PointXYZ>::decodePointCloud` | Phase 080 weak / near-threshold；不是稳定 public-positive。 |
| `octree_roundtrip_public_256/1024` | 每次计时真实 public encode + decode 往返。 | `OctreePointCloudCompression<PointXYZ>::encodePointCloud` + `decodePointCloud` | Phase 080 weak / near-threshold；说明完整公开链路稀释 direct helper 收益。 |

QEMU smoke（仿真小型验证）只证明 RVV bench binary 可运行和日志形状，不作为性能结论。最终性能数据来自
Milkv-Jupiter repeated board summary。

## 正确性与高效性证据链

| evidence area | 当前证据 | 结论边界 |
| --- | --- | --- |
| correctness（正确性） | `make -C test-rvv/io/point_coding run_test_compare`：Std/RVV 各 11 个 gtest 通过。 | 覆盖 component、production-shaped、public roundtrip smoke、production-direct rounding、non-compatible fallback / traits preservation。 |
| QEMU smoke | `make run_qemu_bench_smoke POINT_CODING_QEMU_BENCH_SMOKE_ARGS='--iterations 1 --warmup-iterations 0 --case-filter decode_production_direct_traits_xyzi_1024'` 通过；Phase 080 的 `octree_decode_public_256` / `octree_roundtrip_public_256` smoke 也通过。 | 只证明可运行和日志形状；不作为性能结论。 |
| asm attribution（反汇编归属） | `make dump_bench_rvv` 后可见 `vlse8.v`、`vfwcvt.f.xu.v`、`vfncvt.f.f.w`、`vsse32.v`、`vssseg3e32.v`。 | 证明 production-direct bench 命中 RVV byte load、f64 widen/narrow 和 AoS store。 |
| board performance（板卡性能） | `log/board/repeated_phase060_production_decode/summary.md`：10-run median `1.22x / 1.23x / 1.18x / 1.16x`。`log/board/repeated_phase070_traits_decode/summary.md`：10-run median `1.27x / 1.20x / 1.27x / 1.19x`。`log/board/repeated_phase080_public_octree/summary.md`：public median `1.01x / 1.01x / 1.01x / 1.02x`。 | Phase 060/070 支持 Milkv-Jupiter point coder decode direct / traits-direct 采纳；Phase 080 只说明 public boundary 收益被稀释。 |
| Evidence Doctor | `log/board/repeated_phase060_production_decode/evidence_doctor.md`、`log/board/repeated_phase070_traits_decode/evidence_doctor.md` 与 `log/board/repeated_phase080_public_octree/evidence_doctor.md` | Phase 060 为 `Errors=0，Warnings=1，Suggestions=0`；Phase 070 为 `Errors=0，Warnings=2，Suggestions=0`；Phase 080 为 `Errors=1，Warnings=2，Suggestions=4`。Phase 080 不作为 stable public-positive evidence。 |
| fallback | 非 RVV build correctness、traits gate 审计，以及 traits-gated extra-field preservation gtests。 | 不覆盖所有可能自定义点型的性能；非 compatible 点型依赖 `if constexpr` gate 回到标量路径。 |

## Production Closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| production file | 修改 `io/include/pcl/compression/point_coding.h`，新增 `decodePointsStd`、`decodePointsRVV`、exact `PointXYZ` dispatch 和 traits gate dispatch。 | source diff |
| public API | 不改变 public API、函数签名、成员状态或返回语义。 | source diff |
| compile gate | RVV helper 只在 `__RVV10__` 下编译。 | Std/RVV correctness |
| adopted RVV path | exact `PointCoding<PointXYZ>::decodePoints`。 | Phase 060 board summary |
| adopted traits path | `PointXYZI` / `PointXYZRGB` compatible traits gate。 | Phase 070 board summary |
| scalar-only paths | encode path、非 RVV build、非 compatible `PointT`；完整 octree traversal / entropy coding 未被此 patch 改写。 | fallback matrix / scope audit / Phase 080 public audit |
| evidence basis | 接入后的 production-direct board 数据。 | `log/board/repeated_phase060_production_decode/summary.md`、`log/board/repeated_phase070_traits_decode/summary.md` |
| rollback boundary | 可移除 RVV helper 和 `decodePoints` gate；`decodePointsStd` 保留原标量主体。 | single production source diff |

## 后续方向

当前 exact `PointXYZ` 与 traits-gated representative decode patch 已关闭 production closeout。Phase 080 已补完整 public octree end-to-end timing，并显示公开入口收益只有 `1.01x..1.02x` 的 near-threshold weak signal，且 Evidence Doctor 有退化频率 Error / Warning。当前没有值得继续自动推进的同边界优化方向；若未来真实 workload（真实工作负载）或 profile（性能剖析）证明 point coder decode 仍是公开链路主成本，再另开 profile / ablation phase。

暂不建议继续的方向：

- encode production RVV：当前 f32 fast path 有 double reference 语义风险，f64 exact path 在板卡上太重。
- 再堆更多同类 traits 代表点型：当前 compatible 代表点型已经有 positive board 结果，继续同类 synthetic case 难以改变生产决策。
- 继续扩大同类 public octree synthetic runs：Phase 080 已说明完整公开链路稀释收益；没有真实 workload / profile 时，更多复跑难以导出新的 production patch。
- 其它点/color coder family：应按各自 topic 推进。
