CC := gcc
INCLUDES := -I.
SOURCES := $(wildcard *.c)
OBJECTS := $(patsubst %.c,%.o,${SOURCES})
TARGET := example
FLAGS := -lc

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(INCLUDES) $(FLAGS) -o $(TARGET)

%.o: %.c
	$(CC) -c $(INCLUDES) $< -o $@

clean:
	rm -f *.o
	rm -f ${TARGET}
