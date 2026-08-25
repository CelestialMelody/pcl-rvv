# Phase 060 Result: PointXYZ + Normal production expansion

## 当前结论

本阶段完成 `pfh-pointxyz-normal-production-expansion`。生产 helper 现在支持两个 exact 点型组合：

- `pcl::PointNormal -> pcl::PointNormal`
- `pcl::PointXYZ -> pcl::Normal`

`PointXYZ + Normal` 使用同一 direct AoS RVV production family（生产实现族）：source cloud 只通过
`RVVXYZAoSFloatLayout<PointInT>` 读取 xyz，normal cloud 通过 PFH 本地 normal AoS gate 读取
`normal_x/normal_y/normal_z`。其它点型组合仍 fallback（回退）到原标量路径。

接入后板卡 repeated benchmark（重复板卡性能测试）在 `PointXYZ + Normal` 的 production-detail 和
production-public 两条边界均为 positive，按用户确认的“板卡有收益即可采纳”策略，本阶段将 exact
`PointXYZ + Normal` 写成 adopted production behavior（已采纳生产行为）。

## 执行动作回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| A1 扩展 production gate | done | `features/include/pcl/features/impl/pfh.hpp` | `kPFHDirectAoSRVVSupportedPointTypes` 支持 exact `PointNormal -> PointNormal` 和 exact `PointXYZ -> Normal`。 |
| A2 新增 correctness / fallback test | done | `make -B -C test-rvv/features/pfh run_test_compare` | Std 3/3、RVV 6/6 pass；RVV-only 覆盖 `PointXYZ + Normal` helper 和小邻域 / 非默认 bins fallback。 |
| A3 新增 bench case | done | `test-rvv/features/pfh/src/bench_pfh.cpp` | 新增 `component_pfh_signature_pointxyz_normal` 与 `public_pfh_pointxyz_normal_k`。 |
| A4 本地 asm 验证 | done | `make -B -C test-rvv/features/pfh dump_bench_rvv` | `nm -C` 可见 `PointNormal` 与 `PointXYZ, Normal` 两个 production helper 实例；`PointXYZ + Normal` helper 范围含 `vluxei32.v`、`vfmacc.vv`、`vfsqrt.v`、`vfdiv.vv`。 |
| A5 board smoke | done | `make -C test-rvv/features/pfh board_smoke BENCH_ARGS='--side 32 --k 32 --iterations 3 --warmup 1'` | 板端 RVV tests 6/6 pass；新组合 component 约 `1.97x`，public 约 `1.84x`。 |
| A6 board repeated | done | `make -C test-rvv/features/pfh board_repeated BENCH_ARGS='--side 32 --k 32 --iterations 8 --warmup 2' REPEATED_BOARD_OUTPUT_DIR=log/board/pi3-pointxyz-normal/repeated` | 5-run repeated 完成；新组合 decision bucket 为 positive。 |
| A7 Evidence Doctor / registry | done | `make -C test-rvv/features/pfh evidence_doctor_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/pi3-pointxyz-normal/repeated`；registry check | Doctor `0E/0W/12S`；registry check fresh。 |
| A8 文档回填 | done | 本 result、matrix、roadmap、phase README、`doc-rvv/features/pfh-RVV.zh.md`、features queue、handoff | `PointXYZ + Normal` 写成独立 adopted scope；未外推到泛型 traits。 |

## Board repeated 结果

输入：Milkv-Jupiter，synthetic PFH grid，`side=32`、`points=1024`、`k=32`、`iterations=8`、
`warmup=2`、5 runs。B/A 方向为 `Std ms / RVV ms`，大于 1 表示 RVV 更快。

| case | evidence role | 5-run B/A | mean / median | min / max | decision |
| --- | --- | --- | --- | --- | --- |
| `component_pfh_signature_pointxyz_normal` | production-detail | `1.92, 1.93, 1.92, 1.92, 1.92` | `1.922x / 1.92x` | `1.92x / 1.93x` | positive |
| `public_pfh_pointxyz_normal_k` | production-public | `1.85, 1.87, 1.85, 1.86, 1.84` | `1.854x / 1.85x` | `1.84x / 1.87x` | positive |
| `component_pfh_signature` | production-shaped cross-check | `1.96, 1.96, 1.97, 1.97, 1.96` | `1.964x / 1.96x` | `1.96x / 1.97x` | existing adopted path remains positive |
| `public_pfh_k` | production-public cross-check | `1.90, 1.91, 1.90, 1.90, 1.90` | `1.902x / 1.90x` | `1.90x / 1.91x` | existing adopted path remains positive |

## Evidence Doctor 与 registry

- Manifest：`test-rvv/features/pfh/log/board/pi3-pointxyz-normal/repeated/evidence_manifest.json`
- Doctor：`test-rvv/features/pfh/log/board/pi3-pointxyz-normal/repeated/evidence_doctor.md`
- JSON：`test-rvv/features/pfh/log/board/pi3-pointxyz-normal/repeated/evidence_doctor.json`
- Registry：`test-rvv/features/pfh/log/evidence_registry.json`
- Result：`Errors=0, Warnings=0, Suggestions=12`

