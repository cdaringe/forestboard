.DEFAULT_GOAL := help
PIO ?= $(shell command -v pio 2>/dev/null || if test -x .venv/bin/pio; then echo .venv/bin/pio; else echo $(HOME)/.platformio/penv/bin/pio; fi)
ENV ?= genericSTM32F411RE
ALL_ENVS := -e genericSTM32F411RE -e genericSTM32F446RE -e genericSTM32F411RE_oled_debug -e genericSTM32F446RE_oled_debug

.PHONY: help setup hooks build build-all upload debug-build debug-upload clean test format format-check check
help:
	@echo "forestboard: make setup | hooks | build | build-all | upload | test | check"
	@echo "             make debug-build | debug-upload | format | format-check | clean"
	@echo "Override board: make build ENV=genericSTM32F446RE; override CLI: PIO=/path/to/pio"
	@echo "Uploads require ROM DFU mode; release BOOT0 and tap RESET afterward."

setup: hooks
	python3 -m venv .venv
	.venv/bin/python -m pip install platformio
	@echo "PlatformIO installed in .venv. Native checks also require g++ and clang-format."

hooks:
	git config --local core.hooksPath .githooks

build:
	$(PIO) run -e $(ENV)

build-all:
	$(PIO) run $(ALL_ENVS)

upload:
	$(PIO) run -e $(ENV) -t upload

debug-build:
	$(PIO) run -e $(ENV)_oled_debug

debug-upload:
	$(PIO) run -e $(ENV)_oled_debug -t upload

clean:
	$(PIO) run -e $(ENV) -t clean

test:
	test/host/run.sh
	python3 test/test_format_hook.py

format:
	rg --files src include test/host -g '*.cpp' -g '*.h' | xargs clang-format -i

format-check:
	rg --files src include test/host -g '*.cpp' -g '*.h' | xargs clang-format --dry-run --Werror

check: format-check test build-all
