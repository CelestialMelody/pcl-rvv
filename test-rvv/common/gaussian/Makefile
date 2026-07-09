# =============================================================================
#  PCL common/gaussian RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk. The local convolve compare and
# gaussian.cpp object-only diagnostic remain gaussian-specific extensions.
# =============================================================================

TOPIC  := gaussian
MODULE := common

FOCUS_DIR      := gaussian.cpp
FILTER_REPORT  := log/filtered_gaussian.log
ANALYZE_REPORT := log/analyze_gaussian.log

TARGET_BENCH     := bench_gaussian
TARGET_BENCH_STD := bench_gaussian_std
TARGET_BENCH_RVV := bench_gaussian_rvv
SRCS_BENCH       := bench_gaussian.cpp

TARGET_TEST     := test_gaussian
TARGET_TEST_STD := test_gaussian_std
TARGET_TEST_RVV := test_gaussian_rvv
BOARD_TARGET_TEST := test_gaussian
SRCS_TEST       := test_gaussian.cpp

TARGET_TEST_COMPARE ?= test_gaussian_convolve_compare
SRCS_TEST_COMPARE   := test_gaussian_convolve_compare.cpp
HDRS_TEST_COMPARE   := gaussian_convolve_float_local.hpp
TEST_COMPARE_OUTPUT_FILE ?= $(OUTPUT_DIR_QEMU)/run_test_convolve_compare.log

GAUSSIAN_CPP = $(PCL_SOURCE_ROOT)/common/src/gaussian.cpp
GAUSSIAN_OBJ = $(BUILD_DIR)/$(ARCH)/gaussian_cpp.o

BENCH_COMPARE_OUTPUT_FILE = $(OUTPUT_DIR_QEMU)/bench_compare.log
REMOTE_DIR ?= ~/pcl-test/common/gaussian

LIBS_BENCH := -lpcl_common -lm
LIBS_TEST  := -lpcl_common -lgtest -lpthread -lm

include ../../mk/rvv-topic.mk

TARGET_TEST_COMPARE_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_COMPARE)
INCLUDES_TEST_COMPARE := -I$(GTEST_ROOT)/include
CXXFLAGS_TEST_COMPARE := -std=c++17 -O3 -g -march=rv64gcv -mabi=lp64d \
	-D__RVV10__ \
	$(INCLUDES_TEST_COMPARE) $(EXTRA_CXXFLAGS)
LDFLAGS_TEST_COMPARE := -L$(GTEST_ROOT)/lib -Wl,-rpath-link=$(GTEST_ROOT)/lib
LIBS_TEST_COMPARE := -lgtest -lpthread -lm
RUN_CMD_TEST_COMPARE := LD_LIBRARY_PATH=$(GTEST_ROOT)/lib:$$LD_LIBRARY_PATH qemu-riscv64 -L $(RISCV_SYSROOT) -cpu rv64,v=true,vlen=256,elen=64

$(GAUSSIAN_OBJ): $(GAUSSIAN_CPP) | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] compile_gaussian_cpp (object only, for vec-missed on gaussian.cpp)..."
	$(CXX) $(CXXFLAGS) -c -o $(GAUSSIAN_OBJ) $(GAUSSIAN_CPP)

compile_gaussian_cpp: $(GAUSSIAN_OBJ)
	@echo "[DONE] $(GAUSSIAN_OBJ) ; see vec-missed under $(LOG_VEC_MISS_DIR)/"

$(TARGET_TEST_COMPARE_BIN): $(SRCS_TEST_COMPARE) $(HDRS_TEST_COMPARE) | $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] test_gaussian_convolve_compare: local hpp + gtest only (no -lpcl_common)..."
	$(CXX) $(CXXFLAGS_TEST_COMPARE) $(SRCS_TEST_COMPARE) $(LDFLAGS_TEST_COMPARE) $(EXTRA_LDFLAGS) $(LIBS_TEST_COMPARE) -o $(TARGET_TEST_COMPARE_BIN)

run_test_convolve_compare: clean_test_convolve_compare $(TARGET_TEST_COMPARE_BIN) | $(OUTPUT_DIR_QEMU)
	@echo "[RUN] test_gaussian_convolve_compare (no libpcl_common, always -D__RVV10__)..."
	$(RUN_CMD_TEST_COMPARE) ./$(TARGET_TEST_COMPARE_BIN) 2>&1 | tee $(TEST_COMPARE_OUTPUT_FILE)

generate_vec_report_gaussian_cpp: $(GAUSSIAN_OBJ) | $(LOG_DIR) $(LOG_VEC_MISS_DIR)
	@latest_file=""; \
	if ls "$(LOG_VEC_MISS_DIR)"/vec_missed_*.log >/dev/null 2>&1; then \
		latest_file=$$(realpath "$$(ls -1t "$(LOG_VEC_MISS_DIR)"/vec_missed_*.log | head -n 1)"); \
	else \
		: > "$(LATEST_VEC_LOG)"; \
	fi; \
	if [ -n "$$latest_file" ]; then \
		ln -sf "$$latest_file" "$(LATEST_VEC_LOG)"; \
	fi; \
	echo "[INFO] vec-missed for gaussian.cpp TU -> filter gaussian.cpp"; \
	grep 'gaussian.cpp' '$(LATEST_VEC_LOG)' -A 1 > '$(FILTER_REPORT)' || true; \
	$(PYTHON_RUN) "$(ANALYZE_VEC_SCRIPT)" "$(FILTER_REPORT)" -o "$(ANALYZE_REPORT)" -s "$(VEC_LOGS_DIR)"; \
	echo "[DONE] $(ANALYZE_REPORT)"

clean_test_convolve_compare:
	rm -f $(TARGET_TEST_COMPARE_BIN)

clean_gaussian_cpp:
	rm -f $(GAUSSIAN_OBJ)

REMOTE_LIB_DIR ?= ~/pcl-test/lib
TEMP_LIB_DIR ?= ./slim_lib
SYSROOT ?= $(RISCV_SYSROOT)

deploy_lib:
	@echo "[PREPARE] Creating local slim_lib directory..."
	@mkdir -p $(TEMP_LIB_DIR)
	@echo "[COPY] Collecting libraries..."
	@cp $(PCL_INSTALL_ROOT)/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
	@cp $(RISCV_DEPS)/boost/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
	@cp $(RISCV_DEPS)/lz4/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
	@cp $(RISCV_DEPS)/hdf5/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
	@cp $(RISCV_DEPS)/flann/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
	@cp $(RISCV_DEPS)/libpng/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
	@cp $(RISCV_DEPS)/zlib/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
	@cp $(RISCV_DEPS)/gtest/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
	@cp $(SYSROOT)/lib/libstdc++.so* $(SYSROOT)/lib/libc.so* $(SYSROOT)/lib/libm.so* $(SYSROOT)/lib/libgcc_s.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
	@echo "[STRIP] Slimming down libraries..."
	@$(STRIP) -s $(TEMP_LIB_DIR)/*.so* 2>/dev/null || true
	@echo "[DEPLOY] Syncing slim lib to board..."
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_LIB_DIR)"
	@rsync -e "$(RSYNC_SSH)" -avzP $(TEMP_LIB_DIR)/ $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_LIB_DIR)/
	@rm -rf $(TEMP_LIB_DIR)
	@echo "[DONE] All essential libs are deployed."

clean: clean_test_convolve_compare clean_gaussian_cpp

.PHONY: run_test_convolve_compare clean_test_convolve_compare compile_gaussian_cpp generate_vec_report_gaussian_cpp clean_gaussian_cpp deploy_lib
