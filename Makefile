# RicochetFix - build with MinGW-w64 (32-bit, the games are 32-bit).
#   make            -> build/winmm.dll
#   make dist       -> build/RicochetFix-<version>.zip
CROSS   ?= i686-w64-mingw32-
CC      = $(CROSS)gcc
PYTHON  ?= python3
VERSION := $(shell sed -n 's/^\#define RF_VERSION "\(.*\)"/\1/p' src/rf.h)

CFLAGS  = -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type \
          -fno-strict-aliasing -DWINVER=0x0601 -D_WIN32_WINNT=0x0601
LDFLAGS = -shared -static -static-libgcc -s -Wl,--enable-stdcall-fixup \
          -Wl,--subsystem,windows
LIBS    = -luser32 -lgdi32 -lversion

SRCS = src/main.c src/proxy.c src/window.c src/ddraw_hooks.c src/iat.c src/config.c src/log.c
GEN  = src/proxy_names.h src/proxy_stubs.S src/proxy.def

all: build/winmm.dll

$(GEN): tools/gen_proxy.py src/winmm_exports.txt src/ddraw_exports.txt
	$(PYTHON) tools/gen_proxy.py

build/winmm.dll: $(SRCS) src/rf.h $(GEN)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SRCS) src/proxy_stubs.S src/proxy.def $(LDFLAGS) $(LIBS)

dist: build/winmm.dll
	@rm -rf build/dist && mkdir -p build/dist
	cp build/winmm.dll RicochetFix.ini build/dist/
	cp dist/README.txt build/dist/RicochetFix-README.txt
	cd build/dist && zip -q -r ../RicochetFix-$(VERSION).zip .
	@echo "build/RicochetFix-$(VERSION).zip"

clean:
	rm -rf build

.PHONY: all dist clean
