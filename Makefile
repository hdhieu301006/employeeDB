TARGET_SRV = bin/dbserver
TARGET_CLI = bin/dbcli

SRC_SRV = $(wildcard src/srv/*.c)
OBJ_SRV = $(SRC_SRV:src/srv/%.c=obj/srv/%.o)

SRC_CLI = $(wildcard src/cli/*.c)
OBJ_CLI = $(SRC_CLI:src/cli/%.c=obj/cli/%.o)

default: $(TARGET_SRV) $(TARGET_CLI)

run: clean default
	./$(TARGET_SRV) -f ./mynewdb.db -n -p 8080 & \
	SRV_PID=$$!; \
	sleep 1; \
	./$(TARGET_CLI) -h 127.0.0.1 -p 8080 -a "Hieu,Hanoi,120"; \
	./$(TARGET_CLI) -h 127.0.0.1 -p 8080 -l; \
	kill -9 $$SRV_PID

clean:
	rm -f obj/srv/*.o obj/cli/*.o
	rm -f bin/*
	rm -f *.db

$(TARGET_SRV): $(OBJ_SRV)
	gcc -o $@ $^

$(OBJ_SRV): obj/srv/%.o: src/srv/%.c
	gcc -c $< -o $@ -Iinclude

$(TARGET_CLI): $(OBJ_CLI)
	gcc -g -o $@ $^

$(OBJ_CLI): obj/cli/%.o: src/cli/%.c
	gcc -c $< -o $@ -Iinclude

.PHONY: run default clean
