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
void handleError(const char* message, int exitCode);

int main(int argc, char* argv[]) {
    int opt;
    int numJobs = 1;

    while ((opt = getopt(argc, argv, "j:")) != -1) {
        switch (opt) {
            case 'j':
                numJobs = atoi(optarg);
                if (numJobs < 1)
                    handleError("Number of jobs must be at least 1", 1);
                break;
            default:
                handleError("Usage: ./nyuenc [-j njobs] <file1> [file2] ...\n", 1);
        }
    }

    if (optind == argc)
        handleError("Expected argument after options\n", 1);

    EncoderState state = {0, 0, 0};

    for (int i = optind; i < argc; i++) {
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
    if (fd == -1)
        handleError("Error: unable to open file", 1);

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        handleError("Error: unable to get file size", 1);
    }

    char *addr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        handleError("Error: unable to mmap file", 1);
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

void handleError(const char* message, int exitCode) {
    fprintf(stderr, "%s\n", message);
    if (exitCode != 0) {
        exit(exitCode);
    }
}
