execwsock
=========

Creates unix socket, runs a programs and removes the socket after the program ends. It's a way to control the process status that may be used for init systems or for single-instance locking scripts.


Motivation
----------

To make a supervisor for OpenRC. But the upstream didn't like this solution, so the utility is abandened, see https://github.com/xaionaro/sockrund/.
