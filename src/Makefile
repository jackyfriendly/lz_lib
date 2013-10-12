
LIBS= -lpthread

SRC=$(wildcard *.c)
OBJECTS=$(patsubst %.c,%.o,$(wildcard *.c))

TARGET= liblz.a
	
all:$(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

liblz.a: $(OBJECTS)
	ar cr $@ $^

clean:
	rm -f $(TARGET)
