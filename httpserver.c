// Asgn 2: A simple HTTP server.
// By: Eugene Chou
//     Andrew Quinn
//     Brian Zhao

#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "debug.h"
#include "response.h"
#include "request.h"
#include "rwlock.h"
#include "queue.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/stat.h>

void handle_connection(int);
void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);
void *dummy_function(void *);
void write_entry(char *, conn_t *, const Response_t *);
queue_t *q;
int num_threads;
int lock_count = 0;

typedef struct lock {
    char *filename;
    rwlock_t *rw;
    int activeThreads;
} lock_t;

lock_t ** lockArray;

rwlock_t * get_lock(lock_t ** lockArray, const char * uri, int lock_count){
    for(int i = 0; i < lock_count; i++){
        if(strcmp(lockArray[i]->filename, uri) == 0){
            return lockArray[i]->rw;
        }
    }
    return NULL;
}
// it searches the array, finds the associated rwlock with a URI
// and then returns the lock_t for it


void write_entry(char *req, conn_t *conn, const Response_t *res) {
    char *uri = conn_get_uri(conn);
    char *id = conn_get_header(conn, "Request-Id");
    uint16_t code = response_get_code(res);
    if (id == NULL) {
        id = "0";
    }
    // (void) uri;
    // (void) req;
    // (void) code;
    fprintf(stderr, "%s,/%s,%d,%s\n", req, uri, code, id);
    // fprintf(stderr, "%s\n", uri);
}
// void write_entry2(char * req, char* uri, char* id, const Response_t * res){
//     // char * uri = conn_get_uri(conn);
//     // char * id = conn_get_header(conn, "Request-Id");
//     uint16_t code = response_get_code(res);
//     // if(id == NULL){
//     //     id = "0";
//     // }
//     // (void) uri;
//     // (void) req;
//     fprintf(stderr, "%s,/%s,%d,%s\n", req, uri, code, id);
//     // fprintf(stderr, "%s\n", uri);
// }
int main(int argc, char **argv) {
    if (argc < 2) {
        warnx("wrong arguments: %s port_num", argv[0]);
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    /* Use getopt() to parse the command-line arguments */
    num_threads = 4;
    int opt = 0;
    while (opt != -1) {
        opt = getopt(argc, argv, "t:");
        if (opt == 't') {
            num_threads = strtoull(optarg, NULL, 10);
            break;
        }
    }
    char *endptr = NULL;
    size_t port = (size_t) strtoull(argv[optind], &endptr, 10);
    if (endptr && *endptr != '\0') {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }

    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);
    Listener_Socket sock;
    if (listener_init(&sock, port) < 0) {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }

    /* Initialize worker threads, the queue, and other data structures */
    // queue
    q = queue_new(num_threads);
    // lock array
    lockArray = calloc(num_threads, sizeof(lock_t));
    // worker threads
    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, dummy_function, NULL);
    }
    // how big does the q need to be?
    // what function should I initialize the threads to execute?
    // do I need to make a dummy function for the threads to initialize to?
    /* Hint: You will need to change how handle_connection() is used */
    while (1) {
        uintptr_t connfd = listener_accept(&sock);
        queue_push(q, (void *) connfd); // push new connections to the queue
    }
    queue_delete(&q);
    for(int i = 0; i < lock_count; i++){
        rwlock_delete(&lockArray[i]->rw);
        free(&lockArray[i]);
    }
    free(lockArray);
    // rwlock_delete(&rw);
    return EXIT_SUCCESS;
}

void *dummy_function(void *arg) {
    (void) arg;
    while (true) { // wait for the next connection
        uintptr_t connfd = -1;
        if (queue_pop(q, (void *) (&connfd)) == true) { // no need to block, queue does it for you
            // connection found, handle connection
            handle_connection(connfd);
            close(connfd);
        }
    }
}

void handle_connection(int connfd) {
    conn_t *conn = conn_new(connfd);
    const Response_t *res = conn_parse(conn);
    if (res != NULL) {
        conn_send_response(conn, res);
    } else {
        // debug("%s", conn_str(conn));
        const Request_t *req = conn_get_request(conn);
        if (req == &REQUEST_GET) {
            handle_get(conn);
        } else if (req == &REQUEST_PUT) {
            handle_put(conn);
        } else {
            handle_unsupported(conn);
        }
    }
    conn_delete(&conn);
}

