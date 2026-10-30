TARGET_NAME := ddlc
DDLC_PORT   ?= $(abspath $(CURDIR)/../..)

ifneq ($(filter GB300%,$(FROGGY_TYPE)),)
  override platform := gb300
else
  platform ?= sf2000
endif

TARGET      := $(TARGET_NAME)_libretro_$(platform).a
ARCHIVE     := $(DDLC_PORT)/build/$(platform)/libretro_love.a

ifeq ($(platform), sf2000)
	STATIC_LINKING = 1
endif
ifeq ($(platform), gb300)
	STATIC_LINKING = 1
endif

all:
	rm -f ./*.a
	$(MAKE) -C "$(DDLC_PORT)" $(platform)
	cp -f "$(ARCHIVE)" ./$(TARGET)

clean:
	rm -f ./*.a

.PHONY: all clean
