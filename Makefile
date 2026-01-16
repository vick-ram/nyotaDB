CC = gcc
CFLAGS = -Wall -Wextra -g -I.
LDFLAGS = -lreadline

SRCS = rdbms/main.c rdbms/storage.c rdbms/parser.c rdbms/executor.c rdbms/repl.c rdbms/webserver.c rdbms/btree.c
OBJS = $(SRCS:.c=.o)
TARGET = nyotadb

.PHONY: all clean run web

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

rdbms/%.o: rdbms/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

web: $(TARGET)
	./$(TARGET) --web

clean:
	rm -f $(OBJS) $(TARGET) test.db