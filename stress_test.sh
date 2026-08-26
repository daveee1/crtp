#!/bin/bash

HOST="localhost"
PORT=8080
VERBOSE=8
HOLD_TIME=4

run_client_session() {
    CLIENT_ID=$1
    TASK_A=$2
    TASK_B=$3

    (
        # Activate two tasks
        echo "a $TASK_A"
        sleep 0.5
        echo "a $TASK_B"
        
        sleep $HOLD_TIME
        
        # Deactivate ONLY the exact tasks this client activated
        echo "b $TASK_A"
        sleep 0.5
        echo "b $TASK_B"
        sleep 0.5
        echo "quit"
        sleep 0.2
    ) | ./build/client $HOST $PORT $VERBOSE
}

# 6 clients operating on disjoint/overlapping pairs
run_client_session 1 1 2 &
run_client_session 2 2 3 &
run_client_session 3 3 4 &
run_client_session 4 4 1 &
run_client_session 5 1 3 &
run_client_session 6 2 4 &

wait