include common.mk
COMMON_DEPS=Makefile common.mk

MURTH_MODULES := \
	murth/nyatomic.o \
	murth/jack.o \
	murth/instructions.o \
	murth/program.o \
	murth/murth.o

#VISMURTH_SOURCES := \
#	vismurth/visuport.cpp \
#	vismurth/Ground.cpp \
#	vismurth/Central.cpp \
#	vismurth/Halfedge.cpp \
#	vismurth/FlatShadedMesh.cpp \
#	murth/main_linux.cpp
#
#MODULES=$(addprefix build/, $(patsubst %.cpp, %.o, $(SOURCES)))
#C_MODULES=$(patsubst %.c, %.o, $(filter %.c, $(SOURCES)))
#CXX_MODULES=$(patsubst %.cpp, %.o, $(filter %.cpp, $(SOURCES)))
#MODULES=$(C_MODULES) $(CXX_MODULES)

.PHONY: clean

#vismurth_bin: $(DEPS) $(MODULES) 3p/kapusha/libkapusha.a
#	$(LD) $(LDFLAGS) $(MODULES) -L3p/kapusha -lkapusha -o vismurth_bin

play: $(COMMON_DEPS) $(MURTH_MODULES) murth/play_jack.o
	$(LD) $(LDFLAGS) $(MURTH_MODULES) murth/play_jack.o -o play

#test: $(DEPS) murth/murth.o murth/test.o
#	$(LD) $(LDFLAGS) murth/murth.o murth/test.o -o test

#3p/kapusha/libkapusha.a:
#	make -j5 -C 3p/kapusha

clean:
	@rm -f $(MURTH_MODULES)

#deepclean: clean
#	@make -C 3p/kapusha clean
