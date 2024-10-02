OUTPUT_DIR := ./bin
SRC_FILES := $(shell find $(SOURCEDIR) -name '*.cpp')
OBJ_FILES := $(patsubst ./%.cpp,$(OUTPUT_DIR)/%.o,$(SRC_FILES))
CPPFLAGS := -std=c++11

$(OUTPUT_DIR)/%.o: ./%.cpp
	mkdir -p $(dir $@)
	g++ $(CPPFLAGS) -c -o $@ $< -I/usr/include/

all: $(OBJ_FILES) rth_converter

$(OBJ_FILES): | $(OUTPUT_DIR)

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

rth_converter: $(OBJ_FILES)
	g++ $(CPPFLAGS) -o $(OUTPUT_DIR)/$@ $^ /usr/lib64/libhdf5_cpp.so /usr/lib64/libhdf5.so /usr/lib64/libboost_date_time.so /usr/lib64/libboost_program_options.so
