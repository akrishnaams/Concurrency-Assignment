#!/bin/bash

# Define constants
NUM_TOTAL_REQUESTS=10000000
SERVER_EXEC="./server"
CLIENT_EXEC="./client"
TIMEOUT=60  # Time in seconds (2 minutes)
OUTPUT_DIR="outputs"
HASH_TABLE_SIZE=1000
NUM_THREADS=1
RESULT_FILE="results.txt"

# Create output directory if it doesn't exist
rm -rf $OUTPUT_DIR
mkdir -p $OUTPUT_DIR
touch $RESULT_FILE

# Iterate over thread counts
for HASH_TABLE_SIZE in 10 30 100 300 1000 3000 10000 30000 100000; do

    NUM_REQUESTS=$NUM_TOTAL_REQUESTS
    
    echo "Compiling with NUM_THREADS=$NUM_THREADS and NUM_REQUESTS=$NUM_REQUESTS"

    sed -i "s/#define NUM_REQUESTS.*/#define NUM_REQUESTS $NUM_REQUESTS/" server.cpp
    sed -i "s/#define NUM_REQUESTS.*/#define NUM_REQUESTS $NUM_REQUESTS/" client.cpp
    sed -i "s/#define NUM_PROCESSING_THREADS.*/#define NUM_PROCESSING_THREADS $NUM_THREADS/" server.cpp
    sed -i "s/#define NUM_CLIENT_THREADS.*/#define NUM_CLIENT_THREADS $NUM_THREADS/" server.cpp

    # Compile the server and client with the required flags
    g++ -std=c++17 -pthread server.cpp -o server -lrt
    if [ $? -ne 0 ]; then
        echo "Compilation of server.cpp failed!"
        exit 1
    fi

    g++ -std=c++17 -pthread client.cpp -o client -lrt
    if [ $? -ne 0 ]; then
        echo "Compilation of client.cpp failed!"
        exit 1
    fi

    # Run server and client with timeout
    echo "Running server and client with NUM_THREADS=$NUM_THREADS HASH_TABLE_SIZE=$HASH_TABLE_SIZE"
    timeout $TIMEOUT $SERVER_EXEC $HASH_TABLE_SIZE > $OUTPUT_DIR/output_server.txt 2> $OUTPUT_DIR/server_stderr.txt &
    SERVER_PID=$!

    sleep 5

    timeout $((TIMEOUT - 10)) $CLIENT_EXEC > $OUTPUT_DIR/output_client_${HASH_TABLE_SIZE}.txt 2> $OUTPUT_DIR/client_stderr.txt &
    CLIENT_PID=$!

    # Wait for both processes to complete or timeout
    wait $SERVER_PID $CLIENT_PID
    
    echo "Execution with NUM_THREADS=$NUM_THREADS completed. Outputs saved in $OUTPUT_DIR."

done

python3 plot_values.py >> $RESULT_FILE
echo "Plotting and Analysis Completed"
rm -rf $SERVER_EXEC $CLIENT_EXEC

echo "All iterations completed"