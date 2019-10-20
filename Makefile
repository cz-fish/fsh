CC       = gcc
CXX      = g++
AS       = gas
LD       = g++

CFLAGS   = -c -Wall -ansi -pedantic
CFLAGS_DEBUG = -g -O0
CXXFLAGS = $(CFLAGS)
CXXFLAGS_DEBUG = -g -O0
LDFLAGS  =
LDFLAGS_DEBUG = -g
LIBS     = -lreadline -lncurses

BIN      = fsh
OBJS     = main.o grammar.o tokenizer.o pattern.o expr.o support.o

.PHONY:  all clean debug
.SUFFIXES:

all: $(BIN)


fsh: $(OBJS)
	$(LD) -o fsh $(LDFLAGS) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) -o $@ $(CFLAGS) $<

%.o: %.cpp
	$(CXX) -o $@ $(CXXFLAGS) $<

clean:
	rm -f *.o $(BIN)

debug: CFLAGS := $(CFLAGS_DEBUG) $(CFLAGS) -DDEBUG
debug: CXXFLAGS := $(CXXFLAGS_DEBUG) $(CXXFLAGS) -DDEBUG
debug: LDFLAGS := $(LDFLAGS_DEBUG) $(LDFLAGS)
debug: fsh


#header dependencies
main.o: const.h

main.o tokenizer.o grammar.o: tokenizer.h
main.o grammar.o: grammar.h
tokenizer.o grammar.o support.o pattern.o: support.h
main.o pattern.o grammar.o: pattern.h
main.o expr.o: expr.h

#temporary
tokenmain.o: tokenizer.h grammar.h
tok: tokenmain.o tokenizer.o grammar.o support.o
	$(CC) -o tokenizer tokenmain.o tokenizer.o grammar.o support.o


