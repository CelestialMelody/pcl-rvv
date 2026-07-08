# =============================================================================
# Shared local/board environment for PCL RVV tests.
#
# This fragment owns machine-specific configuration: source/dependency paths,
# cross-toolchain prefix, library search paths, QEMU runtime, and optional board
# deployment settings. Test/bench build rules live in rvv-topic.mk.
# =============================================================================

# Keep piped test/benchmark commands honest: without pipefail, a failing test
# followed by "| tee log" reports success because tee exits with status 0.
SHELL := /bin/bash
.SHELLFLAGS := -o pipefail -c

# Path to test-rvv, inferred from this file's location. Keeping this independent
# from the including Makefile lets legacy tests include only rvv-env.mk.
ifndef TEST_RVV_ROOT
TEST_RVV_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
endif
TEST_RVV_SHARED_SCRIPT_DIR ?= $(TEST_RVV_ROOT)/script

# Optional local overrides. This file is intentionally not committed; copy
# test-rvv/config.mk.example to test-rvv/config.mk for machine-specific paths.
-include $(TEST_RVV_ROOT)/config.mk

# Build architecture currently supported by test-rvv. Kept configurable to make
# unsupported values fail with an explicit error rather than silently producing
# host binaries.
ARCH ?= riscv

# Host-side layout.
#
# PCL_SRC_DIR defaults to the parent of test-rvv, so a normal checkout works
# without hard-coded absolute paths. RV_INSTALL_DIR defaults to a sibling
# "riscv" directory only as a convenience convention; users with a different
# install layout should override it in test-rvv/config.mk or on the make command
# line. The legacy PCL_SOURCE_ROOT and RISCV_DEPS names are kept as aliases for
# topic Makefiles that already refer to them.
PCL_SRC_DIR      ?= $(abspath $(TEST_RVV_ROOT)/..)
PCL_SOURCE_ROOT  ?= $(PCL_SRC_DIR)
RV_INSTALL_DIR   ?= $(abspath $(PCL_SRC_DIR)/../riscv)
PCL_INSTALL_ROOT ?= $(RV_INSTALL_DIR)/pcl-rvv
RISCV_DEPS       ?= $(RV_INSTALL_DIR)

# Toolchain prefix. If the cross toolchain is in PATH, the default resolves to
# riscv64-unknown-linux-gnu-gcc/g++. Otherwise set CROSS_COMPILE to an absolute
# prefix such as /opt/riscv64/bin/riscv64-unknown-linux-gnu-.
RISCV_TARGET  ?= riscv64-unknown-linux-gnu
CROSS_COMPILE ?= $(RISCV_TARGET)-

ifeq ($(ARCH),riscv)
ifeq ($(origin CC),default)
CC = $(CROSS_COMPILE)gcc
endif
ifeq ($(origin CXX),default)
CXX = $(CROSS_COMPILE)g++
endif
STRIP ?= $(CROSS_COMPILE)strip
OBJDUMP ?= $(CROSS_COMPILE)objdump
RISCV_SYSROOT := $(shell $(CC) -print-sysroot)

# Third-party install roots. Override individual roots only when your
# RV_INSTALL_DIR layout uses different subdirectory names.
BOOST_ROOT  ?= $(RISCV_DEPS)/boost
EIGEN_ROOT  ?= $(RISCV_DEPS)/eigen-rvv
GTEST_ROOT  ?= $(RISCV_DEPS)/gtest
FLANN_ROOT  ?= $(RISCV_DEPS)/flann
LZ4_ROOT    ?= $(RISCV_DEPS)/lz4
HDF5_ROOT   ?= $(RISCV_DEPS)/hdf5
ZLIB_ROOT   ?= $(RISCV_DEPS)/zlib
LIBPNG_ROOT ?= $(RISCV_DEPS)/libpng

EIGEN_RVV_FLAGS ?= -DEIGEN_RISCV64_USE_RVV10 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl
LIB_DIRS_LIST ?= \
	$(PCL_INSTALL_ROOT)/lib \
	$(BOOST_ROOT)/lib \
	$(GTEST_ROOT)/lib \
	$(FLANN_ROOT)/lib \
	$(LZ4_ROOT)/lib \
	$(HDF5_ROOT)/lib \
	$(ZLIB_ROOT)/lib \
	$(LIBPNG_ROOT)/lib
empty :=
space := $(empty) $(empty)
LIB_PATH_VAL := $(subst $(space),:,$(LIB_DIRS_LIST))

LDFLAGS = $(foreach dir,$(LIB_DIRS_LIST),-L$(dir) -Wl,-rpath-link=$(dir))
RUN_CMD = LD_LIBRARY_PATH=$(LIB_PATH_VAL):$$LD_LIBRARY_PATH qemu-riscv64 -L $(RISCV_SYSROOT) -cpu rv64,v=true,vlen=256,elen=64

# Defaults used by legacy self-contained Makefiles. New topic Makefiles may
# override these after including rvv-env.mk when they need custom behavior.
CXXFLAGS_ARCH ?= -march=rv64gcv -mabi=lp64d \
	$(if $(LOG_FILE),-fopt-info-vec-missed=$(LOG_FILE)) \
	-DPCL_SILENCE_MALLOC_WARNING=1
CXXFLAGS_ARCH += $(EIGEN_RVV_FLAGS)
ifeq ($(USE_PCL_RVV10),1)
CXXFLAGS_ARCH += -D__RVV10__
endif

VEC_REGEX_STR ?= "[[:space:]]+v[a-z0-9]+(\.[a-z0-9]+)*[[:space:]]+"
endif

ifneq ($(filter $(ARCH),riscv),$(ARCH))
$(error Unsupported ARCH=$(ARCH), expected riscv)
endif

# Validate configured directories, not individual headers. This catches a fresh
# checkout with missing local dependency paths while keeping topic-specific
# source/header checks out of the common build fragment.
CONFIG_REQUIRED_DIRS := \
	$(PCL_SOURCE_ROOT) \
	$(RV_INSTALL_DIR) \
	$(PCL_INSTALL_ROOT)/lib
CONFIG_MISSING_DIRS := $(foreach dir,$(CONFIG_REQUIRED_DIRS),$(if $(wildcard $(dir)/.),,$(dir)))
ifneq ($(strip $(CONFIG_MISSING_DIRS)),)
$(error Required test-rvv directories do not exist: $(CONFIG_MISSING_DIRS). Set PCL_SRC_DIR/RV_INSTALL_DIR/PCL_INSTALL_ROOT in $(TEST_RVV_ROOT)/config.mk or on the make command line)
endif

# Optional board deployment settings. Leave connection details empty in the
# repository; board targets check them before attempting ssh/rsync.
REMOTE_USER ?=
REMOTE_IP   ?=
REMOTE_DIR  ?= /root/pcl-test/$(MODULE)/$(TOPIC)
REMOTE_SCRIPT_DIR = $(REMOTE_DIR)/script
REMOTE_BOARD_OUTPUT_DIR = $(REMOTE_DIR)/output
BOARD_BENCH_COMPARE_OUTPUT_FILE ?= $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log
BOARD_TEST_OUTPUT_FILE ?= $(OUTPUT_DIR_BOARD)/run_test.log
BOARD_LABEL ?= RVV board
SSH_OPTS ?= -F $(HOME)/.ssh/config
SSH_CMD ?= ssh $(SSH_OPTS)
RSYNC_SSH ?= ssh $(SSH_OPTS)
BOARD_RUN_MK ?= $(TEST_RVV_ROOT)/mk/rvv-board-run.mk
