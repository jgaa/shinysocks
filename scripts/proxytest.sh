#!/bin/bash

# SOCKS Proxy configuration
PROXY_HOST="localhost"
PROXY_PORT="1080"

# Test URL
TEST_URL="http://httpbin.org/ip"

# Function to test SOCKS4 proxy
test_socks4() {
    echo "Testing SOCKS4 proxy..."
    curl -x socks4a://$PROXY_HOST:$PROXY_PORT --verbose $TEST_URL
    if [ $? -eq 0 ]; then
        echo "SOCKS4 Test Successful!"
    else
        echo "SOCKS4 Test Failed!"
        exit 1  # Exit with error if test fails
    fi
    echo "--------------------------------------"
}

# Function to test SOCKS5 proxy
test_socks5() {
    echo "Testing SOCKS5 proxy..."
    curl -x socks5h://$PROXY_HOST:$PROXY_PORT --verbose $TEST_URL
    if [ $? -eq 0 ]; then
        echo "SOCKS5 Test Successful!"
    else
        echo "SOCKS5 Test Failed!"
        exit 1  # Exit with error if test fails
    fi
    echo "--------------------------------------"
}

# Main execution
echo "Starting SOCKS Proxy Test..."
test_socks4
test_socks5
echo "Testing Completed Successfully!"
exit 0  # Exit successfully if all tests pass

