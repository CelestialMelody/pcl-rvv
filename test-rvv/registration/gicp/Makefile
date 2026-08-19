# =============================================================================
#  PCL registration/gicp RVV diagnostic Makefile
# =============================================================================

TOPIC  := gicp
MODULE := registration

FOCUS_DIR      := pcl/registration/include/pcl/registration/impl/gicp.hpp
FILTER_REPORT  := log/filtered_gicp.log
ANALYZE_REPORT := log/analyze_gicp.log

TARGET_BENCH     := bench_gicp
TARGET_BENCH_STD := bench_gicp_std
TARGET_BENCH_RVV := bench_gicp_rvv
SRCS_BENCH       := src/bench_gicp.cpp

TARGET_TEST     := test_gicp
TARGET_TEST_STD := test_gicp_std
TARGET_TEST_RVV := test_gicp_rvv
SRCS_TEST       := src/test_gicp.cpp

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

LIBS_CORE := -lpcl_registration -lpcl_search -lpcl_kdtree -lpcl_common -lflann_cpp -lboost_filesystem -lboost_iostreams -lboost_system -llz4 -lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

GICP_QEMU_RESIDUAL_SMOKE_ARGS ?= --case-filter residual --rows 4096 --iterations 2 --warmup-iterations 1
GICP_QEMU_COVARIANCE_SMOKE_ARGS ?= --case-filter covariance --points 512 --k 20 --iterations 2 --warmup-iterations 1
GICP_QEMU_ALL_SMOKE_ARGS ?= --case-filter all --rows 4096 --points 512 --k 20 --iterations 2 --warmup-iterations 1

include ../../mk/rvv-topic.mk

QEMU_RESIDUAL_BENCH_LOG := $(OUTPUT_DIR_QEMU)/run_bench_residual_rvv.log
QEMU_COVARIANCE_BENCH_LOG := $(OUTPUT_DIR_QEMU)/run_bench_covariance_rvv.log
QEMU_ALL_BENCH_LOG := $(OUTPUT_DIR_QEMU)/run_bench_all_rvv.log
QEMU_SMOKE_EVIDENCE_DOCTOR_MD := $(OUTPUT_DIR_QEMU)/evidence_doctor.md
EVIDENCE_REGISTRY := $(LOG_DIR)/evidence_registry.json

GICP_BOARD_REPEATED_RUNS ?= 5
GICP_BOARD_BENCH_ITERATIONS ?= 80
GICP_BOARD_BENCH_WARMUP_ITERATIONS ?= 10
GICP_BOARD_RUN_LABEL ?= component_diagnostic_repeated
GICP_BOARD_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(GICP_BOARD_RUN_LABEL)
GICP_BOARD_SUMMARY = $(GICP_BOARD_REPEATED_DIR)/summary.md
GICP_BOARD_EVIDENCE_MANIFEST = $(GICP_BOARD_REPEATED_DIR)/evidence_manifest.json
GICP_BOARD_EVIDENCE_DOCTOR_MD = $(GICP_BOARD_REPEATED_DIR)/evidence_doctor.md
GICP_BOARD_BENCH_ARGS ?= --case-filter all --rows 65536 --points 32768 --k 20 --iterations $(GICP_BOARD_BENCH_ITERATIONS) --warmup-iterations $(GICP_BOARD_BENCH_WARMUP_ITERATIONS)
GICP_PRODUCTION_BOARD_RUN_LABEL ?= production_public_align_pointxyz_repeated
GICP_PRODUCTION_BOARD_REPEATED_RUNS ?= 5
GICP_PRODUCTION_BOARD_BENCH_ARGS ?= --case-filter production --points 1024 --iterations 10 --warmup-iterations 2
GICP_PRODUCTION_BOARD_REPEATED_DIR = $(OUTPUT_DIR_BOARD)/$(GICP_PRODUCTION_BOARD_RUN_LABEL)
GICP_PRODUCTION_BOARD_SUMMARY = $(GICP_PRODUCTION_BOARD_REPEATED_DIR)/summary.md
GICP_PRODUCTION_BOARD_EVIDENCE_MANIFEST = $(GICP_PRODUCTION_BOARD_REPEATED_DIR)/evidence_manifest.json
GICP_PRODUCTION_BOARD_EVIDENCE_DOCTOR_MD = $(GICP_PRODUCTION_BOARD_REPEATED_DIR)/evidence_doctor.md

