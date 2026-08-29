#!/bin/bash

HOST="localhost"
PORT=8080
VERBOSE=2 # Kept low to avoid terminal bottleneck
NUM_CLIENTS=100

run_chaotic_client() {
    CLIENT_ID=$1

    (
        # 1. Random initial delay (0.0 to 0.5s) to stagger server connections
        sleep $(awk "BEGIN {print rand()*0.5}")

        # Send HELP randomly
        if [ $((RANDOM % 3)) -eq 0 ]; then
            echo "help"
            sleep 0.1
        fi

        # 2. Heavy Activation Phase (Random tasks 1 to 4)
        for i in {1..4}; do
            TASK=$(( (RANDOM % 4) + 1 ))
            echo "a $TASK"
            sleep $(awk "BEGIN {print rand()*0.2}")
        done

        # 3. Cross-Deactivation Phase (Deactivate random tasks)
        for i in {1..3}; do
            TARGET_TASK=$(( (RANDOM % 4) + 1 ))
            echo "b $TARGET_TASK"
            sleep $(awk "BEGIN {print rand()*0.1}")
        done

        # 4. Occasional Global STOP trigger (1 in 20 chance per client)
        if [ $((RANDOM % 100)) -eq 0 ]; then
            echo "stop"
        fi

        # 5. Exit
        echo "quit"
        sleep 0.1
    ) | ./build/client $HOST $PORT $VERBOSE > /dev/null 2>&1
}

echo "=== Starting Stress Test with $NUM_CLIENTS Clients ==="

# Spawn 100 concurrent clients in the background
for id in $(seq 1 $NUM_CLIENTS); do
    run_chaotic_client $id &
done

# Wait for all background client processes to finish
wait

echo "=== Stress Test Complete ==="