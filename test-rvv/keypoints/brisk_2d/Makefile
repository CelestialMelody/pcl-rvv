# =============================================================================
# PCL keypoints/brisk_2d RVV topic Makefile
#
# 本 topic 先验证 BRISK scale-space downsample（尺度空间下采样）路径。
# QEMU 只用于 correctness（正确性）和日志形状；性能结论必须来自板卡或目标硬件。
# =============================================================================

TOPIC  := brisk_2d
MODULE := keypoints
TOPIC_DOC_PREFIX := test-rvv/keypoints/brisk_2d

FOCUS_DIR      := pcl/keypoints/src/brisk_2d.cpp
FILTER_REPORT  := log/filtered_brisk_2d.log
ANALYZE_REPORT := log/analyze_brisk_2d.log

TARGET_TEST     := test_brisk_2d
TARGET_TEST_STD := test_brisk_2d_std
TARGET_TEST_RVV := test_brisk_2d_rvv
SRCS_PRODUCTION = $(PCL_SOURCE_ROOT)/keypoints/src/brisk_2d.cpp $(PCL_SOURCE_ROOT)/keypoints/src/agast_2d.cpp
SRCS_TEST       = src/test_brisk_2d.cpp $(SRCS_PRODUCTION)

TARGET_BENCH     := bench_brisk_2d
TARGET_BENCH_STD := bench_brisk_2d_std
TARGET_BENCH_RVV := bench_brisk_2d_rvv
SRCS_BENCH       = src/bench_brisk_2d.cpp $(SRCS_PRODUCTION)
BENCH_ARGS       ?= --case-filter all --iterations 100 --warmup-iterations 10

REMOTE_DIR ?= /root/pcl-test/keypoints/brisk_2d
REMOTE_BENCH_ARGS ?= $(BENCH_ARGS)
REMOTE_TEST_ARGS ?=

BRISK_PHASE000_DIR := doc/phases/000-current-state-and-downsample-diagnostic
BRISK_PHASE010_DIR := doc/phases/010-public-compute-end-to-end
BRISK_EVIDENCE_REGISTRY := log/evidence_registry.json
BRISK_REPEATED_BOARD_DIR ?= log/board/repeated-phase000-downsample
BRISK_REPEATED_MANIFEST := $(BRISK_PHASE000_DIR)/repeated-evidence-manifest.json
BRISK_REPEATED_DOCTOR_MD := $(BRISK_PHASE000_DIR)/repeated-evidence-doctor.md
BRISK_REPEATED_DOCTOR_JSON := $(BRISK_PHASE000_DIR)/repeated-evidence-doctor.json
BRISK_REPEATED_RUNS ?= 5
BRISK_PUBLIC_REPEATED_BOARD_DIR ?= log/board/repeated-phase010-public-compute
BRISK_PUBLIC_REPEATED_MANIFEST := $(BRISK_PHASE010_DIR)/repeated-evidence-manifest.json
BRISK_PUBLIC_REPEATED_DOCTOR_MD := $(BRISK_PHASE010_DIR)/repeated-evidence-doctor.md
BRISK_PUBLIC_REPEATED_DOCTOR_JSON := $(BRISK_PHASE010_DIR)/repeated-evidence-doctor.json
BRISK_PUBLIC_BENCH_ARGS ?= --case-filter all --iterations 30 --warmup-iterations 5

USE_PCL_SOURCE_HEADERS ?= 1
PCL_SOURCE_INCLUDES = \
	-I$(PCL_SOURCE_ROOT)/keypoints/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/search/include \
	-I$(PCL_SOURCE_ROOT)/kdtree/include \
	-I$(PCL_SOURCE_ROOT)/test/include
ifeq ($(USE_PCL_SOURCE_HEADERS),1)
PCL_INCLUDE = $(PCL_SOURCE_INCLUDES) -I$(PCL_INSTALL_ROOT)/include/pcl-1.15
else
PCL_INCLUDE = -I$(PCL_INSTALL_ROOT)/include/pcl-1.15
endif

INCLUDES = \
	-I$(CURDIR)/include \
	$(PCL_INCLUDE) \
	-I$(PCL_SOURCE_ROOT) \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include \
	-I$(FLANN_ROOT)/include \
	-I$(LZ4_ROOT)/include \
	-I$(HDF5_ROOT)/include \
	-I$(ZLIB_ROOT)/include \
	-I$(LIBPNG_ROOT)/include

LIBS_KEYPOINTS := \
	-lpcl_common \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lz -lhdf5 -llz4 -lpng -lm
LIBS_TEST  := $(LIBS_KEYPOINTS) -lgtest -lgtest_main -lpthread
LIBS_BENCH := $(LIBS_KEYPOINTS)

include ../../mk/rvv-topic.mk

run_qemu_smoke: run_test_compare run_bench_rvv

