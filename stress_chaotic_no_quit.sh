#!/bin/bash

HOST="localhost"
PORT=8080
VERBOSE=4
NUM_CLIENTS=8

run_chaotic_worker() {
    CLIENT_ID=$1
    IS_LAST=$2

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
        
        # 4. Global STOP trigger executed ONLY by the last client
        if [ "$IS_LAST" -eq 1 ]; then
            echo "stop"
            sleep 1.0 # Ensure buffer flushes before closing stdin
        else
            # 5. KEEP ALIVE (No quit command)
            # Sleep indefinitely so the client process and socket remain open 
            # until the server forces the connection closed during 'stop'.
            sleep 40
        fi
    ) | ./build/client $HOST $PORT $VERBOSE > /dev/null 2>&1
}

echo "=== Starting Chaotic Test with $NUM_CLIENTS Clients ==="
echo "Note: Clients will NOT quit. Client $NUM_CLIENTS will issue STOP."

# Spawn all clients in the background
for id in $(seq 1 $NUM_CLIENTS); do
    if [ "$id" -eq "$NUM_CLIENTS" ]; then
        run_chaotic_worker $id 1 &
    else
        run_chaotic_worker $id 0 &
    fi
done

# Wait for all background client processes to finish
# (The long 'sleep 3600' in workers will end prematurely when the server 
# closes the sockets and ./build/client terminates).
wait

echo "=== Stress Test Complete ==="