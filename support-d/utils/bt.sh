gdb /usr/local/freeswitch/bin/freeswitch $1 \
        --eval-command='set pagination off' \
        --eval-command='thread apply all bt' \
        --eval-command='quit'
