# Bench 输出合同

bench 原始输出应尽量保持同一版式，便于 QEMU 和板卡日志共用分析脚本。

推荐字段：

```text
Dataset: <workload>
Iterations: <N>
Build: <std|rvv>
<case-name>: <avg> ms/iter
Total Time: <total> ms
Checksum: <value>
```

## 检查项

运行 compare 后检查分析日志：

- `Dataset` 能解析到明确 workload。
- `Iterations` 能解析到实际迭代次数。
- `Total Time` 不为 `n/a`。
- std/RVV case 对齐。
- 没有条目缺失或表头错位。

如果同一份日志里多个 case 迭代次数不同，要么统一迭代次数，要么调整输出格式，让每项 Total Time 语义正确。

## Case 说明

每个 bench case 在文档中至少说明：

- 对应哪个函数入口。
- 数据规模、点类型、字段、indices 或参数组合。
- 是否命中 RVV 主路径、fallback 路径或间接受益路径。
- speedup 计算方式。
- 该 case 证明什么语义或性能点。

fallback case 用于证明未覆盖路径保持语义和成本接近，不作为 RVV 主路径性能结论。
