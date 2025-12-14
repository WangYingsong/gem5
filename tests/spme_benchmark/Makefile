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
