# Phase 070 Plan: generic-xyz-point-type-expansion

## 阶段意图和边界

本阶段验证 `transformation_estimation_2D` 的 PointXYZ-like 泛型点类型候选，但先停留在
test-rvv 诊断和证据层，不修改 production dispatch。目标是证明“当前算法只读取 source /
target 的 x/y/z，且两侧可以分别使用不同的 PCL 点类型”在 traits、布局、正确性和目标硬件
性能上是否成立。

本阶段只覆盖：

- ordered-cloud-pair（顺序点云对，source/target 按相同下标一一对应）；
- `Scalar=float`；
- dense、所有点 finite、点数不少于 16 的 RVV candidate；
- source / target 分别 gate，允许不同点类型；
- `PointXYZ`、`PointXYZI`、`PointNormal`、`PointXYZINormal` 和必要 mixed pair。

本阶段不覆盖：

- source-indexed-cloud-pair、dual-indexed-cloud-pair、correspondence-pair；
- `Scalar=double` 的 RVV 扩展；
- RGB/RGBA 额外字段语义；
- 非法 index / correspondence 安全合同；
- 任何 production dispatch 修改或当前窄范围 patch 的回滚、覆盖和提交。

三个 indexed/correspondence row source 必须在独立阶段继续，不能继承本阶段的
ordered-cloud-pair 结论。只有本阶段的 generic evidence bundle 闭合并重新形成 PI1 后，
才允许讨论 production dispatch。

## 当前输入

| 输入 | 当前事实 |
| --- | --- |
| production patch | `transformation_estimation_2D.hpp` 当前仍是 exact `PointXYZ -> PointXYZ`、`Scalar=float`、dense finite ordered-cloud-pair gate |
| Phase 060 | 已完成恢复审阅；窄范围 patch 保留，generic point type 仍未批准 |
| 现有 candidate | `include/te2d.h` 和 TE2D 测试支撑内部头已有两遍中心化 RVV math，但旧 gate 仍以 legacy member gate 为主 |
| 公共 traits | `common/include/pcl/rvv_point_traits.h` 提供 `RVVXYZAoSFloatLayout<PointT>`、字段 datatype、POD、sizeof、offset 和 alignment gate |
| 公共 load | `pcl::rvv_load::strided_load3_f32m2` 支持按当前点类型 stride 和 x/y/z offset 加载 |
| 现有 board | 板卡可用性已由 Phase 050/060 的 `Milkv-Jupiter` repeated 复核；本阶段必须执行有界 representative point-type bench |

## 泛型 Gate 设计

### PointXYZ-like gate

本阶段的候选 gate 使用：

```cpp
pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value
```

它必须证明：

- `pcl::traits::has_xyz<PointT>::value` 为 true；
- x/y/z 都是 PCL traits 注册的单个 `float`；
- `pcl::traits::offset<PointT, pcl::fields::{x,y,z}>` 可取得当前点型 offset；
- `typename pcl::traits::POD<PointT>::type` 是 standard-layout；
- `sizeof(PointT) == sizeof(POD)`；
- 点型 stride 和三个字段 offset 满足 float alignment。

这个 gate 只描述字段语义和 AoS 访存前提，不包含规模、dense/finite、`Scalar`、row source、
输出矩阵、角度求解或 production dispatch。生产接入时仍需在算法入口保留运行期 gate 和
fallback。

### Source / Target 分别审计

source 和 target 不能共享 offset、stride 或 `sizeof` 假设：

| 侧别 | 必须独立证明 | 本阶段最小组合 |
| --- | --- | --- |
| `PointSource` | source layout、POD、sizeof、x/y/z offset、finite 语义 | `PointXYZ`、`PointXYZI`、`PointNormal`、`PointXYZINormal` |
| `PointTarget` | target layout、POD、sizeof、x/y/z offset、finite 语义 | `PointXYZ`、`PointXYZI`、`PointNormal`、`PointXYZINormal` |
| mixed | 两侧 gate 同时成立且 stride 不相同也能正确加载 | `PointXYZI -> PointXYZ`、`PointXYZ -> PointXYZI`、`PointNormal -> PointXYZINormal`、`PointXYZINormal -> PointNormal` |

本算法只读取 x/y/z，不读取 intensity 或 normal 字段。测试必须用非零、有限的额外字段
构造样本，并证明改变这些额外字段不会改变估计矩阵；这只证明当前算法的输入语义，不代表
其它会写回完整 `PointT` 的算法可以复用同一 gate。

## 候选和实现动作

