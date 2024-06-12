#include <iostream>
#include <fstream>
#include <cstdlib> 
#include <cmath>
#include <atomic>
#include <cstring>
#include <string>
#include <queue>
#include <vector>
#include <mutex>
#include <unistd.h> 
# include <fcntl.h>
# include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <thread>
# include <netdb.h>

using namespace std;

#define MAX_INPUT 1024
#define MAX_TOKENS 30

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define PURPLE  "\033[35m"

typedef struct {
    int clientSocket;
    string jobID;
    string job;
} Job;

struct jobCommanderInfo {
    string serverName;
    int portNum;
    string jobCommanderInputCommand;
};

struct JobBuffer {
    vector<Job> jobs;
    int count, bufferSize;
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    JobBuffer(int size) : jobs(size), count(0), bufferSize(size) {
        pthread_mutex_init(&mutex, nullptr);
        pthread_cond_init(&cond, nullptr);
    }

    ~JobBuffer() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }
};

struct ControllerInfo {
    int sockfd;
    JobBuffer *jobBuffer;
    pthread_cond_t jobsDone;
};

void perror_exit(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}