# =============================================================================
#  PCL registration/ndt RVV diagnostic Makefile
# =============================================================================

TOPIC  := ndt
MODULE := registration

FOCUS_DIR      := pcl/registration/include/pcl/registration/impl/ndt.hpp
FILTER_REPORT  := log/filtered_ndt.log
ANALYZE_REPORT := log/analyze_ndt.log

TARGET_BENCH     := bench_ndt
TARGET_BENCH_STD := bench_ndt_std
TARGET_BENCH_RVV := bench_ndt_rvv
TARGET_BENCH_GPROF_RVV := bench_ndt_rvv_gprof
SRCS_BENCH       := src/bench_ndt.cpp

TARGET_TEST     := test_ndt
TARGET_TEST_STD := test_ndt_std
TARGET_TEST_RVV := test_ndt_rvv
SRCS_TEST       := src/test_ndt.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/registration/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include \
	-I$(FLANN_ROOT)/include \
	-I$(LZ4_ROOT)/include \
	-I$(HDF5_ROOT)/include \
	-I$(ZLIB_ROOT)/include \
	-I$(LIBPNG_ROOT)/include

LIBS_CORE := \
	-lpcl_registration \
	-lpcl_common \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lflann -llz4 -lhdf5 -lz -lpng \
	-lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

NDT_QEMU_HESSIAN_SMOKE_ARGS ?= --case-filter hessian --samples 4096 --iterations 2 --warmup-iterations 1
NDT_QEMU_GRADIENT_SMOKE_ARGS ?= --case-filter gradient --samples 4096 --iterations 2 --warmup-iterations 1
NDT_QEMU_ALL_SMOKE_ARGS ?= --case-filter all --samples 4096 --iterations 2 --warmup-iterations 1
NDT_QEMU_PUBLIC_SMOKE_ARGS ?= --case-filter production-public-align-pointxyz --public-points 512 --public-max-iterations 2 --public-resolution 0.35 --public-search-method direct1 --iterations 1 --warmup-iterations 0

include ../../mk/rvv-topic.mk

QEMU_HESSIAN_BENCH_LOG := $(OUTPUT_DIR_QEMU)/run_bench_hessian_rvv.log
QEMU_GRADIENT_BENCH_LOG := $(OUTPUT_DIR_QEMU)/run_bench_gradient_rvv.log
QEMU_ALL_BENCH_LOG := $(OUTPUT_DIR_QEMU)/run_bench_all_rvv.log
QEMU_PUBLIC_BENCH_LOG := $(OUTPUT_DIR_QEMU)/run_bench_public_rvv.log
QEMU_SMOKE_EVIDENCE_MANIFEST := $(OUTPUT_DIR_QEMU)/evidence_manifest.json
QEMU_SMOKE_EVIDENCE_DOCTOR_MD := $(OUTPUT_DIR_QEMU)/evidence_doctor.md
EVIDENCE_REGISTRY := $(LOG_DIR)/evidence_registry.json

