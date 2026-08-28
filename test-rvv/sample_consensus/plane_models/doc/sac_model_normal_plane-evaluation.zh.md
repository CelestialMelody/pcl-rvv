# sac_model_normal_plane 函数级评估

## S2 函数级评估

`SampleConsensusModelNormalPlane<PointT, PointNT>` 在普通平面距离外，额外用点法线和模型法线的锐角差异约束内点。公开入口包括：

- `selectWithinDistance`：输出内点索引，并把每个内点的混合距离写入 `error_sqr_dists_`。
- `countWithinDistance`：只统计混合距离小于阈值的点数。
- `getDistancesToModel`：对 `indices_` 中每个点写出混合距离。

标量路径对每个 `indices_[i]` 读取点坐标、法线和 curvature（曲率），计算 `d_euclid = |ax + by + cz + d|`，再用 `getAngle3D` 得到法线和模型法线夹角并取锐角侧，最后计算 `|weight * d_normal + (1 - weight) * d_euclid|`。RVV 路径把坐标、normal 和 curvature 通过 indexed gather（按索引离散加载）装入 `vfloat32m2_t`，使用 `distRVV_f32m2` 和 `getAcuteAngle3DRVV_f32m2`，并在 `selectWithinDistanceRVV` 中用 mask + `vcompress`（按掩码压缩）连续写回。

