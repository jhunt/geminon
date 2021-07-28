LDLIBS := -lssl -lcrypto
CFLAGS := -Wall

IMAGE_PREFIX ?= iamjameshunt/

default: geminon gurl

docker:
	docker build -t $(IMAGE_PREFIX)geminon:latest .
	docker build -t $(IMAGE_PREFIX)gurl:latest -f Dockerfile.gurl .
push:
	docker push $(IMAGE_PREFIX)geminon:latest $(IMAGE_PREFIX)gurl:latest

geminon: geminon.o init.o url.o fs.o server.o request.o
	$(CC) $(LDFLAGS) -g -Wall -o $@ $+ $(LDLIBS)
	ldd $@

gurl: gurl.c init.o url.o client.o response.o
	$(CC) $(LDFLAGS) -g -Wall -o $@ $+ $(LDLIBS)
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
