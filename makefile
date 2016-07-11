my-index: my-index.o libhashfile.o rbtree.o
	gcc my-index.o libhashfile.o rbtree.o -o my-index

my-index.o: my-index.c list.h
	gcc -Wall -O -c my-index.c -o my-index.o

libhashfile.o: libhashfile.c libhashfile.h
	gcc -Wall -O -c libhashfile.c -o libhashfile.o

rbtree.o: rbtree.c rbtree.h
	gcc -Wall -O -c rbtree.c -o rbtree.o

play: my-index
	./my-index sourcefile/ fslhomes-user008-2013-01-22.128kb.hash.anon

clean:
	rm -f *.o my-index
