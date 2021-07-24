geminon: main.o url.o fs.o server.o
	$(CC) -g -Wall -o $@ $+

test: t/url t/fs
	prove -v $+
t/url: t/url.o url.o
t/fs:  t/fs.o  fs.o

url.o: url.c fsm.url.c
fsm.url.c: url.pl
	./url.pl > $@

fs.o: fs.c fsm.fs.c
fsm.fs.c: fs.pl
	./fs.pl > $@

clean:
	rm -f t/*.o *.o geminon fsm.*.c
	lcov --zerocounters --directory .
	rm -rf coverage/

coverage:
	make CFLAGS="-ftest-coverage -fprofile-arcs -Wall" LDFLAGS="-ftest-coverage -fprofile-arcs -Wall" test
	lcov --no-external --capture --directory . --output-file $@.info
	genhtml $@.info --output-directory $@
