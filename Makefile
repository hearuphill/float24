# 编译器
CXX = g++

# 编译选项
CXXFLAGS = -Wall -std=c++11

# 源文件目录
SRC_DIR = src

# 目标文件目录
OBJ_DIR = obj

# 二进制文件目录
BIN_DIR = bin

# 可执行文件名称
TARGET = $(BIN_DIR)/main

# 源文件
SRCS = $(wildcard $(SRC_DIR)/*.cpp)

# 目标文件
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# 生成的依赖文件
DEPS = $(OBJS:.o=.d)

# 默认目标
all: $(TARGET)

# 链接
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

# 编译
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

# 包含依赖文件
-include $(DEPS)

# 清理
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean
