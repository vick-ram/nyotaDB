#!/bin/bash

# NyotaDB Run Script
# This script builds and runs the NyotaDB database system

set -e  # Exit on any error

echo "Cleaning previous build..."
make clean

echo "Building NyotaDB..."
make

echo "Build completed successfully."

# Check if argument is provided
if [ "$1" == "--web" ]; then
    echo "Starting NyotaDB web server..."
    ./nyotadb --web &
    SERVER_PID=$!

    # wait a moment for the server to start
    sleep 2

    # open webapp
    echo "Opening webapp in browser......"
    xdg-open http://localhost:8081/webapp/index.html

    #wait for the server process
    wait $SERVER_PID

else
    echo "Starting NyotaDB REPL..."
    ./nyotadb
fi