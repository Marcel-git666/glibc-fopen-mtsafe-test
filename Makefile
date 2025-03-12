CC = gcc
CFLAGS = -Wall -g -pthread -DSTANDALONE_TEST
# -fsanitize=thread
LDFLAGS = -pthread
# -fsanitize=thread

tst-fopen-mtsafe: libio/tst-fopen-mtsafe.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f tst-fopen-mtsafe

re: clean tst-fopen-mtsafe

.PHONY: clean
