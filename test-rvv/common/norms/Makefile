# =============================================================================
#  PCL common/norms RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk. The one-process Std vs RVV compare
# and vec probe remain norms-specific extensions.
# =============================================================================

TOPIC  := norms
MODULE := common

FOCUS_DIR      := pcl/common/include/pcl/common/impl/norms.hpp
FILTER_REPORT  := log/filtered_norms.log
ANALYZE_REPORT := log/analyze_norms.log

TARGET_BENCH     := bench_norms
TARGET_BENCH_STD := bench_norms_std
TARGET_BENCH_RVV := bench_norms_rvv
SRCS_BENCH       := bench_norms.cpp

TARGET_TEST     := test_norms
TARGET_TEST_STD := test_norms_std
TARGET_TEST_RVV := test_norms_rvv
BOARD_TARGET_TEST := test_norms
SRCS_TEST       := test_norms.cpp

TARGET_TEST_COMPARE ?= test_norms_std_vs_rvv_compare
SRCS_TEST_COMPARE   := test_norms_std_vs_rvv_compare.cpp
TEST_STD_VS_RVV_COMPARE_OUTPUT_FILE ?= $(OUTPUT_DIR_QEMU)/run_test_std_vs_rvv_compare.log

NORMS_PROBE_SRC := norms_vec_probe.cpp
NORMS_PROBE_OBJ = $(BUILD_DIR)/$(ARCH)/norms_vec_probe.o

REMOTE_DIR ?= ~/pcl-test/common/norms

LIBS_BENCH := -lpcl_common -lm
LIBS_TEST  := -lpcl_common -lgtest -lgtest_main -lpthread -lm

include ../../mk/rvv-topic.mk

TARGET_TEST_COMPARE_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_COMPARE)
CXXFLAGS_TEST_COMPARE := -std=c++17 -O3 -g -march=rv64gcv -mabi=lp64d \
	$(EIGEN_RVV_FLAGS) \
	-D__RVV10__ \
	-DPCL_NO_PRECOMPILE \
	-DPCL_SILENCE_MALLOC_WARNING=1 \
	$(INCLUDES) $(EXTRA_CXXFLAGS)
LIBS_TEST_COMPARE := -lpcl_common -lgtest -lpthread -lm

$(NORMS_PROBE_OBJ): $(NORMS_PROBE_SRC) | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] compile_norms_probe -> $(NORMS_PROBE_OBJ) (USE_PCL_RVV10=$(USE_PCL_RVV10))"
	$(CXX) $(CXXFLAGS) -c -o $(NORMS_PROBE_OBJ) $(NORMS_PROBE_SRC)

compile_norms_probe: $(NORMS_PROBE_OBJ)
	@echo "[DONE] $(NORMS_PROBE_OBJ) ; vec-missed under $(LOG_VEC_MISS_DIR)/"

$(TARGET_TEST_COMPARE_BIN): $(SRCS_TEST_COMPARE) | $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] test_norms_std_vs_rvv_compare (Std vs RVV in one process, -D__RVV10__)..."
	$(CXX) $(CXXFLAGS_TEST_COMPARE) $(SRCS_TEST_COMPARE) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_TEST_COMPARE) -o $(TARGET_TEST_COMPARE_BIN)

run_test_std_vs_rvv_compare: clean_test_std_vs_rvv_compare $(TARGET_TEST_COMPARE_BIN) | $(OUTPUT_DIR_QEMU)
	@echo "[RUN] test_norms_std_vs_rvv_compare..."
	$(RUN_CMD) ./$(TARGET_TEST_COMPARE_BIN) 2>&1 | tee $(TEST_STD_VS_RVV_COMPARE_OUTPUT_FILE)

clean_test_std_vs_rvv_compare:
	rm -f $(TARGET_TEST_COMPARE_BIN)

clean_probe:
	rm -f $(NORMS_PROBE_OBJ)

deploy_test_std_vs_rvv_compare: deploy_files clean_test_std_vs_rvv_compare
	@$(MAKE) -C $(CURDIR) $(TARGET_TEST_COMPARE_BIN)
	@$(STRIP) -s ./$(TARGET_TEST_COMPARE_BIN) -o ./$(TARGET_TEST_COMPARE)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_TEST_COMPARE)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_TEST_COMPARE)
	@rm -f ./$(TARGET_TEST_COMPARE)_stripped

clean: clean_test_std_vs_rvv_compare clean_probe

.PHONY: run_test_std_vs_rvv_compare clean_test_std_vs_rvv_compare compile_norms_probe clean_probe deploy_test_std_vs_rvv_compare