当前判断是 production patch 已保留，并且 phase 000 已把 `PointXYZ + Normal` 的公开入口 dispatch / fallback、helper 缓冲区合同、QEMU 正确性、反汇编归属、板卡性能和 Evidence Doctor 证据闭合。phase 040 进一步把 source 侧公开入口 gate 从字段语义收紧到 AoS byte-offset layout，并证明 `PointXYZI + Normal`、`PointXYZINormal + Normal` 代表点型在 public entry（公开入口）下与 direct RVV helper 输出一致；注册 xyz 但非 standard-layout 的 source 点型会回退 Standard helper。phase 050 已补 `PointXYZI + Normal` 和 `PointXYZINormal + Normal` 的 protected helper hot path（受保护 helper 热点路径）5-run board performance，两个代表点型的三条 helper 都为 positive-stable。phase 060 已补 `PointXYZ + PointNormal`、`PointXYZ + PointXYZINormal` normal cloud 的 public-vs-direct RVV correctness，并证明 registered float normal/curvature 但非 standard-layout 的 normal 点型会回退 Standard helper。phase 070 已补 `PointXYZI` / `PointXYZINormal` source 与 `PointNormal` / `PointXYZINormal` normal cloud 的 4 个代表性交叉组合 public-vs-direct RVV correctness。证据边界仍是窄范围 production：新增性能不覆盖完整泛型点类型全集、更多 normal-like 点型、`Scalar=double`、完整公开入口计时或新的 RVV 实现族选择。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `selectWithinDistance` | production public entry | 公开内点选择入口，负责预分配、RVV / 标量分流和收缩输出。 | SAC 方法调用 | `selectWithinDistanceRVV` 或 `selectWithinDistanceStandard` | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` |
| `countWithinDistance` | production public entry | 公开内点计数入口。 | SAC 方法调用 | `countWithinDistanceRVV` 或 `countWithinDistanceStandard` | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` |
| `getDistancesToModel` | production public entry | 公开稠密距离输出入口。 | SAC 方法调用 | `getDistancesToModelRVV` 或 `getDistancesToModelStandard` | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` |
| `src/test_sample_consensus_plane_models.cpp` | test support | 历史 GTest，包含 plane 回归和 normal-plane RVV helper 对拍。 | `make run_test_*`、`run_normal_plane_public_tests` | PCL protected helper proxy | correctness gate | `test-rvv/sample_consensus/plane_models/src/test_sample_consensus_plane_models.cpp` |
| `src/bench_sac_normal_plane.cpp` | bench wrapper | 对三条 normal-plane helper 做 Std/RVV benchmark。 | `make run_bench_*`、board.mk | compare script | performance diagnostic | `test-rvv/sample_consensus/plane_models/src/bench_sac_normal_plane.cpp` |
| topic-local doc suite | documentation | README、testing、correctness、benchmark/evidence、optimization evidence 和 code map 的 role 分工。 | phase 010 | phase result / Handoff | recovery pointer | `test-rvv/sample_consensus/plane_models/README.zh.md`、`test-rvv/sample_consensus/plane_models/doc/*.zh.md` |
| `optimization-matrix.zh.md` | phase matrix | 跟踪候选族、证据和下一动作。 | phase loop | Handoff / reviewer | recovery pointer | `test-rvv/sample_consensus/plane_models/doc/phases/optimization-matrix.zh.md` |
| phase 000 result | closeout | 记录公开入口、fallback、helper resize、QEMU、asm、board 和 Evidence Doctor 结论。 | phase loop | roadmap / queue / Handoff | phase closeout | `test-rvv/sample_consensus/plane_models/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/result.zh.md` |
| phase 040 result | phase closeout | 记录 source AoS gate、`PointXYZI` / `PointXYZINormal` 代表点型和 non-AoS fallback 结论。 | phase loop | roadmap / queue / Handoff | representative point-type correctness | `test-rvv/sample_consensus/plane_models/doc/phases/040-normal-plane-aospoint-gate-expansion/result.zh.md` |
| phase 050 result | phase closeout | 记录代表性 AoS source 点型 protected helper performance、Evidence Doctor Warning 解释和 registry 状态。 | phase loop | roadmap / queue / Handoff | representative point-type performance | `test-rvv/sample_consensus/plane_models/doc/phases/050-normal-plane-representative-aos-source-performance/result.zh.md` |
| phase 060 result | phase closeout | 记录代表性 normal layout correctness、non-AoS normal fallback 和板卡 public alias 结果。 | phase loop | roadmap / queue / Handoff | representative normal layout correctness | `test-rvv/sample_consensus/plane_models/doc/phases/060-normal-plane-normal-layout-expansion/result.zh.md` |
| phase 070 result | phase closeout | 记录代表性 source × normal 交叉组合 public correctness、板卡 public alias 和 freshness check 结果。 | phase loop | roadmap / queue / Handoff | representative source-normal cross correctness | `test-rvv/sample_consensus/plane_models/doc/phases/070-normal-plane-cross-point-type-layout/result.zh.md` |

## 生产接入判断

当前生产源码已有 RVV patch，因此本 topic 不是“是否开始写 production”的空白候选，而是“已有 production patch 是否证据闭合”。phase 000 证据支持保留当前 production patch：公开入口测试已证明支持布局会命中 RVV，unsupported curvature layout 会标量 fallback，board helper compare 三项为 positive，Evidence Doctor 无 finding。phase 040 修正了 gate 边界：source 必须满足 `RVVXYZAoSFloatLayout<PointT>`，normal 必须满足 normal/curvature 单 float 且 AoS-compatible 的本地 gate，点云规模还必须满足 32-bit byte offset 上界；否则公开入口走 Standard fallback。

不把结论升级为 fully adopted generic behavior。`PointXYZI` 和 `PointXYZINormal` source 已关闭代表性 AoS source correctness 与 helper performance；`PointNormal` 和 `PointXYZINormal` normal cloud 已关闭代表性 normal layout correctness；这两条轴的 4 个代表性交叉组合已关闭 production-public correctness。若后续要覆盖更多 PCL xyz AoS 点型、其它 `PointNT` normal-like layout、未注册点类型、非 float curvature、公开入口性能或 `Scalar=double`，需要另开 point-type / normal-layout / performance expansion phase 并重新闭合 correctness、fallback、asm、board 和 Evidence Doctor evidence。

## S11 Closeout

| 项 | 结果 |
| --- | --- |
| EvidenceDecision | `production patch retained / phase 030 repeated board positive-stable / phase 040 representative AoS source correctness closed / phase 050 representative source performance positive-stable / phase 060 representative normal layout correctness closed / phase 070 representative cross correctness closed` |
| QEMU correctness | `make -C test-rvv/sample_consensus/plane_models run_normal_plane_public_tests`：RVV public alias 13/13 passed；`make -C test-rvv/sample_consensus/plane_models run_test_compare`：Std/RVV 各 32/32 passed。 |
| Board correctness | `SSH_AUTH_SOCK=<injected> make -C test-rvv/sample_consensus/plane_models run_board_normal_plane_public_tests`：13/13 passed；phase 010 后默认使用板卡侧 PCD fixture；本轮输出含远端 clock skew warning，不改变测试结论。 |
| Board performance | 单次 board compare：`selectWithinDistance` 9.79x、`countWithinDistance` 11.52x、`getDistancesToModel` 10.52x；phase 030 repeated summary：`selectWithinDistance` 9.64x median / 9.24x min、`countWithinDistance` 12.72x median / 12.56x min、`getDistancesToModel` 12.02x median / 11.66x min；phase 050 representative source summary：`PointXYZI` 三项 median 为 8.04x / 6.10x / 5.41x，`PointXYZINormal` 三项 median 为 7.37x / 8.52x / 8.44x。 |
| ASM attribution | `dump_bench_rvv` 后，三个 normal-plane RVV symbols 内均可定位 RVV 指令。 |
| Evidence Doctor / registry | phase 000 `run_board_evidence_doctor` 与 phase 030 repeated doctor 均为 Errors=0、Warnings=0、Suggestions=0；phase 050 doctor 为 Errors=0、Warnings=5、Suggestions=0，Warnings 已按点型 / helper 独立报告解释；`evidence_status`、`repeated_evidence_status` 和 `phase050_evidence_status` 均输出 fresh。 |

phase 010 已关闭 `plane_models` 测试支撑 source layout、topic-local doc suite 和 board benchmark 的 host/remote fixture 参数边界。phase 020 已关闭 topic-local Evidence registry / manifest alias 自动化。phase 030 已关闭 5-run repeated board summary、repeated manifest / doctor 和 registry freshness。phase 040 已关闭 source AoS gate、代表性 source 点型 public correctness 和 non-AoS fallback。phase 050 已关闭代表性 source 点型 helper performance。phase 060 已关闭代表性 normal layout public correctness 和 non-AoS normal fallback。phase 070 已关闭代表性 source × normal 交叉组合 correctness；后续更多 source / normal 点型全集、公开入口性能或 `Scalar=double` 扩展必须作为新 phase 执行。
