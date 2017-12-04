CFLAGS+=-Wall -pedantic -fdiagnostics-show-category=name -std=c11

ipxnchat: ipxnchat.c
	clang $(CFLAGS) $< -o $@ -lncursesw -lpthread
