# 上游原始测试与 x86 SIMD 对照

## 上游原始测试

如果目标是公共模板头，或上游已有对应原始测试，应优先提供：

```text
run_upstream_test_std
run_upstream_test_rvv
run_upstream_test_compare
run_test_all
```

作用：

- 专项测试验证新增 RVV 路径。
- 上游原始测试验证原 API 行为未破坏。

上游原始测试不是每个主题的强制项。没有对应上游测试，或上游测试覆盖范围远大于当前函数时，在评估文档中说明不强制新增，不能写成验证失败。

## 参数与数据

若上游测试源码或构建脚本已有参数要求，应优先复用：

- 查看测试源码中的 `argc` 检查、文件读取路径和输出参数。
- 复用仓库 `test/` 中已有 PCD、txt、xml 等数据。
- 通过 `UPSTREAM_TEST_ARGS` 指向原路径，不复制测试数据到每个专项目录。
- 若测试需要输出文件或临时目录，Makefile 暴露可覆盖变量并确保目录存在。
- 直接 `run_upstream_test` 应有默认输出文件变量，避免日志命令退化为裸 `tee`。

如果上游测试出现链接或运行时依赖错误，不要直接写成环境阻塞。先检查交叉编译指南和既有专项 Makefile，补齐依赖目录、`LDFLAGS`、`LD_LIBRARY_PATH`、`LIBS_*` 和 rpath-link。只有补齐后仍失败，才记录为真实环境问题。

常见需要显式补齐或检查的依赖包括：

- `flann`
- `lz4`
- `hdf5`
- `zlib`
- `libpng`
- `boost_filesystem`
- `boost_iostreams`
- `boost_system`
- `pcl_search`
- `pcl_kdtree`
- `pcl_octree`
- `pcl_io`
- `pcl_sample_consensus`

处理顺序：

1. 查上游测试源码和 CMake 参数，确认运行参数、输入数据和输出文件。
2. 查交叉编译指南和已有同类专项 Makefile，确认依赖安装前缀、库目录和运行时搜索路径。
3. 在当前专项 Makefile 补 `LIB_DIRS_LIST`、`LDFLAGS`、`LD_LIBRARY_PATH`、`LIBS_*` 和 rpath-link。
4. 复跑 `run_upstream_test_std`、`run_upstream_test_rvv` 或 `run_upstream_test_compare`。
5. 只有补齐后仍缺库、缺数据或运行失败，才在评估文档中写成环境或数据阻塞，并附待补配置或待补跑命令。

## x86 SIMD 对照

如果目标文件已有 SSE/AVX 或其它 x86 SIMD 路径，可补：

```text
run_bench_x86_std
run_bench_x86_simd
run_bench_x86_compare
```

命名规则：

- RISC-V 对比侧叫 RVV。
- x86 对比侧叫 SIMD。
- 不使用混合命名。

x86 baseline 可关闭显式 SSE/AVX 宏并禁用自动向量化；x86 SIMD 使用宿主机合适的 native flags。x86 只用于平台内 baseline vs SIMD 对照，不与 RISC-V 目标硬件做绝对耗时比较。

## 双库或双构建对拍

当热点实现位于共享库或预编译库中，可执行文件宏不能改变已编译库内路径。此时需要两套库或两次构建分别代表 Std/RVV 路径。文档必须说明：

- 哪个翻译单元决定实际路径。
- bench 可执行宏只影响自身还是会影响库实现。
- 标量/RVV 库如何部署到目标硬件。
- 哪些日志来自同一轮、同一数据集和同一 iterations。
