/*
 *  This code is borrowed from a course at Boston University and 
 *  has been adapted for use in CPSC 317 at UBC
 *
 */


#ifndef __STP_H_

#define __STP_H_

#define STP_MAXWIN    65535 
#define STP_MTU       300 /* MTU size */
#define STP_MSS       (STP_MTU - sizeof(stp_header)) /* MSS Size */
#define STP_TIMED_OUT (-3)

/* In the above if MSS and MTU don't mean anything to you then read the text */


/*
 * Packet types
 */
#define STP_DATA  0x01
#define STP_ACK   0x02
#define STP_SYN   0x04
#define STP_FIN   0x08
#define STP_RESET 0x10

/*
 * State types
 */
/* shared */
#define STP_ESTABLISHED 0x20
#define STP_CLOSED      0x21
#define STP_LISTEN      0x22
#define STP_TIME_WAIT   0x23

/* This structure is used to manage received sent packets. It is not
 * the packet that is actually sent or received.
 */
typedef struct pktbuf_tag {
  
  struct pktbuf_tag *next;
  
  unsigned short int seqno;
  int len;
  char data[STP_MTU];
  
} pktbuf;


/* This is the actual definition of the packet header on the
 * wire. Note there is an extra field (data_octets) to be used as the
 * area where the data is actually stored.
 */
typedef struct {
  unsigned short int type; 
  unsigned short int window; 
  unsigned short int seqno;
  unsigned char checksum;
  unsigned char data_octets[];
} stp_header;

/* 
 * All of the receiver's state is stored in the following structure,
 * including the received messages, which have to be delivered to
 * ReceiveApp in order.
 */
typedef struct {
  int state;                 /* protocol state: normally ESTABLISHED */
  int fd;                    /* UDP socket descriptor */
  
  unsigned short rwnd;       /* latest advertised window */
  
  unsigned short NBE;        /* next byte expected */
  unsigned short LBRead;     /* last byte read */
  unsigned short LBReceived; /* last byte received */

  unsigned short ISN;        /* initial sequence number */

  pktbuf *recvQueue;         /* Pointer to the first node of the receive queue */

} stp_recv_ctrl_blk;


/* Declarations for STP.C */

void sendpkt(int fd, int type, unsigned short window, unsigned short seqno, char* data, int len);
void sendpkt2(int fd, int type, unsigned short window, unsigned short seqno, char* data, int len, int corrupted);
int readpkt(int fd, void* pkt, int len);
void dump(char dir, void* pkt, int len);
unsigned int hostname_to_ipaddr(const char *s);
int readWithTimer(int fd, char *pkt, int ms);
void reset(int fd);
unsigned char checksum(stp_header *stpHeader, int len);

/* Declarations for RECEIVER_LIST.C */
void add_packet(stp_recv_ctrl_blk *info, unsigned short seqno, int len, char *data);
pktbuf *get_packet(stp_recv_ctrl_blk *info, unsigned short seqno);
void free_packet(pktbuf *pbuf);

/* Declarations for WRAPAROUND.C */
int greater(unsigned short val1, 
	    unsigned short val2);
unsigned short minus(unsigned short greaterVal, 
		     unsigned short lesserVal);
unsigned short plus(unsigned short val1,  unsigned short val2);

#endif
