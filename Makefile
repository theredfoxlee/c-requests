TARGET := http

.PHONY: all clean

http: http.c
	gcc -lcurl -o $@ $<

clean:
	rm -f $(TARGET)
