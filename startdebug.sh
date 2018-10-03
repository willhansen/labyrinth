#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail
#set -o xtrace

trap "kill 0" EXIT

mkfifo temppipe
uxterm -e "tty > temppipe; sleep 100000"&
VISUAL_TERM="$(cat temppipe)"
rm temppipe

EXE="build/labyrinth"
uxterm -e "gdb -tui -tty=${VISUAL_TERM} ${EXE}"&

wait

