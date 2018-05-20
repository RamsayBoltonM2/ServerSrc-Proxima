CXX	= g++
BIN_DIR = ../lib
BIN = $(BIN_DIR)/libgame.a

GCC_VERSION = $(shell $(CC) --version 2>&1 | grep "(GCC)" | cut -d' ' -f3  | cut -d'.' -f1)

CFLAGS = -Wall -O2 -w -pipe -fno-exceptions -I../include

LIBS =

OBJFILES = grid.o attribute.o targa.o

default:
	$(MAKE) $(BIN)

$(BIN): $(OBJFILES)
	if [ ! -d $(BIN_DIR) ]; then mkdir $(BIN_DIR); fi
	ar cru $(BIN) $(OBJFILES) $(LIBS)
	ranlib $(BIN)
	chmod 700 $(BIN)

clean:
	rm -f *.o
	rm -f $(BIN)

dep:
	touch Depend
	$(CXX) $(CFLAGS) -MM *.cc > Depend

$(OBJFILES):
	$(CXX) $(CFLAGS) -c $<

include Depend
