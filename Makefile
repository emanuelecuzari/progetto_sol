#MAKEFILE

INCDIR = ./include
LIBDIR = ./lib
SRCDIR = ./src
BINDIR = ./bin
CC	=  gcc
CFLAGS	= -Wall -pedantic
INCLUDES = -I $(INCDIR)
LIBS = -l bt
LDFLAGS = -L $(LIBDIR) -Wl,-rpath=$(LIBDIR)