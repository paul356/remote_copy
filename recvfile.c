#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>
#include <errno.h>
#include <arpa/inet.h>

#include "sendrecv.h"

int main(int argc, char *argv[])
{
    int udpSock;
    struct sockaddr_in myAddr, peerAddr;
    char *ip;
    char *fileName;
    char *buf;
    char *bmp;

    FILE *fp;
    int offset;
    int unitSize;
    int bufLen;
    int unitNum;
    int recvNum;
    int connIdx;

    struct packet_header* header;
    socklen_t sockLen;
    int recvLen;
    int ret;

    if (argc != 6) {
        printf("recvfile <peer-ip> <unit-size> <offset> <unit-num> <filename>\n");
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
        printf("not enough memory, malloc(%d) fail\n", bufLen);
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

    fp = fopen(fileName, "r+");
    if (!fp) {
        printf("can't open %s\n", fileName);
        free(buf);
        free(bmp);
        close(udpSock);
        return -1;
    }

    recvNum = 0;
    while (recvNum < unitNum) 
    {
        sockLen = sizeof(struct sockaddr);
        recvLen = recvfrom(udpSock, buf, bufLen, 0, (struct sockaddr *)(&peerAddr), &sockLen);
        if (recvLen <= 0)
        {
            printf("fail to recv data, errno=%d", errno);
            continue;
        }
        
        if (recvLen < sizeof(struct packet_header))
        {
            printf("packet too small, size=%d", recvLen);
            continue;
        }

        header = (struct packet_header*)(&buf[0]);
        if (header->magic != MAGIC_NUM)
        {
            printf("invalid packet, magic=%x\n", header->magic);
            continue;
        }

        if (header->type != DATA_PACKET || bufLen != unitSize + sizeof(*header))
        {
            printf("wrong packet type, type=%d\n", header->type);
            continue;
        }

        if (header->unitId < offset || header->unitId > offset + unitNum)
        {
            printf("unitId(%d) out of range\n", header->unitId);
            continue;
        }

        if (!bmp[header->unitId - offset])
        {
            fseek(fp, header->unitId * unitSize, SEEK_SET);
            fwrite(&buf[sizeof(struct packet_header)], 1, unitSize, fp);

            bmp[header->unitId - offset] = 1;
            recvNum ++;
        }

        header->type = ACK_DATA;
        ret = sendto(udpSock, buf, sizeof(struct packet_header), 0, (struct sockaddr*)&peerAddr, sizeof(peerAddr)); 
        if (ret < 0)
        {
            printf("ack packet unitId(%d) failed\n", header->unitId);
        }
    }

    free(buf);
    free(bmp);
    close(udpSock);
    fclose(fp);

    return 0;
}