12 个 suggestion 均为环境 metadata / binary hash 建议，覆盖 diagnostic、existing adopted 和新
`PointXYZ + Normal` case。它们不阻塞当前 positive bucket；若后续扩大到泛型 traits 或更多目标硬件，应补
taskset、governor、freq、temperature 和 binary hash。

## Diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `component_pfh_signature_pointxyz_normal` 为 production-detail；`public_pfh_pointxyz_normal_k` 为 production-public。 |
| A/B boundary | Std build 原标量 `PFHEstimation<PointXYZ, Normal>` vs RVV build 同一 production helper / public overload。 |
| 当前决策问题 | `RVV-vs-scalar`：exact `PointXYZ + Normal` 是否值得纳入当前 production dispatch。 |
| diagnostic 是否可外推到 production | 不外推。本阶段使用接入后的 production direct correctness、asm 和板卡 repeated 作为采纳依据。 |
| comparison-boundary / baseline mismatch 风险 | aggregate bench 同时包含历史 diagnostic case 和 existing adopted case；本结论只使用新组合的 production-detail / production-public rows。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已经是 bounded production probe；结果为 positive，因此无需降级或回滚。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要。当前没有为 `PointXYZ + Normal` 选择新实现族，只是把已采纳 direct AoS family 扩展到新 exact 点型组合。 |

## 范围与未验证项

已验证范围：

- `__RVV10__` RVV 构建。
- exact `pcl::PointXYZ` source cloud + exact `pcl::Normal` normals cloud。
- `PFHSignature125`、float fields、source xyz AoS + normals normal AoS。
- `use_cache_ == false`、`nr_split == 5`、`indices.size() >= 4`。
- synthetic dense finite cloud，public KSearch bench `side=32,k=32`。

仍未验证范围：

- PointXYZ-like / Normal-like 泛型 traits 集合。
- `PointXYZI`、`PointXYZINormal`、自定义点类型或其它 source / normal 组合。
- `use_cache_ == true`、OMP path、非默认 bins、`Scalar=double`。
- histogram scatter/vectorized copy、更多规模和其它目标硬件。

## Doc suite closeout

| area | current shape scan | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- |
| README navigation | 已补 `test-rvv/features/pfh/README.zh.md`。 | adopted | 入口、命令、证据白名单和当前 adopted 范围可定位。 | 无。 |
| evaluation_production | 已补 `test-rvv/features/pfh/doc/pfh-evaluation.zh.md`。 | adopted | production patch scope、fallback matrix、board evidence 和 EvidenceDecision 可审查。 | 无。 |
| testing_overview / correctness / benchmark | 合并在 README、evaluation、phase results 和长期文档中。 | adopted | 当前 topic 文件规模较小，case label 和命令已在这些文档中列清。 | 若后续扩成泛型 traits 或更多 row source，再拆独立 role 文档。 |
| optimization roadmap / matrix | 已更新。 | adopted | `test-rvv/features/pfh/doc/optimization-roadmap.zh.md` 与 `doc/phases/optimization-matrix.zh.md`。 | 无。 |
| production_topic_doc | 已刷新。 | adopted | `doc-rvv/features/pfh-RVV.zh.md` 使用 Phase 040 与 060 接入后板卡数据。 | 无。 |
| artifact tracking | 路径限定 status 显示 topic 产物仍是未跟踪提交候选。 | adopted | 用户未要求 commit；Handoff 列出 dirty isolation 和不默认提交 raw logs。 | 提交前精确 staged selection。 |

## 继续 / 停止决策

`continue_stop_decision`: stop。

当前值得在本 topic 内继续推进且已授权的高价值优化动作已经关闭到 exact 点型组合：

- `PointNormal -> PointNormal` adopted。
- `PointXYZ -> Normal` adopted。
- direct AoS family 比 staged SoA 更适合作为当前 production family。

不建议继续在同一轮扩大到泛型 traits、cache path、OMP path 或 histogram scatter RVV：

- 泛型 PointXYZ-like / Normal-like traits 需要公共 normal AoS gate、更多点型编译/运行证据和更宽 fallback 矩阵，范围已经超过当前 exact 点型生产扩展。
- cache path 涉及 pair feature cache 状态和 key 访问，不应在没有 profile 指向时接入。
- OMP path 涉及 OpenMP/RVV 嵌套调度和线程成本，需另开 topic。
- histogram scatter 或 125-bin copy 相比 O(k^2) pair math 占比小，且 scatter 有 bin conflict（直方图冲突累加）语义风险；当前没有证据显示值得优先接入。

因此本阶段暂停在“两个 exact 常见组合已采纳、其它方向暂缓 / 另开新 phase 或 topic”的状态。