NDT_BOARD_REPEATED_RUNS ?= 5
NDT_BOARD_BENCH_ITERATIONS ?= 40
NDT_BOARD_BENCH_WARMUP_ITERATIONS ?= 5
NDT_BOARD_RUN_LABEL ?= derivative_accumulation_repeated
NDT_BOARD_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(NDT_BOARD_RUN_LABEL)
NDT_BOARD_SUMMARY := $(NDT_BOARD_REPEATED_DIR)/summary.md
NDT_BOARD_EVIDENCE_DOCTOR_MD := $(NDT_BOARD_REPEATED_DIR)/evidence_doctor.md
NDT_BOARD_BENCH_ARGS ?= --case-filter all --samples 65536 --iterations $(NDT_BOARD_BENCH_ITERATIONS) --warmup-iterations $(NDT_BOARD_BENCH_WARMUP_ITERATIONS)
NDT_BOARD_PUBLIC_RUN_LABEL ?= public_entry_profile_repeated
NDT_BOARD_PUBLIC_REPEATED_RUNS ?= 3
NDT_BOARD_PUBLIC_BENCH_ITERATIONS ?= 3
NDT_BOARD_PUBLIC_BENCH_WARMUP_ITERATIONS ?= 1
NDT_BOARD_PUBLIC_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(NDT_BOARD_PUBLIC_RUN_LABEL)
NDT_BOARD_PUBLIC_SUMMARY := $(NDT_BOARD_PUBLIC_REPEATED_DIR)/summary.md
NDT_BOARD_PUBLIC_EVIDENCE_DOCTOR_MD := $(NDT_BOARD_PUBLIC_REPEATED_DIR)/evidence_doctor.md
NDT_BOARD_PUBLIC_BENCH_ARGS ?= --case-filter production-public-align-pointxyz --public-points 4096 --public-max-iterations 8 --public-resolution 0.35 --public-search-method direct7 --iterations $(NDT_BOARD_PUBLIC_BENCH_ITERATIONS) --warmup-iterations $(NDT_BOARD_PUBLIC_BENCH_WARMUP_ITERATIONS)
NDT_BOARD_PUBLIC_PROFILE_DIR ?= $(OUTPUT_DIR_BOARD)/public_entry_profile_probe
NDT_BOARD_PUBLIC_PROFILE_ARGS ?= --case-filter production-public-align-pointxyz --public-points 4096 --public-max-iterations 8 --public-resolution 0.35 --public-search-method direct7 --iterations 1 --warmup-iterations 0
NDT_BOARD_PUBLIC_GPROF_DIR ?= $(OUTPUT_DIR_BOARD)/public_entry_gprof_probe
NDT_BOARD_REMOTE_LIB_DIR ?= /root/pcl-test/lib
NDT_BOARD_DOUBLE_EXP_RUN_LABEL ?= double_exp_probe_repeated
NDT_BOARD_DOUBLE_EXP_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(NDT_BOARD_DOUBLE_EXP_RUN_LABEL)
NDT_BOARD_DOUBLE_EXP_SUMMARY := $(NDT_BOARD_DOUBLE_EXP_REPEATED_DIR)/summary.md
NDT_BOARD_DOUBLE_EXP_EVIDENCE_DOCTOR_MD := $(NDT_BOARD_DOUBLE_EXP_REPEATED_DIR)/evidence_doctor.md

$(NDT_BOARD_REPEATED_DIR) $(NDT_BOARD_PUBLIC_REPEATED_DIR) $(NDT_BOARD_PUBLIC_PROFILE_DIR) $(NDT_BOARD_PUBLIC_GPROF_DIR) $(NDT_BOARD_DOUBLE_EXP_REPEATED_DIR) $(OUTPUT_DIR_BOARD)/test_smoke:
	mkdir -p $@

run_test_derivative:
	@$(MAKE) -C $(CURDIR) run_test_compare TEST_ARGS="--gtest_filter=NDTDerivativeDiagnostic.*"

run_bench_hessian_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="$(NDT_QEMU_HESSIAN_SMOKE_ARGS)" BENCH_RVV_OUTPUT_FILE=$(QEMU_HESSIAN_BENCH_LOG)

run_bench_gradient_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="$(NDT_QEMU_GRADIENT_SMOKE_ARGS)" BENCH_RVV_OUTPUT_FILE=$(QEMU_GRADIENT_BENCH_LOG)

run_bench_all_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="$(NDT_QEMU_ALL_SMOKE_ARGS)" BENCH_RVV_OUTPUT_FILE=$(QEMU_ALL_BENCH_LOG)

run_bench_public_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="$(NDT_QEMU_PUBLIC_SMOKE_ARGS)" BENCH_RVV_OUTPUT_FILE=$(QEMU_PUBLIC_BENCH_LOG)

generate_qemu_smoke_evidence_manifest: run_bench_all_smoke dump_bench_rvv
	$(PYTHON) script/generate_ndt_qemu_evidence_manifest.py \
		--rvv-log $(QEMU_ALL_BENCH_LOG) \
		--asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm \
		--output $(QEMU_SMOKE_EVIDENCE_MANIFEST)

