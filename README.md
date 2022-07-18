# P2P Port forward utility

Utility to create a P2P tunnel between any two machines, regardless of their NAT or firewall.

## Prior art:
- Port forwarding on your router. (Requires technical skills, requires access to router)
- UPnP: Temporary portforward by your device asking the router to do it. (Disabled in most routers)
- WebRTC can open P2P channels between two machines, if at least one of them is located behind a non-symmetric (non-enterprise) NAT.

## Goal:
- Open a tunnel even if both devices are behind a symmetric NAT, using UDP hole punching, and the birthday paradox.
- Similair user experience to SSH Port forward, but allow both TCP and UDP ports to be forwarded to the remote machine
- Opened tunnel is UDP, giving superior speed if implemented correctly.
- Users can forward TCP or UDP. Need to implement TCP like communications over UDP link.

## Components:
- Command and Control (C2) server: to pass messages between devices before a P2P tunnel can be opened
- IP server(s): similair to STUN server in WebRTC, helps devices identify their own public IP and NAT properties.    
  - Optional, an advanced user could set these properties, if known and static
  - Multiple servers needed to test NAT properties.
- Client application: Constantly running in background, connected to C2. Waiting to receive connection requests from other clients.
  - Should also include features for key generation, user access control, .. so the user doesn't need to edit config files

## Security:
- C2 server only acts to facilitate message passing between devices.
- All communications between devices E2E encrypted when running through the C2 server.
- Optionally, opened tunnel also E2E encrypted, to achieve port forward and VPN like behaviour simultaneously.
- Permission to open a tunnel to your machine is restricted with public keys, similair to SSH.
- Using widely accepted high security algorithms (Ed25519, ECDH, AES-256-GCM)
- NOTE: Do not use this in security critical applications until properly tested and examined by experts.

# Progress
Early stages of development. Don't expect it to work yet.
- Object and callback oriented wrapper for windows sockets
- Object oriented wrapper above the neccessare OpenSSL primitives
- Components needed for an E2E, authenticated connection
- IP Server functional