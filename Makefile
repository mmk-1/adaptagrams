CXX ?= g++

ROOT_DIR := $(abspath .)
COLA_DIR ?= $(ROOT_DIR)/cola
HOLA_SRC := $(ROOT_DIR)/hola_metrics.cpp
HOLA_TARGET := $(ROOT_DIR)/hola_metrics

CXXFLAGS ?= -std=gnu++17 -O2 -g -Wall -Wextra
CPPFLAGS += -I$(COLA_DIR)

VPSC_LIBDIR := $(COLA_DIR)/libvpsc/.libs
COLA_LIBDIR := $(COLA_DIR)/libcola/.libs
AVOID_LIBDIR := $(COLA_DIR)/libavoid/.libs
DIALECT_LIBDIR := $(COLA_DIR)/libdialect/.libs

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

.PHONY: all clean

all: $(HOLA_TARGET)

$(HOLA_TARGET): $(HOLA_SRC)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(HOLA_TARGET)
