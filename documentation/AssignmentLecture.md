##CNET Assignment

- All nodes are considered hosts, not routers. Each has ability to store and forward messages.
- Physical layer is implemented, we must implement other layers.
- Ability to generate messages is not required. 
- CNET Simulator can randomly generate messages to help test architecture.
- Utilize both write\_reliable() and write\_unreliable() to test with random errors.
- Manage coordination in Data-Link-Layer correctly. (Sequencing).
- Dynamically adjust window size.
- Routing is FIXED. Can utlize a routing-table to help if needed. Matrix form.
- Selective Reject or Go-back-N are both acceptable. Go-back-N is easier!

#####Steps for Developing Simulation Model

1. Specify the Topology and general simulation configuration
2. Specify the nodes
3. Test Model: Look at different configs. Change the variables.

#####Nodes

- Local variables are specific only to that node.
- Only required function is reboot_node(). This is essential.
- reboot\_node() must set all event handlers. Must also CHECK() them.

#####Functions

CNET\_read\_application: 
 
```
Sender SENDS message via PHYSICAL layer

IMPORTS: destAddress, actualMessage, messageLength
```

CNET\_read\_physical

```
Destination RECEIVES message via PHYSICAL layer

IMPORTS: link, frame, lengthFrame

Each node has a fixed number of links. Topology defines the LINKS.
Link shows WHO the sender is, where the message arrived from.
```

![Example Topology](/Users/connor/Desktop/123.png)

#####When A is Sending:
- Application Ready Event
- read application. 
	- whats the Destination?
		- set link number accordingly
		- set the sequence number accordingly
- transmit frame
	- ACK: send simple ack frame
	- DATA: 
		- disable\_application for specific destination. 
		- send write\_physical() and start timer.

#####When B/C is Sending:
- Application Ready Event
- read application 
- transmit frame
- Destination is implicit, we only send to A.
	- ACK: send simple ack frame
	- DATA: 
		- disable\_application for specific destination. 
		- send write\_physical() and start timer.

#####When A is Receiving:
- Physical Ready Event
- read physical
- Who is the sender? Which link?
	- ACK: 
		- Check is sequence number correct?
		- CORRECT: stop timer, enable\_application() for the sender.
		- INCORRECT: ignore + wait for correct OR for timeout.
	- DATA: 
		- Check is sequence number correct?
		- CORRECT: accept message, write\_application(). Transmit ACK to correct link.
		- INCORRECT: ignore message + wait for correct.

#####When B/C is Receiving:
- Physical Ready Event
- read physical 
- ACK: 
	- Check is sequence number correct? 
	- Stop timer. 
	- enable\_application() for node A.
- DATA: 
	- Check is sequence number correct? 
	- write\_application(). Transmit ack to A.




