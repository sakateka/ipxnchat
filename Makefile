CFLAGS+=-Wall -pedantic -std=c11

ipxnchat: ipxnchat.c
	clang $(CFLAGS) $< -o $@ -lncursesw
