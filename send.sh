#!/bin/bash
./sendfile 192.168.3.161 $((8*1024)) 0 500 ~/code/cljstest.zip &
./sendfile 192.168.3.161 $((8*1024)) 500 500 ~/code/cljstest.zip &
./sendfile 192.168.3.161 $((8*1024)) 1000 500 ~/code/cljstest.zip &
./sendfile 192.168.3.161 $((8*1024)) 1500 500 ~/code/cljstest.zip &
./sendfile 192.168.3.161 $((8*1024)) 2500 500 ~/code/cljstest.zip &
./sendfile 192.168.3.161 $((8*1024)) 3000 500 ~/code/cljstest.zip &
./sendfile 192.168.3.161 $((8*1024)) 3500 500 ~/code/cljstest.zip &
./sendfile 192.168.3.161 $((8*1024)) 4000 500 ~/code/cljstest.zip &
./sendfile 192.168.3.161 $((8*1024)) 4500 500 ~/code/cljstest.zip &
./sendfile 192.168.3.161 $((8*1024)) 5000 500 ~/code/cljstest.zip &
./sendfile 192.168.3.161 $((8*1024)) 5500 500 ~/code/cljstest.zip &
