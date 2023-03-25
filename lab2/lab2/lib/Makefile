all : printk.o

printk.o : printk.c
	$(GCC) $(CFLAG) -c printk.c -o printk.o

clean :
	$(shell rm *.o 2>/dev/null)