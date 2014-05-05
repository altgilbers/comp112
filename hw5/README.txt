I didn't finish this project, but I had many of the pieces in place.

My plan was to have each node send out it's periodic "i'm alive" broadcast with the timestamp of the latest message it had recieved.

A newly started node would wait for a couple of periods to hear from a node that was already running.   If it did, it would ask the most current node to be brought up to date.

When a message is recieved, it is broadcast out the subnet (along with the time of original receipt) to bring the rest of the nodes up to date.

I was also hoping to include a hash of all the key-value pairs in the beacon ping, in addition to the timestamp as a way to ensure integrity.


