CC=gcc

ODIR=../obj
IDIR=../inc
OFILE=../test

CFLAGS=-I. -I$(IDIR)

_DEPS = render.h world.h util.h input.h common.h
DEPS  = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ  = render.o main.o world.o util.o input.o
OBJ   = $(patsubst %,$(ODIR)/%,$(_OBJ))

LIBS=`sdl2-config --cflags --libs` -lSDL2_image -lm

DEBUG ?= 0
ifeq ($(DEBUG), 1)
    CFLAGS += -g
endif

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -o $@ -c $< $(CFLAGS) $(LIBS)

build: $(OBJ) $(DEPS)
	$(CC) -o $(OFILE) $^ $(CFLAGS) $(LIBS)

# main.c is overwritten to include SDL
$(ODIR)/main.o: main.c render.c $(DEPS)
	$(CC) -o $(ODIR)/main.o -c main.c $(LIBS) $(CFLAGS)

# render.c is overwritten to include SDL and math.h
$(ODIR)/render.o: render.c
	$(CC) -o $(ODIR)/render.o -c render.c $(LIBS) $(CFLAGS)

clean:
	rm -f $(ODIR)/*.o
	rm -f $(OFILE)
