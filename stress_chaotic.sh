#!/bin/bash

HOST="localhost"
PORT=8080
VERBOSE=4 # Kept moderate for scannability
HOLD_TIME=0.1
WAIT_TIME=0.2
NUM_WORKERS=100
TASKS_PER_CLIENT=50

run_chaotic_worker() {
    CLIENT_ID=$1

    (
        # 1. Random initial delay to stagger connections
        sleep $(awk "BEGIN {print rand()*0.5}")

        # Optional random HELP command
        if [ $((RANDOM % 3)) -eq 0 ]; then
            echo "help"
            sleep 0.2
        fi

        # 2. Send 50 tasks (activation + deactivation pairs)
        for (( task=1; task<=TASKS_PER_CLIENT; task++ )); do
            # Chaotic Activation Phase (2 random tasks)
            TASK_A=$(( (RANDOM % 4) + 1 ))
            TASK_B=$(( (RANDOM % 4) + 1 ))
            
            echo "a $TASK_A"
            echo "a $TASK_B"
            
            sleep $(awk "BEGIN {print rand()*0.1 + 0.05}")
            
            # Chaotic Deactivation Phase
            echo "b $TASK_A"
            echo "b $TASK_B"
            
            sleep $(awk "BEGIN {print rand()*0.05 + 0.01}")
        done
        
        sleep 0.01
        echo "quit"
        sleep 0.1
    ) | ./build/client $HOST $PORT $VERBOSE
}

send_stop_session() {
    CLIENT_ID=$1

    (
        # Send 50 tasks prior to stopping
        for (( task=1; task<=TASKS_PER_CLIENT; task++ )); do
            TASK_A=$(( (RANDOM % 4) + 1 ))
            TASK_B=$(( (RANDOM % 4) + 1 ))
            
            echo "a $TASK_A"
            sleep 0.01
            echo "a $TASK_B"
            
            sleep 0.02
            
            echo "b $TASK_A"
            sleep 0.01
            echo "b $TASK_B"
            sleep 0.01
        done

        # Issues the global 'stop' command
        echo "stop"
        sleep 1.0 # Holds stdin open to ensure full transmission across socket
    ) | ./build/client $HOST $PORT $VERBOSE
}

echo "=== Starting Chaotic Test ($NUM_WORKERS Workers + 1 Stop Client) ==="

# 1. Run chaotic client sessions in parallel background tasks
for id in $(seq 1 $NUM_WORKERS); do
    run_chaotic_worker $id &
done

# 2. Wait for background workers to finish their task loops cleanly
wait

echo "=== All Worker Clients Completed. Spawning Stop Client (STOP)... ==="

# 3. Last client runs its 50 tasks and issues the global STOP command
send_stop_session $((NUM_WORKERS + 1))

# 4. Final synchronization hold
wait

echo "All client sessions finished."