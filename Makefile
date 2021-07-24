url: url.o
url.o: url.c fsm.c
fsm.c: fsm.pl
	./fsm.pl > $@
