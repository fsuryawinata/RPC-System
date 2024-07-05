# RPC-System

This project is an Remote Procedure Call (RPC) System is written in C and runs in a Linux VM.

## Overview

The RPC System consists of two components: the client and the server.

- **Client Side**: Receives messages from the server, executes the given commands, and returns responses to the server.
- **Server Side**: Sends messages to the client side requesting one of the two commands (`rpc_handle` to run a function on the client side or `rpc_register` to register a new function) and receives the responses.

The server is initialized using a port.

## How to Run

To run the server and client, connect to the Linux VM and execute the following command in the terminal:

```sh
./rpc-server -p <port> & ./rpc-client -i <ip-address> -p <port>
```
where port is the TCP port number of the server and ip-address is your ip address

Alternatively, open 2 terminals and run the client and server command seperately
