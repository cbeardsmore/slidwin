# Makefile For CNET Sliding Window Simulation
# CC200 Assignment
# Last Modified: 21/05/16
# Connor Beardsmore - 15504319

run:
	cnet -O ASSIGNMENT

start:
	cnet -O -g ASSIGNMENT

clean :
	rm -f *.log
	rm -f *.cnet
	rm -f *.o