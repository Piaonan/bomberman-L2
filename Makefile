EXEC=btooom.exe
SOURCES=main.c playground.c player.c menu.c tool.c
OBJECTS=$(SOURCES:.c=.o)
CC=gcc
CFLAGS=-W -Wall -std=gnu99 -g -Werror
 
.PHONY: default clean
 
default: $(EXEC)
 
tool.o: tool.c tool.h
menu.o: menu.c menu.h tool.h
player.o: player.c player.h tool.h
playground.o: playground.c playground.h player.h tool.h
main.o: main.c playground.h player.h menu.h tool.h
 
%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)
 
$(EXEC): $(OBJECTS)
	$(CC) -o $@ $^ -lm
 
clean:
	rm -rf $(EXEC) $(OBJECTS) $(SOURCES:.c=.c~) $(SOURCES:.c=.h~) Makefile~
