#!/bin/bash

COUNTER_FILE=".copilot/handoff/.fix-count"
MAX_FIXES=3

case "$1" in
  increment)
    if [ -f "$COUNTER_FILE" ]; then
      count=$(cat "$COUNTER_FILE")
      echo $((count + 1)) > "$COUNTER_FILE"
    else
      echo 1 > "$COUNTER_FILE"
    fi
    echo "Fix count: $(cat "$COUNTER_FILE")/$MAX_FIXES"
    ;;
  check)
    if [ -f "$COUNTER_FILE" ]; then
      count=$(cat "$COUNTER_FILE")
      if [ "$count" -ge "$MAX_FIXES" ]; then
        echo "ERROR: Fix limit reached ($count/$MAX_FIXES). Rolling back."
        echo "Please review the handoff files and try a different approach."
        exit 1
      fi
    fi
    echo "Fix count OK: $(cat "$COUNTER_FILE" 2>/dev/null || echo 0)/$MAX_FIXES"
    ;;
  reset)
    echo 0 > "$COUNTER_FILE"
    echo "Fix counter reset."
    ;;
  *)
    echo "Usage: fix-counter.sh [increment|check|reset]"
    ;;
esac
