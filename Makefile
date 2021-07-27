LDLIBS := -lssl -lcrypto
CFLAGS := -Wall

default: geminon gurl

geminon: geminon.o init.o url.o fs.o server.o request.o
	$(CC) -g -Wall -o $@ $+ $(LDLIBS)
	ldd $@

gurl: gurl.c init.o url.o client.o response.o
	$(CC) -g -Wall -o $@ $+ $(LDLIBS)
	ldd $@

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
	which lcov >/dev/null 2>&1 && lcov --zerocounters --directory . || true
	rm -rf coverage/

coverage:
	make CFLAGS="-ftest-coverage -fprofile-arcs -Wall" LDFLAGS="-ftest-coverage -fprofile-arcs -Wall" test
	lcov --no-external --capture --directory . --output-file $@.info
	genhtml $@.info --output-directory $@
