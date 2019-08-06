CC=$(CROSS_COMPILE)gcc

CFLAGS+= -std=c99 -D_POSIX_C_SOURCE=200112L
CFLAGS+= -Wall -Wextra -Werror -Wshadow -Wno-unused
CFLAGS+= -g

LDLIBS += -lutil
LDFLAGS= -g

default: all
all: ptyproxy

ptyproxy: ptyproxy.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) ptyproxy ptyproxy.o
