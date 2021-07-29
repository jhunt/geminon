LDLIBS := -lssl -lcrypto
CFLAGS := -Wall

AFL_CC ?= afl-clang

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

fuzz-url: fuzz-url.fo url.fo
	$(AFL_CC) $(LDFLAGS) -g -Wall -o $@ $+

run-fuzz-url:
	afl-fuzz -i fuzzing/url/in -o fuzzing/url/findings -- ./fuzz-url

# I usually just run tmux with ~8 open panes, running `make fuzzer$N`
fuzzer01:
	afl-fuzz -i fuzzing/url/in -o fuzzing/url/findings -M $@ -- ./fuzz-url
fuzzer%:
	afl-fuzz -i fuzzing/url/in -o fuzzing/url/findings -S $@ -- ./fuzz-url

test: t/url t/fs
	prove -v $+
t/url: t/url.o url.o
t/fs:  t/fs.o  fs.o

url.c: fsm.url.c
fsm.url.c: url.pl
	./url.pl > $@

fs.c: fsm.fs.c
fsm.fs.c: fs.pl
	./fs.pl > $@

%.fo: %.c
	$(AFL_CC) $(CFLAGS) -o $@ -c $+

clean:
	rm -f t/*.o *.o geminon fsm.*.c
	rm -f *.fo fuzz-url
	which lcov >/dev/null 2>&1 && lcov --zerocounters --directory . || true
	rm -rf coverage/

coverage:
	make CFLAGS="-ftest-coverage -fprofile-arcs -Wall" LDFLAGS="-ftest-coverage -fprofile-arcs -Wall" test
	lcov --no-external --capture --directory . --output-file $@.info
	genhtml $@.info --output-directory $@
