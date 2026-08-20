# correctness tests

`src/test_triangulation.cpp` 包含两个 GoogleTest case：

| test | 检查点 | 失败含义 |
| --- | --- | --- |
| `ParamGridCandidateMatchesScalarReference` | scalar reference 和 RVV candidate 生成的 `(u, v, z)` 点数、坐标和 checksum 一致。 | 参数网格写入语义不等价，禁止性能判断。 |
| `SurfaceCandidateMatchesScalarReference` | 测试专用 full-path helper 的点、polygon 和 checksum 一致。 | 参数点或 polygon 顺序破坏下游 mesh 语义，禁止性能判断。 |

当前复核命令：

```sh
make run_test_compare
```

结果：Std 和 RVV 两侧均为 `2 tests passed`。这是 QEMU correctness 证据，不是性能证据。

登记路径：

- `test-rvv/surface/triangulation/log/qemu/run_test_std.log`
- `test-rvv/surface/triangulation/log/qemu/run_test_rvv.log`
