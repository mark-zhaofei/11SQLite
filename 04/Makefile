SRC = $(wildcard *.c)
ELF = $(patsubst %.c, %.elf, $(SRC))


CPPFLAGS += -Iinc/

LDFLAGS  += -Llib/
LDFLAGS  += -lsqlite3_arm
LDFLAGS  += -lpthread
LDFLAGS  += -ldl
LDFLAGS  += -Wl,-rpath=.

CROSS = arm-none-linux-gnueabi-
CC = $(CROSS)gcc


all:$(ELF)

$(ELF):%.elf:%.c
	$(CC) -o $@ $^ $(CPPFLAGS) $(LDFLAGS)

clean:
	-@$(RM) $(ELF)
	-@echo "Done."
