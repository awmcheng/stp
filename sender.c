/************************************************************************
 * Adapted from a course at Boston University for use in CPSC 317 at UBC
 * 
 *
 * The interfaces for the STP sender (you get to implement them), and a 
 * simple application-level routine to drive the sender.  
 *
 * This routine reads the data to be transferred over the connection
 * from a file specified and invokes the STP send functionality to 
 * deliver the packets as an ordered sequence of datagrams.
 *
 * Version 1.0 
 * 
 *
 *************************************************************************/

#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/file.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <math.h>


#include "stp.h"
#define PKT_SIZE 4096
#define STP_SUCCESS 1
#define STP_ERROR -1


//Sender states
#define STP_SYN_SENT   0x24
#define STP_CLOSING   0x25
#define FIN_WAIT   0x26	//We do not neet to implement this state.
#define ARRAYSIZE 25 // ceiling of (receiver window size of 5000 / MTU of 300 ) is 17 so allocating 25

int SenderMaxWin = 5000;        /* Maximum window size */


typedef struct {
  
	int state;	 /* protocol state: normally ESTABLISHED */
	int sock; 	/* UDP socket descriptor */

	unsigned short swnd;       // latest advertised sender window size
	unsigned short NBE;        // next byte expected - next ACK seq Num expected
	unsigned short NextSeqNum;     // last byte ACKed = next ACK SEQ Num - length
	unsigned short LBSent; 	// last byte Sent not ACKed

	unsigned short numBytesInFlight;
	unsigned short ISN;        /* initial sequence number */

	unsigned short seqArray[25]; // pointer to seqnumber array
	struct itimerval timeArray[25]; // pointer to array of itimerval DATA


	//pktbuf *sendQueue;         /* Pointer to the first node of the send queue */
     
} stp_send_ctrl_blk;

//Returns 0 if two unsigned char are equal. 
int compareSum(unsigned char* a,unsigned char* b, int size)
{
	while(size-- > 0) 
	{
		if ( *a != *b ) 
		{ 
			return (*a < *b ) ? -1 : 1; 
		}
		a++; b++;
	}
	return 0;

	
} 

//Read packet (stop and wait approach)
int readPacket(stp_send_ctrl_blk *stp_CB, char *pkt, unsigned short int type)
{
	int readTemp = readWithTimer(stp_CB->sock, pkt, 1000);
	int numberofTimeouts =0;
	unsigned short seqNum;
	if(type == STP_FIN)
	{
		seqNum = stp_CB->NextSeqNum;
	}
	else if(type==STP_SYN)
		seqNum = stp_CB->ISN;
		
	else
		seqNum = stp_CB->NextSeqNum;
	
	
	while (readTemp==STP_TIMED_OUT){
			printf("Sorry timed out...\n ");
			
			numberofTimeouts++;
			switch (numberofTimeouts)
			{
			case 1 : sendpkt(stp_CB-> sock, type, 0, seqNum, 0,0);
				readTemp = readWithTimer(stp_CB->sock, pkt, 2000);
				break;
			case 2 : sendpkt(stp_CB-> sock, type, 0, seqNum, 0,0);
				readTemp = readWithTimer(stp_CB->sock, pkt, 4000);
				break;
			case 3 : reset(stp_CB->sock);
				break;
			}
		
	}
	
	stp_header *stpHeader = (stp_header *) pkt;
	unsigned char sum = checksum(stpHeader, 0);
	unsigned char originalSum = stpHeader->checksum;
	
	unsigned char* sumPt = &sum;
	unsigned char* originalSumPt = &originalSum;
	//printf("Check Sum: %d\n", compareSum(sumPt, originalSumPt, sizeof(sum)));
	if(compareSum(sumPt, originalSumPt, sizeof(sum)) != 0)
	{
		printf("ACK was corrupted. Retransmit\n");
		memset(pkt, 0, PKT_SIZE);
		
		sendpkt(stp_CB-> sock, type, 0, seqNum, 0,0);
		readTemp = readPacket(stp_CB, pkt,type);
		
	}
	
	return readTemp;
}

