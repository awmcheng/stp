#
# This code is adapted from a programming course at 
# Boston University for use in CPSC 317 at UBC
#
#
# Version 1.0
#


CC     = gcc
CFLAGS = -g -Wall
CLIBSSolaris  =  -lsocket -lnsl
CLIBSLinux = 
all:
	@echo "usage: make Linux|Solaris|clean|realclean|emacsClean"
Linux: SendAppL ReceiveAppL 
Solaris: SendAppS ReceiveAppS 



SendAppL: senderL.o stpL.o wraparoundL.o 
	$(CC) -o $@ $(CLIBSLinux) $(CFLAGS) $^

ReceiveAppL: receiverL.o wraparoundL.o receiver_listL.o stpL.o 
	$(CC)  -o $@ $(CLIBSLinux) $(CFLAGS) $^

senderL.o: stp.h sender.c
	$(CC) -c -o  $@  $(CFLAGS) sender.c

receiverL.o: stp.h receiver.c
	$(CC) -c -o  $@  $(CFLAGS) receiver.c

wraparoundL.o: stp.h wraparound.c
	$(CC) -c -o  $@  $(CFLAGS) wraparound.c

receiver_listL.o: stp.h receiver_list.c
	$(CC) -c -o  $@  $(CFLAGS) receiver_list.c

stpL.o: stp.h stp.c
	$(CC) -c -o  $@  $(CFLAGS) stp.c



SendAppS: senderS.o stpS.o wraparoundS.o 
	$(CC) -o $@ $(CLIBSSolaris) $(CFLAGS) $^

ReceiveAppS: receiverS.o wraparoundS.o receiver_listS.o stpS.o 
	$(CC)  -o $@ $(CLIBSSolaris) $(CFLAGS) $^

senderS.o: stp.h sender.c
	$(CC) -c -o  $@  $(CFLAGS) sender.c

receiverS.o: stp.h receiver.c
	$(CC) -c -o  $@  $(CFLAGS) receiver.c

wraparoundS.o: stp.h wraparound.c
	$(CC) -c -o  $@  $(CFLAGS) wraparound.c

receiver_listS.o: stp.h receiver_list.c
	$(CC) -c -o  $@  $(CFLAGS) receiver_list.c

stpS.o: stp.h stp.c
	$(CC) -c -o  $@  $(CFLAGS) stp.c




realclean: emacsClean clean

clean:
	-rm -f *.o SendAppL ReceiveAppL  SendAppS ReceiveAppS

emacsClean:
	-rm -f *~
