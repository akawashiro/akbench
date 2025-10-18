#!/bin/bash

# Test script for JSON output functionality
set -e

AKBENCH="$1"

if [ -z "$AKBENCH" ]; then
    echo "Usage: $0 <path-to-akbench>"
    exit 1
fi

# Test 1: latency_atomic with JSON output
echo "Test 1: Testing latency_atomic with JSON output"
OUTPUT=$("$AKBENCH" latency_atomic -i 3 -w 1 --json 2>&1)
echo "$OUTPUT" | python3 -m json.tool > /dev/null
echo "$OUTPUT" | python3 -c "import sys, json; d=json.load(sys.stdin); assert 'latency_atomic' in d; assert 'average' in d['latency_atomic']; assert 'stddev' in d['latency_atomic']; assert 'unit' in d['latency_atomic']; assert d['latency_atomic']['unit'] == 'ns'"
echo "✓ Test 1 passed"

# Test 2: bandwidth_memcpy with JSON output
echo "Test 2: Testing bandwidth_memcpy with JSON output"
OUTPUT=$("$AKBENCH" bandwidth_memcpy -i 3 -w 1 -d 1048576 --json 2>&1)
echo "$OUTPUT" | python3 -m json.tool > /dev/null
echo "$OUTPUT" | python3 -c "import sys, json; d=json.load(sys.stdin); assert 'bandwidth_memcpy' in d; assert 'average' in d['bandwidth_memcpy']; assert 'stddev' in d['bandwidth_memcpy']; assert 'unit' in d['bandwidth_memcpy']; assert d['bandwidth_memcpy']['unit'] == 'GiByte/sec'"
echo "✓ Test 2 passed"

# Test 3: latency_all with JSON output
echo "Test 3: Testing latency_all with JSON output"
OUTPUT=$("$AKBENCH" latency_all -i 3 -w 1 --json 2>&1)
echo "$OUTPUT" | python3 -m json.tool > /dev/null
echo "$OUTPUT" | python3 -c "import sys, json; d=json.load(sys.stdin); assert 'latency_atomic' in d; assert 'latency_barrier' in d; assert 'latency_semaphore' in d"
echo "✓ Test 3 passed"

# Test 4: bandwidth_all with JSON output
echo "Test 4: Testing bandwidth_all with JSON output"
OUTPUT=$("$AKBENCH" bandwidth_all -i 3 -w 1 -d 1048576 --json 2>&1)
echo "$OUTPUT" | python3 -m json.tool > /dev/null
echo "$OUTPUT" | python3 -c "import sys, json; d=json.load(sys.stdin); assert 'bandwidth_memcpy' in d; assert 'bandwidth_tcp' in d; assert 'bandwidth_uds' in d"
echo "✓ Test 4 passed"

# Test 5: all with JSON output (combined test)
echo "Test 5: Testing all with JSON output"
OUTPUT=$("$AKBENCH" all -i 3 -w 1 -d 1048576 --json 2>&1)
echo "$OUTPUT" | python3 -m json.tool > /dev/null
echo "$OUTPUT" | python3 -c "import sys, json; d=json.load(sys.stdin); assert 'latency_tests' in d; assert 'bandwidth_tests' in d; assert 'latency_atomic' in d['latency_tests']; assert 'bandwidth_memcpy' in d['bandwidth_tests']"
echo "✓ Test 5 passed"

# Test 6: bandwidth_memcpy_mt with threads parameter and JSON
echo "Test 6: Testing bandwidth_memcpy_mt with threads and JSON output"
OUTPUT=$("$AKBENCH" bandwidth_memcpy_mt -n 2 -i 3 -w 1 -d 1048576 --json 2>&1)
echo "$OUTPUT" | python3 -m json.tool > /dev/null
echo "$OUTPUT" | python3 -c "import sys, json; d=json.load(sys.stdin); assert 'bandwidth_memcpy_mt' in d; assert 'threads' in d['bandwidth_memcpy_mt']; assert d['bandwidth_memcpy_mt']['threads'] == 2"
echo "✓ Test 6 passed"

echo ""
echo "All JSON output tests passed! ✓"
