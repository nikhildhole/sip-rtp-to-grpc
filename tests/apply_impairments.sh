#!/bin/bash

INTERFACE="eth0" # Docker container interface
COMMAND=$1
VALUE=$2

set_impairment() {
    local TYPE=$1
    local VAL=$2
    echo "Applying $TYPE impairment: $VAL"
    # Clear existing rules
    docker exec -u 0 gateway_sut tc qdisc del dev $INTERFACE root 2>/dev/null
    
    case $TYPE in
        jitter)
            # Example: tc qdisc add dev eth0 root netem delay 100ms 10ms
            docker exec -u 0 gateway_sut tc qdisc add dev $INTERFACE root netem delay ${VAL}ms 5ms
            ;;
        loss)
            docker exec -u 0 gateway_sut tc qdisc add dev $INTERFACE root netem loss ${VAL}%
            ;;
        duplicate)
            docker exec -u 0 gateway_sut tc qdisc add dev $INTERFACE root netem duplicate ${VAL}%
            ;;
        reorder)
            docker exec -u 0 gateway_sut tc qdisc add dev $INTERFACE root netem delay 10ms reorder ${VAL}% 50%
            ;;
        clear)
            echo "Impairments cleared."
            ;;
        *)
            echo "Usage: $0 {jitter|loss|duplicate|reorder|clear} [value]"
            exit 1
            ;;
    esac
}

set_impairment $COMMAND $VALUE
