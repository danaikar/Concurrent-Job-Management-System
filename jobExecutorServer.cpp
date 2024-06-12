#include <iostream>
#include "../include/headers.hpp"

using namespace std;

atomic<bool> termination(false);
atomic<int> totalJobs;

int runningJobs = 0;
pthread_mutex_t runningJobsMutex = PTHREAD_MUTEX_INITIALIZER;
int concurrency = 1;
pthread_mutex_t concurrencyMutex = PTHREAD_MUTEX_INITIALIZER;

void handle_sigusr1(int sig) {
    termination = true;
}

/* a controller thread is created for each commander connection and its
   function is to submit jobs and notify worker threads execute them */

void* controllerThread(void* arg) {
    char generalBuffer[MAX_INPUT];
    ControllerInfo* cInfo = (ControllerInfo*)arg;
    JobBuffer *jBuffer = cInfo->jobBuffer;

    while (!termination) {
        /* receive command from client */
        memset(generalBuffer, 0, sizeof(generalBuffer));
        int r = read(cInfo->sockfd, generalBuffer, MAX_INPUT - 1);
        if (r < 0) {
            perror("read");
            break;
        } 
        cout << GREEN << "Received: " << generalBuffer << RESET << endl;
    
        /* convert string into char* and split tokens */
        char *tokens[MAX_TOKENS]; 
        char *token = strtok(generalBuffer, " ");
        int i = 0;
        while(token != NULL) {
            tokens[i++] = token;
            token = strtok(NULL, " ");
        }
        tokens[i] = NULL;  

        /* handle each case differently */
        if(strcmp(tokens[0], "exit") == 0) {
            termination = true;
            string message = "SERVER TERMINATED\n";
            if (write(cInfo->sockfd, message.c_str(), message.length()) < 0) {  
                perror("write1");
            }
            close(cInfo->sockfd);
             
            /* empty the buffer and signal worker threads */            
            string mes = "SERVER TERMINATED BEFORE EXECUTION\n";
            for (int j = 0; j < jBuffer->count; j++) {
                Job job = jBuffer->jobs[j];
                if (write(job.clientSocket, mes.c_str(), mes.length()) < 0) {  
                    perror("write2");
                }
                close(job.clientSocket);
            }
            jBuffer->count = 0;
            pthread_cond_broadcast(&jBuffer->cond);
            pthread_mutex_unlock(&jBuffer->mutex);

            break;
        } 
        else if(strcmp(tokens[0], "issueJob") == 0) {  
            totalJobs++;
            string jobCommand;
            for (int j = 1; j < i; j++) {  // merge the commands into a single string
                jobCommand += tokens[j];
                jobCommand += " ";
            }
            Job newJob;  // the triplet (jobID, job, clientSocket) we insert to the buffer
            newJob.jobID = "job_" + to_string(totalJobs);  // make job_XX format
            newJob.job = jobCommand;
            newJob.clientSocket = cInfo->sockfd; 

            string jobStr = newJob.jobID + "," + newJob.job + "," + to_string(cInfo->sockfd) + "\n";  // merge the triplet into one string

            /* add the job into the buffer */
            pthread_mutex_lock(&jBuffer->mutex);  // lock the buffers mutex to access it with safety
            if (jBuffer->count < jBuffer->bufferSize) {  // if the buffer is not full
                jBuffer->jobs[jBuffer->count] = newJob;
                jBuffer->count++;
                cout << "JOB (count=" << jBuffer->count << ") with id=" << newJob.jobID << " INSERTED TO BUFFER \n";
                pthread_cond_signal(&jBuffer->cond);  // signal worker thread about the new job

                string mess = "JOB < " + newJob.jobID + ", " + newJob.job + "> SUBMITTED\n";
                if (write(cInfo->sockfd, mess.c_str(), mess.length()) < 0) {
                    perror("write3");
                }
            }          
            pthread_mutex_unlock(&jBuffer->mutex);
            break;
        }
        else if(strcmp(tokens[0], "setConcurrency") == 0) {
            cout << "Concurrency set from " << concurrency;
            pthread_mutex_lock(&concurrencyMutex);
            concurrency = atoi(tokens[1]);
            pthread_mutex_unlock(&concurrencyMutex);
            cout << " to" << concurrency << endl;
            
            string mess = "CONCURRENCY SET AT " + to_string(concurrency) + "\n";
            if (write(cInfo->sockfd, mess.c_str(), mess.length()) < 0) {
                perror("write4");
            }
                
            close(cInfo->sockfd);
            break;
        }
        else if(strcmp(tokens[0], "stop") == 0) {
            pthread_mutex_lock(&jBuffer->mutex);    // to edit the buffer, lock mutex 

            int found = 0;
            for (int j = 0; j < jBuffer->count; j++) {
                if (jBuffer->jobs[j].jobID == tokens[1]) {
                    /* remove all waiting job from the buffer */
                    for (int k = j; k < jBuffer->count - 1; k++) {
                        jBuffer->jobs[k] = jBuffer->jobs[k + 1];
                    }
                    jBuffer->count--;

                    string mess = "JOB REMOVED\n";
                    if (write(cInfo->sockfd, mess.c_str(), mess.length()) < 0) {
                        perror("write5");
                    }
                    found = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&jBuffer->mutex);

            if (found == 0) {
                string mess = "JOB NOT FOUND\n";
                if (write(cInfo->sockfd, mess.c_str(), mess.length()) < 0) {
                    perror("write6");
                }
            }

            close(cInfo->sockfd);
            break;
        }
        else if(strcmp(tokens[0], "poll") == 0) {
            pthread_mutex_lock(&jBuffer->mutex);
            vector<string> jobsList;

            for (int j = 0; j < jBuffer->count; j++) {
                //if (jBuffer->jobs[j].clientSocket == cInfo->sockfd) {
                    jobsList.push_back(jBuffer->jobs[j].jobID);
                //}
            }
            pthread_mutex_unlock(&jBuffer->mutex);

            string mess;
            if (jobsList.empty()) {
                mess = "NO PENDING JOBS\n";
            } else {
                mess = "JOBS IN BUFFER: ";
                for (const auto& jobID : jobsList) {
                    mess += jobID + " ";
                }
                mess += "\n";
            }

            if (write(cInfo->sockfd, mess.c_str(), mess.length()) < 0) {
                perror("write7");
            }

            close(cInfo->sockfd);
            break;
        }
        else {
            cout << "UNKNOWN COMMAND\n";
            close(cInfo->sockfd);
            break;
        }
    }
    
    cout << "CONTROLLER THREAD EXITS\n";
    return nullptr;
}




/* a pool of worker is being created by the main thread and each of 
   them waits a job to be added in the Buffer in order to execute it */

void* workerThread(void* arg) {
    JobBuffer *jBuffer = (JobBuffer*)arg;
    
    while (!termination) {
        Job job;

        /* if buffer is empty, wait for a job */
        pthread_mutex_lock(&jBuffer->mutex);   // to edit the buffer, lock the mutex 
        while (jBuffer->count == 0 && !termination) {  
            pthread_cond_wait(&jBuffer->cond, &jBuffer->mutex);
        }

        if (termination) {
            pthread_mutex_unlock(&jBuffer->mutex);
            break;
        }
        job = jBuffer->jobs[0];
        pthread_mutex_unlock(&jBuffer->mutex);

        if (jBuffer->count > 0) { 
            pthread_mutex_lock(&runningJobsMutex);  // check concurrency 
            while (runningJobs >= concurrency && !termination) {
                pthread_mutex_unlock(&runningJobsMutex);
                pthread_mutex_lock(&runningJobsMutex);
            }
            if (termination) {
                pthread_mutex_unlock(&runningJobsMutex);
                break;
            }
            runningJobs++;
            pthread_mutex_unlock(&runningJobsMutex);

            pid_t pid = fork();
            if (pid == 0) {  // Child process
                cout << "Executing " << job.jobID << endl;
                
                char *args[MAX_TOKENS];
                char *token = strtok(const_cast<char*>(job.job.c_str()), " ");
                int i = 0;
                while (token != NULL) {
                    args[i++] = token;
                    token = strtok(NULL, " ");
                }
                args[i] = NULL;

                /* open a pid.out file and redirect the output of exec here */
                string outputFileName = to_string(getpid()) + ".output";
                int fd = open(outputFileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    perror("open");
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);

                execvp(args[0], args);
                perror_exit("execvp");
                
            } 
            else if (pid > 0) {  // Parent process
                int status;
                waitpid(pid, &status, 0);

                pthread_mutex_lock(&jBuffer->mutex);   // to edit the buffer, lock the mutex 
                if (jBuffer->count > 0) {
                    /* get the first job and then rearrange the buffer */ 
                    for (int i = 1; i < jBuffer->count; i++) {
                        jBuffer->jobs[i - 1] = jBuffer->jobs[i];
                    }
                }                
                jBuffer->count--;
                pthread_mutex_unlock(&jBuffer->mutex);

                /* open the pid.out file */
                string outputFileName = to_string(pid) + ".output";
                int fd = open(outputFileName.c_str(), O_RDONLY);
                if (fd == -1) {
                    perror("open");
                    continue;
                }

                /* read the pid.out content */
                string messStart = "-----" + job.jobID + " output start------\n";
                write(job.clientSocket, messStart.c_str(), messStart.length());

                /* read continuously X bytes chuncks until read returns 0 - and send one chunck a time */
                char buffer[MAX_INPUT];
                int bytesRead;
                while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
                    if (write(job.clientSocket, buffer, bytesRead) == -1) {
                        perror("write8");
                        break;
                    }
                }
                close(fd);

                string messEnd = "-----" + job.jobID + " output end------\n";
                write(job.clientSocket, messEnd.c_str(), messEnd.length());
                close(job.clientSocket);

                /* delete .out file */
                remove(outputFileName.c_str());

                pthread_mutex_lock(&runningJobsMutex);
                runningJobs--;
                pthread_mutex_unlock(&runningJobsMutex);
            } 
            else {
                perror("Fork failed");
                pthread_mutex_lock(&runningJobsMutex);
                runningJobs--;
                pthread_mutex_unlock(&runningJobsMutex);
            }
        }

    }

    return nullptr;
}


int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Wrong number of arguments\n");
        return -1;
    }

    int sockfd, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    struct sockaddr *clientptr = (struct sockaddr *)&clientAddr;
    struct sockaddr *serverptr = (struct sockaddr *)&serverAddr;
    socklen_t clientlen;  

    signal(SIGUSR1, handle_sigusr1);  

    int portnum = atoi(argv[1]);
    int bufferSize = atoi(argv[2]);
    int threadPoolSize = atoi(argv[3]);
    
    JobBuffer jobBuffer(bufferSize);

    pthread_mutex_init(&jobBuffer.mutex, NULL);
    pthread_cond_init(&jobBuffer.cond, NULL);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror_exit("socket");
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(portnum);

    /* Bind socket to address */
    if (bind(sockfd, serverptr, sizeof(serverAddr)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    /* Listen for connections */
    if (listen(sockfd, bufferSize) == -1) {
        perror("listen");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", portnum);

    /* main thread creates threadPoolSize worker threads */
    pthread_t workerThreads[threadPoolSize];
    for (int i = 0; i < threadPoolSize; i++) {
        pthread_create(&workerThreads[i], NULL, workerThread, (void *)&jobBuffer);
    }

    while (!termination) {
        /* accept connection */
        clientlen = sizeof(clientAddr);
        if ((clientSocket = accept(sockfd, clientptr, &clientlen)) < 0) {  // blocks until a client connection request is received
            if (termination) break;
            perror("accept");
            continue;
        }
        cout << "Accepted connection\n";
        
        if (termination) {
            close(clientSocket);
            break;
        }
        
        /* create an instance of controller to pass it as argument in the thread */
        ControllerInfo* cInfo = new ControllerInfo();
        cInfo->sockfd = clientSocket;
        cInfo->jobBuffer = &jobBuffer;

        cout << "!! 1 !!\n";

        /* for each connection, create a controller thread */
        pthread_t contrThread;
        if (pthread_create(&contrThread, NULL, controllerThread, (void*)cInfo) != 0) {
            perror("pthread_create");
            close(clientSocket);
            delete cInfo;
            continue;
        }        
        pthread_detach(contrThread);  // main thread doesnt wait the controller thread to finish
        
        cout << "!! 2 !!\n";

        if (termination) {
            break;
        }
        cout << "!! 3 !!\n";

    }

    cout << "! join worker threads !\n";
    for (int i = 0; i < threadPoolSize; i++) {
        pthread_kill(workerThreads[i], SIGUSR1);
        pthread_join(workerThreads[i], NULL);
    }

    pthread_mutex_destroy(&jobBuffer.mutex);
    pthread_cond_destroy(&jobBuffer.cond);

    close(sockfd);
    return 0;
}

