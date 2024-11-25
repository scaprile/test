# Test closing connections to update credentials

## server

- Opens 3 HTTPS servers
- Servers have two endpoints:
  - `/api/stats` returns the number of connections
  - `/api/close/0` request closing connections in server 0. Same for 1 and 2

## client

- Opens 3 sets of 10 connections each, one set against one server.
- Each connection requests `/api/stats`
- Once the 10 connections have been established, the connection to server 2 requests closing connections in server 1, and that in server one does the same for those in server 0.
- When a set of connections sees there are no outstanding connections, starts again.
- Connections are spaced differently for each set, so set 0 ends first, then set 1, then set 2

As a result:
- set 0 is established
- set 1 is established
- set 1 requests server 0 to close connections
- set 0 connections start again
- set 2 is established
- set 2 requests server 1 to close connections
- set 1 connections start again
- set 1 is established
- set 1 requests server 0 to close connections
- set 0 connections start again
Set 2 will not request server 1 to close connections again. If you want, you need to do it manually, connecting to `server2/api/close/1`, where `server2` address can be seen at `servers.h`
