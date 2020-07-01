#
#MAKEFILE
#

#NOTA: se si vogliono visualizzare \
stampe di debug aggiungere $(DEBUGFLAG) \
alla seconda regola di compilazione $(EXE)

INCDIR = ./include
LIBDIR = ./lib
SRCDIR = ./src
CURRENT = .

CC	=  gcc
CFLAGS	= -Wall -g -pedantic -std=c99
INCLUDES = -I $(INCDIR)
LIBS = -lb
PLIB = -lpthread
LDFLAGS = -L $(LIBDIR)
#DEBUGFLAG = -DDEBUG=1
EXE = $(CURRENT)/supermercato

.PHONY: all test clean 

$(SRCDIR)/%.o : $(SRCDIR)/%.c
		$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@ 

$(EXE) : $(SRCDIR)/supermercato.c $(LIBDIR)/libb.a $(SRCDIR)/cassiere.o $(SRCDIR)/cliente.o $(SRCDIR)/direttore.o $(SRCDIR)/util.o $(SRCDIR)/parsing.o $(SRCDIR)/statlog.o 
		$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LIBS) $(PLIB)

$(LIBDIR)/libb.a : $(SRCDIR)/codaCassa.o $(SRCDIR)/icl_hash.o
		ar rvs $@ $^ 

all: $(EXE)

test:
	@echo "---Inizio sessione di testing---"
	$(EXE) &
	sleep 25
	pkill -HUP -f supermercato
	@echo "---Test terminato---"

test_quit:
	@echo "---Inizio sessione di testing---"
	$(EXE) &
	sleep 25
	pkill -QUIT -f supermercato
	@echo "---Test terminato---"

clean: 
	rm -f $(EXE) $(SRCDIR)/*.o $(LIBDIR)/b.a core *.~
	clear
	@echo "Rimossi file creati con makefile"
	
