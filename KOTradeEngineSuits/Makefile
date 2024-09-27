OUTPUT_DIR := ./bin
SRC_FILES := $(shell find $(SOURCEDIR) -name '*.cpp')
OBJ_FILES := $(patsubst ./%.cpp,$(OUTPUT_DIR)/%.o,$(SRC_FILES))
CPPFLAGS := -std=c++11

$(OUTPUT_DIR)/%.o: ./%.cpp
	mkdir -p $(dir $@)
	g++ $(CPPFLAGS) -c -o $@ $< -I/dat/matterhorn/bin/vcpkg/installed/x64-linux/include/ -I/usr/include/ -fexceptions -Wno-deprecated

all: $(OBJ_FILES) KOTradeEngine

$(OBJ_FILES): | $(OUTPUT_DIR)

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

KOTradeEngine: $(OBJ_FILES)
	g++ $(CPPFLAGS) -o $(OUTPUT_DIR)/$@ $^ /dat/matterhorn/bin/vcpkg/installed/x64-linux/lib/libquickfix.a /dat/matterhorn/bin/vcpkg/installed/x64-linux/lib/libssl.a /usr/lib64/libboost_date_time.so /usr/lib64/libboost_program_options.so -lpthread -lz /usr/lib64/libhdf5_cpp.so /usr/lib64/libhdf5.so
