EXE_FILE := cregex
CC := gcc
#CFLAGS := -g -Wall -Werror -fsanitize=address -fno-omit-frame-pointer -static-libasan
CFLAGS := -O3 -Wall -Werror
SRC := cregex.c main.c
all :
	$(CC) $(CFLAGS) $(SRC) -o $(EXE_FILE)
.PHONY : clean
clean :
	rm $(EXE_FILE)