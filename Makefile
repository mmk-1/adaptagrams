CXX ?= g++

ROOT_DIR := $(abspath .)
COLA_DIR ?= $(ROOT_DIR)/cola
HOLA_SRC := $(ROOT_DIR)/hola_metrics.cpp
HOLA_TARGET := $(ROOT_DIR)/hola_metrics

VPSC_LIBDIR := $(COLA_DIR)/libvpsc/.libs
COLA_LIBDIR := $(COLA_DIR)/libcola/.libs
AVOID_LIBDIR := $(COLA_DIR)/libavoid/.libs
DIALECT_LIBDIR := $(COLA_DIR)/libdialect/.libs
COLA_LIB := $(DIALECT_LIBDIR)/libdialect.so

CXXFLAGS ?= -std=gnu++17 -O2 -g -Wall -Wextra
CPPFLAGS += -I$(COLA_DIR)

LDFLAGS += \
	-L$(DIALECT_LIBDIR) \
	-L$(COLA_LIBDIR) \
	-L$(AVOID_LIBDIR) \
	-L$(VPSC_LIBDIR) \
	-Wl,-rpath,$(DIALECT_LIBDIR) \
	-Wl,-rpath,$(COLA_LIBDIR) \
	-Wl,-rpath,$(AVOID_LIBDIR) \
	-Wl,-rpath,$(VPSC_LIBDIR)
LDLIBS += -ldialect -lcola -lavoid -lvpsc -lm

.PHONY: all hola clean

all:
	@./build-hola-metrics.sh

hola: $(HOLA_TARGET)

$(HOLA_TARGET): $(HOLA_SRC) $(COLA_LIB)
	@test -f $(COLA_LIB) || { \
		echo "error: $(COLA_LIB) not found. Run ./build-hola-metrics.sh first." >&2; \
		exit 1; \
	}
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(HOLA_SRC) -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(HOLA_TARGET)

clean-all: clean
	@if [ -f $(COLA_DIR)/Makefile ]; then $(MAKE) -C $(COLA_DIR) clean; fi
