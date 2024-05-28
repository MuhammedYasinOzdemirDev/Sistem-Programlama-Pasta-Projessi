CC = gcc
CFLAGS = -pthread -Wall -Wextra
TARGET = pasta_projesi
INPUT_FILE = malzemeler.txt

all: $(TARGET)

# Derleme işlemi
$(TARGET): main.o
	$(CC) $(CFLAGS) -o $(TARGET) main.o

main.o: 171421005_pasta.c
	$(CC) $(CFLAGS) -c 171421005_pasta.c -o main.o

# Temizleme işlemi
clean:
	rm -f *.o $(TARGET)

# Derleme ve çalıştırma işlemi
run: $(TARGET)
	./$(TARGET) -i $(INPUT_FILE)
