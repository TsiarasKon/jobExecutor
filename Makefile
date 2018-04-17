OBJS 	= main.o worker.o postinglist.o trie.o util.o
SOURCE	= main.c worker.c postinglist.c trie.c util.c
HEADER  = paths.h postinglist.h trie.h util.h
OUT  	= jobExecutor
CC		= gcc
FLAGS   = -g3 -c -pedantic -std=c99 -Wall

$(OUT): $(OBJS)
	$(CC) -g3 $(OBJS) -o $@

main.o: main.c
	$(CC) $(FLAGS) main.c

worker.o: worker.c
	$(CC) $(FLAGS) worker.c

postinglist.o: postinglist.c
	$(CC) $(FLAGS) postinglist.c

trie.o: trie.c
	$(CC) $(FLAGS) trie.c

util.o: util.c
	$(CC) $(FLAGS) util.c


clean:
	rm -f $(OBJS) $(OUT)

count:
	wc $(SOURCE) $(HEADER)
