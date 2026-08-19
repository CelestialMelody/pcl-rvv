# organized_fast_mesh RVV 主题

这是 surface 模块 `organized_fast_mesh` 的 topic-local doc suite（主题本地文档套件）入口。
当前阶段只做诊断和实现准备，不修改 production 头文件。

## 当前结论

这个 topic 已进入函数级评估：主路径是 organized 网格的规则扫描，热点更像
finite mask（有限值掩码）+ adaptive diagonal choice（自适应对角线选择）+ 保序写回，
适合先做 production-shaped diagnostic（生产形态诊断）。

## 先读

1. `doc/organized_fast_mesh-evaluation.zh.md`
2. `doc/optimization-roadmap.zh.md`
3. `doc/phases/README.zh.md`
4. `doc/phases/000-current-state-and-gaps/plan.zh.md`
5. `doc/phases/000-current-state-and-gaps/result.zh.md`
6. `doc/test-support-code-map.zh.md`
7. `doc/testing-overview.zh.md`
8. `doc/correctness-tests.zh.md`
9. `doc/benchmark-and-evidence.zh.md`

## 常用命令

- `make -C test-rvv/surface/organized_fast_mesh run_test`
- `make -C test-rvv/surface/organized_fast_mesh run_test_compare`
- `make -C test-rvv/surface/organized_fast_mesh ALLOW_QEMU_BENCH_COMPARE=1 BENCH_ARGS='--iterations 2 --warmup-iterations 1' run_bench_compare`
- `make -C test-rvv/surface/organized_fast_mesh BENCH_ARGS='--iterations 20 --warmup-iterations 5' board_smoke`
- `make -C test-rvv/surface/organized_fast_mesh generate_vec_report`

## 证据白名单

- `log/qemu/`
- `log/board/`
- `log/evidence_registry.json`
