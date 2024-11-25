SERVER ?= server                  # Program we are building
CLIENT ?= client                  # Program we are building
PACK ?= ./pack                    # Packing executable
DELETE = rm -rf                   # Command to remove files
OUT ?= -o                         # Compiler argument for output file
SSOURCES = server.c mongoose.c packed_fs.c       # Source code files
CSOURCES = client.c mongoose.c packed_fs.c       # Source code files
CFLAGS = -W -Wall -Wextra -g -I.  # Build options

# Mongoose build options. See https://mongoose.ws/documentation/#build-options
CFLAGS_MONGOOSE += -DMG_ENABLE_PACKED_FS=1

ifeq ($(OS),Windows_NT)   # Windows settings. Assume MinGW compiler. To use VC: make CC=cl CFLAGS=/MD OUT=/Feprog.exe
  SERVER ?= server.exe          # Use .exe suffix for the binary
  CLIENT ?= client.exe
  PACK = pack.exe               # Packing executable
  CC = gcc                      # Use MinGW gcc compiler
  CFLAGS += -lws2_32            # Link against Winsock library
  DELETE = cmd /C del /Q /F /S  # Command prompt command to delete files
  OUT ?= -o	                # Build output
  MAKE += WINDOWS=1 CC=$(CC)
endif

all: $(SERVER) $(CLIENT)
	@echo "Execute '$(RUN) ./$(SERVER)' in a window (or send it to background)"
	@echo "then execute '$(RUN) ./$(CLIENT)' in another window (or the same one)"

$(SERVER): $(SSOURCES)       # Build program from sources
	$(CC) $(SSOURCES) $(CFLAGS) $(CFLAGS_MONGOOSE) $(CFLAGS_EXTRA) $(OUT) $(SERVER)

$(CLIENT): $(CSOURCES)       # Build program from sources
	$(CC) $(CSOURCES) $(CFLAGS) $(CFLAGS_MONGOOSE) $(CFLAGS_EXTRA) $(OUT) $(CLIENT)

clean:                    # Cleanup. Delete built program and all build artifacts
	$(DELETE) $(SERVER) $(CLIENT) *.o *.obj *.dSYM $(PACK)

# Generate packed filesystem for serving credentials
packed_fs.c: $(wildcard certs/*) Makefile
	$(CC) ../../test/pack.c -o $(PACK)
	$(PACK) $(wildcard certs/*) > $@

# see https://mongoose.ws/tutorials/tls/#how-to-build for TLS build options