void handle_get(conn_t *conn) {
    char *uri = conn_get_uri(conn);
    char *req = "GET";
    // char *id = conn_get_header(conn, "Request-Id");
    const Response_t *res = NULL;
    rwlock_t *rw = get_lock(lockArray, uri, lock_count);
    if(rw == NULL){
        rw = rwlock_new(READERS, 1);
        lock_t * lock = calloc(1, sizeof(lock_t));
        lock->rw = rw;
        lock->activeThreads = 1;
        lock->filename = uri;
        lockArray[lock_count] = lock;
        lock_count++;
    }
    // debug("Handling GET request for %s", uri);
    // What are the steps in here?
    reader_lock(rw);
    // 1. Open the file.
    // If open() returns < 0, then use the result appropriately
    //   a. Cannot access -- use RESPONSE_FORBIDDEN
    //   b. Cannot find the file -- use RESPONSE_NOT_FOUND
    //   c. Other error? -- use RESPONSE_INTERNAL_SERVER_ERROR
    // (Hint: Check errno for these cases)!
    int fd = open(uri, O_RDONLY, 0664);
    if (fd < 0) {
        if (errno == EACCES) {
            res = &RESPONSE_FORBIDDEN;
        }
        if (errno == ENOENT) {
            res = &RESPONSE_NOT_FOUND;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
        }
    }
    // 2. Get the size of the file.
    // (Hint: Checkout the function fstat())!
    struct stat fileStat;
    fstat(fd, &fileStat);
    // 3. Check if the file is a directory, because directories *will*
    // open, but are not valid.
    // (Hint: Checkout the macro "S_IFDIR", which you can use after you call fstat()!)
    if (S_ISDIR(fileStat.st_mode)) {
        res = &RESPONSE_FORBIDDEN;
    }
    size_t size = fileStat.st_size;
    // 4. Send the file
    // (Hint: Checkout the conn_send_file() function!)
    if (res == NULL) { // if res is NULL, no issues were encountered- set response to "OK"
        res = &RESPONSE_OK;
    }
    // conn_send_response(conn, &RESPONSE_OK);
    // (void) size;
    conn_send_file(conn, fd, size);
    // (void) req;
    write_entry(req, conn, res);
    // 5. Close the file
    reader_unlock(rw);
    close(fd);
    // conn_send_response(conn, &RESPONSE_OK);
    return;
}

void handle_put(conn_t *conn) {
    char *uri = conn_get_uri(conn);
    char *req = "PUT";
    const Response_t *res = NULL;
    rwlock_t *rw = get_lock(lockArray, uri, lock_count);
    if(rw == NULL){
        rw = rwlock_new(READERS, 1);
        lock_t * lock = calloc(1, sizeof(lock_t));
        lock->rw = rw;
        lock->activeThreads = 1;
        lock->filename = uri;
        lockArray[lock_count] = lock;
        lock_count++;
    }
    // debug("Handling PUT request for %s", uri);
    // What are the steps in here?
    writer_lock(rw);
    // 1. Check if file already exists before opening it.
    // (Hint: check the access() function)!
    int exist = access(uri, F_OK);
    // 2. Open the file.
    // If open() returns < 0, then use the result appropriately
    //   a. Cannot access -- use RESPONSE_FORBIDDEN
    //   b. File is a directory -- use RESPONSE_FORBIDDEN
    //   c. Cannot find the file -- use RESPONSE_FORBIDDEN
    //   d. Other error? -- use RESPONSE_INTERNAL_SERVER_ERROR
    // (Hint: Check errno for these cases)!
    int fd = creat(uri, 0664);
    if (fd < 0) {
        if (errno == EACCES) {
            res = &RESPONSE_FORBIDDEN;
        }
        if (errno == ENOENT) {
            res = &RESPONSE_NOT_FOUND;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
        }
    }
    // writer_unlock(rw);
    // char temp[] = "tempXXXXXX";
    // int tempfd = mkstemp(temp);
    conn_recv_file(conn, fd);
    // writer_lock(rw);
    // rename(uri, temp);
    // remove(temp);
    // 3. Receive the file
    // (Hint: Checkout the conn_recv_file() function)!
    // 4. Send the response
    // (Hint: Checkout the conn_send_response() function)!
    if (res == NULL) {
        if (exist == 0) {
            res = &RESPONSE_OK;
        } else {
            res = &RESPONSE_CREATED;
        }
    }
    // create a temp file to receive message body contents into
    conn_send_response(conn, res);
    // (void)req;
    write_entry(req, conn, res);
    writer_unlock(rw);
    // 5. Close the file
    close(fd);
    return;
}

void handle_unsupported(conn_t *conn) {
    // debug("Handling unsupported request");
    // Send responses
    conn_send_response(conn, &RESPONSE_NOT_IMPLEMENTED);
}