collect_repeated_board_evidence:
	@rm -rf $(BRISK_REPEATED_BOARD_DIR)
	@set -e; for run in 01 02 03 04 05; do \
	  echo "[BOARD] brisk_2d downsample repeated run $$run -> $(BRISK_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(BRISK_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase000-downsample-run-$$run"; \
	done

generate_board_evidence_manifest:
	@python3 script/generate_brisk_2d_evidence_manifest.py \
	  --repeat-root $(BRISK_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_brisk_2d_rvv.full.asm \
	  --output $(BRISK_REPEATED_MANIFEST) \
	  --summary-title "brisk_2d Phase 000 repeated board summary" \
	  --summary-role production_direct

run_repeated_board_evidence_doctor: generate_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(BRISK_REPEATED_MANIFEST) \
	  --output $(BRISK_REPEATED_DOCTOR_MD) \
	  --json-output $(BRISK_REPEATED_DOCTOR_JSON) \
	  --fail-on never

record_repeated_board_evidence_state: run_repeated_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(BRISK_EVIDENCE_REGISTRY) \
	  --manifest $(BRISK_REPEATED_MANIFEST) \
	  --doctor $(BRISK_REPEATED_DOCTOR_MD) \
	  --path $(BRISK_REPEATED_DOCTOR_JSON) \
	  --target run_repeated_board_evidence_doctor \
	  --backend board \
	  --run-label brisk-2d-phase000-downsample-repeated-board \
	  --case-filter brisk_halfsample_640x480,brisk_halfsample_641x481_tail,brisk_twothirdsample_640x480,brisk_construct_pyramid_640x480 \
	  --evidence-role production_direct \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/000-current-state-and-downsample-diagnostic/result.zh.md \
	  --freshness-state recorded

collect_public_repeated_board_evidence:
	@rm -rf $(BRISK_PUBLIC_REPEATED_BOARD_DIR)
	@set -e; for run in 01 02 03 04 05; do \
	  echo "[BOARD] brisk_2d public compute repeated run $$run -> $(BRISK_PUBLIC_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(BRISK_PUBLIC_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase010-public-compute-run-$$run" \
	    BENCH_ARGS="$(BRISK_PUBLIC_BENCH_ARGS)"; \
	done

generate_public_board_evidence_manifest:
	@python3 script/generate_brisk_2d_evidence_manifest.py \
	  --repeat-root $(BRISK_PUBLIC_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_brisk_2d_rvv.full.asm \
	  --output $(BRISK_PUBLIC_REPEATED_MANIFEST) \
	  --summary-title "brisk_2d Phase 010 public compute repeated board summary" \
	  --summary-role production_mixed_detail_and_public

run_public_repeated_board_evidence_doctor: generate_public_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(BRISK_PUBLIC_REPEATED_MANIFEST) \
	  --output $(BRISK_PUBLIC_REPEATED_DOCTOR_MD) \
	  --json-output $(BRISK_PUBLIC_REPEATED_DOCTOR_JSON) \
	  --fail-on never

record_public_repeated_board_evidence_state: run_public_repeated_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(BRISK_EVIDENCE_REGISTRY) \
	  --manifest $(BRISK_PUBLIC_REPEATED_MANIFEST) \
	  --doctor $(BRISK_PUBLIC_REPEATED_DOCTOR_MD) \
	  --path $(BRISK_PUBLIC_REPEATED_DOCTOR_JSON) \
	  --target run_public_repeated_board_evidence_doctor \
	  --backend board \
	  --run-label brisk-2d-phase010-public-compute-repeated-board \
	  --case-filter brisk_halfsample_640x480,brisk_halfsample_641x481_tail,brisk_twothirdsample_640x480,brisk_construct_pyramid_640x480,brisk_public_compute_320x240 \
	  --evidence-role production_mixed_detail_and_public \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/010-public-compute-end-to-end/result.zh.md \
	  --freshness-state recorded

repeated_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(BRISK_EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(BRISK_REPEATED_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(BRISK_REPEATED_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(BRISK_REPEATED_DOCTOR_JSON)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(BRISK_PUBLIC_REPEATED_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(BRISK_PUBLIC_REPEATED_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(BRISK_PUBLIC_REPEATED_DOCTOR_JSON)' \
	  --doc $(TOPIC_DOC_PREFIX)/doc/phases/000-current-state-and-downsample-diagnostic/result.zh.md \
	  --doc $(TOPIC_DOC_PREFIX)/doc/phases/010-public-compute-end-to-end/result.zh.md \
	  --doc $(TOPIC_DOC_PREFIX)/doc/brisk_2d-evaluation.zh.md \
	  --fail-on never

.PHONY: run_qemu_smoke collect_repeated_board_evidence generate_board_evidence_manifest \
	run_repeated_board_evidence_doctor record_repeated_board_evidence_state \
	collect_public_repeated_board_evidence generate_public_board_evidence_manifest \
	run_public_repeated_board_evidence_doctor record_public_repeated_board_evidence_state \
	repeated_evidence_status
