SRC=$(wildcard *.c)
OBJ=${SRC:.c=.o}

CFLAGS += -I./include
LDFLAGS += -lm 

.PHONY: strip clean

default: meteor_decode

strip: meteor_decode
	strip $^

meteor_decode: ${OBJ}
	gcc -o $@ $^ ${LDFLAGS}

main.o: main.c include/options.h
	gcc ${CFLAGS} -c -o $@ $<

%.o: %.c
	gcc ${CFLAGS} -c -o $@ $<

clean:
	rm -f ${OBJ}