run_qemu_smoke_evidence_doctor: generate_qemu_smoke_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(QEMU_SMOKE_EVIDENCE_MANIFEST) \
		--output $(QEMU_SMOKE_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_qemu_correctness_state: run_test_compare
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(TEST_STD_OUTPUT_FILE) \
		--path $(TEST_RVV_OUTPUT_FILE) \
		--target run_test_compare \
		--backend qemu \
		--run-label qemu-ndt-correctness-phase000 \
		--case-filter all \
		--evidence-role qemu_correctness \
		--doc-ref README.zh.md \
		--doc-ref doc/testing-overview.zh.md \
		--doc-ref doc/correctness-tests.zh.md \
		--doc-ref doc/ndt-evaluation.zh.md \
		--doc-ref doc/phases/000-current-state-and-gaps/result.zh.md \
		--doc-ref doc/phases/020-double-exp-helper-or-fused-formula-probe/result.zh.md

record_qemu_smoke_evidence_state: run_qemu_smoke_evidence_doctor
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(QEMU_ALL_BENCH_LOG) \
		--path $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm \
		--manifest $(QEMU_SMOKE_EVIDENCE_MANIFEST) \
		--doctor $(QEMU_SMOKE_EVIDENCE_DOCTOR_MD) \
		--target run_bench_all_smoke \
		--backend qemu \
		--run-label qemu-ndt-derivative-smoke-phase000 \
		--case-filter all \
		--evidence-role qemu_smoke_only \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/ndt-evaluation.zh.md \
		--doc-ref doc/phases/000-current-state-and-gaps/result.zh.md \
		--doc-ref doc/phases/020-double-exp-helper-or-fused-formula-probe/result.zh.md

record_qemu_public_smoke_state: run_bench_public_smoke
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(QEMU_PUBLIC_BENCH_LOG) \
		--target run_bench_public_smoke \
		--backend qemu \
		--run-label qemu-ndt-public-entry-smoke-phase010 \
		--case-filter production-public-align-pointxyz \
		--evidence-role qemu_smoke_only \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/ndt-evaluation.zh.md \
		--doc-ref doc/phases/010-public-entry-profile-or-closeout-confirmation/plan.zh.md \
		--doc-ref doc/phases/010-public-entry-profile-or-closeout-confirmation/result.zh.md

generate_evidence_manifest: summarize_board_ndt_derivative_repeated

run_evidence_doctor: generate_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(NDT_BOARD_REPEATED_DIR)/evidence_manifest.json \
		--output $(NDT_BOARD_EVIDENCE_DOCTOR_MD) \
		--fail-on never

run_board_test_smoke: deploy_board | $(OUTPUT_DIR_BOARD)/test_smoke
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_DIR)/log/board/test_smoke' REMOTE_TEST_ARGS='$(REMOTE_TEST_ARGS)'"
	@$(MAKE) -C $(CURDIR) fetch_board_logs OUTPUT_DIR_BOARD=$(OUTPUT_DIR_BOARD)/test_smoke REMOTE_BOARD_OUTPUT_DIR='$(REMOTE_DIR)/log/board/test_smoke'

check_board_profile_tools: check_board_ssh
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "set -e; if command -v perf >/dev/null 2>&1; then echo perf:available; else echo perf:missing; fi; if command -v timeout >/dev/null 2>&1; then echo timeout:available; else echo timeout:missing; fi"

