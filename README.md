# ⚡ Job Execution System

A **multi-threaded job execution system** that efficiently handles task management using a **server-client architecture**. The system allows multiple clients (Commander) to submit jobs to a central server, which then executes the tasks using a pool of worker threads.

---

## How to Run

### **Start the Server**
To launch the **Job Executor Server**, run: ./bin/jobExecutorServer <num_workers> <buffer_size> 

Example: ./bin/jobExecutorServer 5000 5 
5 : The port number on which the server listens for connections. 
<num_workers>: Number of worker threads to handle job execution. 
<buffer_size>: Size of the buffer for handling job queues.

To launch the **Job Commander**, run: ./bin/jobCommander <server_address> <port> <command>

Example: ./bin/jobCommander linux01.di.uoa.gr 5000 issueJob ls 
<server_address>: Address of the server.  
<port>: The port number the server is listening on. 
<command>: Command to be issued (e.g., issueJob, poll, exit, setConcurrency, stop).

## Commander Details  

The **Commander** connects to the server, allowing users to issue various commands. It processes commands in two main ways:

### `issueJob` Command  
- Sends a job request to the **server** while continuing to accept other commands **concurrently**.  
- Waits for a **job confirmation message** and execution output from a separate **response thread**.  

### Other Commands (`poll`, `exit`, `setConcurrency`, `stop`)  
- These commands **send a request to the server** and wait for an **immediate response**.  

---

## Server Details  

The **Server** continuously listens for incoming connections and **creates a new Controller Thread** for each connected client.  
The **Controller Thread** manages the **submission and execution of jobs** through **Worker Threads**.

---

## Controller Thread  

### **🔹 Role:**  
Each **Controller Thread** is responsible for **processing commands** from a connected **Commander**.  

### **🔹 Functionality:**  
- **Submits jobs** to the **Job Queue**.  
- **Notifies Worker Threads** for job execution.  
- **Handles different commands** to ensure proper **job management** and response delivery.  

---

## Command Handling  

### **`issueJob` Execution Process**  
1.The **Controller Thread** receives the job request.  
2️.The **job is queued**, and a **Worker Thread** is notified.  
3️.The **Worker Thread executes the job** and sends the result back to the **Commander** via a **response thread**.  

### **Handling Other Commands (`poll`, `exit`, `setConcurrency`, `stop`)**  
- Directly **processed** by the **Controller Thread**.  
- The command is sent, executed, and an **immediate response** is returned to the **Commander**.  

---

## Buffer Handling  

### **🔹 Buffer Size:**  
- Defined during **server startup**, it determines the **maximum number of jobs** that can be **queued**.  

### **🔹 Queue Management:**  
- Jobs are processed in a **FIFO (First-In, First-Out)** manner.  
- If the **buffer is full**, **new job requests may be blocked** until space is available.  

---

## Worker Threads  

### **🔹 Initialization:**  
- A **fixed number of Worker Threads** is created based on the **user's configuration**.  

### **🔹 Job Execution:**  
- Each **Worker Thread waits** for jobs in the queue.  
- Once assigned, the **Worker Thread executes the job**.  
- The **execution result** is sent back to the respective **Commander thread**.  

---

## Code Design Details  

### **🔹 Thread Management:**  
- Uses **multi-threading** to handle **multiple client connections** and **concurrent job executions** efficiently.  

### **🔹 Synchronization:**  
- Implements **mutexes** and **condition variables** to manage shared resources and job queues, ensuring **thread safety**.  