| id | 动作 | 产物 | 完成判据 |
| --- | --- | --- | --- |
| G1 | traits gate probe | `include/te2d.h` 和 TE2D 测试支撑内部头、`src/test_te2d.cpp` | 四类代表点型的 `RVVXYZAoSFloatLayout` 结果可打印、可断言；source/target 使用各自 offset 和 sizeof；不引入本地重复 traits gate |
| G2 | generic test-only candidate | `include/te2d.h` 和 TE2D 测试支撑内部头 | ordered-cloud-pair generic candidate 在 RVV build 下使用 traits offset，非 RVV / 不满足 gate / 小规模 / 非 dense / 非有限输入回退标量；不改 production |
| G3 | representative correctness | `src/test_te2d.cpp` | exact 四类点型、source/target mixed pair、额外字段不影响矩阵、near-cancellation、small/non-dense/non-finite fallback、current production generic public scalar boundary 全部通过 |
| G4 | QEMU generic smoke | `Makefile`、`src/bench_te2d.cpp`、topic-local manifest | generic case-filter 至少覆盖代表点型和 mixed pair；QEMU 只报告路径、日志形状、checksum 和 asm 输入，不写性能结论 |
| G5 | asm attribution | `script/generate_te2d_asm_summary.py` 或等价 topic-local wrapper | generic candidate 的 RVV 指令归属到 generic bench lambda / test-support fixture；不能误写成 production symbol |
| G6 | representative board bench | `Makefile`、`board.mk`、board summary/manifest/doctor | 5 runs、20 iterations、5 warmup；至少覆盖四类点型的 same-type ordered pair 和 mixed pair；按 point type / size 分组，保留 min/median/max、B/A<1 和 binary hash |
| G7 | Evidence Doctor / registry | `log/board/generic_xyz_point_types_repeated/**`、`log/qemu/generic_xyz_point_types/**`、registry | Doctor Errors 先修复或降级；Warnings/Suggestions 写入 result；registry fresh，文档引用和 case label 一致 |
| G8 | phase decision | 本 result、roadmap、matrix、evaluation、Handoff | 只有 correctness、fallback、QEMU、asm、代表性 board 和 Doctor 证据闭合，才可形成新的 PI1 候选；不直接修改 production dispatch |

## Correctness / fallback 矩阵

| 类别 | case | 期望 |
| --- | --- | --- |
| traits | `PointXYZ`、`PointXYZI`、`PointNormal`、`PointXYZINormal` source/target 各自 gate | 全部通过；offset/sizeof 按点型分别记录 |
| same-type | 四类点型各自 ordered pair | generic candidate 与 public scalar / fused Std reference 在误差预算内一致 |
| mixed | `PointXYZI -> PointXYZ`、`PointXYZ -> PointXYZI`、`PointNormal -> PointXYZINormal`、`PointXYZINormal -> PointNormal` | source/target 不同 stride 时矩阵仍一致 |
| extra fields | intensity、normal 使用不同非零有限值 | 估计矩阵只依赖 x/y/z；额外字段变化不改变结果 |
| small input | 8 points | `used_rvv=false`、`used_fallback=true`，结果与 scalar reference 一致 |
| non-dense | `is_dense=false` 且全部 finite | candidate fallback；不把 dense flag 当作 traits gate |
| non-finite | x/y/z 任一侧含 NaN/Inf | candidate fallback；保持当前 public scalar 语义 |
| non-RVV | Std build | 不编译 RVV intrinsic；generic candidate 走 scalar fallback |
| Scalar fallback | `TransformationEstimation2D<PointXYZI, PointXYZINormal, double>` | public API 仍正确，当前 production exact-float gate 不命中 |
| row-source boundary | source-indexed、dual-indexed、correspondence | 本阶段不接入 generic candidate；继续单独 scalar / row-source matrix |

## Bench 和证据合同

### Case-filter

新增 `generic-xyz-point-types` case-filter。case label 至少包含：

- `generic 2D PointXYZ->PointXYZ 4K/64K/256K`
- `generic 2D PointXYZI->PointXYZI 4K/64K/256K`
- `generic 2D PointNormal->PointNormal 4K/64K/256K`
- `generic 2D PointXYZINormal->PointXYZINormal 4K/64K/256K`
- `generic 2D PointXYZI->PointXYZ 64K`
- `generic 2D PointXYZ->PointXYZI 64K`
- `generic 2D PointNormal->PointXYZINormal 64K`
- `generic 2D PointXYZINormal->PointNormal 64K`

每个 case 的计时边界包含 generic candidate、2D solve 和 checksum，不包含输入构造。
它仍是 test-only generic candidate；QEMU timing 不进入性能结论。

### ASM / Evidence Doctor

- QEMU manifest 的 `point_type` 必须记录 `source->target`，不能继续硬编码 `PointXYZ`。
- board manifest 需要保留 `source_point_type`、`target_point_type`、`row_source`、size、run_count、
  binary hash、timer boundary 和 gate。
- generic candidate asm 归属到 test-support generic lambda / fixture；即使出现 `vlsseg3e32.v`
  或 `vfmacc`，也不能写成 production dispatch。
- board Evidence Doctor 的 Error（例如 checksum mismatch、case set mismatch、strict A/B
  metadata 缺失）必须先修复；Warning（例如某点型/规模退化、环境字段缺失）必须解释或降级
  point-type 性能结论；Suggestion 进入下一阶段或 Handoff。

