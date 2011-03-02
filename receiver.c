/* This code is adapted from a course at Boston University for use in
 * CPSC 317 at UBC.
 *
 * Implementation of a STP receiver. This module implements the
 * receiver-side of the protocol and dumps the contents of the
 * connection to a file called "OutputFile" in the current directory.
 *
 * It also simulates network misbehavior by dropping packets, dropping
 * ACKs, corrupting packets and swapping some packets so that they
 * arrive out of order.
 *
 * Version 1.0 
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#include "stp.h"

#if defined(RAND_MAX)
#undef RAND_MAX
#endif 
#define RAND_MAX 2147483647   /* = 2^31-1 */

int ReceiverMaxWin = 5000;        /* Maximum window size */

double PacketLossProbability              = 0.0; /* packet loss probability */
double AckLossProbability                 = 0.0; /* ACK loss probability */
double OutOfOrderPacketArrivalProbability = 0.0; /* probability of an out-of-order arrival */
double CorruptedPacketProbability         = 0.0; /* probability of corruption on arrived packet */
double CorruptedACKProbability            = 0.0; /* probability of corruption on ACK */

/* See the implementation of stp_recv_ctrl_blk in stp.h */

/* Global file descriptor for the output file. */
int outFile = -1;

/*******************************************************************/
/* Since the protocol STP is event driven, we define             */
/* a structure stp_event to describe the event coming            */
/* in, which enables a state transition.  All events              */
/* are in the form of packets from our peer.                      */
/******************************************************************/

typedef struct stp_event_t {
  char *pkt; /* Pointer to the packet from peer */
  int len;   /* The length of the packet */
} stp_event;


//**************************************************************

/*  Return 1 with probability p, and 0 otherwise */
int event_happens(double p) {
  
  long val = lrand48();
  
  if (val < RAND_MAX * p)
    return 1;
  else
    return 0;
}

/*
 * Open a UDP connection.
 */
int udp_open(char *remote_IP_str, int remote_port, int local_port)
{
  int      fd;
  uint32_t dst;
  struct   sockaddr_in sin;
  
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) 
    {
      perror("Error creating UDP socket");
      return -1;
    }
  
  /* Bind the local socket to listen at the local_port. */
  printf ("Binding locally to port %d\n", local_port);
  memset((char *)&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(local_port);
  
  if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) 
    {
      perror("Bind failed");
      return (-2);
    }
  
  /* Connect, i.e. prepare to accept UDP packets from <remote_host, remote_port>.  */
  /* Listen() and accept() are not necessary with UDP connection setup.            */ 
  dst = hostname_to_ipaddr(remote_IP_str);
  
  if (!dst) {
    printf("Invalid sending host name: %s\n", remote_IP_str);
    return -4;
  }
  printf ("Configuring  UDP \"connection\" to <%u.%u.%u.%u, port %d>\n", 
          (ntohl(dst)>>24) & 0xFF, (ntohl(dst)>>16) & 0xFF, 
          (ntohl(dst)>>8) & 0XFF, ntohl(dst) & 0XFF, remote_port);
  
  memset((char *)&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(remote_port);
  sin.sin_addr.s_addr = dst;
  if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) 
      {
      perror("connect");
      return(-1);
      }
  printf ("UDP \"connection\" to <%u.%u.%u.%u port %d> configured\n", 
          (ntohl(dst)>>24) & 0xFF, (ntohl(dst)>>16) & 0xFF, 
          (ntohl(dst)>>8) & 0XFF, ntohl(dst) & 0XFF , remote_port);
  
  return (fd);
}


/*
 * Send an STP ack back to the source.  The stp_CB tells
 * us what frame we expect, so we ack that sequence number.
 */
void stp_send_ack(stp_recv_ctrl_blk *stp_CB)
{
  if (!event_happens(AckLossProbability) || stp_CB->state == STP_LISTEN) {
    /* Won't drop packets when we are sending out the ACK to
       acknowledge the SYN */
    sendpkt2(stp_CB->fd, STP_ACK, stp_CB->rwnd, stp_CB->NBE, 0, 0,
             event_happens(CorruptedACKProbability));
  } else {
    printf("ACK (%u) dropped\n", stp_CB->NBE); 
  }
}


/*
 * Routine that simulates handing off a message to the application.
 * (We'll just write it to the output file directly.)
 * STP ensures that messages are delivered in sequence.
 */
