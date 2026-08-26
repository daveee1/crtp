This README is designed to reflect the high engineering standards of the project. It is structured to serve as professional documentation for a GitHub repository or a technical portfolio, explaining not just *what* the code does, but the *theoretical foundations* behind it.

***

# Real-Time Task Execution Supervisor

## 📌 Overview
This project is a high-performance, distributed simulation of a **Real-Time Operating System (RTOS) Task Executive**. It utilizes a Client-Server architecture over TCP/IP to allow remote, dynamic activation and deactivation of periodic tasks. 

The core innovation of this system is its **Admission Control Policy**: the server does not blindly accept tasks. Instead, it performs rigorous **Response Time Analysis (RTA)** to ensure that adding a new task will not violate the deadlines of existing tasks, thereby guaranteeing system determinism.

## 🧠 Theoretical Foundation

### 1. Schedulability Analysis
The system implements a two-stage admission control mechanism based on **Rate Monotonic Scheduling (RMS)** principles:

*   **Utilization Factor Test (Necessary Condition):** 
    Before performing expensive computations, the system checks the total processor utilization $U$:
    $$U = \sum_{i=1}^{n} \frac{C_i}{P_i} \leq 1.0$$
    If $U > 1$, the task is immediately rejected as the system is physically over-utilized.
*   **Response Time Analysis (Exact Test):**
    If the utilization is within the bounds but falls in the inconclusive region ($0.693 < U \leq 1.0$), the system executes an iterative RTA algorithm to calculate the worst-case response time $R_i$ for the task set:
    $$R_i^{(k+1)} = C_i + \sum_{j \in hp(i)} \left\lceil \frac{R_i^{(k)}}{P_j} \right\rceil C_j$$
    A task is only activated if $R_i \leq D_i$ (Deadline) for all tasks in the set.

### 2. Timing Determinism
To prevent "drift" (cumulative timing errors), the system uses **Absolute Time Wake-up** logic. Instead of relative sleeps, it calculates the next absolute deadline using `CLOCK_MONOTONIC` and `clock_nanosleep(TIMER_ABSTIME)`, ensuring high-precision periodic execution even under heavy system load.

## 🏗️ System Architecture

### **The Server (The Executive)**
*   **Connection Manager:** Uses `select()` to manage multiple concurrent client connections.
*   **Command Parser:** Translates incoming TCP byte-streams into high-level commands (`ACTIVATE`, `BLOCK`, etc.).
*   **Schedulability Engine:** The mathematical core that validates task sets.
*   **Task Manager:** Spawns and manages worker threads. It uses a fixed-size array (`tasks_active`) to manage concurrent execution slots.

### **The Client (The Controller)**
*   A lightweight CLI tool that communicates with the server.
*   Allows users to interactively request task changes or view system status.

### **Concurrency & Synchronization**
The system is highly concurrent and utilizes several POSIX primitives:
*   **`pthread_mutex`**: Protects the `tasks_active` array and prevents race conditions during RTA calculations.
*   **`sem_t` (Semaphores)**: Controls access to the task slots, ensuring the server never exceeds its maximum task capacity.
*   **`pthread_cond_t` (Condition Variables)**: Manages the client connection pool, blocking new clients gracefully when the capacity is reached.

## 🛠️ Technical Stack
*   **Language:** C (C99/C11)
*   **Libraries:** POSIX Threads (`pthreads`), Sockets (`sys/socket.h`), Real-time extensions (`time.h`).
*   **Operating System:** Linux/Unix-based systems.

## 🚀 Getting Started

### Compilation
Exploit the 'make' file available in the main directory 
```bash
# Compile the Server
make 
```

or use `gcc` to compile the server, client, and utility files. Note that the math library (`-lm`) and thread library (`-pthread`) are required.

```bash
# Compile the Server
gcc -o server server.c task.c rta.c utils.c -pthread -lm

# Compile the Client
gcc -o client client.c utils.c -pthread -lm
```

### Running the Simulation

1.  **Start the Server:**
    ```bash
    ./server <PORT> <VERBOSE_LEVEL>
    # Example: ./server 8080 2
    ```
    *Verbosity levels: 0 (Error only), 1 (Warning), 2+ (Info/Analysis)*

2.  **Start the Client:**
    ```bash
    ./client <IP_ADDRESS> <PORT> <VERBOSE_LEVEL>
    # Example: ./client 127.0.0.1 8080 2
    ```

3.  **Client Commands:**
    *   `a <ID>`: Activate task (e.g., `a 1`)
    *   `b <ID>`: Block/Deactivate task (e.g., `b 1`)
    *   `help`: Show help menu
    *   `quit`: Disconnect client
    *   `stop`: Force server shutdown

## 🧪 Stress Testing
A bash script `stress_test.sh` is provided to simulate high-concurrency scenarios. It launches 6 simultaneous clients performing overlapping task activations and deactivations to test the robustness of the mutex/semaphore logic.

```bash
chmod +x stress_test.sh
./stress_test.sh
```

## 📄 License
This project is provided for educational and research purposes.