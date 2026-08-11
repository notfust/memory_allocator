CC ?= cc
TRICORE_CC ?= tricore-gcc
CPPFLAGS ?= -Iincludes
COMMON_CFLAGS := -std=c99 -Wall -Wextra -Wconversion -Wpedantic
BUILD_DIR := build
HOST_DEBUG_TEST := $(BUILD_DIR)/host-debug/test_memory_allocator
HOST_RELEASE_TEST := $(BUILD_DIR)/host-release/test_memory_allocator
SANITIZER_TEST := $(BUILD_DIR)/host-sanitize/test_memory_allocator
TARGET_DEBUG_OBJECT := $(BUILD_DIR)/target-debug/memory_allocator.o
TARGET_RELEASE_OBJECT := $(BUILD_DIR)/target-release/memory_allocator.o
SOURCES := sources/memory_allocator.c tests/test_memory_allocator_alignment.c

.PHONY: all debug release test-host test-sanitize target-debug target-release clean

all: debug

debug: $(HOST_DEBUG_TEST)

release: $(HOST_RELEASE_TEST)

test-host: debug
	$(HOST_DEBUG_TEST)

$(HOST_DEBUG_TEST): $(SOURCES) includes/memory_allocator.h includes/memory_allocator_platform.h | $(BUILD_DIR)/host-debug
	$(CC) $(CPPFLAGS) $(COMMON_CFLAGS) -O0 -g $(SOURCES) -o $@

$(HOST_RELEASE_TEST): $(SOURCES) includes/memory_allocator.h includes/memory_allocator_platform.h | $(BUILD_DIR)/host-release
	$(CC) $(CPPFLAGS) $(COMMON_CFLAGS) -O2 $(SOURCES) -o $@

test-sanitize: $(SANITIZER_TEST)
	$(SANITIZER_TEST)

$(SANITIZER_TEST): $(SOURCES) includes/memory_allocator.h includes/memory_allocator_platform.h | $(BUILD_DIR)/host-sanitize
	$(CC) $(CPPFLAGS) $(COMMON_CFLAGS) -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer $(SOURCES) -o $@

target-debug: $(TARGET_DEBUG_OBJECT)

target-release: $(TARGET_RELEASE_OBJECT)

$(TARGET_DEBUG_OBJECT): sources/memory_allocator.c includes/memory_allocator.h includes/memory_allocator_platform.h | $(BUILD_DIR)/target-debug
	$(TRICORE_CC) $(CPPFLAGS) $(COMMON_CFLAGS) -O0 -g -DTRICORE_TARGET -c sources/memory_allocator.c -o $@

$(TARGET_RELEASE_OBJECT): sources/memory_allocator.c includes/memory_allocator.h includes/memory_allocator_platform.h | $(BUILD_DIR)/target-release
	$(TRICORE_CC) $(CPPFLAGS) $(COMMON_CFLAGS) -O2 -DTRICORE_TARGET -c sources/memory_allocator.c -o $@


$(BUILD_DIR) $(BUILD_DIR)/host-debug $(BUILD_DIR)/host-release $(BUILD_DIR)/host-sanitize $(BUILD_DIR)/target-debug $(BUILD_DIR)/target-release:
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)