void stp_consume(char *pkt, int len)
{
  //char b[1000];
  write(outFile, pkt, len);
  printf("consume: %d bytes\n", len);
  
  // Debugging code, if needed
  // strncpy(b, pkt, len);
  ///b[len] = '\0';
  /*printf("Contents: <%s>\n", b);*/
}

int stp_receive_state_transition_machine(stp_recv_ctrl_blk *stp_CB, stp_event *pe)
{
  
  unsigned short seqno;
  stp_header *srh = (stp_header *)pe->pkt;
  int type;
  unsigned short int LBA; /* Last byte accepted */
  
  /* If the length is too short for a header, that's an error */
  if (pe->len < sizeof(*srh)) {
    printf("Size too short.\n");
    reset(stp_CB->fd); 
    return -1;
  }
  
  /* Checks if the sum of bytes is correct */
  if (srh->checksum != checksum(srh, pe->len - sizeof(stp_header))) {
    printf("Sum of bytes doesn't match. Ignoring packet.\n");
    // Packet is ignored.
    return 0;
  }
  
  /* Strip out the fields of the header from the packet */
  type = ntohs(srh->type);
  seqno = ntohs(srh->seqno);
  
  switch (stp_CB->state) 
    {
    case STP_LISTEN: 
      if (type != STP_SYN) 
        {
          printf("Not SYN.\n");
          reset(stp_CB->fd); 
          return -1;
        }
      
      stp_CB->ISN = seqno;
      stp_CB->LBRead = seqno;
      stp_CB->LBReceived = seqno;
      stp_CB->NBE = plus(seqno, 1);
      stp_CB->state = STP_ESTABLISHED;
      stp_send_ack(stp_CB);
      return 0;
      
      break; 
      
    case STP_TIME_WAIT: 
      if (type != STP_FIN) 
        {
          reset(stp_CB->fd); 
          return -1;
        }
      /* if the seqno of the FIN is wrong, reset the connection */
      if (seqno != stp_CB->NBE) 
        {
          reset(stp_CB->fd);
          return -1;
        }
      
      /* otherwise, ack the FIN and remain in time wait */
      stp_send_ack(stp_CB);
      return 0;
      
      break; 
      
    case STP_ESTABLISHED: 
      /* otherwise, we're in the established state */
      
      switch (type) 
        {
        case STP_RESET: 
          fprintf (stderr, "Reset received from sender -- closing\n");
          sendpkt2(stp_CB->fd, STP_RESET, 0, 0, 0, 0, event_happens(CorruptedACKProbability));
          return -1;
          break; 
          
        case STP_SYN: 
          if(seqno == stp_CB->ISN)
            {
              /* this is a retransmission of the first SYN, acknowledge */
              sendpkt2(stp_CB->fd, STP_ACK, stp_CB->rwnd, plus(stp_CB->ISN, 1),
                       0, 0, event_happens(CorruptedACKProbability));
              return 0;
            }
          break; 
          
        case STP_FIN: 
          /* This FIN is only valid if its sequence number is NBE.           */
          /* Otherwise the FIN is occuring prior to our receipt of all data. */
          if (seqno != stp_CB->NBE) 
            {
              printf("FIN seq not equal to NBE (%u != %u).\n", seqno, stp_CB->NBE);
              reset(stp_CB->fd);
              return -1;
            }
          stp_CB->state = STP_TIME_WAIT;
          
          /*
           * TRICKY CODE ALERT:
           * Increment NBE in the FIN-ACK,
           * then decrement again in case the FIN-ACK is lost.
           */
          stp_CB->NBE = plus(stp_CB->NBE, 1); 
          stp_send_ack(stp_CB);
          stp_CB->NBE = minus(stp_CB->NBE, 1); 
          
          return 1;  
          /* Indicate that the file transfer is complete. Note that we
           * do not go into the state TIME_WAIT in this implementation
           * of the receiver.
           */
          break; 
          
        case STP_DATA: 
          
          LBA = plus(stp_CB->LBRead, ReceiverMaxWin);
          
          if (greater(stp_CB->NBE, seqno)) 
            {
              /* retransmitted packet that we've already received do
               * nothing except send an ACK (at function bottom)
               */
            }
          else if(greater(seqno, LBA))
            {
              printf("Packet seqno too large to fit in receive window.\n");
              reset(stp_CB->fd);
              return -1;
            } 
          
          /*
           * New data has arrived. If the ACK arrives in order, hand
           * the data directly to the application (consume it) and see
           * if we've filled a gap in the sequence space. Otherwise,
           * stash the packet in a buffer. In either case, send back
           * an ACK for the highest contiguously received packet.
           */
          
          else if (seqno == stp_CB->NBE) 
            {
              
              pktbuf *next;
              unsigned short lastByte = plus(seqno, (pe->len - sizeof(*srh) -1));
              /* Bug Fixed on 10/29/2003 */
              
              /* packet in order - send to application */
              stp_consume(pe->pkt + sizeof(*srh), pe->len - sizeof(*srh));
              seqno = plus(seqno, (pe->len - sizeof(*srh)));
              
              if (greater(lastByte, stp_CB->LBReceived))
                stp_CB->LBReceived = lastByte;
              
              /*
               * Now check if the arrival of this packet
               * allows us to consume any more packets.
               */
              
              while((next = get_packet(stp_CB, seqno)) != NULL) 
                {
                  printf("Batch reading!!\n");
                  seqno = plus(seqno,next->len);
                  stp_consume(next->data, next->len);
                  free_packet(next);
                }
              
              stp_CB->NBE = seqno;
              stp_CB->LBRead = minus(seqno,1); /* Bug Fixed on 10/29/2003 */
              
            } 
          else 
            {
              /* packet out of order but within receive window, copy
               * the data and record the seqno to validate the buffer
               */
              
              unsigned short lastByte = plus(seqno, (pe->len - sizeof(*srh) -1));
              /* Bug Fixed on 10/29/2003 */
              
              add_packet(stp_CB, seqno, pe->len - sizeof(*srh), pe->pkt + sizeof(*srh));
              
              if (greater(lastByte, stp_CB->LBReceived))
                stp_CB->LBReceived = lastByte;
              
            }
          
          if (minus(stp_CB->LBReceived, stp_CB->LBRead) > ReceiverMaxWin)
            {
              printf("Not in feasible window.\n");
              
              reset(stp_CB->fd);
              return -1;
            }
          
          
          stp_CB->rwnd = ReceiverMaxWin - minus(stp_CB->LBReceived, stp_CB->LBRead);
          
          printf("rwnd adjusted: (%u)\n", stp_CB->rwnd);
          
          /* Always send an ACK back to the sender. */
          stp_send_ack(stp_CB);
          return 0;
          break; 
          
        default: 
          /* Invalid packet received */
          printf("Invalid packet.\n");
          reset(stp_CB->fd); 
          return -1;
          
        } /*end of switch(type) in ESTABLISHED state*/
      
    default: 
      return -1;
    } /*end of switch (stp_CB->state )*/
  
} /* end of stp_receive_state_transition_machine */

