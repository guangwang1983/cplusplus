OUTPUT_DIR := ./bin
SRC_FILES := $(shell find ./*/ -name '*.cpp')
OBJ_FILES := $(patsubst ./%.cpp,$(OUTPUT_DIR)/%.o,$(SRC_FILES))
CPPFLAGS := -std=c++11

all: KOTradeEngine SignalSimulator statsGenerator FXTradeEngineMultiBook

$(OUTPUT_DIR)/%.o: ./%.cpp
	mkdir -p $(dir $@)
	g++ $(CPPFLAGS) -c -o $@ $< -I/dat/matterhorn/bin/vcpkg/installed/x64-linux/include/ -I/usr/include/ -fexceptions -Wno-deprecated

$(OUTPUT_DIR)/TradeEngineMain.o: TradeEngineMain.cpp
	g++ $(CPPFLAGS) -c $< -o $@ -I/dat/matterhorn/bin/vcpkg/installed/x64-linux/include/ -I/usr/include/ -fexceptions -Wno-deprecated

$(OUTPUT_DIR)/SignalSimulatorMain.o: SignalSimulatorMain.cpp
	g++ $(CPPFLAGS) -c $< -o $@ -I/dat/matterhorn/bin/vcpkg/installed/x64-linux/include/ -I/usr/include/ -fexceptions -Wno-deprecated

$(OUTPUT_DIR)/statsGeneratorMain.o: statsGeneratorMain.cpp
	g++ $(CPPFLAGS) -c $< -o $@ -I/dat/matterhorn/bin/vcpkg/installed/x64-linux/include/ -I/usr/include/ -fexceptions -Wno-deprecated

$(OUTPUT_DIR)/FXTradeEngineMainMultiBook.o: FXTradeEngineMainMultiBook.cpp
	g++ $(CPPFLAGS) -c $< -o $@ -I/dat/matterhorn/bin/vcpkg/installed/x64-linux/include/ -I/usr/include/ -fexceptions -Wno-deprecated

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

KOTradeEngine: $(OBJ_FILES) $(OUTPUT_DIR)/TradeEngineMain.o
	g++ $(CPPFLAGS) -o $(OUTPUT_DIR)/$@ $^ /dat/matterhorn/bin/vcpkg/installed/x64-linux/lib/libquickfix.a /dat/matterhorn/bin/vcpkg/installed/x64-linux/lib/libssl.a /usr/lib64/libboost_date_time.so /usr/lib64/libboost_program_options.so -lpthread -lz /usr/lib64/libhdf5_cpp.so /usr/lib64/libhdf5.so

SignalSimulator: $(OBJ_FILES) $(OUTPUT_DIR)/SignalSimulatorMain.o
	g++ $(CPPFLAGS) -o $(OUTPUT_DIR)/$@ $^ /dat/matterhorn/bin/vcpkg/installed/x64-linux/lib/libquickfix.a /dat/matterhorn/bin/vcpkg/installed/x64-linux/lib/libssl.a /usr/lib64/libboost_date_time.so /usr/lib64/libboost_program_options.so -lpthread -lz /usr/lib64/libhdf5_cpp.so /usr/lib64/libhdf5.so

statsGenerator: $(OBJ_FILES) $(OUTPUT_DIR)/statsGeneratorMain.o
	g++ $(CPPFLAGS) -o $(OUTPUT_DIR)/$@ $^ /dat/matterhorn/bin/vcpkg/installed/x64-linux/lib/libquickfix.a /dat/matterhorn/bin/vcpkg/installed/x64-linux/lib/libssl.a /usr/lib64/libboost_date_time.so /usr/lib64/libboost_program_options.so -lpthread -lz /usr/lib64/libhdf5_cpp.so /usr/lib64/libhdf5.so

FXTradeEngineMultiBook: $(OBJ_FILES) $(OUTPUT_DIR)/FXTradeEngineMainMultiBook.o
	g++ $(CPPFLAGS) -o $(OUTPUT_DIR)/$@ $^ /dat/matterhorn/bin/vcpkg/installed/x64-linux/lib/libquickfix.a /dat/matterhorn/bin/vcpkg/installed/x64-linux/lib/libssl.a /usr/lib64/libboost_date_time.so /usr/lib64/libboost_program_options.so -lpthread -lz /usr/lib64/libhdf5_cpp.so /usr/lib64/libhdf5.so

