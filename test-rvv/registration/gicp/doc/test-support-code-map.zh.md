# GICP 测试支撑代码地图

## 当前结构

```text
include/gicp.h
  -> include/impl/gicp_fixtures.hpp
  -> include/impl/gicp_references.hpp
  -> include/impl/gicp_candidates.hpp
src/test_gicp.cpp
src/bench_gicp.cpp
script/generate_gicp_board_repeated_summary.py
```

## Shape Scan（形态扫描）

| shape | present | paths | roles found | risk if unchanged | decision | next action |
| --- | --- | --- | --- | --- | --- | --- |
| root test source | no | none | none | none | not_applicable with evidence | none |
| root bench source | no | none | none | none | not_applicable with evidence | none |
| `src/` source | yes | `src/test_gicp.cpp`、`src/bench_gicp.cpp` | test / bench entry | 当前足够短 | adopted | none |
| aggregator header | yes | `include/gicp.h` | stable include entry | none | adopted | none |
| internal helpers | yes | `include/impl/*.hpp` | fixtures / references / candidates | none | adopted | none |
| legacy `test_support/` | no | none | none | none | not_applicable with evidence | none |
| topic-local script | yes | `script/generate_gicp_board_repeated_summary.py` | board summary / manifest | needs validation | adopted | run after board |
| evidence registry | yes | `test-rvv/registration/gicp/log/evidence_registry.json` | freshness | registry 只登记 summary/log/doctor/asm，不登记 raw board run | adopted | rerun `make evidence_status` after doc updates |

## 证据输出地图

| output | path | publication decision |
| --- | --- | --- |
| QEMU Std correctness | `test-rvv/registration/gicp/log/qemu/run_test_std.log` | local-only raw log |
| QEMU RVV correctness | `test-rvv/registration/gicp/log/qemu/run_test_rvv.log` | local-only raw log |
| QEMU RVV bench smoke | `test-rvv/registration/gicp/log/qemu/run_bench_all_rvv.log` | local-only smoke log |
| QEMU asm dump | `test-rvv/registration/gicp/build/asm/riscv/bench_gicp_rvv.asm` | local-only build artifact |
| QEMU Evidence Doctor | `test-rvv/registration/gicp/log/qemu/evidence_doctor.md` | keep as metadata-limited aid |
| Board repeated summary | `test-rvv/registration/gicp/log/board/component_diagnostic_repeated/summary.md` | keep |
| Board repeated manifest | `test-rvv/registration/gicp/log/board/component_diagnostic_repeated/evidence_manifest.json` | local-only metadata |
| Board Evidence Doctor | `test-rvv/registration/gicp/log/board/component_diagnostic_repeated/evidence_doctor.md` | keep |
| Board Hessian loop summary | `test-rvv/registration/gicp/log/board/dfddf_loop_dense_repeated/summary.md` | keep |
| Board Hessian loop manifest | `test-rvv/registration/gicp/log/board/dfddf_loop_dense_repeated/evidence_manifest.json` | local-only metadata |
| Board Hessian loop Evidence Doctor | `test-rvv/registration/gicp/log/board/dfddf_loop_dense_repeated/evidence_doctor.md` | keep |
| Board production public summary | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_repeated/summary.md` | keep |
| Board production public manifest | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_repeated/evidence_manifest.json` | local-only metadata |
| Board production public Evidence Doctor | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_repeated/evidence_doctor.md` | keep |
| Board production public 4096 quick summary | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_4096_repeated/summary.md` | keep as expansion evidence |
| Board production public 4096 quick manifest | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_4096_repeated/evidence_manifest.json` | local-only metadata |
| Board production public 4096 quick Evidence Doctor | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_4096_repeated/evidence_doctor.md` | keep as expansion evidence |
| Board dfddf production public summary | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_repeated/summary.md` | keep |
| Board dfddf production public manifest | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_repeated/evidence_manifest.json` | local-only metadata |
| Board dfddf production public Evidence Doctor | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_repeated/evidence_doctor.md` | keep |
| Board dfddf production public 4096 quick summary | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_4096_repeated/summary.md` | keep as expansion evidence |
| Board dfddf production public 4096 quick manifest | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_4096_repeated/evidence_manifest.json` | local-only metadata |
| Board dfddf production public 4096 quick Evidence Doctor | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_4096_repeated/evidence_doctor.md` | keep as expansion evidence |
| Board clean dfddf production public summary | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_clean_repeated/summary.md` | keep as no-production decision evidence |
| Board clean dfddf production public manifest | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_clean_repeated/evidence_manifest.json` | local-only metadata |
| Board clean dfddf production public Evidence Doctor | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_clean_repeated/evidence_doctor.md` | keep as no-production decision evidence |
| Board clean dfddf production public 4096 quick summary | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_clean_4096_repeated/summary.md` | keep as no-production expansion evidence |
| Board clean dfddf production public 4096 quick manifest | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_clean_4096_repeated/evidence_manifest.json` | local-only metadata |
| Board clean dfddf production public 4096 quick Evidence Doctor | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_clean_4096_repeated/evidence_doctor.md` | keep as no-production expansion evidence |
| Board raw repeated dirs | `test-rvv/registration/gicp/log/board/component_diagnostic_repeated/run-*` | do not publish by default |
