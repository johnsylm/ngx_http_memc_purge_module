#!/usr/bin/expect  
spawn telnet 127.0.0.1 11211
expect "Escape character is '^]'."
send "flush_all\r"
expect "OK"
send "quit\r"
