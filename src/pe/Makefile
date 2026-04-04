include ../Makefile.inc

TARGET = ../../targets/objs

all		:	$(TARGET)/ELFParser.o

$(TARGET)/ELFParser.o	:	ELFParser.cpp $(INCLUDE)/ELFParser.h
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@

