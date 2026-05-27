# test-rvv/common/norms

针对 `common/include/pcl/common/impl/norms.hpp` 的函数级评估见 [norms-evaluation.zh.md](./norms-evaluation.zh.md)。

**板卡**：将本目录与二进制部署到设备后，在设备上进入对应路径执行 `make -f board.mk run_bench_compare`（或 `run_bench_std` / `run_bench_rvv` / `run_test`）。路径默认值见 `board.mk` 中 `REMOTE_DIR`、`REMOTE_LIB_DIR`。

常用目标（开发机交叉编译 / QEMU）：

- `make compile_norms_probe USE_PCL_RVV10=0`：生成 `norms_vec_probe.o` 与 vec-missed 日志  
- `make run_bench_std` / `make run_bench_rvv`：基线与 RVV bench（QEMU）；`bench_norms` 输出 **us/iter**（微秒/次），便于读亚毫秒耗时  
- `make run_bench_compare`：对比两份 bench 日志；计时项名称含 `dim=`，与 `analyze_bench_compare.py` 的 **Avg (us)** 列一致（脚本亦兼容仅 **ms/iter** 的旧日志）  
- `make run_test_std` / `make run_test_rvv`：单测  

头文件实现随源码树 `-I` 生效；`libpcl_common` 仅链入满足依赖，范数主体为头内联。
