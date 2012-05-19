CC = g++
params = -Wall -ggdb -D_FILE_OFFSET_BITS=64


define complete
@echo
@echo "- compile complete"
@echo
endef


hfsprescue : hfsprescue.o
	$(CC) $(params) hfsprescue.o  -o hfsprescue

ifdef complete
	$(complete)
endif




hfsprescue.o : hfsprescue.c
	$(CC)  -c hfsprescue.c

clean :
	rm -f hfsprescue.o hfsprescue
