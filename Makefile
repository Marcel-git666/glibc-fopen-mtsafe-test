CC = gcc
CFLAGS = -Wall -g -pthread -DSTANDALONE_TEST
LDFLAGS = -pthread

tst-fopen-mtsafe: io/tst-fopen-mtsafe.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f tst-fopen-mtsafe

.PHONY: clean
