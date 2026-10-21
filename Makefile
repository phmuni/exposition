PROJ_NAME = exposition

# Diretórios
SRC_DIR   = src
BIN_DIR   = bin
BUILD_DIR = build
RAYLIB    = dependencies/raylib

# Arquivos fonte e objetos (objetos ficam em build/ e não são versionados)
C_SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
H_SOURCES = $(wildcard $(SRC_DIR)/*.h)
OBJ       = $(C_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Biblioteca estática do raylib
A_SOURCES = $(RAYLIB)/lib/libraylib.a

# Compilador
CC = clang++

# Flags de compilação (headers do raylib e do próprio projeto)
CFLAGS = -std=c++20 -Wall -Wextra -O2 -I $(RAYLIB)/include -I $(SRC_DIR)

# Flags de linkagem (frameworks do macOS)
LDFLAGS = -framework CoreVideo \
          -framework IOKit     \
          -framework Cocoa     \
          -framework GLUT      \
          -framework OpenGL

# Regra principal
all: $(BIN_DIR)/$(PROJ_NAME)

# Linkagem final
$(BIN_DIR)/$(PROJ_NAME): $(OBJ) | $(BIN_DIR)
	$(CC) -o $@ $^ $(A_SOURCES) $(LDFLAGS)

# Gera cada .o dentro de build/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(H_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

run: all
	./$(BIN_DIR)/$(PROJ_NAME)

# Remove só os objetos intermediários
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run clean
