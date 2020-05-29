#MAKEFILE

INCDIR = ./include
LIBDIR = ./lib
SRCDIR = ./src
BINDIR = .
CC	=  gcc
CFLAGS	= -Wall -g -pedantic -std=c99
INCLUDES = -I $(INCDIR)
LIBS = -lb -lpthread
LDFLAGS = -L $(LIBDIR)
EXE = $(BINDIR)/supermercato

.PHONY: all test clean

$(SRCDIR)/%.o : %.c
		$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@ 

$(EXE) : $(SRCDIR)/supermercato.c $(SRCDIR)/cassiere.o $(SRCDIR)/cliente.o $(SRCDIR)/direttore.o $(SRCDIR)/util.o $(SRCDIR)/parsing.o $(LIBDIR)/b.a
		$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@ $(LDFLAGS) $(LIBS)

$(LIBDIR)/b.a : $(SRCDIR)/codaCassa.o $(SRCDIR)/icl_hash.o
		ar rvs $@ $^ 

all: $(EXE)

test:
	@echo "---Inizio sessione di testing---"
	$(EXE) &
	sleep(25)
	pkill -HUP -f supermercato  
	@echo "---Test terminato---"

clean: 
	rm -f $(EXE) $(SRCDIR)/*.o $(LIBDIR)/b.a core *.~
	@echo "Rimossi file creati con makefile"
	clear