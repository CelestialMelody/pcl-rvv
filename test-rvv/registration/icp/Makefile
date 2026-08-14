# =============================================================================
#  PCL registration/icp RVV diagnostic Makefile
# =============================================================================

TOPIC  := icp
MODULE := registration

FOCUS_DIR      := pcl/registration/include/pcl/registration/impl/icp.hpp
FILTER_REPORT  := log/filtered_icp.log
ANALYZE_REPORT := log/analyze_icp.log

TARGET_BENCH     := bench_icp
TARGET_BENCH_STD := bench_icp_std
TARGET_BENCH_RVV := bench_icp_rvv
SRCS_BENCH       := src/bench_icp.cpp

TARGET_TEST     := test_icp
TARGET_TEST_STD := test_icp_std
TARGET_TEST_RVV := test_icp_rvv
SRCS_TEST       := src/test_icp.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/kdtree/include \
	-I$(PCL_SOURCE_ROOT)/registration/include \
	-I$(PCL_SOURCE_ROOT)/search/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(FLANN_ROOT)/include \
	-I$(LZ4_ROOT)/include \
	-I$(GTEST_ROOT)/include

LIBS_CORE := \
	-lpcl_registration \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_common \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lflann -llz4 \
	-lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

include ../../mk/rvv-topic.mk

ICP_BOARD_RUNS ?= 5
ICP_BOARD_ITERATIONS ?= 20
ICP_BOARD_WARMUP_ITERATIONS ?= 3
ICP_BOARD_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/transform_cloud_repeated
ICP_BOARD_REPEATED_SUMMARY := $(ICP_BOARD_REPEATED_DIR)/summary.md
ICP_BOARD_EVIDENCE_MANIFEST := $(ICP_BOARD_REPEATED_DIR)/evidence_manifest.json
ICP_BOARD_EVIDENCE_DOCTOR_MD := $(ICP_BOARD_REPEATED_DIR)/evidence_doctor.md

QEMU_SMOKE_EVIDENCE_MANIFEST := $(OUTPUT_DIR_QEMU)/evidence_manifest.json
QEMU_SMOKE_EVIDENCE_DOCTOR_MD := $(OUTPUT_DIR_QEMU)/evidence_doctor.md
EVIDENCE_REGISTRY := $(LOG_DIR)/evidence_registry.json

print-%:
	@printf '%s\n' '$($*)'

generate_qemu_smoke_evidence_manifest: $(BENCH_COMPARE_OUTPUT_FILE)
	$(PYTHON) script/generate_icp_evidence_manifest.py \
		--summary $(BENCH_COMPARE_OUTPUT_FILE) \
		--std-log $(BENCH_STD_OUTPUT_FILE) \
		--rvv-log $(BENCH_RVV_OUTPUT_FILE) \
		--output $(QEMU_SMOKE_EVIDENCE_MANIFEST)

run_qemu_smoke_evidence_doctor: generate_qemu_smoke_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(QEMU_SMOKE_EVIDENCE_MANIFEST) \
		--output $(QEMU_SMOKE_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_qemu_smoke_evidence_state: run_qemu_smoke_evidence_doctor
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(TEST_STD_OUTPUT_FILE) \
		--path $(TEST_RVV_OUTPUT_FILE) \
		--path $(BENCH_STD_OUTPUT_FILE) \
		--path $(BENCH_RVV_OUTPUT_FILE) \
		--summary $(BENCH_COMPARE_OUTPUT_FILE) \
		--manifest $(QEMU_SMOKE_EVIDENCE_MANIFEST) \
		--doctor $(QEMU_SMOKE_EVIDENCE_DOCTOR_MD) \
		--target run_bench_compare \
		--backend qemu \
		--run-label qemu-transform-cloud-smoke-historical \
		--case-filter transform-cloud \
		--evidence-role qemu_smoke_only \
		--doc-ref README.zh.md \
		--doc-ref doc/icp-evaluation.zh.md \
		--doc-ref ../../../doc-rvv/registration/icp-RVV.zh.md

record_qemu_correctness_state:
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(TEST_STD_OUTPUT_FILE) \
		--path $(TEST_RVV_OUTPUT_FILE) \
		--target run_test_compare \
		--backend qemu \
		--run-label qemu-transform-cloud-correctness \
		--case-filter transform-cloud \
		--evidence-role qemu_correctness \
		--doc-ref README.zh.md \
		--doc-ref doc/icp-evaluation.zh.md \
		--doc-ref ../../../doc-rvv/registration/icp-RVV.zh.md

collect_board_transform_cloud_repeated:
	$(PYTHON) script/collect_icp_board_repeated.py \
		--runs $(ICP_BOARD_RUNS) \
		--iterations $(ICP_BOARD_ITERATIONS) \
		--warmup-iterations $(ICP_BOARD_WARMUP_ITERATIONS) \
		--output-dir $(ICP_BOARD_REPEATED_DIR)

generate_board_evidence_manifest: $(ICP_BOARD_REPEATED_SUMMARY)
	$(PYTHON) script/generate_icp_board_evidence_manifest.py \
		--summary $(ICP_BOARD_REPEATED_SUMMARY) \
		--output $(ICP_BOARD_EVIDENCE_MANIFEST) \
		--device "$(BOARD_LABEL)" \
		--runs $(ICP_BOARD_RUNS) \
		--iterations $(ICP_BOARD_ITERATIONS) \
		--warmup-iterations $(ICP_BOARD_WARMUP_ITERATIONS)

run_board_evidence_doctor: generate_board_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(ICP_BOARD_EVIDENCE_MANIFEST) \
		--output $(ICP_BOARD_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_evidence_state: run_board_evidence_doctor
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(BOARD_TEST_OUTPUT_FILE) \
		--summary $(ICP_BOARD_REPEATED_SUMMARY) \
		--manifest $(ICP_BOARD_EVIDENCE_MANIFEST) \
		--doctor $(ICP_BOARD_EVIDENCE_DOCTOR_MD) \
		--target collect_board_transform_cloud_repeated \
		--backend board \
		--run-label board-transform-cloud-repeated \
		--case-filter transform-cloud \
		--evidence-role production_direct \
		--doc-ref README.zh.md \
		--doc-ref doc/icp-evaluation.zh.md \
		--doc-ref ../../../doc-rvv/registration/icp-RVV.zh.md

record_evidence_state: record_board_evidence_state
run_evidence_doctor: run_board_evidence_doctor
generate_evidence_manifest: generate_board_evidence_manifest

evidence_status:
	$(PYTHON) ../../script/evidence_registry.py check \
		--registry $(EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/registration/icp/log/qemu/*.log' \
		--scan-glob 'test-rvv/registration/icp/log/qemu/*.md' \
		--scan-glob 'test-rvv/registration/icp/log/qemu/*.json' \
		--scan-glob 'test-rvv/registration/icp/log/board/*.log' \
		--scan-glob 'test-rvv/registration/icp/log/board/*.md' \
		--scan-glob 'test-rvv/registration/icp/log/board/*.json' \
		--scan-glob 'test-rvv/registration/icp/log/board/*/*.md' \
		--scan-glob 'test-rvv/registration/icp/log/board/*/*.json' \
		--exclude-glob 'test-rvv/registration/icp/log/board/analyze_bench_compare.log' \
		--exclude-glob 'test-rvv/registration/icp/log/board/run_bench_*.log' \
		--doc test-rvv/registration/icp/README.zh.md \
		--doc test-rvv/registration/icp/doc/icp-evaluation.zh.md \
		--doc doc-rvv/registration/icp-RVV.zh.md \
		--require-doc-ref \
		--fail-on never
