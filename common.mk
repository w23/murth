CC ?= clang
CXX ?= clang
LD ?= clang
C_FLAGS = -g -Wall -Werror -fno-exceptions -I. -I3p/kapusha `pkg-config --cflags jack`
LDFLAGS = -lm `pkg-config --libs jack`

ifeq ($(DEBUG),1)
	C_FLAGS += -O0 -g -DDEBUG=1
else
	C_FLAGS += -Os -fomit-frame-pointer
endif

ifeq ($(RPI),1)
	C_FLAGS += -DKAPUSHA_RPI=1 -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux
	LDFLAGS += -lGLESv2 -lEGL -lbcm_host -L/opt/vc/lib
else
	C_FLAGS += `pkg-config --cflags sdl` -march=native
	LDFLAGS += `pkg-config --libs sdl` -lGL
endif

CXXFLAGS = $(C_FLAGS) -std=c++11 -fno-rtti -Wno-invalid-offsetof
CFLAGS += $(C_FLAGS) -std=c99

.SUFFIXES: .cpp .o
.SUFFIXES: .c .o

.cpp.o: $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.c.o: $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<
