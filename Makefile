# Builds a tape with the JVM and one class: make [CLASS=Name] [JAVA_DIR=dir]
# and zx-jvm.tap, which has only the JVM and expects a class as the next CODE block.

CLASS    ?= Fibonacci
JAVA_DIR ?= examples
BUILD    ?= build

JVM_ADDR   = 32768
CLASS_ADDR = 49152

export ZCCCFG ?= $(dir $(shell command -v zcc))../share/z88dk/lib/config/

TAPE = $(BUILD)/$(CLASS).tap
JVM_TAPE = $(BUILD)/zx-jvm.tap

all: $(TAPE) $(JVM_TAPE)

$(BUILD)/zxjvm.bin: src/zxjvm.c
	mkdir -p $(BUILD)
	zcc +zx -subtype=none -o $@ $<
	rm -f zcc_opt.def

$(BUILD)/$(CLASS).class: $(JAVA_DIR)/$(CLASS).java
	mkdir -p $(BUILD)
	javac -d $(BUILD) $<

$(TAPE): $(BUILD)/zxjvm.bin $(BUILD)/$(CLASS).class tools/MkTap.java
	java tools/MkTap.java $@ $(BUILD)/zxjvm.bin $(JVM_ADDR) $(CLASS_ADDR) $(BUILD)/$(CLASS).class $(CLASS)

$(JVM_TAPE): $(BUILD)/zxjvm.bin tools/MkTap.java
	java tools/MkTap.java $@ $(BUILD)/zxjvm.bin $(JVM_ADDR) $(CLASS_ADDR)

run: $(TAPE)
	fuse --machine 48 --auto-load --tape $(TAPE)

test:
	tests/run.sh

clean:
	rm -rf $(BUILD)

.PHONY: all run test clean