void setTimer(stp_send_ctrl_blk *stp_CB, int ms){
	// assumes the LBSent is the most recent
	int i=0;
	while(i<25){
		// curious about checking for seqnum of 0
		// check for 1 sec initial timeout and find check first avail seqArray[i] for 0 and then using index i complete the timeArray[i]
		if(ms==1000){
		if(stp_CB->seqArray[i]==0 && (double)stp_CB->timeArray[i].it_value.tv_sec==0.0 && (double)stp_CB->timeArray[i].it_value.tv_usec==0.0){
			stp_CB->seqArray[i]=stp_CB->LBSent;
			stp_CB->timeArray[i].it_interval.tv_sec =0; // repeat 0 seconds 
			stp_CB->timeArray[i].it_interval.tv_usec = 0; // repeat 0 microseconds time
			stp_CB->timeArray[i].it_value.tv_sec =ms/1000; // set timer to go off in ms/1000 seconds
			stp_CB->timeArray[i].it_value.tv_usec =0; // microseconds of time to go off in default to 0 microseconds
			setitimer(ITIMER_REAL, &stp_CB->timeArray[i],0);
			break;
			}
		}
		else if(ms==2000||ms==4000){
			if(stp_CB->seqArray[i]==stp_CB->LBSent){
			stp_CB->timeArray[i].it_interval.tv_sec =0; // repeat 0 seconds 
			stp_CB->timeArray[i].it_interval.tv_usec = 0; // repeat 0 microseconds time
			stp_CB->timeArray[i].it_value.tv_sec =ms/1000; // set timer to go off in ms/1000 seconds
			stp_CB->timeArray[i].it_value.tv_usec =0; // microseconds of time to go off in default to 0microseconds					
			setitimer(ITIMER_REAL, &stp_CB->timeArray[i],0);
			break;		
			}
		}
	i++;
	}
}

// resets timers after receiving ack, to be called from check ack
void resetTimer(stp_send_ctrl_blk *stp_CB){
	int i=0;
	while (i < 25){
		if(stp_CB->seqArray[i]==stp_CB->LBSent){
			stp_CB->seqArray[i]=0;
			stp_CB->timeArray[i].it_interval.tv_sec =0; // repeat 0 seconds 
			stp_CB->timeArray[i].it_interval.tv_usec = 0; // repeat 0 microseconds time
			stp_CB->timeArray[i].it_value.tv_sec =0; // set timer to go off in ms/1000 seconds
			stp_CB->timeArray[i].it_value.tv_usec =0; // microseconds of time to go off in default to 0microseconds				
			break;
		}
	i++;
	}
}



/*
 * Send STP. This routine is to send a data packet no greater than
 * MSS bytes. If more than MSS bytes are to be sent, the routine
 * breaks the data into multiple packets. It will keep sending data
 * until the send window is full. At which point it reads data from
 * the network to, hopefully, get the ACKs that open the window. You
 * will need to be careful about timing your packets and dealing with
 * the last piece of data.
 * 
 * The function returns STP_SUCCESS on success, or STP_ERROR on error.
 */
int stp_send (stp_send_ctrl_blk *stp_CB, unsigned char* data, int length) {
  

  char* data1 = (char *)data;
  sendpkt(stp_CB->sock, STP_DATA, stp_CB->swnd, stp_CB->NextSeqNum, data1, length);
 
	//checks if the latest seqno sent is larger than the NBE and stores it as stp_CB->LBSent (last byte sent)
	// using wraparound methods

// check using greater method from wraparound if seqno is greater than ISN as well as greater than last stp_CB.LBSent
	if((greater(stp_CB->NextSeqNum, stp_CB->LBSent)==1))
		stp_CB->LBSent = stp_CB->NextSeqNum; // want to update LBSent to the NextSeqNum

 
  char pkt[PKT_SIZE];
	
	int readTemp = readPacket(stp_CB, pkt, STP_DATA);
	if (readTemp<0){
		return STP_ERROR;
	}
	
	printf("Received packet back\n");
	
	
		
	stp_header *stpHeader = (stp_header *) pkt;
  	//unsigned short type = ntohs(stpHeader->type);
  	unsigned short seqno = ntohs(stpHeader->seqno);
  	unsigned short win = ntohs(stpHeader->window);
	stp_CB->NextSeqNum = seqno;
	stp_CB->NBE = (seqno+length);
	stp_CB->swnd = win;


	// checkTimer(stp_CB);
  
  return STP_SUCCESS;
}
 
