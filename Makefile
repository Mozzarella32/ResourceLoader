BUILD_DIR := build

.PHONY: all debug release clean build clang-format clang-tidy

NUM_THREADS := $(shell nproc)

GENERATOR ?=

ifeq ($(GENERATOR),)
GENERATOR_FLAG := 
else
GENERATOR_FLAG := -G "$(GENERATOR)"
endif

all: release build

debug: clean
	@mkdir -p $(BUILD_DIR)
	cmake -S . -B $(BUILD_DIR) $(GENERATOR_FLAG) -DCMAKE_BUILD_TYPE=Debug

release: clean
	@mkdir -p $(BUILD_DIR)
	cmake -S . -B $(BUILD_DIR) $(GENERATOR_FLAG) -DCMAKE_BUILD_TYPE=Release

build:
	cmake --build $(BUILD_DIR) -j$(NUM_THREADS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET_PATH) $(LOG_PATH) $(RESOURCE_PATH)

clang-format:
	@CLANG_FORMAT=$$(command -v clang-format 2>/dev/null || true); \
	if [ -z "$$CLANG_FORMAT" ]; then \
		echo "clang-format not found; please install clang-format."; \
		exit 1; \
	fi; \
	FILES=$$(find include src -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" -o -name "*.h" -o -name "*.hpp" \) 2>/dev/null || true); \
	if [ -z "$$FILES" ]; then \
		echo "No source/header files found in include/ or src/ to format."; \
		exit 0; \
	fi; \
	printf "%s\n" $$FILES | xargs -r $$CLANG_FORMAT -style=file -i; \
	echo "clang-format: formatted files under include/ and src/."

clang-tidy:
	@CLANG_TIDY=$$(command -v clang-tidy 2>/dev/null || true); \
	if [ -z "$$CLANG_TIDY" ]; then \
		echo "clang-tidy not found; please install clang-tidy."; \
		exit 1; \
	fi; \
	if [ ! -f $(BUILD_DIR)/compile_commands.json ]; then \
		echo "compile_commands.json missing in $(BUILD_DIR). Run 'make configure' (or run cmake) to generate it."; \
		exit 1; \
	fi; \
	FILES=$$(find src -type f \( -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" -o -name "*.c" \) 2>/dev/null || true); \
	HEADER_FILES=$$(find include -type f \( -name "*.h" -o -name "*.hpp" -o -name "*.hh" \) 2>/dev/null || true); \
	ALL=$$(printf "%s\n%s\n" "$$FILES" "$$HEADER_FILES" | sed '/^$$/d'); \
	if [ -z "$$ALL" ]; then \
		echo "No C/C++ source/header files found in src/ or include/ to analyze."; \
		exit 0; \
	fi; \
	printf "%s\n" $$ALL | xargs -r -n 10 $$CLANG_TIDY -p $(BUILD_DIR) --quiet || true; \
	echo "clang-tidy: analysis complete for files under include/ and src/ (see output above)."
