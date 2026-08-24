# Integral Image Normal 测试支撑代码地图

## 本文职责

本文帮助 reviewer 从文档跳到 test support（测试支撑代码）、diagnostic helper（诊断辅助函数）、bench wrapper（性能测试包装）和 evidence script（证据脚本）。它不承担性能结论。

## 总调用图

```text
production scalar source
  features/include/pcl/features/impl/integral_image_normal.hpp
      |
      | production probe：computeFeature() map-prep 前缀 Std/RVV helper
      | 语义参考：initAverage3DGradientMethod() diff_x / diff_y 写入
      | 形态参考：AVERAGE_3D_GRADIENT diff / integral / query 三段 profile
      v
test-only aggregation
  include/integral_image_normal.h
      |
      v
internal helper
  include/impl/integral_image_normal_map_prep.hpp
      |                         |
      v                         v
src/test_integral_image_normal.cpp   src/bench_integral_image_normal.cpp
      |                         |
      v                         v
QEMU correctness logs          board repeated logs
                                |
                                v
script/generate_integral_image_normal_evidence_manifest.py
                                |
                                v
log/board/repeated-summary.md + evidence_manifest.json -> evidence_doctor.py
```

## 文件角色

| 文件 | 角色 | 证据边界 |
| --- | --- | --- |
| `include/integral_image_normal.h` | 聚合入口 | 只服务 test-rvv topic，不是 production API。 |
| `include/impl/integral_image_normal_map_prep.hpp` | reference / candidate helper | RVV candidate 与标量 fallback 在同一 test-only helper 中；当前包含 map-prep 和 avg3d diff-buffer helper，不证明 production dispatch。 |
| `features/include/pcl/features/impl/integral_image_normal.hpp` | production helper | Phase 050 新增 map-prep Std/RVV helper；只覆盖 public `compute()` 的前缀，fallback 到 Std。 |
| `src/test_integral_image_normal.cpp` | correctness harness（正确性测试框架） | 构造 reference，验证 map-prep / diff-buffer helper 语义；调用 public `compute()` 验证 production distance map。 |
| `src/bench_integral_image_normal.cpp` | diagnostic + production direct bench harness（诊断与生产直连性能测试框架） | 计 map-prep helper、diff-buffer helper、AVERAGE_3D_GRADIENT profile 和 `prod_compute_*` public 入口。 |
| `script/generate_integral_image_normal_evidence_manifest.py` | topic-local manifest wrapper（主题本地证据清单包装脚本） | 解析本 topic 的 case label、checksum、timer boundary 和 repeated run 目录。 |
| `Makefile` / `board.mk` | 构建、QEMU、board 和 evidence target | 复用 `test-rvv/mk/rvv-topic.mk` 和 `rvv-board-run.mk`；`run_board_integral_image_normal_repeated` 采集 5-run repeated。 |

## 拆分审计

当前结构已经采用配置默认的 `include/` 聚合入口、`include/impl/` 内部 helper 和 `src/` 测试 / bench 源文件。内部 helper 同时包含标量 fallback 和 RVV candidate，但职责仍集中在 integral_image_normal 的 test-only diagnostic helper 上，未达到继续拆分阈值。Phase 050 的 production helper 放在 production header 的 detail namespace（内部命名空间）中，test-only helper 没有混入 production 代码；production direct test / bench 通过 public API 调用真实路径。

## Evidence Output 关系

| 输出 | 生产动作 | 当前状态 |
| --- | --- | --- |
| `log/board/repeated-summary.md` | `make run_board_integral_image_normal_repeated` 或 `make run_board_evidence_doctor` 调用 topic wrapper 生成 | ignored-local summary evidence |
| `log/board/evidence_manifest.json` | 同上 | ignored-local manifest |
| `log/board/evidence_doctor.md` | `test-rvv/script/evidence_doctor.py` | ignored-local doctor report |
| `log/evidence_registry.json` | `make record_board_evidence_state` | ignored-local registry |

raw run logs 仅用于本机复核和重新生成 summary，不作为默认提交候选。