collect_board_ndt_public_perf_probe: deploy_board | $(NDT_BOARD_PUBLIC_PROFILE_DIR)
	@rm -rf "$(NDT_BOARD_PUBLIC_PROFILE_DIR)"/*
	@remote_dir="$(REMOTE_DIR)/log/board/public_entry_profile_probe"; \
	$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "set -e; mkdir -p '$$remote_dir'; cd $(REMOTE_DIR); if command -v perf >/dev/null 2>&1; then perf record -F 99 -g -o '$$remote_dir/perf.data' ./$(TARGET_BENCH_RVV) $(NDT_BOARD_PUBLIC_PROFILE_ARGS) > '$$remote_dir/run_bench_rvv.log' 2> '$$remote_dir/perf_record.log' || true; perf report -i '$$remote_dir/perf.data' --stdio --no-children --sort symbol,dso > '$$remote_dir/perf_report_stdio.txt' 2> '$$remote_dir/perf_report.log' || true; else echo 'perf missing on board' > '$$remote_dir/perf_record.log'; ./$(TARGET_BENCH_RVV) $(NDT_BOARD_PUBLIC_PROFILE_ARGS) > '$$remote_dir/run_bench_rvv.log' 2>&1; fi"; \
	rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$(NDT_BOARD_PUBLIC_PROFILE_DIR)/"

build_gprof_bench_rvv:
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_BENCH=$(TARGET_BENCH_GPROF_RVV) EXTRA_CXXFLAGS='$(EXTRA_CXXFLAGS) -pg' EXTRA_LDFLAGS='$(EXTRA_LDFLAGS) -pg' $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_GPROF_RVV)

collect_board_ndt_public_gprof_probe: deploy_files build_gprof_bench_rvv | $(NDT_BOARD_PUBLIC_GPROF_DIR)
	@rm -rf "$(NDT_BOARD_PUBLIC_GPROF_DIR)"/*
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_GPROF_RVV) $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_BENCH_GPROF_RVV)
	@remote_dir="$(REMOTE_DIR)/log/board/public_entry_gprof_probe"; \
	$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "set -e; mkdir -p '$$remote_dir'; cd $(REMOTE_DIR); rm -f gmon.out; LD_LIBRARY_PATH=$(NDT_BOARD_REMOTE_LIB_DIR):\$$LD_LIBRARY_PATH ./$(TARGET_BENCH_GPROF_RVV) $(NDT_BOARD_PUBLIC_PROFILE_ARGS) > '$$remote_dir/run_bench_rvv.log' 2>&1; if [ -f gmon.out ]; then mv gmon.out '$$remote_dir/gmon.out'; gprof ./$(TARGET_BENCH_GPROF_RVV) '$$remote_dir/gmon.out' > '$$remote_dir/gprof.txt' 2> '$$remote_dir/gprof.log' || true; else echo 'gmon.out missing' > '$$remote_dir/gprof.log'; fi"; \
	rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$(NDT_BOARD_PUBLIC_GPROF_DIR)/"

collect_board_ndt_derivative_repeated: deploy_board | $(NDT_BOARD_REPEATED_DIR)
	@rm -rf "$(NDT_BOARD_REPEATED_DIR)"/run-*
	@set -e; for run in $$(seq 1 $(NDT_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(NDT_BOARD_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(NDT_BOARD_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] ndt derivative accumulation repeated run $$run_id/$(NDT_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(NDT_BOARD_BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

collect_board_ndt_public_repeated: deploy_board | $(NDT_BOARD_PUBLIC_REPEATED_DIR)
	@rm -rf "$(NDT_BOARD_PUBLIC_REPEATED_DIR)"/run-*
	@set -e; for run in $$(seq 1 $(NDT_BOARD_PUBLIC_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(NDT_BOARD_PUBLIC_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(NDT_BOARD_PUBLIC_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] ndt public-entry profile repeated run $$run_id/$(NDT_BOARD_PUBLIC_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(NDT_BOARD_PUBLIC_BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

summarize_board_ndt_derivative_repeated:
	$(PYTHON) script/generate_ndt_board_repeated_summary.py \
		--input-dir $(NDT_BOARD_REPEATED_DIR) \
		--output $(NDT_BOARD_SUMMARY) \
		--run-label $(NDT_BOARD_RUN_LABEL) \
		--expected-runs $(NDT_BOARD_REPEATED_RUNS) \
		--evidence-role pre_production_diagnostic \
		--boundary 'test helper' \
		--timer-boundary 'excludes voxel neighbor search, point derivative precompute, line search, Eigen solver and production dispatch.' \
		--title 'NDT derivative accumulation board repeated summary'

summarize_board_ndt_public_repeated:
	$(PYTHON) script/generate_ndt_board_repeated_summary.py \
		--input-dir $(NDT_BOARD_PUBLIC_REPEATED_DIR) \
		--output $(NDT_BOARD_PUBLIC_SUMMARY) \
		--run-label $(NDT_BOARD_PUBLIC_RUN_LABEL) \
		--expected-runs $(NDT_BOARD_PUBLIC_REPEATED_RUNS) \
		--evidence-role production_public_profile \
		--boundary 'public overload cross-build profile' \
		--timer-boundary 'public align; target voxel grid setup is outside timed loop, per-align derivative, line search and solver are included.' \
		--title 'NDT public-entry board repeated summary'

collect_board_ndt_double_exp_repeated: deploy_board | $(NDT_BOARD_DOUBLE_EXP_REPEATED_DIR)
	@rm -rf "$(NDT_BOARD_DOUBLE_EXP_REPEATED_DIR)"/run-*
	@set -e; for run in $$(seq 1 $(NDT_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(NDT_BOARD_DOUBLE_EXP_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(NDT_BOARD_DOUBLE_EXP_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] ndt double-exp probe repeated run $$run_id/$(NDT_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(NDT_BOARD_BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

summarize_board_ndt_double_exp_repeated:
	$(PYTHON) script/generate_ndt_board_repeated_summary.py \
		--input-dir $(NDT_BOARD_DOUBLE_EXP_REPEATED_DIR) \
		--output $(NDT_BOARD_DOUBLE_EXP_SUMMARY) \
		--run-label $(NDT_BOARD_DOUBLE_EXP_RUN_LABEL) \
		--expected-runs $(NDT_BOARD_REPEATED_RUNS) \
		--evidence-role pre_production_diagnostic \
		--boundary 'test helper with topic-local double exp prototype' \
		--timer-boundary 'excludes voxel neighbor search, point derivative precompute, line search, Eigen solver and production dispatch.' \
		--title 'NDT double exp probe board repeated summary'

record_board_ndt_derivative_state: summarize_board_ndt_derivative_repeated
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(NDT_BOARD_SUMMARY) \
		--manifest $(NDT_BOARD_REPEATED_DIR)/evidence_manifest.json \
		--doctor $(NDT_BOARD_EVIDENCE_DOCTOR_MD) \
		--target collect_board_ndt_derivative_repeated \
		--backend board \
		--run-label board-ndt-derivative-diagnostic-repeated-phase000 \
		--case-filter all \
		--evidence-role pre_production_diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/ndt-evaluation.zh.md \
		--doc-ref doc/phases/000-current-state-and-gaps/plan.zh.md \
		--doc-ref doc/phases/000-current-state-and-gaps/result.zh.md

run_public_evidence_doctor: summarize_board_ndt_public_repeated
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(NDT_BOARD_PUBLIC_REPEATED_DIR)/evidence_manifest.json \
		--output $(NDT_BOARD_PUBLIC_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_ndt_public_state: summarize_board_ndt_public_repeated
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(NDT_BOARD_PUBLIC_SUMMARY) \
		--manifest $(NDT_BOARD_PUBLIC_REPEATED_DIR)/evidence_manifest.json \
		--doctor $(NDT_BOARD_PUBLIC_EVIDENCE_DOCTOR_MD) \
		--target collect_board_ndt_public_repeated \
		--backend board \
		--run-label board-ndt-public-entry-profile-repeated-phase010 \
		--case-filter production-public-align-pointxyz \
		--evidence-role production_public_profile \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/ndt-evaluation.zh.md \
		--doc-ref doc/phases/010-public-entry-profile-or-closeout-confirmation/plan.zh.md \
		--doc-ref doc/phases/010-public-entry-profile-or-closeout-confirmation/result.zh.md

run_double_exp_evidence_doctor: summarize_board_ndt_double_exp_repeated
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(NDT_BOARD_DOUBLE_EXP_REPEATED_DIR)/evidence_manifest.json \
		--output $(NDT_BOARD_DOUBLE_EXP_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_ndt_double_exp_state: summarize_board_ndt_double_exp_repeated
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(NDT_BOARD_DOUBLE_EXP_SUMMARY) \
		--manifest $(NDT_BOARD_DOUBLE_EXP_REPEATED_DIR)/evidence_manifest.json \
		--doctor $(NDT_BOARD_DOUBLE_EXP_EVIDENCE_DOCTOR_MD) \
		--target collect_board_ndt_double_exp_repeated \
		--backend board \
		--run-label board-ndt-double-exp-probe-repeated-phase020 \
		--case-filter all \
		--evidence-role pre_production_diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/ndt-evaluation.zh.md \
		--doc-ref doc/phases/020-double-exp-helper-or-fused-formula-probe/plan.zh.md \
		--doc-ref doc/phases/020-double-exp-helper-or-fused-formula-probe/result.zh.md

run_board_bench_ndt_derivative_repeated: collect_board_ndt_derivative_repeated summarize_board_ndt_derivative_repeated run_evidence_doctor record_board_ndt_derivative_state
run_board_bench_ndt_public_repeated: collect_board_ndt_public_repeated summarize_board_ndt_public_repeated run_public_evidence_doctor record_board_ndt_public_state
run_board_bench_ndt_double_exp_repeated: collect_board_ndt_double_exp_repeated summarize_board_ndt_double_exp_repeated run_double_exp_evidence_doctor record_board_ndt_double_exp_state
run_board_bench_repeated: run_board_bench_ndt_derivative_repeated

evidence_status:
	$(PYTHON) ../../script/evidence_registry.py check \
		--registry $(EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/registration/ndt/log/qemu/*.log' \
		--scan-glob 'test-rvv/registration/ndt/log/qemu/*.md' \
		--scan-glob 'test-rvv/registration/ndt/log/board/**/summary.md' \
		--scan-glob 'test-rvv/registration/ndt/log/board/**/evidence_doctor.md' \
		--scan-glob 'test-rvv/registration/ndt/build/asm/riscv/*.asm' \
		--exclude-glob 'test-rvv/registration/ndt/build/asm/riscv/*.full.asm' \
		--doc test-rvv/registration/ndt/README.zh.md \
		--doc test-rvv/registration/ndt/doc/testing-overview.zh.md \
		--doc test-rvv/registration/ndt/doc/correctness-tests.zh.md \
		--doc test-rvv/registration/ndt/doc/benchmark-and-evidence.zh.md \
		--doc test-rvv/registration/ndt/doc/optimization-evidence.zh.md \
		--doc test-rvv/registration/ndt/doc/test-support-code-map.zh.md \
		--doc test-rvv/registration/ndt/doc/ndt-evaluation.zh.md \
		--doc test-rvv/registration/ndt/doc/phases/000-current-state-and-gaps/result.zh.md \
		--doc test-rvv/registration/ndt/doc/phases/010-public-entry-profile-or-closeout-confirmation/result.zh.md \
		--doc test-rvv/registration/ndt/doc/phases/020-double-exp-helper-or-fused-formula-probe/result.zh.md \
		--require-doc-ref \
		--fail-on never

.PHONY: run_test_derivative run_bench_hessian_smoke run_bench_gradient_smoke run_bench_all_smoke run_bench_public_smoke generate_qemu_smoke_evidence_manifest run_qemu_smoke_evidence_doctor record_qemu_correctness_state record_qemu_smoke_evidence_state record_qemu_public_smoke_state generate_evidence_manifest run_evidence_doctor run_board_test_smoke check_board_profile_tools collect_board_ndt_public_perf_probe build_gprof_bench_rvv collect_board_ndt_public_gprof_probe collect_board_ndt_derivative_repeated collect_board_ndt_public_repeated summarize_board_ndt_derivative_repeated summarize_board_ndt_public_repeated collect_board_ndt_double_exp_repeated summarize_board_ndt_double_exp_repeated record_board_ndt_derivative_state run_public_evidence_doctor record_board_ndt_public_state run_double_exp_evidence_doctor record_board_ndt_double_exp_state run_board_bench_ndt_derivative_repeated run_board_bench_ndt_public_repeated run_board_bench_ndt_double_exp_repeated run_board_bench_repeated evidence_status
