**How to Run**
-   Server:  ./bin/jobExecutorServer <port> <num_workers> <buffer_size>
    Example:
    ./bin/jobExecutorServer 5000 5 5
    <port>: The port number on which the server listens for connections.
    <num_workers>: Number of worker threads to handle job execution.
    <buffer_size>: Size of the buffer for handling job queues.

-   Commander:  ./bin/jobCommander <server_address> <port> <command>
    ./bin/jobCommander linux01.di.uoa.gr 5000 issueJob ls
    <server_address>: Address of the server.
    <port>: The port number the server is listening on.
    <command>: Command to be issued (e.g., issueJob, poll, exit, setConcurrency, stop).


**Commander Details**
The commander connects to the server, allowing users to issue various commands. It handles commands in two main ways:

1. issueJob Command:
Sends a job request to the server and continues accepting other commands concurrently.
Waits for a job confirmation message and the job execution output from a different thread.

2. Other Commands:
Commands like poll, exit, setConcurrency, and stop send a request to the server and wait for an immediate response.


**Server Details**
The server continuously listens for incoming connections and creates a new controller thread for each client connection. The controller thread manages the submission and execution of jobs through worker threads.

Controller Thread
- Role: Each controller thread is responsible for processing commands from a connected commander.
- Functionality:
    - Submits jobs to the job queue.
    - Notifies worker threads to execute the jobs.
    - Handles different commands uniquely to ensure proper job management and response delivery.


**Command Handling**
1. issueJob:
 - Received by the controller thread.
 - Job is queued and a worker thread is notified.
 - Worker thread executes the job and sends the result back to the commander through a different response thread.

2. poll, exit, setConcurrency, stop:
 - Directly handled by the controller thread.
 - Sends the command to the server, waits for execution, and returns the response to the commander.


**Buffer Handling**
- Buffer Size: Defined during server startup, this determines the maximum number of jobs that can be queued.
- Queue Management: Ensures jobs are processed in a FIFO manner. When the buffer is full, new job requests may be blocked until a slot becomes available.


**Worker Threads**
- Initialization: A fixed number of worker threads are created based on the users choice.
- Job Execution: Each worker thread waits for jobs in the queue, executes them, and sends the results back to the appropriate commander thread.


**Code Design Details**
- Thread Management: Uses multithreading to handle multiple client connections and concurrent job executions efficiently.
- Synchronization: Proper synchronization mechanisms (e.g., mutexes, condition variables) are used to manage shared resources and job queues, ensuring thread safety.
