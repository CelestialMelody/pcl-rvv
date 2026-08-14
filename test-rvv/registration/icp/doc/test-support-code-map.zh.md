# ICP transformCloud 测试支撑代码地图

## 本文职责

本文把 `test-rvv/registration/icp` 的测试支撑代码、script、output 和 production helper 对齐到可审查路径。
测试语义见 `doc/correctness-tests.zh.md`，bench/evidence 规则见 `doc/benchmark-and-evidence.zh.md`。

## 总调用图

```text
src/test_icp.cpp
  -> include/test_icp.h
    -> include/icp.h
      -> include/impl/icp_transform_cloud.hpp
  -> ExposedICP::transformPublic
    -> pcl::IterativeClosestPoint::transformCloud

src/bench_icp.cpp
  -> include/bench_icp.h
    -> include/icp.h
    -> ExposedICP::transformPublic
      -> pcl::IterativeClosestPoint::transformCloud

script/collect_icp_board_repeated.py
  -> make run_board_bench_compare + fetch_board_logs
  -> test-rvv/script/analyze_bench_repeated.py
  -> log/board/transform_cloud_repeated/summary.md

script/generate_icp_board_evidence_manifest.py
  -> log/board/transform_cloud_repeated/summary.md
  -> log/board/transform_cloud_repeated/evidence_manifest.json
  -> test-rvv/script/evidence_doctor.py
```

## 稳定聚合入口

| 文件 | 职责 | 下游 |
| --- | --- | --- |
| `include/icp.h` | topic-level test support 聚合入口。 | `include/test_icp.h`、`include/bench_icp.h`。 |
| `include/test_icp.h` | gtest 聚合入口，保持 test source 薄。 | `src/test_icp.cpp`。 |
| `include/bench_icp.h` | bench 聚合入口和 harness。 | `src/bench_icp.cpp`。 |
| `include/impl/icp_transform_cloud.hpp` | 内部 helper、fixtures、reference 和 diagnostic candidate。 | test / bench 间接使用。 |

当前 topic 已采用 `include/impl/` 内部目录，不保留 legacy `test_support/` 目录。

## Fixtures 与公共类型

| 符号 | 作用 | 使用者 |
| --- | --- | --- |
| `FieldLayout` | 保存运行期 `x/y/z/normal_x/normal_y/normal_z` offset。 | reference、candidate、fallback tests。 |
| `TransformStats` | 记录 input 点数、XYZ/normal 写回数和 RVV/fallback 分支。 | diagnostic gtest。 |
| `pointXYZLayout` / `pointNormalLayout` | 标准 PCL 点型 offsets。 | diagnostic tests。 |
| `makeRigidTransform` | 固定 3x4 rigid transform。 | tests 和 bench。 |
| `makePointXYZCloud` | synthetic `PointXYZ` cloud。 | tests 和 bench。 |
| `makePointNormalCloud` | synthetic `PointNormal` cloud。 | tests 和 bench。 |
| `makePointXYZICloud` | generic XYZ test fixture。 | `src/test_icp.cpp`。 |
| `makePointXYZINormalCloud` | generic XYZ+normal test fixture。 | `src/test_icp.cpp`。 |

## Reference 与 Candidate

| 符号 | 层级 | 职责 | production 对照 |
| --- | --- | --- | --- |
| `finiteFloat` | test utility | `std::isfinite` 包装。 | production Std helper 的 finite check。 |
| `transformCloudStd` | test reference | 用 `memcpy` 和 runtime offsets 复刻上游标量语义。 | `pcl::registration::detail::transformCloudStandard`。 |
| `finiteMaskF32M2` | test-only RVV helper | 生成 NaN/Inf mask。 | `finiteMaskF32M2ICP`。 |
| `transformPointXYZRVV` | test-only RVV candidate | `PointXYZ` RVV full-cloud transform。 | `transformCloudXYZRVV`。 |
| `transformPointNormalRVV` | test-only RVV candidate | `PointNormal` RVV XYZ+normal transform。 | `transformCloudXYZNormalRVV`。 |
| `transformCloudCandidate` | diagnostic dispatch | size/layout gate 后选择 test-only RVV 或 Std reference。 | `tryTransformCloudRVV` + `transformCloudStandard`。 |

Test-only candidate 保留是为了诊断和 regression 对照；production direct 证据必须通过
`IterativeClosestPoint::transformCloud`。

## Bench Harness 与 Case Registry

| 符号 / 文件 | 作用 |
| --- | --- |
| `rvv_icp_bench::ExposedICP` | 暴露 protected `transformCloud`，不改变 production 实现。 |
| `Benchmarker` | warmup、iteration timing 和 checksum 输出。 |
| `benchPointXYZ` | 运行 `PointXYZ` 64K / 256K cases。 |
| `benchPointNormal` | 运行 `PointNormal` 64K / 256K cases。 |
| `run_icp_bench` | 输出 banner、build 类型、dataset、iterations 和 4 个 case。 |
| `src/bench_icp.cpp` | 调用 `run_icp_bench()` 的薄 main。 |

## Script 与 Output

| 文件 / target | 职责 | 默认提交边界 |
| --- | --- | --- |
| `script/collect_icp_board_repeated.py` | 多轮 board Std/RVV compare，聚合 repeated summary。 | script 可提交；raw per-run log 默认 local-only。 |
| `script/generate_icp_board_evidence_manifest.py` | 从 board summary 生成 production_direct manifest。 | manifest 默认可重新生成，当前不作为提交主证据。 |
| `script/generate_icp_evidence_manifest.py` | 历史 QEMU smoke manifest。 | 只用于历史 smoke 边界。 |
| `log/board/transform_cloud_repeated/summary.md` | board repeated 摘要。 | 可提交 evidence summary。 |
| `log/board/transform_cloud_repeated/evidence_doctor.md` | board Evidence Doctor。 | 可提交 evidence summary。 |
| `log/qemu/evidence_doctor.md` | 历史 QEMU smoke Doctor。 | 可提交为历史边界说明。 |

## Production 代码对照

| production 符号 | 测试支撑对照 | 证据 |
| --- | --- | --- |
| `IterativeClosestPoint::transformCloud` | `ExposedICP::transformPublic` | production direct gtest、bench、asm。 |
| `transformCloudStandard` | `support::transformCloudStd` | fallback tests、Std/RVV comparison、asm symbol。 |
| `tryTransformCloudRVV` | `support::transformCloudCandidate` | layout / size / Scalar gate tests。 |
| `transformCloudXYZRVV` | `transformPointXYZRVV` | `PointXYZ` correctness 和 board repeated。 |
| `transformCloudXYZNormalRVV` | `transformPointNormalRVV` | `PointNormal` correctness 和 board repeated。 |

## Production 与 Test Support 边界

- `include/impl/icp_transform_cloud.hpp` 是 test-only diagnostic support，不被 production include。
- `registration/include/pcl/registration/impl/icp.hpp` 是唯一 production 入口。
- Bench 通过 production `transformCloud` 计时；不会直接调用 test-only RVV candidate。
- QEMU bench compare 是历史 smoke，不是默认恢复动作。
- Topic 没有 legacy root evaluation pointer、compatibility alias 或旧 `test_support/` wrapper。
