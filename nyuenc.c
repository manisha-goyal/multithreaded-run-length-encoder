#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>

#define MAX_CHUNK_SIZE 4096

typedef struct {
    int input_order;
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

pthread_mutex_t tasks_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t completed_tasks_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tasks_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t completed_tasks_cond = PTHREAD_COND_INITIALIZER;

Queue tasks;
Queue completed_tasks;

void init_queue(Queue* queue);
void enqueue(Queue* queue, Data* data);
Data* dequeue(Queue* queue);
void* encode();
void handle_error(const char* message, int exitCode);
void create_thread_pool(int num_jobs);
int create_task_queue(int arg_pos, int argc, char *argv[]);
void process_completed_tasks(int task_count);

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

int main(int argc, char *argv[]) {
    int opt;
    int num_jobs = 1;

    while ((opt = getopt(argc, argv, "j:")) != -1) {
        switch (opt) {
            case 'j':
                num_jobs = atoi(optarg);
                if (num_jobs < 1)
                    handle_error("Number of jobs must be at least 1", 1);
                break;
            default:
                handle_error("Usage: ./nyuenc [-j njobs] <file1> [file2] ...\n", 1);
        }
    }

    if (optind >= argc)
        handle_error("Usage: ./nyuenc [-j njobs] <file1> [file2] ...\n", 1);

    init_queue(&tasks);
    init_queue(&completed_tasks);

    create_thread_pool(num_jobs);
    int task_count = create_task_queue(optind, argc, argv);
    process_completed_tasks(task_count);
    return 0;
}

void* encode() {
    while (1) {
        pthread_mutex_lock(&tasks_mutex);
        while (tasks.size == 0)
            pthread_cond_wait(&tasks_cond, &tasks_mutex);
        Data* data = dequeue(&tasks);
        pthread_mutex_unlock(&tasks_mutex);
        
        char *read_ptr = data->input_data;
        char *end_ptr = data->input_data + data->input_size;
        char *write_ptr = data->encoded_data = malloc(data->input_size * 2);
        while (read_ptr < end_ptr) {
            char current_char = *read_ptr++;
            int count = 1;
            while (read_ptr < end_ptr && current_char == *read_ptr) {
                read_ptr++;
                count++;
            }
            *write_ptr++ = current_char;
            *write_ptr++ = (unsigned int)count;
        }
        data->enc_size = write_ptr - data->encoded_data;
        
        pthread_mutex_lock(&completed_tasks_mutex);
        enqueue(&completed_tasks, data);
        pthread_cond_signal(&completed_tasks_cond);
        pthread_mutex_unlock(&completed_tasks_mutex);
    }
}

void create_thread_pool(int num_jobs) {
    pthread_t threads[num_jobs];
    for (int i = 0; i < num_jobs; i++) {
        int thread = pthread_create(&threads[i], NULL, encode, NULL);
        if(thread)
            handle_error("Error: unable to create threads", 1);
    }
}

int create_task_queue(int arg_pos, int argc, char *argv[]) {
    int input_order = 0;
    int task_count = 0;
    while (arg_pos < argc) {
        int fd = open(argv[arg_pos++], O_RDONLY);
        if (fd == -1)
            handle_error("Error: unable to open file", 1);

        struct stat sb;
        if (fstat(fd, &sb) == -1) {
            close(fd);
            handle_error("Error: unable to get file size", 1);
        }

        char* input_data = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (input_data == MAP_FAILED) {
            close(fd);
            handle_error("Error: unable to mmap file", 1);
        }

        int chunks = sb.st_size / MAX_CHUNK_SIZE + (sb.st_size % MAX_CHUNK_SIZE == 0 ? 0 : 1);
        for (int i = 0; i < chunks; i++) {
            Data* data = malloc(sizeof(Data));
            data->input_order = input_order++;
            data->input_data = input_data + (i * MAX_CHUNK_SIZE);
            data->input_size = (i < chunks - 1) ? MAX_CHUNK_SIZE : (sb.st_size - (i * MAX_CHUNK_SIZE));
            pthread_mutex_lock(&tasks_mutex);
            enqueue(&tasks, data);
            pthread_cond_signal(&tasks_cond);
            pthread_mutex_unlock(&tasks_mutex);
        }
        task_count += chunks;
        close(fd);
    }

    return task_count;
}

void process_completed_tasks(int task_count) {
    Data* completed_tasks_ordered[task_count];
    char prev_last_char = 0;
    unsigned int prev_last_count = 0;
    int task_index = 0;

    for(int i = 0; i < task_count; i++)
        completed_tasks_ordered[i] = NULL;

    while (task_index < task_count) {
        if (completed_tasks_ordered[task_index] != NULL) {
            Data *data = completed_tasks_ordered[task_index];
            if (task_index > 0 && prev_last_char == data->encoded_data[0]) {
                data->encoded_data[1] += prev_last_count;
                write(STDOUT_FILENO, data->encoded_data, data->enc_size - 2);
            }
            else {
                if (prev_last_count > 0) {
                    write(STDOUT_FILENO, &prev_last_char, 1);
                    write(STDOUT_FILENO, &prev_last_count, 1);
                }
                write(STDOUT_FILENO, data->encoded_data, data->enc_size - 2);
            }
            prev_last_char = data->encoded_data[data->enc_size - 2];
            prev_last_count = data->encoded_data[data->enc_size - 1];
            task_index++;
            continue;
        }

        pthread_mutex_lock(&completed_tasks_mutex);
        while (completed_tasks.size == 0)
            pthread_cond_wait(&completed_tasks_cond, &completed_tasks_mutex);
        Data *encoded_chunk = dequeue(&completed_tasks);
        pthread_mutex_unlock(&completed_tasks_mutex);
        completed_tasks_ordered[encoded_chunk->input_order] = encoded_chunk;
    }

    write(STDOUT_FILENO, &prev_last_char, 1);
    write(STDOUT_FILENO, &prev_last_count, 1);
}

void handle_error(const char* message, int exitCode) {
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
https://www.educative.io/answers/how-to-create-a-simple-thread-in-c
https://dev.to/quantumsheep/basics-of-multithreading-in-c-4pam
https://www.cs.cmu.edu/afs/cs/academic/class/15492-f07/www/pthreads.html#SCHEDULING
https://code-vault.net/lesson/j62v2novkv:1609958966824
https://alperenbayramoglu2.medium.com/thread-pools-in-a-nutshell-527414eef5f2
https://stackoverflow.com/questions/10600250/is-it-necessary-to-call-pthread-join
*/
