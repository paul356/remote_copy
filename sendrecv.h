#ifndef __SEND_RECV__
#define __SEND_RECV__

#define PORT_BASE 28080
#define MAGIC_NUM 0x900D
#define SEND_INTERVAL 5

enum packet_type {
    DATA_START,
    DATA_PACKET,
    REQUEST_DATA,    
    ACK_DATA,
};

struct packet_header {
    int magic;
    int type;
    int unitId;
    int pad;
};

#endif
