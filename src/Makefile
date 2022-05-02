CFLAGS=-Wall -Werror -fsanitize=undefined -O2 -g -pthread

OBJ=list.o dutchblitz.o pile.o fairlock.o

all:    dutchblitz

$(OBJ): cards.h pile.h list.h fairlock.h

dutchblitz: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

clean:
	rm -f $(OBJ)
