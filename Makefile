SHELL := /bin/bash
TOP   := $(CURDIR)

HOSTCC    ?= gcc
HOSTCXX   ?= g++
FROG      ?= /opt/frog-toolchain/mipsel-mti-elf_stable/bin/mipsel-mti-elf-
MIPS_AR   ?= $(FROG)ar
MULTICORE ?= $(HOME)/gb300/sf2000_multicore
SDCARD    ?= /media/$(USER)/GB300

SRC_DIR      := $(TOP)/src
INC_DIR      := $(TOP)/include
VENDOR_DIR   := $(TOP)/vendor
LUA_DIR      := $(VENDOR_DIR)/lua-5.1.5/src
LIBRETRO_DIR := $(TOP)/libretro
RUNNER_DIR   := $(TOP)/runner

BUILD_DIR    := $(TOP)/build
NATIVE_DIR   := $(BUILD_DIR)/native
GB300_DIR    := $(BUILD_DIR)/gb300
SF2000_DIR   := $(BUILD_DIR)/sf2000

INCLUDES     := -I$(INC_DIR) -I$(VENDOR_DIR) -I$(LUA_DIR) -I$(LIBRETRO_DIR)
COMMON_DEFS  := -DSCREEN_WIDTH=320 -DSCREEN_HEIGHT=240

