#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct {
    char* input_data;      
    int input_size;     
    char* encoded_data;   
    int enc_size; 
} Data;

typedef struct Node {
    Data* data;
    struct Node* next;
} Node;

typedef struct {
    Node* head;
    Node* tail;
    int size;
} Queue;

void init_queue(Queue* queue);
void enqueue(Queue* queue, Data* data);
Data* dequeue(Queue* queue);
void encode_data(Data *data);
void handleError(const char* message, int exitCode);

void init_queue(Queue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
}

void enqueue(Queue* queue, Data* data) {
    Node* newNode = malloc(sizeof(Node));
    newNode->data = data;
    newNode->next = NULL;

    if (queue->head == NULL) {
        queue->head = newNode;
    } else {
        queue->tail->next = newNode;
    }
    queue->tail = newNode;
    queue->size++;
}

Data* dequeue(Queue* queue) {
    Node* tempNode = queue->head;
    Data* data = tempNode->data;
    queue->head = queue->head->next;

    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    free(tempNode);
    queue->size--;
    return data;
}

void encode_data(Data* data) {
    data->encoded_data = malloc(data->input_size * 2);
    int j = 0;
    for (int i = 0; i < data->input_size;) {
        int count = 1;
        while ((i + count) < data->input_size && data->input_data[i] == data->input_data[i + count]) {
            count++;
        }
        data->encoded_data[j++] = data->input_data[i];
        data->encoded_data[j++] = (unsigned int)count;
        i += count;
    }
    data->enc_size = j;
}

int main(int argc, char* argv[]) {
    int opt;
    int num_jobs = 1;

    while ((opt = getopt(argc, argv, "j:")) != -1) {
        switch (opt) {
            case 'j':
                num_jobs = atoi(optarg);
                if (num_jobs < 1)
                    handleError("Number of jobs must be at least 1", 1);
                break;
            default:
                handleError("Usage: ./nyuenc [-j njobs] <file1> [file2] ...\n", 1);
        }
    }

    if (optind >= argc)
        handleError("Expected argument after options\n", 1);

    Queue tasks;
    Queue completed_tasks;
    init_queue(&tasks);
    init_queue(&completed_tasks);

    int arg_pos = optind;
    while (arg_pos < argc) {
        int fd = open(argv[arg_pos++], O_RDONLY);
        if (fd == -1)
            handleError("Error: unable to open file", 1);

        struct stat sb;
        if (fstat(fd, &sb) == -1) {
            close(fd);
            handleError("Error: unable to get file size", 1);
        }

        char* input_data = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (input_data == MAP_FAILED) {
            close(fd);
            handleError("Error: unable to mmap file", 1);
        }

        int chunk_count = sb.st_size / 4096 + (sb.st_size % 4096 == 0 ? 0 : 1);
        for (int i = 0; i < chunk_count; i++) {
            Data* data = malloc(sizeof(Data));
            data->input_data = input_data + (i * 4096);
            data->input_size = (i < chunk_count - 1) ? 4096 : (sb.st_size - (i * 4096));
            enqueue(&tasks, data);
        }
        close(fd);
    }

    while (tasks.size > 0) {
        Data* data = dequeue(&tasks);
        encode_data(data);
        enqueue(&completed_tasks, data);
    }

    char prev_last_char = 0;
    unsigned int prev_last_count = 0;
    while (completed_tasks.size > 0) {
        Data* data = dequeue(&completed_tasks);
        if (prev_last_count > 0 && prev_last_char == data->encoded_data[0]) {
            data->encoded_data[1] += prev_last_count;
            write(STDOUT_FILENO, data->encoded_data, data->enc_size - 2);
        } else {
            if(prev_last_count > 0) {
                write(STDOUT_FILENO, &prev_last_char, 1);
                write(STDOUT_FILENO, &prev_last_count, 1);
            }
            write(STDOUT_FILENO, data->encoded_data, data->enc_size - 2);
        }
        prev_last_char = data->encoded_data[data->enc_size - 2];
        prev_last_count = data->encoded_data[data->enc_size - 1];
    }

    write(STDOUT_FILENO, &prev_last_char, 1);
    write(STDOUT_FILENO, &prev_last_count, 1);

    return 0;
}

void handleError(const char* message, int exitCode) {
    fprintf(stderr, "%s\n", message);
    if (exitCode != 0) {
        exit(exitCode);
    }
}

/*Reference:
https://people.cs.rutgers.edu/~pxk/416/notes/c-tutorials/getopt.html
https://eric-lo.gitbook.io/memory-mapped-io/shared-memory
https://www.prepbytes.com/blog/c-programming/unsigned-int-in-c/#:~:text=The%20general%20syntax%20for%20declaring,count%20of%20type%20unsigned%20int.
https://www.educative.io/answers/what-is-the-write-function-in-c
https://www.codesdope.com/blog/artiscle/making-a-queue-using-linked-list-in-c/
https://www.how2lab.com/programming/c/structure-function#:~:text=A%20structure%20can%20be%20transferred,by%20passing%20address%20of%20variable.
https://www.cs.ucf.edu/courses/cop3502/nihan/spr03/queue.pdf
*/
