#!/bin/bash

HOST="localhost"
PORT=8080
VERBOSE=4 # Kept moderate for scannability
HOLD_TIME=1
WAIT_TIME=2
NUM_WORKERS=9

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

        # 2. Chaotic Activation Phase (2 random tasks)
        TASK_A=$(( (RANDOM % 4) + 1 ))
        TASK_B=$(( (RANDOM % 4) + 1 ))
        
        echo "a $TASK_A"
        sleep $(awk "BEGIN {print rand()*0.3 + 0.1}")
        echo "a $TASK_B"
        
        sleep $(awk "BEGIN {print rand()*0.5 + 1.0}")
        
        # 3. Chaotic Deactivation Phase
        echo "b $TASK_A"
        sleep $(awk "BEGIN {print rand()*0.3 + 0.1}")
        echo "b $TASK_B"
        
        sleep 0.5
        echo "quit"
        sleep 0.2
    ) | ./build/client $HOST $PORT $VERBOSE
}

send_stop_session() {
    CLIENT_ID=$1

    (
        # 1. Chaotic activations prior to stopping
        TASK_A=$(( (RANDOM % 4) + 1 ))
        TASK_B=$(( (RANDOM % 4) + 1 ))
        
        echo "a $TASK_A"
        sleep $HOLD_TIME
        echo "a $TASK_B"
        
        sleep $WAIT_TIME
        
        echo "b $TASK_A"
        sleep $HOLD_TIME
        echo "b $TASK_B"
        sleep 1

        # 2. Issues the global 'stop' command
        echo "stop"
        sleep 1.0 # Holds stdin open to ensure full transmission across socket
    ) | ./build/client $HOST $PORT $VERBOSE
}

echo "=== Starting Chaotic Test ($NUM_WORKERS Workers + 1 Stop Client) ==="

# 1. Run 9 chaotic client sessions in parallel background tasks
for id in $(seq 1 $NUM_WORKERS); do
    run_chaotic_worker $id &
done

# 2. Wait for background workers to finish their task loops cleanly
wait

echo "=== All Worker Clients Completed. Spawning 10th Client (STOP)... ==="

# 3. Client 10 runs its chaotic session and issues the global STOP command
send_stop_session 10

# 4. Final synchronization hold
wait

echo "All 10 client sessions finished."