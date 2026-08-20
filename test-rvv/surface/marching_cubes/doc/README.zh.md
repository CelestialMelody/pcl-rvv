# marching_cubes topic 入口

这个 topic 已从 RVV 诊断推进到 generic production adopted（泛型生产采纳）状态。当前 `surface/include/pcl/surface/impl/marching_cubes.hpp` 在 `__RVV10__` 下使用 `pcl::rvv::RVVXYZAoSFloatLayout<PointNT>` 作为 gate（门禁）：满足 xyz AoS float traits（字段布局特征）的 `PointNT` 命中 active-cell prepass（活跃体素预扫描），不满足 gate 的模板实例回退原标量扫描。

## 先读什么

1. `doc/marching_cubes-evaluation.zh.md`
2. `doc/phases/README.zh.md`
3. `doc/optimization-roadmap.zh.md`
4. `doc/phases/optimization-matrix.zh.md`

## 当前状态

- Phase 000：edge interpolation 诊断，已完成。
- Phase 010：cube-index prepass 诊断，已完成，强正向。
- Phase 020：结构对齐 + 生产边界审计，已完成。
- Phase 030：`PointNormal` production direct 接入已完成并获用户确认保留。
- Phase 040：接入后稳态证据 + 泛型点类型扩展审计，已完成。
- Phase 050：generic point type expansion 已完成接入验证；四个代表点型 5-run board repeated 均为 positive，Evidence Doctor 无 finding。

## 常用命令

```bash
make -C test-rvv/surface/marching_cubes run_test_compare
make -C test-rvv/surface/marching_cubes dump_bench_rvv
make -C test-rvv/surface/marching_cubes run_board_edge_interpolation_evidence_doctor
make -C test-rvv/surface/marching_cubes run_board_prepass_evidence_doctor
make -C test-rvv/surface/marching_cubes run_board_production_direct_evidence_doctor
make -C test-rvv/surface/marching_cubes run_board_generic_repeated_evidence_doctor
```

## 可提交证据

- `doc/phases/*/result.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/marching_cubes-evaluation.zh.md`
- `doc-rvv/surface/marching_cubes-RVV.zh.md`

## 默认不提交

- `log/` 下的原始 board / QEMU 日志
- `build/` 下的二进制和 asm 产物
