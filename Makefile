CFLAGS+=-Wall -pedantic -std=c99

ipxnchat: ipxnchat.c
	clang $(CFLAGS) $< -o $@ -lcurses