/*
 * Run the receiver polling loop: allocate and initialize the
 * stp_recv_ctrl_blk then enter an infinite loop to process incoming
 * packets.
 */
int stp_receiver_run(char *dst, int sport, int rport)
{
  stp_recv_ctrl_blk *stp_CB = (stp_recv_ctrl_blk *) malloc(sizeof(*stp_CB));
  
  /* Variables used to simulate out-of-order arrivals */
  int  delay_pkt_set = 0;
  int  delay_pkt_len;
  char delay_pkt[STP_MTU];
  
  stp_event *pe = (stp_event *)malloc(sizeof(*pe));
  
  /*
   * Initialize the receiver's stp_CB block 
   * and open the underlying UDP/IP communication channel.
   */
  stp_CB->state = STP_LISTEN;
  if ((stp_CB->fd = udp_open(dst, sport, rport)) < 0) return -1;
  stp_CB->rwnd = ReceiverMaxWin;
  stp_CB->LBRead = 0;
  stp_CB->LBReceived = 0;
  stp_CB->NBE = 1;
  stp_CB->recvQueue = NULL;
  
  /*
   * Enter an infinite loop reading packets from the network
   * and processing them.  The receiver processing loop is
   * simple because (unlike the sender) we do not need to schedule
   * timers or handle any asynchronous events.
   */
  while(1)
    {
      int len;
      unsigned char pkt[STP_MTU];
      
      /* Block until a new packet arrives */
      while ((len = readpkt(stp_CB->fd, pkt, sizeof(pkt))) <= 0)  /* Bug fixed in v1.2 */
        ;   /* Busy wait */
      
      if (event_happens(CorruptedPacketProbability)) {
        int random_byte = lrand48() % len, random_bit = lrand48() % 8;
        //printf("RECEIVED PACKET CORRUPTED: byte %d from %02x",
        //random_byte, pkt[random_byte]);
        printf("RECEIVED PACKET CORRUPTED\n");
        pkt[random_byte] ^= (char) (1 << random_bit);
        //printf(" to %02x\n", pkt[random_byte]);
      }
      
      pe->pkt = (char *) pkt;
      pe->len = len;
      
      /* Do the processing associated with a new packet arrival. But,
       * with probability PLP, we pretend this packet got lost in the
       * network.
       */
      if (event_happens(PacketLossProbability)) 
        { 
          printf("PACKET DROPPED\n");  /* Do nothing */
          continue;
        }    
      else if (!delay_pkt_set && event_happens(OutOfOrderPacketArrivalProbability)) 
        {        
          printf("PACKET DELAYED\n");
          memcpy (delay_pkt, pkt, len);
          delay_pkt_len = len; 
          delay_pkt_set = 1;
        }
      else if (delay_pkt_set) 
        {
          /* Process the packets in reverse order and unset delay_pkt_set bit */
          
          if (stp_receive_state_transition_machine(stp_CB, pe) == -1) 
            {
              return -1;
            }
	  pe->pkt = delay_pkt;
	  pe->len = delay_pkt_len;
          switch (stp_receive_state_transition_machine(stp_CB, pe)) 
            {
            case -1:
              return -1;
              break;
            case 1:
              return 0;
              break;
            }
          delay_pkt_set = 0;
        }
      /*  Otherwise, we're in normal operating mode */
      else
        switch (stp_receive_state_transition_machine(stp_CB,pe)) 
          {
          case -1:  /* Error */
            return -1;
            break;
          case 1:   /* File transfer complete */ 
            return 0;
            break;
          } 
    }
}


