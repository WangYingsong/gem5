# created by Wang Yingsong on 2025-12-14
# tests/spme_benchmark/Makefile
# =============================================================
# SPME 基准测试 Makefile
# 功能：编译当前目录下的所有 C 源文件为静态可执行文件
# =============================================================

CC = gcc
CFLAGS = -static -pthread -O2 -Wall
SRC_DIR = .
BIN_DIR = bin

# 自动查找当前目录下所有的 .c 文件
SRCS = $(wildcard $(SRC_DIR)/*.c)
# 将 .c 文件名替换为 bin 目录下的可执行文件名
BINS = $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%, $(SRCS))

.PHONY: all clean

all: $(BINS)

# 编译规则
$(BIN_DIR)/%: $(SRC_DIR)/%.c
	@mkdir -p $(BIN_DIR)
	@echo "Compiling $< -> $@"
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(BIN_DIR)
