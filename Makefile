CC := gcc
INCLUDES := -Iinc
SOURCES = $(wildcard src/*.c)
SOURCES += $(wildcard *.c)
OBJECTS := $(patsubst %.c,%.o,${SOURCES})
TARGET := example
FLAGS := -lc

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(INCLUDES) $(FLAGS) -o $(TARGET)
	rm -f *.o
	rm -f src/*.o

%.o: %.c
	$(CC) -c $(INCLUDES) $< -o $@

clean:
	rm -f *.o
	rm -f ${TARGET}
