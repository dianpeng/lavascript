# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.5

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/dianpeng/lavascript/dep/zydis-2.0.0-alpha2

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/dianpeng/lavascript/dep/zydis-2.0.0-alpha2/build

# Include any dependencies generated for this target.
include CMakeFiles/FormatterHooks.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/FormatterHooks.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/FormatterHooks.dir/flags.make

CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.o: CMakeFiles/FormatterHooks.dir/flags.make
CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.o: ../examples/FormatterHooks.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/dianpeng/lavascript/dep/zydis-2.0.0-alpha2/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.o"
	/usr/bin/cc  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.o   -c /home/dianpeng/lavascript/dep/zydis-2.0.0-alpha2/examples/FormatterHooks.c

CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.i"
	/usr/bin/cc  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/dianpeng/lavascript/dep/zydis-2.0.0-alpha2/examples/FormatterHooks.c > CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.i

CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.s"
	/usr/bin/cc  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/dianpeng/lavascript/dep/zydis-2.0.0-alpha2/examples/FormatterHooks.c -o CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.s

CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.o.requires:

.PHONY : CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.o.requires

CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.o.provides: CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.o.requires
	$(MAKE) -f CMakeFiles/FormatterHooks.dir/build.make CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.o.provides.build
.PHONY : CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.o.provides

CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.o.provides.build: CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.o


# Object files for target FormatterHooks
FormatterHooks_OBJECTS = \
"CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.o"

# External object files for target FormatterHooks
FormatterHooks_EXTERNAL_OBJECTS =

FormatterHooks: CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.o
FormatterHooks: CMakeFiles/FormatterHooks.dir/build.make
FormatterHooks: libZydis.a
FormatterHooks: CMakeFiles/FormatterHooks.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/dianpeng/lavascript/dep/zydis-2.0.0-alpha2/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable FormatterHooks"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/FormatterHooks.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/FormatterHooks.dir/build: FormatterHooks

.PHONY : CMakeFiles/FormatterHooks.dir/build

CMakeFiles/FormatterHooks.dir/requires: CMakeFiles/FormatterHooks.dir/examples/FormatterHooks.c.o.requires

.PHONY : CMakeFiles/FormatterHooks.dir/requires

CMakeFiles/FormatterHooks.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/FormatterHooks.dir/cmake_clean.cmake
.PHONY : CMakeFiles/FormatterHooks.dir/clean

CMakeFiles/FormatterHooks.dir/depend:
	cd /home/dianpeng/lavascript/dep/zydis-2.0.0-alpha2/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/dianpeng/lavascript/dep/zydis-2.0.0-alpha2 /home/dianpeng/lavascript/dep/zydis-2.0.0-alpha2 /home/dianpeng/lavascript/dep/zydis-2.0.0-alpha2/build /home/dianpeng/lavascript/dep/zydis-2.0.0-alpha2/build /home/dianpeng/lavascript/dep/zydis-2.0.0-alpha2/build/CMakeFiles/FormatterHooks.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/FormatterHooks.dir/depend
