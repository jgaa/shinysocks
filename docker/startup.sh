#!/bin/sh

ulimit -c unlimited
logbt --test

logbt -- shinysocks "$@"
