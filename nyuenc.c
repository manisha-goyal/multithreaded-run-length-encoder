#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct {
    unsigned char currentChar;
    unsigned char count;
    int isInitialized;
} EncoderState;

void encode(char* file, EncoderState *state);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <file1> [file2] ...\n", argv[0]);
        return 1;
    }

    EncoderState state = {0, 0, 0};

    for (int i = 1; i < argc; i++) {
        encode(argv[i], &state);
    }

    if (state.isInitialized) {
        fwrite(&state.currentChar, sizeof(char), 1, stdout);
        fwrite(&state.count, sizeof(char), 1, stdout);
    }
    return 0;
}

void encode(char* file, EncoderState *state) {
    int fd = open(file, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error: unable to open file %s\n", file);
        return;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        fprintf(stderr, "Error: unable to get file size for %s\n", file);
        close(fd);
        return;
    }

    char *addr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "Error: unable to mmap file %s\n", file);
        close(fd);
        return;
    }

    for (off_t i = 0; i < sb.st_size; i++) {
        if (!state->isInitialized) {
            state->currentChar = addr[i];
            state->count = 1;
            state->isInitialized = 1;
        } else if (addr[i] == state->currentChar) {
            state->count++;
        } else {
            fwrite(&state->currentChar, sizeof(char), 1, stdout);
            fwrite(&state->count, sizeof(char), 1, stdout);
            state->currentChar = addr[i];
            state->count = 1;
        }
    }

    munmap(addr, sb.st_size);
    close(fd);
}
