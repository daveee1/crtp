#!/bin/bash

HOST="localhost"
PORT=8080
VERBOSE=8
HOLD_TIME=2
WAIT_TIME=3

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
        sleep 5

        # The 10th client sends the global 'stop' command
        if [ "$IS_LAST" -eq 1 ]; then
            echo "stop"
            sleep 0.2
        fi

        # echo "quit"
        sleep 0.2
    ) | ./build/client $HOST $PORT $VERBOSE
}

# Run 10 client sessions in the background
run_client_session 1  1 2 0 &
run_client_session 2  2 3 0 &
# run_client_session 3  3 4 0 &
# run_client_session 4  4 1 0 &
# run_client_session 5  1 3 0 &
# run_client_session 6  2 4 0 &
# run_client_session 7  4 2 0 &
# run_client_session 8  1 4 0 &
# run_client_session 9  3 2 0 &

# 10th client (IS_LAST = 1 -> issues 'stop')
run_client_session 10 4 3 1 &

# Wait for all background client processes to complete
wait

echo "All 10 client sessions finished."