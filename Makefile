include common.mk

SOURCES := \
	murth/jack.c \
	murth/synth.c \
	murth/program.c \
	murth/vismurth/visuport.cpp \
	murth/vismurth/Ground.cpp \
	murth/vismurth/Central.cpp \
	murth/vismurth/Halfedge.cpp \
	murth/vismurth/FlatShadedMesh.cpp \
	murth/main_linux.cpp

#MODULES=$(addprefix build/, $(patsubst %.cpp, %.o, $(SOURCES)))
C_MODULES=$(patsubst %.c, %.o, $(filter %.c, $(SOURCES)))
CXX_MODULES=$(patsubst %.cpp, %.o, $(filter %.cpp, $(SOURCES)))
MODULES=$(C_MODULES) $(CXX_MODULES)
DEPS=Makefile common.mk

.PHONY: clean

vismurth: $(DEPS) $(MODULES) 3p/kapusha/libkapusha.a
	$(LD) $(LDFLAGS) $(MODULES) -L3p/kapusha -lkapusha -o vismurth

3p/kapusha/libkapusha.a:
	make -j5 -C 3p/kapusha

clean:
	@rm -rf $(MODULES) vismurth

deepclean: clean
	@make -C 3p/kapusha clean