### 板卡复跑预算

- 计划 5 runs；
- 每 run 20 iterations、5 warmup；
- 先跑单次 board smoke，确认 Std/RVV case set、checksum 和 generic gate，再跑 repeated；
- 同一点型 / 规模的 `B/A` 以 median、min、max、`B/A<1` 和 bucket 判断；
- 5-run 后 bucket 稳定则关闭该 case；仍摇摆则标 `unstable`，不无限复跑；
- board 结果只证明代表性点型组合，不能外推到所有 traits-allowed 自定义点型。

## 优化矩阵

| candidate family | row source | point type / Scalar / layout | correctness | bench | board | asm | Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| traits-gated generic xyz candidate | ordered-cloud-pair | PointXYZ / float / AoS | planned G1-G3 | planned G4 | planned G6 | planned G5 | planned G7 | planned |
| traits-gated generic xyz candidate | ordered-cloud-pair | PointXYZI / float / AoS | planned G1-G3 | planned G4 | planned G6 | planned G5 | planned G7 | planned |
| traits-gated generic xyz candidate | ordered-cloud-pair | PointNormal / float / AoS | planned G1-G3 | planned G4 | planned G6 | planned G5 | planned G7 | planned |
| traits-gated generic xyz candidate | ordered-cloud-pair | PointXYZINormal / float / AoS | planned G1-G3 | planned G4 | planned G6 | planned G5 | planned G7 | planned |
| traits-gated generic xyz candidate | ordered-cloud-pair | mixed source/target / float / separate AoS | planned G1-G3 | planned G4 | planned G6 | planned G5 | planned G7 | planned |
| current narrow production dispatch | ordered-cloud-pair | exact PointXYZ / float / AoS | retained Phase 050 | retained board evidence | 4K/64K/256K positive | production public boundary | 0/0/0 | user-review-pending |
| generic xyz production dispatch | ordered-cloud-pair | traits-gated point types / float / AoS | not_applicable before G8 | not_applicable | not_applicable | not_applicable | not_run | deferred until generic evidence closes |
| all generic row-source policies | indexed / dual / correspondence | PointXYZ first, then generic | independent phase | separate case-filter | separate board evidence | separate boundary | separate Doctor | deferred, no carry-over |

## Roadmap / 文档动作

本阶段必须同步：

- `doc/phases/README.zh.md`：新增 Phase 070，更新默认恢复入口；
- `doc/optimization-roadmap.zh.md`：把 generic xyz point-type candidate 放入默认恢复队列，
  并把三个 row-source policy 保持为独立队列；
- `doc/phases/optimization-matrix.zh.md`：加入 source/target point-type 和 generic evidence 行；
- `doc/transformation_estimation_2D-evaluation.zh.md`：更新当前 production boundary、traits
  gate 设计和 Traceability Map；
- `doc/benchmark-and-evidence.zh.md`、`doc/correctness-tests.zh.md`、
  `doc/test-support-code-map.zh.md`：补 generic case-filter、点型矩阵和 evidence boundary；
- `doc-rvv/registration/transformation_estimation_2D-RVV.zh.md`：本阶段不把 test-only generic
  candidate 写成 adopted production behavior；只有 PI5 后用户确认采纳才更新长期文档。

## 完成、继续和停止条件

完成条件：

- G1-G7 的执行事实均有路径和结果；
- correctness / fallback / QEMU / asm / board / Doctor / registry 证据可互相追踪；
- source/target mixed pair 的 gate 和结果已分别记录；
- 没有把 generic diagnostic 写成 production direct；
- result、roadmap、matrix、evaluation、Handoff 已刷新。

继续条件：

- generic candidate 仍有未完成但未阻塞的 point type / mixed pair / board case；
- board 可达且 repeated budget 未用完；
- Evidence Doctor warning 可通过最小复跑或 metadata 补齐继续解释。

合法停止条件：

- board 不可达、工具链失败或 dirty isolation 不安全；
- Evidence Doctor Error 无法修复，或 correctness / asm / board 证据矛盾；
- 继续需要修改 production dispatch，此时必须先输出证据和新的 PI1；
- repeated budget 用完后 decision bucket 仍 unstable，需要 reviewer / 用户判断。

本阶段不能因为单个点型 correctness、单个 QEMU smoke、单个 asm summary 或单个 board case
完成而停止；必须按 `micro_stop_guard` 继续下一个未阻塞动作。

## 默认下一阶段

若 generic evidence bundle 闭合：创建新的 PI1 production integration plan，明确只接入已证明的
traits-gated ordered-cloud-pair point-type 集合，并为生产接入后的 correctness、性能、asm、
fallback、Evidence Doctor 和文档建立独立 PI2-PI5 证据链。

若 generic evidence 不支持扩大：保留当前 exact production patch，写清 point-type-specific
负向证据；随后回到独立的 `row-source-layout-stability-diagnostic`，不把两个结论合并。
