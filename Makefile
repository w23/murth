include common.mk

SOURCES := \
	murth/nyatomic.c \
	murth/jack.c \
	murth/murth.c \
	vismurth/visuport.cpp \
	vismurth/Ground.cpp \
	vismurth/Central.cpp \
	vismurth/Halfedge.cpp \
	vismurth/FlatShadedMesh.cpp \
	murth/main_linux.cpp

#MODULES=$(addprefix build/, $(patsubst %.cpp, %.o, $(SOURCES)))
C_MODULES=$(patsubst %.c, %.o, $(filter %.c, $(SOURCES)))
CXX_MODULES=$(patsubst %.cpp, %.o, $(filter %.cpp, $(SOURCES)))
MODULES=$(C_MODULES) $(CXX_MODULES)
DEPS=Makefile common.mk

.PHONY: clean

vismurth_bin: $(DEPS) $(MODULES) 3p/kapusha/libkapusha.a
	$(LD) $(LDFLAGS) $(MODULES) -L3p/kapusha -lkapusha -o vismurth_bin

play: $(DEPS) murth/murth.o murth/play_jack.o murth/jack.o murth/nyatomic.o
	$(LD) $(LDFLAGS) murth/murth.o murth/play_jack.o murth/jack.o murth/nyatomic.o -o play

test: $(DEPS) murth/murth.o murth/test.o
	$(LD) $(LDFLAGS) murth/murth.o murth/test.o -o test

3p/kapusha/libkapusha.a:
	make -j5 -C 3p/kapusha

clean:
	@rm -f $(MODULES)

deepclean: clean
	@make -C 3p/kapusha clean
