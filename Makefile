test: tests
	prove -v ./tests
tests: tests.o url.o

url: url.o
url.o: url.c fsm.c
fsm.c: fsm.pl
	./fsm.pl > $@

clean:
	rm -f tests *.o
