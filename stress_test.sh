#!/bin/bash

HOST="localhost"
PORT=8080
VERBOSE=3
HOLD_TIME=0.01
WAIT_TIME=0.3

# Set the total number of clients here
NUM_CLIENTS=100

run_client_session() {
    CLIENT_ID=$1
    TASK_A=$2
    TASK_B=$3
    IS_LAST=$4

    (
        # Activate two tasks
        echo "a $TASK_A"
        sleep $HOLD_TIME
        echo "a $TASK_B"
        
        sleep $WAIT_TIME
        
        # Deactivate tasks
        echo "b $TASK_A"
        sleep $HOLD_TIME
        echo "b $TASK_B"
        sleep $HOLD_TIME

        # The last client sends the global 'stop' command
        if [ "$IS_LAST" -eq 1 ]; then
            echo "stop"
            sleep 0.2
        fi

        # echo "quit"
        # sleep 0.2
    ) | ./build/client $HOST $PORT $VERBOSE
}

# Run client sessions dynamically in the background
for ((i=1; i<=NUM_CLIENTS; i++)); do
    # Generate dynamic TASK_A and TASK_B (cycling through values 1-4)
    TASK_A=$(( (i % 4) + 1 ))
    TASK_B=$(( ((i + 1) % 4) + 1 ))

    # Set IS_LAST to 1 for the final client, 0 for all others
    IS_LAST=0
    if [ "$i" -eq "$NUM_CLIENTS" ]; then
        IS_LAST=1
    fi

    run_client_session $i $TASK_A $TASK_B $IS_LAST &
done

# Wait for all background client processes to complete
wait

echo "All $NUM_CLIENTS client sessions finished."