#!/bin/sh

LOGFILE="chat-$(date +%s).log"
echo "Chat log file. File name: $LOGFILE\n" > $LOGFILE
tmux new-session  -d  "tail -f $LOGFILE"
tmux split-window -p 20
tmux send-keys "./client <IP> <PORT> $LOGFILE" 
tmux -2 attach-session -d
