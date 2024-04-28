# TCP File Transfer System

## Overview
This project, part of the BCIT Network Security curriculum, features a TCP client-server application designed for efficient file transfers. Achieving a perfect score of 100%, the system leverages a Finite State Machine (FSM) for handling multiple client connections simultaneously and ensuring reliable data transmission over both IPv4 and IPv6 networks.

## Features
- **Unlimited File Size & Type Transfer**: Supports seamless transfer of files of any size and type.
- **Multiple File Transfer**: Clients can specify multiple files for transfer via command line arguments, including the use of wildcards.
- **Multiplexed Server**: Server efficiently manages multiple client connections at the same time through multiplexing.
- **IPv4/IPv6 Optimization**: Optimized for use over both IPv4 and IPv6 networks.
- **Cross-Platform Compatibility**: Compatible with various operating systems including macOS, Ubuntu, Fedora, and Kali, thanks to ISO C17 programming.
- **Graceful Exit**: Server can be safely terminated with CTRL-C.
- **Duplicate Handling**: Server appends a numerical suffix to duplicate filenames to avoid overwriting.

## Building Instructions

### Server
To build the server application, use the following commands:
```sh
mkdir server/cmake-build-debug
cd server/cmake-build-debug
cmake ../CMakeLists.txt
make
```
### Client
```sh
mkdir client/cmake-build-debug
cd client/cmake-build-debug
cmake ../CMakeLists.txt
make
```
## Running Instructions
### Server
To start the server, run:
```sh
./server -i <IP> -p <PORT> -d <directory to store files>
```
### Client
To initiate a file transfer from the client, run:
```sh
mkdir client/cmake-build-debug
cd client/cmake-build-debug
cmake ../CMakeLists.txt
make
```

## Environment Variables 
### Server Variables
- **IP**: Assign the IP address for the server (IPv4 or IPv6).
- **PORT**: Designate the port number for server operations.
- **Directory**: Specify the directory path where incoming files will be stored.

### Client Variables
- **IP**: Define the server's IP address to connect (IPv4 or IPv6).
- **PORT**: Set the port number to establish the connection with the server.
## Usage Examples
- **IPv4 Server & Client**:
-- Run the server and client using IPv4 addresses for network communication.
- **IPv6 Server & Client**:
--Utilize IPv6 addresses to operate the server and client.
- **Exiting the Server**:
-- Use CTRL-C to safely exit the server application.
- **Multiple File Transfer**:
-- Clients can transfer multiple files in one command by specifying each file path.
- **Wildcard File Selection**:
-- Clients can transfer files using wildcard notations to match file patterns.
- **Handling Duplicates**:
-- When receiving a file that already exists, the server will automatically rename it by appending a numerical suffix.

