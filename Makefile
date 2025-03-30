######################################################################
# Set up variables

# If $VERILATOR_ROOT isn't in the environment, we assume it is part of a
# package install, and verilator is in your path. Otherwise find the
# binary relative to $VERILATOR_ROOT (such as when inside the git sources).
ifeq ($(VERILATOR_ROOT),)
VERILATOR = verilator
VERILATOR_COVERAGE = verilator_coverage
else
export VERILATOR_ROOT
VERILATOR = $(VERILATOR_ROOT)/bin/verilator
VERILATOR_COVERAGE = $(VERILATOR_ROOT)/bin/verilator_coverage
endif

CPPFLAGS= -Wall -Wextra -Wshadow -std=c++14 -O2 -g
VFLAGS = -Wall -j 0

VINCLUDE_DIR=V

.PHONY: all sim test clean

all: sim test

sim: build/rasterizer_sim
test: build/rasterizer_test

build/rasterizer_sim: sim/rasterizer.cpp V/Rasterizer.v
	@mkdir -p $(dir $@)
	verilator -I$(VINCLUDE_DIR) $(VFLAGS) --cc  --build --exe  V/Rasterizer.v sim/rasterizer.cpp -CFLAGS "$(CPPFLAGS)" -LDFLAGS "-lm $(shell pkg-config --libs glfw3 gl)"
	mv obj_dir/VRasterizer $@

build/rasterizer_test: test/rasterizer.cpp V/Rasterizer.v
	@mkdir -p $(dir $@)
	verilator -I$(VINCLUDE_DIR) $(VFLAGS) --cc  --build --exe  V/Rasterizer.v test/rasterizer.cpp -CFLAGS "$(CPPFLAGS)" -LDFLAGS "-lm"
	mv obj_dir/VRasterizer $@

clean:
	rm -rf build
	rm -rf obj_dir

