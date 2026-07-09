# 2d 测试与基准

本目录已接入 `test-rvv/mk/rvv-topic.mk`。本目录只保留 2d topic 自己的源码、PCD 输入、依赖库、edge-store 微基准和 common/math 转调入口；通用构建、QEMU 运行、部署和板卡运行流程来自公共模板。

## 本机 QEMU

```bash
cd test-rvv/2d
make run_test
make run_bench
make run_bench_std
make run_bench_rvv
make run_bench_compare
```

- `run_test` 构建并运行 `build/riscv/test_2d`，输出保存到 `output/qemu/run_test.log`。
- `run_test` 会按固定顺序传入 `pcd/` 下 11 个 PCD 文件，顺序不能调整。
- `run_bench` 默认构建并运行 `build/riscv/bench_2d`，并生成 vec-missed 分析报告。
- `run_bench_std` 构建 `bench_2d_std`，关闭 `USE_PCL_RVV10`。
- `run_bench_rvv` 构建 `bench_2d_rvv`，开启 `USE_PCL_RVV10`。
- `run_bench_compare` 分别运行 std/RVV，并生成 `output/qemu/analyze_bench_compare.log`。

edge-store 微基准保留为本目录专有入口：

```bash
make run_edge_store_test
```

common/math 相关测试入口仍转调到 `test-rvv/common/common`：

```bash
make run_atan2_test
make run_acos_test
make run_expf_test
make run_expf_remez_vs_taylor
```

## 板卡

开发机侧部署：

```bash
cd test-rvv/2d
make deploy_board
make deploy_edge_store_test
```

`deploy_board` 会同步：

- `bench_2d_std`
- `bench_2d_rvv`
- `test_2d`
- `board.mk` 作为板卡侧 `Makefile`
- `script/analyze_bench_compare.py`
- `script/rvv-board-run.mk`
- `pcd/` 下的测试数据到 `/root/pcl-test/2d/pcds/`

板卡侧常用入口：

```bash
cd /root/pcl-test/2d
make run_test
make run_bench_compare
make run_bench_store_compare
```

板卡日志默认写入 `/root/pcl-test/2d/output/`。
