/*
 * Adapted from a course at Boston University for use in CPSC 317 at
 * UBC
 * 
 * Version 1.0
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "stp.h"


/*
 * Convert a DNS name or numeric IP address into an integer value
 * (in network byte order).  This is more general-purpose than
 * inet_addr() which maps dotted pair notation to uint. 
 */
unsigned int hostname_to_ipaddr(const char *s)
{
  if (isdigit(*s))
    return (unsigned int)inet_addr(s);
  else {
    struct hostent *hp = gethostbyname(s);
    if (hp == 0) {
      /* Error */
      return (0);
    } 
    return *((unsigned int **)hp->h_addr_list)[0];
  }
}

/*
 * Print an STP packet to standard output. dir is either 's'ent or
 * 'r'eceived packet
 */
void dump(char dir, void *pkt, int len)
{
  stp_header *stpHeader = (stp_header *) pkt;
  unsigned short type = ntohs(stpHeader->type);
  unsigned short seqno = ntohs(stpHeader->seqno);
  unsigned short win = ntohs(stpHeader->window);
  
  printf("%c %s seq %u win %u len %d\n", dir,
         (type == STP_DATA) ? "dat" : 
         (type == STP_ACK) ? "ack" : 
         (type == STP_SYN) ? "syn" : 
         (type == STP_FIN) ? "fin" : 
         (type == STP_RESET) ? "reset" : "???",
         seqno, win, len);
  
  fflush(stdout);
}

/*
 * Helper function to calculate the sum of the bytes in a packet.
 */
unsigned char checksum(stp_header *stpHeader, int len) {
  
  unsigned char sum = 0;
  int i;
  sum += (stpHeader->type   & 0xff) + (stpHeader->type   >> 8);
  sum += (stpHeader->window & 0xff) + (stpHeader->window >> 8);
  sum += (stpHeader->seqno  & 0xff) + (stpHeader->seqno  >> 8);
  
  for (i = 0; i < len; i++)
    sum += stpHeader->data_octets[i];
  
  return sum;
}


/*
 * Helper function to send an stp packet over the network.
 * As a side effect print the packet header to standard output.
 */
void sendpkt(int fd, int type, unsigned short window,
             unsigned short seqno, char* data, int len)
{
  sendpkt2(fd, type, window, seqno, data, len, 0);
}

/*
 * Helper function to send an STP packet over the network.
 * As a side effect print the packet header to standard output.
 * Has an option to corrupt the packet.
 */
void sendpkt2(int fd, int type, unsigned short window,
              unsigned short seqno, char* data, int len, int corrupted)
{
  unsigned char wrk[STP_MTU];
  stp_header *stpHeader = (stp_header *)wrk;
  stpHeader->type = htons(type);
  stpHeader->window = htons(window);
  stpHeader->seqno = htons(seqno);
  if (data != 0) {
    memcpy((char*)(stpHeader + 1), data, len);
  }
  stpHeader->checksum = checksum(stpHeader, len);
  
  if (corrupted) {
    int random_byte = lrand48() % (sizeof(stp_header) + len);
    int random_bit = lrand48() % 8;
    //printf("SENT PACKET CORRUPTED: byte %d from %02x",
    //random_byte, wrk[random_byte]);
    printf("SENT PACKET CORRUPTED\n");
    wrk[random_byte] ^= (char) (1 << random_bit);
    // printf(" to %02x\n", wrk[random_byte]);
  }
  
  dump('s', wrk, len + sizeof(stp_header));
  if (send(fd, wrk, len + sizeof(stp_header), 0) < 0) {
    perror("write");
    exit(1);
  }
}


/*
 * Helper function to read a STP packet from the network.
 * As a side effect print the packet header to standard output.
 */
int readpkt(int fd, void *pkt, int len)
{
  int cc = recv(fd, pkt, len, 0);
  if (cc > 0) {
    dump('r', pkt, cc);
  }
  return (cc);
}

/*
 * Reset the network connection by sending an RESET packet, print an error
 * message to standard output, and exit.
 */
void reset(int fd)
{
  fprintf(stderr, "protocol error encountered... resetting connection\n");
  sendpkt(fd, STP_RESET, 0, 0, 0, 0);
  exit(0);
}


/*
 * Read a packet from the network but if "ms" milliseconds transpire
 * before a packet arrives, abort the read attempt and return
 * STP_TIMED_OUT. Otherwise, return the length of the packet read.
 */
int readWithTimer(int fd, char *pkt, int ms)
{
  int s;
  fd_set fds;
  struct timeval tv;
  
  tv.tv_sec = ms / 1000;
  tv.tv_usec = (ms - tv.tv_sec * 1000) * 1000;
  
  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  s = select(fd + 1, &fds, 0, 0, &tv);
  if (FD_ISSET(fd, &fds))
    return readpkt(fd, pkt, STP_MTU);
  else
    return STP_TIMED_OUT;
}

/*
 * Set an I/O channel (file descriptor) to non-blocking mode.
 */
void nonblock(int fd)
{       
  int flags = fcntl(fd, F_GETFL, 0);
#if defined(hpux) || defined(__hpux)
  flags |= O_NONBLOCK;
#else
  flags |= O_NONBLOCK|O_NDELAY;
#endif
  if (fcntl(fd, F_SETFL, flags) == -1) {
    perror("fcntl: F_SETFL");
    exit(1);
  }
}