//Creates UDP sockets
int open_udp(char *destination, int destinationPort,int receivePort)
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
	printf ("Binding locally to port %d\n", receivePort);
	memset((char *)&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(receivePort);
  
	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) 
    {
      perror("Bind failed");
      return (-2);
    }
  
	dst = hostname_to_ipaddr(destination);
  
	if (!dst) {
		printf("Invalid sending host name: %s\n", destination);
		return -4;
	}
	printf ("Configuring  UDP \"connection\" to <%u.%u.%u.%u, port %d>\n", 
          (ntohl(dst)>>24) & 0xFF, (ntohl(dst)>>16) & 0xFF, 
          (ntohl(dst)>>8) & 0XFF, ntohl(dst) & 0XFF, destinationPort);
  
	memset((char *)&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(destinationPort);
	sin.sin_addr.s_addr = dst;
	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) 
	{
      perror("connect");
      return(-1);
	}
	printf ("UDP \"connection\" to <%u.%u.%u.%u port %d> configured\n", 
          (ntohl(dst)>>24) & 0xFF, (ntohl(dst)>>16) & 0xFF, 
          (ntohl(dst)>>8) & 0XFF, ntohl(dst) & 0XFF , destinationPort);
	
	
	return fd;
}






/*
 * Open the sender side of the STP connection. Returns the pointer to
 * a newly allocated control block containing the basic information
 * about the connection. Returns NULL if an error happened.
 *
 * Note, to simplify things you should use connect(). When used with a
 * UDP socket all packets then sent and received on the given file
 * descriptor go to and are received from the specified host. Reads
 * and writes are still completed in a datagram unit size, but the
 * application does not have to do the multiplexing and
 * demultiplexing. This greatly simplifies things but restricts the
 * number of "connections" to the number of file descriptors and isn't
 * very good for a pure request response protocol like DNS where there
 * is no long term relationship between the client and server.
 */
stp_send_ctrl_blk * stp_open(char *destination, int destinationPort,
                             int receivePort) {

    unsigned int iseed = (unsigned int) time(NULL);
	srand(iseed);

	// pseudo random seqnumber to start the tcp communication
	int tempISN = 5+ (int)((rand()%(100)));
	printf("MAX_RAND %d\n", tempISN);
	
	printf ("Configuring  UDP \"connection\" to %s, sending to port %d listening for data on port %d\n", 
          destination, destinationPort, receivePort);
    
	stp_send_ctrl_blk *stp_CB = (stp_send_ctrl_blk *) malloc(sizeof(*stp_CB));
	
		
	
	if ((stp_CB->sock = open_udp(destination, destinationPort,receivePort) ) < 0) /* UDP socket descriptor */
	{
		return NULL; 
	}
	
	stp_CB->swnd = SenderMaxWin;    /* latest advertised sender window */
	//stp_CB->NBE = 0;        /* next byte expected */
	stp_CB->NextSeqNum =0;     /* last byte ACKed */
	
	stp_CB->ISN = tempISN;        //initial sequence number should not be zero, this is a random number
	stp_CB->LBSent=stp_CB->ISN; 	/* last byte Sent not ACKed */

	//stp_CB->sendQueue;
	
	sendpkt(stp_CB-> sock, STP_SYN, 0, stp_CB->ISN, 0,0);
	stp_CB->state = STP_SYN_SENT;	 /* protocol state*/
	
	char pkt[PKT_SIZE];
	
	int readTemp = readPacket(stp_CB, pkt, STP_SYN);
	if (readTemp<0){
		return NULL;
	}
	
	printf("Received packet back\n");
	
	/*
	stp_header *stpHeader = (stp_header *) pkt;
	unsigned char sum = checksum(stpHeader, 0);
	unsigned char originalSum = stpHeader->checksum;
	
	unsigned char* sumPt = &sum;
	unsigned char* originalSumPt = &originalSum;
	//printf("Check Sum: %d\n", compareSum(sumPt, originalSumPt, sizeof(sum)));
	if(compareSum(sumPt, originalSumPt, sizeof(sum)) != 0)
	{
		printf("ACK was corrupted. Retransmit\n");
		memset(pkt, 0, PKT_SIZE);
		
		sendpkt(stp_CB-> sock, STP_SYN, 0, stp_CB->ISN, 0,0);
		readTemp = readPacket(stp_CB, pkt,STP_SYN);
		
	}
	*/
	
	stp_CB->state = STP_ESTABLISHED;
	
	stp_header *stpHeader = (stp_header *) pkt;
  	//unsigned short type = ntohs(stpHeader->type);
  	unsigned short seqno = ntohs(stpHeader->seqno);
  	unsigned short win = ntohs(stpHeader->window);
	stp_CB->NextSeqNum = seqno;
	stp_CB->swnd = win;

	



	// print out the arrays for testing purpose
/*
	int i=0;
	while(i<25){
		printf("array seqnum/timer has: at %d :", i);
		printf("seqnum: %d\n", stp_CB->seqArray[i]);
		printf("timeArray: %2.1f\n", (double)stp_CB->timeArray[i].tv_sec);
		i++;
		
	}
*/


	//dump('r', pkt, readTemp);
	//stp_CB->NextSeqNum = tempISN+1;

	
	//int readTemp = readpkt(stp_CB->sock, pkt, sizeof(pkt));
	//printf("%s\n", pkt);
	
	return stp_CB;
}



