CC = gcc
CFLAGS = -g -O0 -Wall -Wextra -std=c11 -Iinclude `pkg-config --cflags gtk+-3.0 sqlite3 cairo`
LDFLAGS = `pkg-config --libs gtk+-3.0 sqlite3 cairo` -lm

SRC = src/main.c src/gui.c src/database.c src/budget.c src/goal.c src/stats.c src/chart.c src/utils.c src/analytics.c
OBJ = $(SRC:.c=.o)
TARGET = finance_manager

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean


