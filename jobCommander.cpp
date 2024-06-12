#include <iostream>
#include "../include/headers.hpp"

using namespace std;

void perror_exit(char *message);


void handle_command(int sockfd, jobCommanderInfo info) {
    char buffer[MAX_INPUT]; // to store server's response
    
    /* send command to server */
    if(write(sockfd, info.jobCommanderInputCommand.c_str(), info.jobCommanderInputCommand.length()) == -1) {
        perror_exit("socket writing");
    }

    cout << PURPLE << "Recieved: " << RESET;

    /* read server's response */
    memset(buffer, 0, MAX_INPUT);
    if(read(sockfd, buffer, MAX_INPUT - 1) < 0) {
        perror_exit("socket reading");
    }

    cout << buffer << endl;
}

void handle_issue(int sockfd, jobCommanderInfo info) {
    char buffer[MAX_INPUT]; // to store server's response

    /* send command to server */
    if (write(sockfd, info.jobCommanderInputCommand.c_str(), info.jobCommanderInputCommand.length()) == -1) {
        perror_exit("socket writing");
    }

    /* read the message JOB SUBMITTED */ 
    cout << PURPLE << "Received: " << RESET;

    memset(buffer, 0, MAX_INPUT);
    int bytesRead = read(sockfd, buffer, MAX_INPUT - 1);
    if (bytesRead < 0) {
        perror_exit("socket reading");
    }
    buffer[bytesRead] = '\0';  
    cout << buffer << endl;
    
    /* read job execution output */
    while (true) {  // loop until all chuncks are read and print them
        memset(buffer, 0, MAX_INPUT);
        bytesRead = read(sockfd, buffer, MAX_INPUT - 1);
        if (bytesRead < 0) {
            perror_exit("socket reading");
        } else if (bytesRead == 0) {
            break; 
        }
        buffer[bytesRead] = '\0'; 
        cout << buffer;
    }
    
    cout << endl;
}



int main(int argc, char *argv[]) {

    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *hp;   

    if (argc < 4) {
        cout << "Error. Wrong Amount of Arguments\n";
        return -1;
    }

    /* fork and put job in background */
    pid_t pid = fork();
    if (pid == -1) {
        perror_exit("fork");
    } else if (pid > 0) {
        return 0;
    }

    jobCommanderInfo info;
    info.serverName = argv[1];
    info.portNum = atoi(argv[2]);
    info.jobCommanderInputCommand = argv[3];
    for (int i = 4; i < argc; i++) {
        info.jobCommanderInputCommand += " " + string(argv[i]);
    }
    cout << info.jobCommanderInputCommand << endl;

    /* create socket */
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        perror_exit ("socket") ;
    
    /* resolve server name */
    if((hp = gethostbyname(argv[1])) == NULL) {
        perror_exit("gethostbyname");
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(info.portNum);
    memcpy(&serv_addr.sin_addr, hp->h_addr, hp->h_length);

    /* connect to server */
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror_exit("connnect");
    }   
    printf("Connecting to %s port %d \n", argv[1], info.portNum);

    /* convert string into char* and split tokens */
    char buf[MAX_INPUT];
    char *tokens[MAX_TOKENS]; 
    strcpy(buf, info.jobCommanderInputCommand.c_str());
    char *token = strtok(buf, " ");
    int i = 0;
    while(token != NULL) {
        tokens[i++] = token;
        token = strtok(NULL, " ");
    }
    tokens[i] = NULL;  

    if(strcmp(tokens[0], "exit") == 0) {
        if(i > 1) {  // handle case where more than one arguments are given
            cout << "Error. To terminate server, type 'exit'\n";
            close(sockfd);
            exit(1);
        }

        handle_command(sockfd, info);
    } 
    else if(strcmp(tokens[0], "issueJob") == 0) {  
        if(i == 1) {  // handle case where no command to execute is given
            cout << "Error. To execute a command, type issueJob <job>\n";
            close(sockfd);
            exit(1);
        }

        handle_issue(sockfd, info);
    }
    else if(strcmp(tokens[0], "setConcurrency") == 0) {  
        if(i != 2) {  // handle case where no level is given
            cout << "Error. To change concurrency level, type setConcurrency <N>\n";
            close(sockfd);
            exit(1);
        }
        else if(atoi(tokens[1]) <= 0) {
            cout << "Error. To change concurrency level, give N > 0\n";
            close(sockfd);
            exit(1);
        }

        handle_command(sockfd, info);
    }
    else if(strcmp(tokens[0], "poll") == 0) {
        if (i != 1) {  // handle case where more than one arguments are given
            cout << "Error. Give poll\n";
            close(sockfd);
            exit(1);
        }
        
        handle_command(sockfd, info);
    } 
    else if(strcmp(tokens[0], "stop") == 0) {
        if(i != 2) {
            cout << "Error. To stop a job, type stop job_XX\n";
            close(sockfd);
            exit(1);
        }
    
        handle_command(sockfd, info);
    }
    else {
        cout << "Error. command not found" << endl;
        close(sockfd);
        exit(1);
    }

    close(sockfd);
    return 0;
}