$(GICP_BOARD_REPEATED_DIR) $(GICP_PRODUCTION_BOARD_REPEATED_DIR) $(OUTPUT_DIR_BOARD)/test_smoke:
	mkdir -p $@

run_test_residual:
	@$(MAKE) -C $(CURDIR) run_test_compare TEST_ARGS="--gtest_filter=GICPResidualDiagnostic.*"

run_test_covariance:
	@$(MAKE) -C $(CURDIR) run_test_compare TEST_ARGS="--gtest_filter=GICPCovarianceDiagnostic.*"

run_bench_residual_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="$(GICP_QEMU_RESIDUAL_SMOKE_ARGS)" BENCH_RVV_OUTPUT_FILE=$(QEMU_RESIDUAL_BENCH_LOG)

run_bench_covariance_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="$(GICP_QEMU_COVARIANCE_SMOKE_ARGS)" BENCH_RVV_OUTPUT_FILE=$(QEMU_COVARIANCE_BENCH_LOG)

run_bench_all_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="$(GICP_QEMU_ALL_SMOKE_ARGS)" BENCH_RVV_OUTPUT_FILE=$(QEMU_ALL_BENCH_LOG)

run_qemu_smoke_evidence_doctor: run_bench_all_smoke dump_bench_rvv
	$(PYTHON) ../../script/evidence_doctor.py \
		--summary-md $(QEMU_ALL_BENCH_LOG) \
		--output $(QEMU_SMOKE_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_qemu_correctness_state: run_test_compare
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(TEST_STD_OUTPUT_FILE) \
		--path $(TEST_RVV_OUTPUT_FILE) \
		--target run_test_compare \
		--backend qemu \
		--run-label qemu-gicp-correctness-phase000 \
		--case-filter all \
		--evidence-role qemu_correctness \
		--doc-ref README.zh.md \
		--doc-ref doc/gicp-evaluation.zh.md \
		--doc-ref doc/phases/000-current-state-and-gaps/result.zh.md \
		--doc-ref doc/phases/003-clean-adoption/result.zh.md \
		--doc-ref doc/phases/004-dfddf-gather-width/result.zh.md \
		--doc-ref doc/phases/005-rollback-no-production-closeout/result.zh.md

record_qemu_smoke_evidence_state: run_qemu_smoke_evidence_doctor
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(QEMU_ALL_BENCH_LOG) \
		--path $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm \
		--doctor $(QEMU_SMOKE_EVIDENCE_DOCTOR_MD) \
		--target run_bench_all_smoke \
		--backend qemu \
		--run-label qemu-gicp-component-smoke-current \
		--case-filter all \
		--evidence-role qemu_smoke_only \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/gicp-evaluation.zh.md \
		--doc-ref doc/phases/000-current-state-and-gaps/result.zh.md \
		--doc-ref doc/phases/002-dfddf-production-probe/result.zh.md \
		--doc-ref doc/phases/005-rollback-no-production-closeout/result.zh.md

generate_evidence_manifest: generate_board_evidence_manifest_gicp_components
run_evidence_doctor: run_board_evidence_doctor_gicp_components

run_board_test_smoke: deploy_board | $(OUTPUT_DIR_BOARD)/test_smoke
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_DIR)/log/board/test_smoke' REMOTE_TEST_ARGS='$(REMOTE_TEST_ARGS)'"
	@$(MAKE) -C $(CURDIR) fetch_board_logs OUTPUT_DIR_BOARD=$(OUTPUT_DIR_BOARD)/test_smoke REMOTE_BOARD_OUTPUT_DIR='$(REMOTE_DIR)/log/board/test_smoke'

