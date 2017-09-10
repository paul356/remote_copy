# remote_copy
quickly copy file between linux hosts using udp
The file to be copied is split into units.
*sendfile.c* and *recvfile.c* each take charge of a range of units in the file. sendfile will send these units to recvfile. sendfile run on the source host. recvfile run on the targe host. check send.sh and recv.sh for example.
