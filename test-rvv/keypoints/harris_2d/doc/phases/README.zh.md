# harris_2d phase index

| phase | status | 默认恢复动作 | plan | result |
| --- | --- | --- | --- | --- |
| 000-current-state-and-diagnostic-scaffold | completed | historical diagnostic baseline；不再作为 production 采纳依据 | `000-current-state-and-diagnostic-scaffold/plan.zh.md` | `000-current-state-and-diagnostic-scaffold/result.zh.md` |
| 020-direct-intensity-stride-store | completed / production adopted | 当前默认恢复到 review / optional expansion；没有未阻塞的同范围性能动作 | `020-direct-intensity-stride-store/plan.zh.md` | `020-direct-intensity-stride-store/result.zh.md` |

## 常用恢复命令

| 命令 | 用途 |
| --- | --- |
| `make -C test-rvv/keypoints/harris_2d run_test_compare` | QEMU correctness（正确性）对拍，Std/RVV 两侧都跑。 |
| `make -C test-rvv/keypoints/harris_2d check_harris_2d_rvv_asm` | 生成并检查 RVV 指令反汇编。 |
| `make -C test-rvv/keypoints/harris_2d board_repeated record_evidence_state_repeated REPEATED_BOARD_TAG=phase020_direct_intensity_stride_store_public_entry HARRIS2D_REPEATED_BENCH_ARGS="--case-filter all --iterations 20 --warmup-iterations 3 --public-entry"` | 板卡 5-run production direct、manifest、Evidence Doctor 和 registry。 |
| `make -C test-rvv/keypoints/harris_2d check_evidence_freshness` | 检查当前 summary / manifest / doctor 是否登记且被文档引用。 |
