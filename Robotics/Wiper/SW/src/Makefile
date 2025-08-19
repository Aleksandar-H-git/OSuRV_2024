#CPPFLAGS=$(shell pkg-config --cflags sdl2) -pthread
#LDFLAGS=$(shell pkg-config --libs sdl2) -pthread

run: main.elf
	./main.elf

main.elf: main.o
	${CC} -o $@ $< ${LDFLAGS} ${LIBS}

main.o: main.c
	${CC} ${CPPFLAGS} -c main.c -o main.o