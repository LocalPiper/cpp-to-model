# Note: plain '=' (not '?=') so it overrides make's built-in g++ default;
# you can still override on the command line with: make CXX=...
CXX = clang++
LLVM_CONFIG ?= $(shell command -v llvm-config || command -v llvm-config-21 || command -v llvm-config-20 || command -v llvm-config-19 || command -v llvm-config-18 || command -v llvm-config-17 || command -v llvm-config-16 || command -v llvm-config-15 || command -v llvm-config-14)

LLVM_VERSION := $(shell $(LLVM_CONFIG) --version | cut -d. -f1)
# Includes + flags from LLVM (treated as system includes to silence its
# header warnings), minus options that would break our translation unit.
LLVM_CXXFLAGS := $(shell $(LLVM_CONFIG) --cxxflags | sed -E 's/-fno-rtti//g; s/-std=c\+\+[0-9]+//g; s/(^| )-I/\1-isystem /g')
LLVM_LDFLAGS := $(shell $(LLVM_CONFIG) --ldflags --system-libs)
LLVM_LIBDIR := $(shell $(LLVM_CONFIG) --libdir)

# clang headers live outside the LLVM include dir on some systems (NixOS).
CLANG_INCLUDE := $(shell { \
  for d in "$(shell $(LLVM_CONFIG) --includedir)" "/usr/include" \
           "/usr/lib/llvm-$(LLVM_VERSION)/include" /nix/store/*clang-*-dev/include; do \
    [ -f "$$d/clang/AST/ASTConsumer.h" ] && { echo "$$d"; break; }; \
  done; } 2>/dev/null)

# C++ standard library headers. Inside nix-shell (or any environment where
# the compiler wrapper already injects include paths, see NIX_CFLAGS_COMPILE)
# we do nothing; otherwise we discover headers manually:
#   1. clang's / g++'s own answer (libstdc++, needs no extra packages),
#   2. libstdc++ store paths + their target-triple subdirectory,
#   3. libc++ store paths (then we also pass -stdlib=libc++).
ifeq ($(strip $(NIX_CFLAGS_COMPILE)),)
STD_INCLUDE := $(shell { \
  for cc in "$(CXX)" g++ gcc; do \
    d="$$($$cc -print-file-name=include 2>/dev/null)"; \
    if [ -n "$$d" ] && [ -f "$$d/cassert" ]; then \
      echo "$$d"; \
      [ -d "$$d/backward" ] && echo "$$d/backward"; \
      exit 0; \
    fi; \
  done; \
  for d in /nix/store/*libcxx-*-dev/include/c++/v1 /nix/store/*gcc-*/include/c++/*; do \
    [ -f "$$d/cassert" ] && { echo "$$d"; exit 0; }; \
  done; } 2>/dev/null)

GCC_MULTILIB := $(shell { \
  for m in /nix/store/*gcc-*/include/c++/*/*/bits/c++config.h; do \
    [ -f "$$m" ] && { echo "$$(dirname "$$(dirname "$$m")")"; break; }; \
  done; } 2>/dev/null)

USES_LIBCXX := $(shell { \
  for cc in "$(CXX)" g++ gcc; do \
    d="$$($$cc -print-file-name=include 2>/dev/null)"; \
    [ -n "$$d" ] && [ -f "$$d/cassert" ] && exit 0; \
  done; \
  for d in /nix/store/*libcxx-*-dev/include/c++/v1; do \
    [ -f "$$d/cassert" ] && { echo yes; exit 0; }; \
  done; } 2>/dev/null)
STDLIB_FLAG := $(if $(USES_LIBCXX),-stdlib=libc++)
STD_ISYSTEM := $(patsubst %,-isystem %,$(STD_INCLUDE) $(if $(USES_LIBCXX),,$(GCC_MULTILIB)))
else
STDLIB_FLAG :=
STD_ISYSTEM :=
endif

CXXFLAGS = -O2 -Wall -Wextra -std=c++20 $(LLVM_CXXFLAGS) \
           $(if $(CLANG_INCLUDE),-isystem $(CLANG_INCLUDE)) \
           $(STD_ISYSTEM) $(STDLIB_FLAG)

# Link the monolithic clang/LLVM libraries (present on distro packages and
# NixOS alike), with an rpath so the binary finds them at runtime.
LDFLAGS = $(LLVM_LDFLAGS) -lclang-cpp -lLLVM -Wl,-rpath,$(LLVM_LIBDIR)

TARGET = web/processor
SRC = cpp/main.cpp \
      cpp/facade.cpp \
      cpp/text_utils.cpp \
      cpp/json_out.cpp \
      cpp/ast/ast_builder.cpp \
      cpp/ast/clang_session.cpp \
      cpp/draw/ast_drawer.cpp
OBJ = $(SRC:.cpp=.o)

# Generate per-object dependency files (-MMD) so header changes rebuild.
CXXFLAGS += -MMD -MP

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

-include $(OBJ:.o=.d)

clean:
	rm -f $(TARGET) $(OBJ) $(OBJ:.o=.d)

.PHONY: all clean
