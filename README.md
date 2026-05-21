# LDS – Local Distributed Storage (v0.1)

## Overview

LDS is a userspace block device implemented using the Linux Network Block Device (NBD) interface.

The system provides a virtual block device backed by a local in memory storage implementation.
This version focuses on the core infrastructure required for a distributed storage system.

The project demonstrates:

* Linux kernel - userspace block device communication
* epoll based event loop
* signal handling
* RAII based C++ design
* storage abstraction layer
* end to end filesystem validation

---

## Architecture

Linux Kernel (NBD)
│
│ ioctl + socket
▼
NBDDriverComm (Driver Layer)
│
│ DriverData requests
▼
IStorage (Abstraction)
│
▼
LocalStorage (In memory backend)

---

## Features (v0.1)

* Userspace block device using Linux NBD
* Single threaded event loop (epoll)
* Local in memory storage backend
* Support for:

  * READ
  * WRITE
  * FLUSH
  * TRIM
  * WRITE_ZEROES
  * DISCONNECT
* Clean shutdown via:

  * SIGINT / SIGTERM
  * q / Q from stdin
* End to End filesystem validation
* RAII based resource management
* Exception safe design

---

## Project Structure

LDS/
│
├── include/
│   ├── DriverData.hpp
│   ├── IDriverComm.hpp
│   ├── IStorage.hpp
│   ├── LocalStorage.hpp
│   └── NBDDriverComm.hpp
│
├── src/
│   ├── DriverData.cpp
│   ├── IDriverComm.cpp
│   ├── IStorage.cpp
│   ├── LocalStorage.cpp
│   └── NBDDriverComm.cpp
│
├── test/
│   └── LDS.cpp
│
├── bin/
│   └── lds
│
├── BashScript.sh
├── Makefile
└── README.md

---

## Build

Requirements:

* Linux
* g++
* make
* root permissions
* NBD kernel module

Build the project:

make

---

## Run

Load NBD kernel module:

sudo modprobe nbd

Run LDS:

sudo ./bin/lds /dev/nbd0 134217728

This creates a virtual block device of 128MB.

---

## End to End Test

Run:

./BashScript.sh

Test flow:

1. Build project
2. Load NBD module
3. Start LDS
4. Create ext4 filesystem
5. Mount device
6. Write test file
7. Read and verify file
8. Unmount device

Expected output:

test passed successfully.
LDS stopped cleanly.

---

## Design Principles

### RAII

All system resources are managed using RAII:

* file descriptors
* epoll instance
* signal descriptors
* terminal modes
* threads

This guarantees deterministic cleanup and no resource leaks.

### Exception Safety

All system failures are translated into exceptions.

The event loop guarantees:

* Failed request -> reply with error
* Clean disconnect
* No resource leaks

### Storage Abstraction

IStorage enables:

* Local storage (current implementation)
* Future distributed storage nodes
* Replication layers
* Caching mechanisms
* Persistent storage backends

---

## Author

Edan Ayzencot