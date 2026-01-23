CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
LDFLAGS = -pthread

SRC_DIR = src
BUILD_DIR = build

COMMON_SRC = ipc.c logger.c podajnik.c
COMMON_OBJ = $(COMMON_SRC:%.c=$(BUILD_DIR)/%.o)

PROGRAMS = kierownik piekarz kasjer klient

all: $(BUILD_DIR) $(PROGRAMS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ===== PROGRAMY =====

kierownik: $(BUILD_DIR)/kierownik.o $(COMMON_OBJ)
	$(CC) $^ -o $(BUILD_DIR)/$@ $(LDFLAGS)

piekarz: $(BUILD_DIR)/piekarz.o $(COMMON_OBJ)
	$(CC) $^ -o $(BUILD_DIR)/$@ $(LDFLAGS)

kasjer: $(BUILD_DIR)/kasjer.o $(COMMON_OBJ)
	$(CC) $^ -o $(BUILD_DIR)/$@ $(LDFLAGS)

klient: $(BUILD_DIR)/klient.o $(COMMON_OBJ)
	$(CC) $^ -o $(BUILD_DIR)/$@ $(LDFLAGS)

# ===== OBIEKTY =====

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ===== CLEAN =====

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