/*
 * Make sure all the outstanding data has been transmitted and
 * acknowledged, and then initiate closing the connection. This
 * function is also responsible for freeing and closing all necessary
 * structures that were not previously freed, including the control
 * block itself. Returns STP_SUCCESS on success or STP_ERROR on error.
 */
int stp_close(stp_send_ctrl_blk *stp_CB) {
	stp_CB->state = STP_CLOSING;
  
	/* This will be for any outstanding data. 
	while(stp_CB->NextSeqNum != stp_CB->ISN)
	{
		//receive rest of packet
	}*/

	sendpkt(stp_CB->sock, STP_FIN, 0, stp_CB->NextSeqNum, 0,0);
  
	char pkt[PKT_SIZE];
	int readTemp = readPacket(stp_CB, pkt, STP_FIN);
	printf("Read Temp: %d\n", readTemp);
	if (readTemp<0){
		close(stp_CB->sock);
		free(stp_CB);
		return STP_ERROR;
	}
	
	
	//printf("Window: %d\n", stp_CB->swnd);
	//printf("%s\n",pkt);
	printf("Connection Closed\n");
	close(stp_CB->sock);
	free(stp_CB);
	
	return STP_SUCCESS;
}


/*
 * This application is to invoke the send-side functionality. Feel
 * free to rewrite or write your own application to test your
 * code. Some examples of other applications that could be used
 * instead:
 * 
 * - A program that reads or generates a log and transmits it through
 *   STP;
 * - A program that reads the standard input and transmits it through
 *   STP;
 */
int main(int argc, char **argv) {
  
  stp_send_ctrl_blk *stp_CB;
  
  char *destinationHost;
  int receivePort, destinationPort;
  int file;
  
  /* You might want to change the size of this buffer to test how your
   * code deals with different packet sizes.
   */
  unsigned char buffer[STP_MSS];
  int num_read_bytes;
  
  /* Verify that the arguments are right*/
  if (argc != 5) {
    fprintf(stderr, "usage: SendApp DestinationIPAddress/Name receiveDataOnPort sendDataToPort filename \n");
    exit(1);
  }
  
  /*
   * Open connection to destination.  If stp_open succeeds the
   * stp_CB should be correctly initialized.
   */
  
  destinationHost = argv[1];
  receivePort = atoi(argv[2]);
  destinationPort = atoi(argv[3]);
  
  stp_CB = stp_open(destinationHost, destinationPort, receivePort);
  if (stp_CB == NULL) {
    /* YOUR CODE HERE */
	perror("stp_control_block cannot be NULL");
	exit(1);
  }
  
  /* Open file for transfer */
  file = open(argv[4], O_RDONLY);
  if (file < 0) {
    perror(argv[4]);
    stp_close(stp_CB);
    exit(1);
  }
  
  /* Start to send data in file via STP to remote receiver. Chop up
   * the file into pieces as large as max packet size and transmit
   * those pieces.
   */
  while(1) {
    num_read_bytes = read(file, buffer, sizeof(buffer));
    
    /* Break when EOF is reached */
    if(num_read_bytes <= 0)
      break;
    
    if(stp_send(stp_CB, buffer, num_read_bytes) == STP_ERROR) {
      /* YOUR CODE HERE */
	perror("STP_ERROR on send");
	exit(1);
    }
  }
  
  
  close(file);
  /* Close the connection to remote receiver */   
  if (stp_close(stp_CB) == STP_ERROR) {
    /* YOUR CODE HERE */
	perror("STP_CLOSE error (Receiver is already closed)");
	exit(1);
  }
  
  return 0;
}


