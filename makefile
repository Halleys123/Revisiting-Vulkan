# REQUIREMENTS
# On windows make command should be used from developer terminal only, otherwise issues may arise
CONFIG ?= Debug

GENERATOR = "Unix Makefiles"
FLAGS = -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=$(CONFIG)
UPDATE_COMPILE_COMMANDS = ./scripts/linux/get_compile_commands.bash

# TODO This script for now don't handle build for release or debug separately, so make those changes in future
# TODO Update this path for linux, I am not sure where will final executable land in linux
RUN_EXECUTABLE = ./build/main
# Removing old method now, because exe needs to read files relative to the location where it is, but by default realtive paths are read from the place where they are started
# RUN_DIR = ./build
# RUN_EXE = ./main

# var for clearing flags on Windows

ifeq ($(OS), Windows_NT)
	SHELL = powershell.exe
	.SHELLFLAGS = -NoProfile -Command

	GENERATOR = "Visual Studio 18 2026"
	FLAGS =
	RUN_EXECUTABLE = ./build/$(CONFIG)/main.exe
	UPDATE_COMPILE_COMMANDS = powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./scripts/win/get_compile_commands.ps1
endif

.PHONY: all build clangd run update_compile_commands

all: build

# This order is necessary because in linux build system should run first then only we can copy final command
build: build/CMakeCache.txt
	@echo "Building $(CONFIG)"
	cmake --build build --config $(CONFIG) --config $(CONFIG)

# Third command should not be removed as it stops multiple regeneration of build system by updating the MakeCache forcefully
build/CMakeCache.txt: CMakeLists.txt
	@echo Making the generator
	cmake -B ./build -S . -G $(GENERATOR) $(FLAGS)
	cmake -E touch build/CMakeCache.txt
	$(MAKE) update_compile_commands

update_compile_commands:
	$(UPDATE_COMPILE_COMMANDS)

clangd: compile_commands.json

compile_commands.json: CMakeLists.txt
	$(UPDATE_COMPILE_COMMANDS)

run: build
	$(RUN_EXECUTABLE)
