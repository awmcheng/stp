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


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/file.h>

#include "stp.h"

#define STP_SUCCESS 1
#define STP_ERROR -1

typedef struct {
  
  int DELETE_ME;     /* used only to make this compile */
  
  /* YOUR CODE HERE */
  
} stp_send_ctrl_blk;



/* ADD ANY EXTRA FUNCTIONS HERE */


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
  
  /* YOUR CODE HERE */
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

     printf ("Configuring  UDP \"connection\" to %s, sending to port %d listening for data on port %d\n", 
          destination, destinationPort, receivePort);
    
  
  /* YOUR CODE HERE */
}


/*
 * Make sure all the outstanding data has been transmitted and
 * acknowledged, and then initiate closing the connection. This
 * function is also responsible for freeing and closing all necessary
 * structures that were not previously freed, including the control
 * block itself. Returns STP_SUCCESS on success or STP_ERROR on error.
 */
int stp_close(stp_send_ctrl_blk *stp_CB) {
  
  /* YOUR CODE HERE */
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
    }
  }
  
  /* Close the connection to remote receiver */   
  if (stp_close(stp_CB) == STP_ERROR) {
    /* YOUR CODE HERE */
  }
  
  return 0;
}