LUA_SRCS     := $(wildcard $(LUA_DIR)/*.c)
LUA_LIB_SRCS := $(filter-out %/lua.c %/luac.c %/print.c, $(LUA_SRCS))

ENGINE_CPP   := $(wildcard $(SRC_DIR)/*.cpp)
ENGINE_C     := $(VENDOR_DIR)/miniz.c $(VENDOR_DIR)/bit.c
CORE_CPP     := $(LIBRETRO_DIR)/libretro_core.cpp

# Native flags (32-bit for consistency with console pointer sizes)
NATIVE_CFLAGS   := -m32 -O2 -g -fPIC $(INCLUDES) $(COMMON_DEFS) -Wall -Wno-unused-function
NATIVE_CXXFLAGS := -m32 -O2 -g -fPIC -std=gnu++11 $(INCLUDES) $(COMMON_DEFS) -Wall -Wno-unused-function

NATIVE_OBJS     := $(patsubst $(LUA_DIR)/%.c, $(NATIVE_DIR)/lua/%.o, $(LUA_LIB_SRCS)) \
                   $(patsubst $(VENDOR_DIR)/%.c, $(NATIVE_DIR)/vendor/%.o, $(ENGINE_C)) \
                   $(patsubst $(SRC_DIR)/%.cpp, $(NATIVE_DIR)/src/%.o, $(ENGINE_CPP)) \
                   $(NATIVE_DIR)/libretro/libretro_core.o

# MIPS flags
MIPS_ARCH     := -EL -march=mips32 -mtune=mips32r2 -msoft-float -G0 -mno-abicalls -fno-pic
MIPS_CFLAGS   := $(MIPS_ARCH) -O3 -ffast-math -funroll-loops -fomit-frame-pointer -fno-math-errno -fno-trapping-math -ffunction-sections -fdata-sections -I$(MULTICORE)/include $(INCLUDES) $(COMMON_DEFS) -DSF2000
MIPS_CXXFLAGS := $(MIPS_ARCH) -O3 -ffast-math -funroll-loops -fomit-frame-pointer -fno-math-errno -fno-trapping-math -std=gnu++11 -fno-strict-aliasing -ffunction-sections -fdata-sections -I$(MULTICORE)/include $(INCLUDES) $(COMMON_DEFS) -DSF2000

GB300_OBJS    := $(patsubst $(LUA_DIR)/%.c, $(GB300_DIR)/lua/%.o, $(LUA_LIB_SRCS)) \
                 $(patsubst $(VENDOR_DIR)/%.c, $(GB300_DIR)/vendor/%.o, $(ENGINE_C)) \
                 $(patsubst $(SRC_DIR)/%.cpp, $(GB300_DIR)/src/%.o, $(ENGINE_CPP)) \
                 $(GB300_DIR)/libretro/libretro_core.o

SF2000_OBJS   := $(patsubst $(LUA_DIR)/%.c, $(SF2000_DIR)/lua/%.o, $(LUA_LIB_SRCS)) \
                 $(patsubst $(VENDOR_DIR)/%.c, $(SF2000_DIR)/vendor/%.o, $(ENGINE_C)) \
                 $(patsubst $(SRC_DIR)/%.cpp, $(SF2000_DIR)/src/%.o, $(ENGINE_CPP)) \
                 $(SF2000_DIR)/libretro/libretro_core.o

.PHONY: all native runner test gb300 sf2000 gb300-core sf2000-core deploy dist clean

all: native runner gb300

# ---- Native Build ----
native: $(NATIVE_DIR)/libretro_love.so
runner: $(NATIVE_DIR)/test_runner_ddlc

$(NATIVE_DIR)/libretro_love.so: $(NATIVE_OBJS)
	@echo "  [LD] $@"
	@mkdir -p $(dir $@)
	@$(HOSTCXX) $(NATIVE_CXXFLAGS) -shared -o $@ $(NATIVE_OBJS) -lm -ldl

$(NATIVE_DIR)/test_runner_ddlc: $(RUNNER_DIR)/test_runner_ddlc.c | $(NATIVE_DIR)
	@echo "  [CC] $@"
	@$(HOSTCC) -m32 -O2 -g $(INCLUDES) -o $@ $< -ldl

$(NATIVE_DIR)/lua/%.o: $(LUA_DIR)/%.c
	@mkdir -p $(dir $@)
	@$(HOSTCC) $(NATIVE_CFLAGS) -c $< -o $@

$(NATIVE_DIR)/vendor/%.o: $(VENDOR_DIR)/%.c
	@mkdir -p $(dir $@)
	@$(HOSTCC) $(NATIVE_CFLAGS) -c $< -o $@

$(NATIVE_DIR)/src/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(HOSTCXX) $(NATIVE_CXXFLAGS) -c $< -o $@

$(NATIVE_DIR)/libretro/libretro_core.o: $(LIBRETRO_DIR)/libretro_core.cpp
	@mkdir -p $(dir $@)
	@$(HOSTCXX) $(NATIVE_CXXFLAGS) -c $< -o $@

# ---- Test Target ----
test: native runner
	@rm -rf screenshots && mkdir -p screenshots
	@$(NATIVE_DIR)/test_runner_ddlc $(NATIVE_DIR)/libretro_love.so game 6000 500 screenshots/ddlc
	@python3 -c "import glob; from PIL import Image; [Image.open(f).save(f[:-4]+'.png') for f in glob.glob('screenshots/*.ppm')]"
	@echo "==> Test complete! Screenshots stored in screenshots/"

# ---- MIPS (GB300) Build ----
gb300: $(GB300_DIR)/libretro_love.a

$(GB300_DIR)/libretro_love.a: $(GB300_OBJS)
	@echo "  [AR] $@"
	@mkdir -p $(dir $@)
	@$(MIPS_AR) rcs $@ $(GB300_OBJS)

$(GB300_DIR)/lua/%.o: $(LUA_DIR)/%.c
	@mkdir -p $(dir $@)
	@$(FROG)gcc $(MIPS_CFLAGS) -DGB300V2 -c $< -o $@

$(GB300_DIR)/vendor/%.o: $(VENDOR_DIR)/%.c
	@mkdir -p $(dir $@)
	@$(FROG)gcc $(MIPS_CFLAGS) -DGB300V2 -c $< -o $@

$(GB300_DIR)/src/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(FROG)g++ $(MIPS_CXXFLAGS) -DGB300V2 -c $< -o $@

$(GB300_DIR)/libretro/libretro_core.o: $(LIBRETRO_DIR)/libretro_core.cpp
	@mkdir -p $(dir $@)
	@$(FROG)g++ $(MIPS_CXXFLAGS) -DGB300V2 -c $< -o $@

# ---- MIPS (SF2000) Build ----
sf2000: $(SF2000_DIR)/libretro_love.a

$(SF2000_DIR)/libretro_love.a: $(SF2000_OBJS)
	@echo "  [AR] $@"
	@mkdir -p $(dir $@)
	@$(MIPS_AR) rcs $@ $(SF2000_OBJS)

$(SF2000_DIR)/lua/%.o: $(LUA_DIR)/%.c
	@mkdir -p $(dir $@)
	@$(FROG)gcc $(MIPS_CFLAGS) -c $< -o $@

$(SF2000_DIR)/vendor/%.o: $(VENDOR_DIR)/%.c
	@mkdir -p $(dir $@)
	@$(FROG)gcc $(MIPS_CFLAGS) -c $< -o $@

$(SF2000_DIR)/src/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(FROG)g++ $(MIPS_CXXFLAGS) -c $< -o $@

$(SF2000_DIR)/libretro/libretro_core.o: $(LIBRETRO_DIR)/libretro_core.cpp
	@mkdir -p $(dir $@)
	@$(FROG)g++ $(MIPS_CXXFLAGS) -c $< -o $@

# ---- Multicore Core Packaging ----
gb300-core: gb300
	@echo "==> Cleaning multicore build for GB300V2..."
	@rm -rf $(MULTICORE)/build
	@cp -f $(GB300_DIR)/libretro_love.a $(TOP)/cores/ddlc/ddlc_libretro_gb300.a
	@echo "==> Building GB300V2 core_87000000 via multicore..."
	@$(MAKE) -C $(MULTICORE) CORE=$(TOP)/cores/ddlc CONSOLE=ddlc FROGGY_TYPE=GB300V2 -j$(shell nproc)
	@cp -f $(MULTICORE)/build/core_87000000 $(TOP)/build/core_87000000_gb300v2
	@echo "==> GB300 core created: $(TOP)/build/core_87000000_gb300v2"

sf2000-core: sf2000
	@echo "==> Cleaning multicore build for SF2000..."
	@rm -rf $(MULTICORE)/build
	@cp -f $(SF2000_DIR)/libretro_love.a $(TOP)/cores/ddlc/ddlc_libretro_sf2000.a
	@echo "==> Building SF2000 core_87000000 via multicore..."
	@$(MAKE) -C $(MULTICORE) CORE=$(TOP)/cores/ddlc CONSOLE=ddlc FROGGY_TYPE=SF2000 -j$(shell nproc)
	@cp -f $(MULTICORE)/build/core_87000000 $(TOP)/build/core_87000000_sf2000
	@echo "==> SF2000 core created: $(TOP)/build/core_87000000_sf2000"

# ---- Deployment to SD Card ----
deploy: gb300-core
	@mountpoint -q "$(SDCARD)" || { echo "ERROR: $(SDCARD) is not a mounted SD card" >&2; exit 1; }
	@echo "==> Deploying DDLC to SD Card at $(SDCARD)..."
	@mkdir -p "$(SDCARD)/cores/ddlc"
	@mkdir -p "$(SDCARD)/system/Deimos/cores"
	@mkdir -p "$(SDCARD)/configs/custom_cores/ddlc"
	@mkdir -p "$(SDCARD)/ROMS/ddlc"
	@mkdir -p "$(SDCARD)/bios"
	@cp -f "$(TOP)/build/core_87000000_gb300v2" "$(SDCARD)/cores/ddlc/core_87000000"
	@cp -f "$(TOP)/build/core_87000000_gb300v2" "$(SDCARD)/system/Deimos/cores/ddlc.sf2k"
	@cp -f "$(TOP)/build/core_87000000_gb300v2" "$(SDCARD)/ROMS/ddlc/ddlc.sf2k"
	@printf "DDLC.gba|ddlc\n" > "$(SDCARD)/configs/custom_cores/ddlc/games.opt"
	@printf "ddlc;ddlc;DDLC.gba" > "$(SDCARD)/ROMS/ddlc/DDLC.gba"
	@printf "ddlc;ddlc;DDLC.gba" > "$(SDCARD)/ROMS/DDLC.gba"
	@cp -rf $(TOP)/game/* "$(SDCARD)/ROMS/ddlc/"
	@touch "$(SDCARD)/bios/enable_xlog.txt"
	@sync
	@echo "==> Deployment to SD card completed successfully!"
	@echo "    Core:  $(SDCARD)/system/Deimos/cores/ddlc.sf2k"
	@echo "    ROM:   $(SDCARD)/ROMS/ddlc/DDLC.gba"
	@echo "    Data:  $(SDCARD)/ROMS/ddlc/"

clean:
	rm -rf $(BUILD_DIR) screenshots build
	@rm -f game/persistent game/settings.sav game/save*.sav

dist: gb300-core sf2000-core
	@mkdir -p dist
	@cp -f build/core_87000000_gb300v2 dist/DDLC_GB300V2.gb3k
	@cp -f build/core_87000000_sf2000 dist/DDLC_SF2000.sf2k
	@echo "==> dist/DDLC_GB300V2.gb3k dist/DDLC_SF2000.sf2k"
