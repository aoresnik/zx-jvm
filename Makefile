# Builds a tape with the JVM and one class: make [CLASS=Name] [JAVA_DIR=dir]
# and zx-jvm.tap, which has only the JVM and expects a class as the next CODE block.

CLASS    ?= Fibonacci
JAVA_DIR ?= examples
BUILD    ?= build

JVM_ADDR = 25600

export ZCCCFG ?= $(dir $(shell command -v zcc))../share/z88dk/lib/config/

TAPE = $(BUILD)/$(CLASS).tap
JVM_TAPE = $(BUILD)/zx-jvm.tap

# Python environment with SkoolKit, for the tests; kept by make clean
VENV = .venv
VENV_STAMP = $(VENV)/installed

all: $(TAPE) $(JVM_TAPE)

$(BUILD)/zxjvm.bin: src/zxjvm.c src/double.c src/double.h
	mkdir -p $(BUILD)
	zcc +zx -subtype=none -O3 -pragma-define:CRT_ORG_CODE=$(JVM_ADDR) -o $@ src/zxjvm.c src/double.c
	rm -f zcc_opt.def

$(BUILD)/$(CLASS).class: $(JAVA_DIR)/$(CLASS).java
	mkdir -p $(BUILD)
	javac -d $(BUILD) $<

$(TAPE): $(BUILD)/zxjvm.bin $(BUILD)/$(CLASS).class tools/MkTap.java
	java tools/MkTap.java $@ $(BUILD)/zxjvm.bin $(JVM_ADDR) $(BUILD)/$(CLASS).class $(CLASS)

$(JVM_TAPE): $(BUILD)/zxjvm.bin tools/MkTap.java
	java tools/MkTap.java $@ $(BUILD)/zxjvm.bin $(JVM_ADDR)

run: $(TAPE)
	fuse --machine 48 --auto-load --tape $(TAPE)

$(VENV_STAMP): tests/requirements.txt
	python3 -m venv $(VENV)
	$(VENV)/bin/pip install -q -r tests/requirements.txt
	touch $@

venv: $(VENV_STAMP)

# The double arithmetic, checked against the host's hardware and, compiled
# by sccz80, on the emulated Z80 against Python's floats
DOUBLE_DRIVER = $(BUILD)/tests/double_driver.bin
DOUBLE_HOST_TEST = $(BUILD)/tests/double_host_test

$(DOUBLE_DRIVER): tests/double/z80_driver.c src/double.c src/double.h
	mkdir -p $(dir $@)
	zcc +zx -subtype=none -O3 -o $@ tests/double/z80_driver.c src/double.c
	rm -f zcc_opt.def

$(DOUBLE_HOST_TEST): tests/double/host_test.c src/double.c src/double.h
	mkdir -p $(dir $@)
	$(CC) -std=c99 -O2 -Wall -Wextra -ffp-contract=off -o $@ tests/double/host_test.c src/double.c -lm

test-double: $(VENV_STAMP) $(DOUBLE_DRIVER) $(DOUBLE_HOST_TEST)
	$(DOUBLE_HOST_TEST)
	$(VENV)/bin/python tests/double/z80_test.py $(DOUBLE_DRIVER)

test: $(VENV_STAMP) test-double
	tests/run.sh

clean:
	rm -rf $(BUILD)

.PHONY: all run venv test test-double clean
