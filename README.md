# P2P Port forward utility

Utility to create a P2P tunnel between any two machines, regardless of their NAT or firewall.
By running the client application in the background on your machine, you can open a direct P2P tunnel to it, and use it to forward ports similair to SSH portforward.

## Components:
- Client application: Runs in background on target machine, accepts incomming connections
- Command and Control (C2) server: Constantly connected to all client applications, to facilitate message passing before a direct tunnel is established
- IP Server: Helps clients determine their own public IP address, and NAT properties. At least two servers are needed to determine the 

## Security:
- All connections between devices are authenticated and encrypted with Ed25519 public keys, much like SSH public key login.
- Strict permission and indentity checks in both server and client applications
- Optionally, opened P2P tunnel is also E2E encrypted, to achieve port forward and VPN like behaviour simultaneously.
- NOTE: Do not use this in security critical applications until properly tested and examined by experts.

# Progress:
Barebones functionality working
- All major components somewhat working
- Client application user interface lacking in features
- All keys hardcoded

# Building
- Currently only Windows socket wrapper available.
## Prerequisites:
- CMake
- OpenSSL dev library. 
  - On Windows/Visual Studio, recommended to use vcpkg to install this.
- wxWidgets library

# Todo:
- Create socket wrapper for linux
- Add faster hole punching for less restrictive NATs