int main(int argc, char **argv)
{
  char* sendingHost;
  int sendersPort, rport;
  
  if (argc < 4 || argc > 9) 
    {
      fprintf(stderr, "usage: ReceiveApp ReceiveDataFromHost doRecvOnPort sendResponseToPort "
              "[packetLossProb [ACKlossProb [DelayedPacketProb "
              "[CorruptedPacketProb [CorruptedACKProb]]]]]\n");
      exit(1);
    }
  
  srand48(time(NULL));
  
  // Extract the arguments 
  int argIndex = 1;
  sendingHost = argv[argIndex++];
  rport = atoi(argv[argIndex++]);
  sendersPort = atoi(argv[argIndex++]);

  if (argc > argIndex) {
    PacketLossProbability = strtod(argv[argIndex++], NULL);
  } 
  
  if (argc > argIndex) {
    AckLossProbability = strtod(argv[argIndex++], NULL);
  } 
  
  if (argc > argIndex) {
    OutOfOrderPacketArrivalProbability = strtod(argv[argIndex++], NULL);
  }
  
  if (argc > argIndex) {
    CorruptedPacketProbability = strtod(argv[argIndex++], NULL);
  }
  
  if (argc > argIndex) {
    CorruptedACKProbability = strtod(argv[argIndex++], NULL);
  }
  
  printf("Listening on port %d From host %s from port %d\n"
         "PacketLossProb %4.2f AckLossProb %4.2f OutOfOrderProb %4.2f\n"
         "CorruptedPacketProb %4.2f CorruptedAckProb %4.2f\n",
         rport, sendingHost, sendersPort,
         PacketLossProbability, AckLossProbability, OutOfOrderPacketArrivalProbability,
         CorruptedPacketProbability, CorruptedACKProbability);
  
  /*
   * Open the output file for writing.  The STP sender tranfers
   * a file to us and we simply dump it to disk.
   */
  outFile = open("OutputFile", O_CREAT|O_WRONLY|O_TRUNC, 0644);
  if (outFile < 0) 
    {
      perror("OutputFile could not be created");
      return 0;
    }
  
  /*
   * "Run" the receiver protocol.  Application can check the return value.
   */
  if (stp_receiver_run(sendingHost, sendersPort, rport) !=  0) {
    printf("File transfer failed.\n");
  } else {
    printf("File transfer completed successfully.\n");
  }
  return 0;
}
