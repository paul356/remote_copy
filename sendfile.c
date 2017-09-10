#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <strings.h>
#include <errno.h>
#include <arpa/inet.h>
#include <time.h>

#include "sendrecv.h"

enum sender_state {
    INIT_STATE,
    SEND_SPECIFIED_UNIT,
    SEND_SEQUENTIAL_UNIT,
    WAIT_MSG,
};

enum sender_state progState = SEND_SEQUENTIAL_UNIT;
enum sender_state lastState = INIT_STATE;

void setProgState(enum sender_state state)
{
    lastState = progState;
    progState = state;

    printf("state %d -> %d\n", lastState, state);
}

void revertLastState()
{
    progState = lastState;
}

int main(int argc, char *argv[])
{
    struct sockaddr_in myAddr, peerAddr;
    int  udpSock; 
    socklen_t sockLen;
    FILE *fp;
    char *buf;
    char *bmp;
    int  bufLen;
    int  recvLen;

    char *ip;
    char *fileName;
    int  unitSize;
    int  offset;
    int  unitNum;
    int  connIdx;
    int  ackNum;

    int  currUnit;
    int  specifiedUnit;

    struct packet_header *header;
    int  ret;
    struct timeval timeout;      
    struct timespec sendInterval;      

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    sendInterval.tv_sec = 0;
    sendInterval.tv_nsec = 1000*1000;

    if (argc != 6) {
        printf("sendfile <peer-ip> <unit-size> <offset> <unit-num> <filename>\n");
        return -1;
    }

    ip       = argv[1];
    unitSize = atoi(argv[2]);
    offset   = atoi(argv[3]);
    unitNum  = atoi(argv[4]);
    fileName = argv[5];
    connIdx  = offset / unitNum;

    if (unitSize <= 0 || offset < 0 || unitNum <= 0)
    {
        printf("check the input parameters\n");
        return -1;
    }

    bufLen = unitSize + sizeof(struct packet_header);
    buf = malloc(bufLen);
    if (!buf)
    {
        printf("not enough memory, malloc(%lu) fail\n", unitSize + sizeof(struct packet_header));
        return -1;
    }

    bmp = malloc(unitNum);
    if (!bmp)
    {
        free(buf);
        printf("not enough memory, malloc(%d) fail\n", unitNum);
        return -1;
    }
    bzero(bmp, unitNum);

    udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    bzero(&myAddr, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    myAddr.sin_port   = htons(PORT_BASE + connIdx);
    myAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    bzero(&peerAddr, sizeof(peerAddr));
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port   = htons(PORT_BASE + connIdx);
    if (inet_aton(ip, &peerAddr.sin_addr) <= 0) {
        printf("ip %s is not right\n", ip);
        close(udpSock);
        return -1;
    }

    ret = bind(udpSock, (struct sockaddr*)&myAddr, sizeof(myAddr));
    if (ret < 0)
    {
        free(buf);
        free(bmp);
        close(udpSock);
        printf("can't bind sock to %d\n", myAddr.sin_port);
        return -1;
    }

    if (setsockopt (udpSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        free(buf);
        free(bmp);
        close(udpSock);
        printf("setsockopt failed\n");
        return -1;
    }

    fp = fopen(fileName, "rb");
    if (!fp) {
        free(buf);
        free(bmp);
        close(udpSock);
        printf("can't open %s\n", fileName);
        return -1;
    }

    currUnit = offset;

    ackNum = 0;
    while (ackNum < unitNum) 
    {
        switch (progState)
        {
            case WAIT_MSG:
                sockLen = sizeof(struct sockaddr_in);
                recvLen = recvfrom(udpSock, buf, bufLen, 0, (struct sockaddr*)&peerAddr, &sockLen);
                
                if (recvLen < 0 && errno == EAGAIN)
                {
                    printf("wait for msg timeout\n");
                    setProgState(SEND_SEQUENTIAL_UNIT);
                    break;
                }
                else if (recvLen <= 0)
                {
                    printf("recv with error %d\n", recvLen);
                    break;
                }

                if (recvLen < sizeof(struct packet_header))
                {
                    printf("recv with error %d\n", recvLen);
                    break;
                }

                header = (struct packet_header*)(&buf[0]);    
                if (header->magic != MAGIC_NUM)
                {
                    printf("invalid packet, magic=%x\n", header->magic);
                    break;
                }

                if (header->type == ACK_DATA)
                {
                    if (header->unitId < offset || header->unitId > offset + unitNum)
                    {
                        printf("ack unit(%d) out of range\n", header->unitId);
                    }
                    else
                    {
                        bmp[header->unitId - offset] = 1;
                        ackNum ++;
                    }
                }
                else if (header->type == DATA_START)
                {
                    setProgState(SEND_SEQUENTIAL_UNIT);
                }
                else if (header->type == REQUEST_DATA)
                {
                    if (header->unitId < offset || header->unitId > offset + unitNum)
                    {
                        printf("request unit(%d) out of range\n", header->unitId);
                    }
                    else
                    {
                        specifiedUnit = header->unitId;
                        setProgState(SEND_SPECIFIED_UNIT);
                    }
                }

                break;
            case SEND_SEQUENTIAL_UNIT:
                if (!bmp[currUnit - offset])
                {
                    fseek(fp, currUnit * unitSize, SEEK_SET);
                    fread(buf + sizeof(struct packet_header), 1, unitSize, fp);

                    header = (struct packet_header*)buf;
                    header->magic = MAGIC_NUM;
                    header->type = DATA_PACKET;
                    header->unitId = currUnit;

                    ret = sendto(udpSock, buf, bufLen, 0, (struct sockaddr*)&peerAddr, sizeof(peerAddr));
                    if (ret < 0)
                    {
                        printf("send packet unitId(%d) failed, errno=%s\n", currUnit, strerror(errno));
                    }
                    else
                    {
                        nanosleep(&sendInterval, NULL);
                    }
                }

                currUnit ++;
                if (currUnit == offset + unitNum)
                {
                    currUnit = offset;
                    fseek(fp, offset * unitSize, SEEK_SET);

                    setProgState(WAIT_MSG);
                }

                break; 
            case SEND_SPECIFIED_UNIT:
                fseek(fp, specifiedUnit * unitSize, SEEK_SET);
                fread(buf + sizeof(struct packet_header), 1, unitSize, fp);

                header = (struct packet_header*)buf;
                header = (struct packet_header*)buf;
                header->magic = MAGIC_NUM;
                header->type = DATA_PACKET;
                header->unitId = currUnit;

                ret = sendto(udpSock, buf, bufLen, 0, (struct sockaddr*)&peerAddr, sizeof(peerAddr));
                if (ret < 0)
                {
                    printf("send packet unitId(%d) failed\n", specifiedUnit);
                }
                else
                {
                    revertLastState();
                }

                break;
            default:
                break;
        }
    }

    free(buf);
    free(bmp);
    close(udpSock);
    fclose(fp);

    return 0;
}
