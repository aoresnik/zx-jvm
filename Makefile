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

# Python environment with SkoolKit, for the tests; kept by make clean
VENV = .venv
VENV_STAMP = $(VENV)/installed

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

$(VENV_STAMP): tests/requirements.txt
	python3 -m venv $(VENV)
	$(VENV)/bin/pip install -q -r tests/requirements.txt
	touch $@

venv: $(VENV_STAMP)

test: $(VENV_STAMP)
	tests/run.sh

clean:
	rm -rf $(BUILD)

.PHONY: all run venv test clean
