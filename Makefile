SRC := $(wildcard *.c)
ELF := $(patsubst %.c, %.elf, $(SRC))
ELF := $(filter-out sqlite3.elf, $(ELF))

CFLAGS += -std=gnu99

CPPFLAGS += -I .
LDFLAGS  += -L .
LDFLAGS  += -lsqlite3
LDFLAGS  += -lpthread
LDFLAGS  += -ldl

all:install $(ELF) 

install:sqlite3.o
	$(CC) -shared -fPIC -o libsqlite3.so $^

sqlite3.o:sqlite3.c
	$(CC) $^ -o $@ -c -fPIC

$(ELF):%.elf:%.c
	$(CC) $^ -o $@ $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)


clean:
	-@$(RM) $(ELF)
	-@echo "Done."

distclean:clean
	-@$(RM) *.so