collect_board_gicp_components_repeated: deploy_board | $(GICP_BOARD_REPEATED_DIR)
	@rm -rf "$(GICP_BOARD_REPEATED_DIR)"/run-*
	@set -e; for run in $$(seq 1 $(GICP_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(GICP_BOARD_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(GICP_BOARD_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] gicp component diagnostic repeated run $$run_id/$(GICP_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(GICP_BOARD_BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

collect_board_gicp_production_public_repeated: deploy_board | $(GICP_PRODUCTION_BOARD_REPEATED_DIR)
	@rm -rf "$(GICP_PRODUCTION_BOARD_REPEATED_DIR)"/run-*
	@set -e; for run in $$(seq 1 $(GICP_PRODUCTION_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(GICP_PRODUCTION_BOARD_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(GICP_PRODUCTION_BOARD_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] gicp production public repeated run $$run_id/$(GICP_PRODUCTION_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(GICP_PRODUCTION_BOARD_BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_gicp_components:
	$(PYTHON) script/generate_gicp_board_repeated_summary.py \
		--input-dir $(GICP_BOARD_REPEATED_DIR) \
		--output-dir $(GICP_BOARD_REPEATED_DIR) \
		--run-label $(GICP_BOARD_RUN_LABEL) \
		--expected-runs $(GICP_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"

run_board_evidence_doctor_gicp_components: generate_board_evidence_manifest_gicp_components
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(GICP_BOARD_EVIDENCE_MANIFEST) \
		--output $(GICP_BOARD_EVIDENCE_DOCTOR_MD) \
		--fail-on never

generate_board_evidence_manifest_gicp_production_public:
	$(PYTHON) script/generate_gicp_board_repeated_summary.py \
		--input-dir $(GICP_PRODUCTION_BOARD_REPEATED_DIR) \
		--output-dir $(GICP_PRODUCTION_BOARD_REPEATED_DIR) \
		--run-label $(GICP_PRODUCTION_BOARD_RUN_LABEL) \
		--expected-runs $(GICP_PRODUCTION_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)" \
		--evidence-role production_public \
		--boundary "public GICP entry" \
		--decision-question "RVV-vs-scalar production public GICP PointXYZ align" \
		--summary-title "GICP production public PointXYZ board repeated summary"

run_board_evidence_doctor_gicp_production_public: generate_board_evidence_manifest_gicp_production_public
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(GICP_PRODUCTION_BOARD_EVIDENCE_MANIFEST) \
		--output $(GICP_PRODUCTION_BOARD_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_gicp_production_public_state: run_board_evidence_doctor_gicp_production_public
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(GICP_PRODUCTION_BOARD_SUMMARY) \
		--manifest $(GICP_PRODUCTION_BOARD_EVIDENCE_MANIFEST) \
		--doctor $(GICP_PRODUCTION_BOARD_EVIDENCE_DOCTOR_MD) \
		--target run_board_bench_gicp_production_public_repeated \
		--backend board \
		--run-label $(GICP_PRODUCTION_BOARD_RUN_LABEL) \
		--case-filter production \
		--evidence-role production_public \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/gicp-evaluation.zh.md \
		--doc-ref doc/phases/001-production-profile-prerequisite/result.zh.md \
		--doc-ref doc/phases/002-dfddf-production-probe/result.zh.md \
		--doc-ref doc/phases/003-clean-adoption/result.zh.md \
		--doc-ref doc/phases/004-dfddf-gather-width/result.zh.md \
		--doc-ref doc/phases/005-rollback-no-production-closeout/result.zh.md

record_board_gicp_components_state: run_board_evidence_doctor_gicp_components
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(GICP_BOARD_SUMMARY) \
		--manifest $(GICP_BOARD_EVIDENCE_MANIFEST) \
		--doctor $(GICP_BOARD_EVIDENCE_DOCTOR_MD) \
		--target collect_board_gicp_components_repeated \
		--backend board \
		--run-label board-gicp-component-diagnostic-repeated-phase000 \
		--case-filter all \
		--evidence-role pre_production_diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/gicp-evaluation.zh.md \
		--doc-ref doc/phases/000-current-state-and-gaps/plan.zh.md \
		--doc-ref doc/phases/000-current-state-and-gaps/result.zh.md

run_board_bench_gicp_components_repeated: collect_board_gicp_components_repeated record_board_gicp_components_state
run_board_bench_gicp_production_public_repeated: collect_board_gicp_production_public_repeated record_board_gicp_production_public_state
run_board_bench_repeated: run_board_bench_gicp_components_repeated

evidence_status:
	$(PYTHON) ../../script/evidence_registry.py check \
		--registry $(EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/registration/gicp/log/qemu/*.log' \
		--scan-glob 'test-rvv/registration/gicp/log/qemu/*.md' \
		--scan-glob 'test-rvv/registration/gicp/log/qemu/*.json' \
		--scan-glob 'test-rvv/registration/gicp/log/board/**/summary.md' \
		--scan-glob 'test-rvv/registration/gicp/log/board/**/evidence_doctor.md' \
		--scan-glob 'test-rvv/registration/gicp/log/board/**/evidence_manifest.json' \
		--scan-glob 'test-rvv/registration/gicp/build/asm/riscv/*.asm' \
		--exclude-glob 'test-rvv/registration/gicp/build/asm/riscv/*.full.asm' \
		--exclude-glob 'test-rvv/registration/gicp/log/qemu/run_bench_rvv.log' \
		--doc test-rvv/registration/gicp/README.zh.md \
		--doc test-rvv/registration/gicp/doc/testing-overview.zh.md \
		--doc test-rvv/registration/gicp/doc/correctness-tests.zh.md \
		--doc test-rvv/registration/gicp/doc/benchmark-and-evidence.zh.md \
		--doc test-rvv/registration/gicp/doc/optimization-evidence.zh.md \
		--doc test-rvv/registration/gicp/doc/test-support-code-map.zh.md \
		--doc test-rvv/registration/gicp/doc/gicp-evaluation.zh.md \
		--doc test-rvv/registration/gicp/doc/phases/000-current-state-and-gaps/result.zh.md \
		--doc test-rvv/registration/gicp/doc/phases/001-production-profile-prerequisite/result.zh.md \
		--doc test-rvv/registration/gicp/doc/phases/002-dfddf-production-probe/result.zh.md \
		--doc test-rvv/registration/gicp/doc/phases/003-clean-adoption/result.zh.md \
		--doc test-rvv/registration/gicp/doc/phases/004-dfddf-gather-width/result.zh.md \
		--doc test-rvv/registration/gicp/doc/phases/005-rollback-no-production-closeout/result.zh.md \
		--require-doc-ref \
		--fail-on never

.PHONY: run_test_residual run_test_covariance run_bench_residual_smoke run_bench_covariance_smoke run_bench_all_smoke run_qemu_smoke_evidence_doctor record_qemu_correctness_state record_qemu_smoke_evidence_state generate_evidence_manifest run_evidence_doctor run_board_test_smoke collect_board_gicp_components_repeated collect_board_gicp_production_public_repeated generate_board_evidence_manifest_gicp_components generate_board_evidence_manifest_gicp_production_public run_board_evidence_doctor_gicp_components run_board_evidence_doctor_gicp_production_public record_board_gicp_production_public_state record_board_gicp_components_state run_board_bench_gicp_components_repeated run_board_bench_gicp_production_public_repeated run_board_bench_repeated evidence_status
