# =============================================================================
# PCL sample_consensus/sac_model_plane RVV topic Makefile
#
# 本 topic 使用共享 rvv-topic.mk；QEMU 只跑 correctness / log-shape，
# 性能结论只来自 board 或目标硬件。
# =============================================================================

TOPIC  := sac_model_plane
MODULE := sample_consensus

EIGEN_RVV_FLAGS := -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl

FOCUS_DIR      := pcl/sample_consensus/include/pcl/sample_consensus
FILTER_REPORT  := log/filtered_sac_model_plane.log
ANALYZE_REPORT := log/analyze_sac_model_plane.log

TARGET_TEST     := test_sac_model_plane
TARGET_TEST_STD := test_sac_model_plane_std
TARGET_TEST_RVV := test_sac_model_plane_rvv
SRCS_TEST       := src/test_sac_model_plane.cpp

TARGET_BENCH     := bench_sac_model_plane
TARGET_BENCH_STD := bench_sac_model_plane_std
TARGET_BENCH_RVV := bench_sac_model_plane_rvv
SRCS_BENCH       := src/bench_sac_model_plane.cpp
BENCH_ARGS       ?= 65536 200

REMOTE_DIR ?= /root/pcl-test/sample_consensus/sac_model_plane
REMOTE_BENCH_ARGS ?= $(BENCH_ARGS)
REMOTE_TEST_ARGS ?=

BASE_PLANE_PUBLIC_FILTER := SampleConsensusModelPlane.PublicEntriesMatchDirectRVVForSupportedLayout:SampleConsensusModelPlane.PointXYZILayoutMatchesStandardPath:SampleConsensusModelPlane.AdditionalAoSPointTypesMatchStandardPath:SampleConsensusModelPlane.CloudOnlyIdentityIndicesMatchStandardPath:SampleConsensusModelPlane.ExplicitEmptyIndicesMatchStandardPath:SampleConsensusModelPlane.AdditionalAoSPointTypesIdentityMatchStandardPath:SampleConsensusModelPlane.SelectHelperResizesEmptyOutputBuffers

USE_PCL_SOURCE_HEADERS ?= 1
PCL_SOURCE_INCLUDES = \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT)/sample_consensus/include
ifeq ($(USE_PCL_SOURCE_HEADERS),1)
PCL_INCLUDE = $(PCL_SOURCE_INCLUDES) -I$(PCL_INSTALL_ROOT)/include/pcl-1.15
else
PCL_INCLUDE = -I$(PCL_INSTALL_ROOT)/include/pcl-1.15
endif

INCLUDES = \
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

LIBS_SAMPLE_CONSENSUS := \
	-lpcl_sample_consensus \
	-lpcl_common \
	-lpcl_io \
	-lpcl_search \
	-lpcl_kdtree \
	-lflann_cpp \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lz -lhdf5 -llz4 -lpng -lm
LIBS_BENCH := $(LIBS_SAMPLE_CONSENSUS)
LIBS_TEST  := $(LIBS_SAMPLE_CONSENSUS) -lgtest -lgtest_main -lpthread

include ../../mk/rvv-topic.mk

run_base_plane_public_tests:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(BASE_PLANE_PUBLIC_FILTER)"

run_board_base_plane_public_tests: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(BASE_PLANE_PUBLIC_FILTER)'"

generate_board_evidence_manifest:
	@python3 script/generate_board_evidence_manifest.py

generate_board_repeated_evidence_manifest:
	@python3 script/generate_board_evidence_manifest.py --repeat-root log/board/repeated

generate_phase_010_identity_evidence_manifest:
	@python3 script/generate_board_evidence_manifest.py \
		--repeat-root log/board/phase-010/identity-stride-select-count/repeated \
		--summary log/board/phase-010/identity-stride-select-count/repeated/summary.md \
		--output log/board/phase-010/identity-stride-select-count/repeated/evidence_manifest.json

generate_phase_010_shuffled_evidence_manifest:
	@python3 script/generate_board_evidence_manifest.py \
		--repeat-root log/board/phase-010/shuffled-stride-select-count/repeated \
		--summary log/board/phase-010/shuffled-stride-select-count/repeated/summary.md \
		--output log/board/phase-010/shuffled-stride-select-count/repeated/evidence_manifest.json

.PHONY: run_base_plane_public_tests run_board_base_plane_public_tests generate_board_evidence_manifest generate_board_repeated_evidence_manifest generate_phase_010_identity_evidence_manifest generate_phase_010_shuffled_evidence_manifest
