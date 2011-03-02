/*
 * This code is adapted from a course at 
 * Boston University for use in CPSC 317 at UBC.
 *
 *
 * A list that stored packets that have already been received
 * by the receiver.
 * 
 * Version 1.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "stp.h"

void printList(stp_recv_ctrl_blk  *info)
{
  pktbuf *curPacket;
  curPacket = info->recvQueue;
  while(curPacket != NULL)
    {
      printf("%d\n", curPacket->seqno);
      curPacket = curPacket->next;
    }
  
}


/* 
 *  Adds a packet in the receive queue so that in the
 *  end the packets saved in the receive queue are sorted
 *  by the sequence number.
 *
 *  Adjusts the last byte read (info->LBRead), which in fact happens to be
 *  one less than the seqno of the first packet.
 */
void add_packet(stp_recv_ctrl_blk  *info, unsigned short seqno, int len, char *data)
{
  pktbuf *curPacket;
  
  curPacket = (pktbuf *)malloc(sizeof(pktbuf));
  
  curPacket->seqno = seqno;
  curPacket->len = len;
  memcpy(curPacket->data, data, len);
  
  /* Find the slot where the packet fits */
  if (info->recvQueue == NULL)
    {
      /* First packet on the list */
      
      curPacket->next = NULL;
      info->recvQueue = curPacket;
    }
  else if (greater(info->recvQueue->seqno, seqno))
    {
      /* The packet which is added should become the first packet on the
	 list, i.e. be inserted before the current first packet on the list */
      
      curPacket->next = info->recvQueue;
      info->recvQueue = curPacket;
    }
  else
    {
      
      /* Walk across the list, find a place for the packet 
	 which would insert the packet between two packets:
	 - the packet previous to the added packet should have a smaller seqno
	 - the packet to the right of the added packet should either
         be NULL or a packet with greater seqno */
      pktbuf *prev = info->recvQueue, *traverse = prev->next;
      
      if(prev->seqno == seqno)
        {
          free(curPacket);
          return;	
	}
      while ((traverse != NULL) && greater(seqno, traverse->seqno))
	{
	  prev = traverse;
	  traverse = traverse->next;
	}
      
      if (traverse == NULL)
	{
	  /* Appending to the end of the list */
	  prev->next = curPacket;
	  curPacket->next = NULL;
	}
      else
	{
	  if(traverse->seqno == seqno)
            {
              free(curPacket);
              return;
            }
          
	  /* Inserting into the middle of the list */
	  prev->next = curPacket;
	  curPacket->next = traverse;
	}
      
    }
  
}

void free_packet(pktbuf *pbuf)
{
  free(pbuf);
}

/*
 * Returns a packet whose sequence number equals to seqno.
 * If no such packet, returns NULL.
 *
 */
pktbuf *get_packet(stp_recv_ctrl_blk *info, unsigned short seqno)
{
  pktbuf *traverse = info->recvQueue, *prev = NULL;
  
  while(traverse)
    {
      if (traverse->seqno == seqno)
	{
          
	  /*
	   * Unlink the node from the list 
	   *
	   */
	  if (prev == NULL)
	    {
	      /* The first node on the list */
	      info->recvQueue = traverse->next;
              
	    }
	  else
	    {
	      /* Some other node */
	      prev->next = traverse->next;
	    }
          
	  return traverse;
	}
      
      prev = traverse;
      traverse = traverse->next;
    }
  
  return NULL;
  
}
