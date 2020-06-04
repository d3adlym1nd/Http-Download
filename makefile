LFLAG=-lssl -lcrypto
CFLAG=-Wall -Wextra -pedantic -g
OBJ=HttpDownload.o
BIN=Httpd
CC=g++
RM=/bin/rm
%.o: %.cpp
	$(CC) $(CFLAG) -c -o $@ $<
$(BIN): $(OBJ)
	$(CC) -o $(BIN) $(OBJ) $(LFLAG)
clean:
	$(RM) -f $(OBJ) $(BIN)
