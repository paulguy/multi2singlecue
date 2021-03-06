OBJS   = multi2singlecue.o
TARGET = multi2singlecue
CFLAGS = -Wall -Wextra -D_FILE_OFFSET_BITS=64 -ggdb
LDFLAGS = -lcuefile

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS)

all: $(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
