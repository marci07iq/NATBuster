# UDP Transport layer
# Implements TCP like behaviour over an underlying UDP link
# Allows fast-track for UDP passthrough at an overhead of 2 bytes / datagram
# Implements periodic ping-pong on UDP

import asyncio
import asyncudp
import struct
from enum import Enum
import time

import os

from sortedcontainers import SortedList, SortedDict

from transport_common import EmittedType


class ChunkType(Enum):
    # Management messages
    # TYPE_MGMT_DC = 0, # Disconnect
    TYPE_MGMT_HELLO = 1,  # Followed by 64 byte pre-shared nonce
    # Keepalive for UDP socket, sent every 5 seconds from both sides.
    TYPE_MGMT_PING = 2,  # Followed by a 64 byte random nonce
    TYPE_MGMT_PONG = 3,  # Followed by the 64 byte reply

    # For TCP over UDP. Used for both internal comms and TCP forwarding
    TYPE_PKT_SINGLE = 16,  # Single packet, followed by 4 byte seq
    TYPE_PKT_START = 17,  # Packet start, followed by 4 byte seq, 3 byte len TODO: Remove len.
    TYPE_PKT_END = 18,  # Packet end, followed by 4 byte seq
    TYPE_PKT_MID = 19,  # Packet content, followed by 4 byte seq

    # For TCP over UDP control
    TYPE_PKT_ACK = 32,  # Ack, followed by 4 byte seq
    # TYPE_PKT_FEC = 33, # Forward error correction ??????

    # For
    TYPE_UDP_PIPE = 48,  # Deliver as-is, 1 extra byte, for internal pipe ID


class PacketChunk:
    def __init__(self, seq, packet_data, packet_data_offset, chunk_max_length):
        # Send state
        self.sent = False
        # Sequence number
        self.seq = seq
        # Bytes of data still to be sent
        data_left = len(packet_data) - packet_data_offset
        # Packet start
        if(packet_data_offset == 0):
            # Can send as single
            if((8 + data_left) <= chunk_max_length):
                data_len = data_left
                self.buffer = bytes(data_len + 8)
                self.type = ChunkType.TYPE_PKT_SINGLE

                struct.pack_into(
                    '<BL' + str(data_len) + 's',
                    self.buffer, 0,
                    self.type, self.seq, packet_data)
                self.new_offset = packet_data_offset + data_len
            # Can't send as single
            else:
                data_len = chunk_max_length - 8
                self.buffer = bytes(chunk_max_length)
                self.type = ChunkType.TYPE_PKT_START

                # Encode data length into 4 bytes
                pkt_len_enc = bytes(4)
                struct.pack_into('<L', pkt_len_enc,
                                 packet_data_offset, len(packet_data))

                struct.pack_into(
                    '<BL3s' + str(data_len) + 's',
                    self.buffer, 0,
                    self.type, self.seq, pkt_len_enc, packet_data)
                self.new_offset = packet_data_offset + data_len

        else:
            # End packet
            self.type = ChunkType.TYPE_PKT_END if(
                data_left <= length - 5) else ChunkType.TYPE_PKT_MID

            data_len = min(data_left, chunk_max_length - 5)
            self.buffer = bytes(data_len + 5)

            struct.pack_into(
                '<BL' + str(data_len) + 's',
                self.buffer, 0,
                self.type, self.seq, packet_data[packet_data_offset:])
            self.new_offset = packet_data_offset + data_len

class PacketChunkIn:
    def __init__(self, buffer):
        self.buffer = buffer
        self.type = buffer[0]
        self.seq = None
        self.data = None

        if(self.type == ChunkType.TYPE_PKT_START):
            self.length = None
            pass
class TransportUDP:
    async def __init__(self, address, port, magic, mtu=1500, maxflight=50):
        # Create UDP socket
        self.addr = (address, port)
        self.udp = await asyncudp.create_socket(remote_addr=self.addr)

        # Set parameters
        self.mtu = mtu
        self.maxflight = maxflight

        # Measure ping on ACKs
        self.ping = 0.1
        self.ping2 = self.ping ** 2
        self.ping_retry_mul = 1.5

        # Congestion management
        self.nflight = 0

        # TCP over UDP implementation
        self.seq_in = 0
        self.seq_out = 0
        self.send_queue = SortedList()
        self.recv_pile = SortedDict()
        self.recv_assemble_list = []

        # Connection management
        self.magic = magic
        self.ping_interval = 5
        self.pong_timeout = 30
        self.last_ping = None
        self.last_pong = None

        self.ticking = False
        self.ticktask = None

    # Generate new sequence number
    def _gen_seq(self):
        self.seq_out += 1
        return self.seq_out

    # Generate a ping and pong
    def _send_ping(self):
        ping_packet = bytes(65)
        struct.pack_into(
            '<B64s', ping_packet, 0,
            ChunkType.TYPE_MGMT_PING, os.urandom(64))

        self.udp.sendto(ping_packet)
        # On the first ping, reset the pong timer
        if(self.last_ping is None):
            self.last_pong = time.time()
        self.last_ping = time.time()

    def _send_pong(self, ping_packet):
        pong_packet[:] = ping_packet
        struct.pack_into(
            '<B', pong_packet, 0,
            ChunkType.TYPE_MGMT_PONG)

        self.udp.sendto(pong_packet)
        self.last_pong = time.time()

    # Internal tick for retransmit, called after network event or timer
    def _tick_impl(self):
        new_time = time.time()

        # Need to send new ping
        if((self.last_ping is not None) and (self.last_ping + self.ping_interval < new_time)):
            self._send_ping()

        if((self.last_pong is not None) and (self.last_pong + self.pong_timeout < new_time)):
            # Connection timed out
            # TODO: Close connection
            pass

        # Re-transmit oldest packet
        while(self.nflight < self.maxflight and len(self.send_queue) > 0):
            # Elem transmitted the earliers
            head_elem = self.send_queue[0]

            if(head_elem[0] + self.ping * self.ping_rt_mul < new_time):
                # Remove from queue
                head_chunk = self.send_queue.pop(0)[1]
                # Re-transmit
                self.udp.sendto(head_chunk.buffer)
                # Re-add to queue
                self.send_queue.add((new_time, head_chunk))

                if(not head_chunk.sent):
                    self.nflight += 1
            else:
                # All packets have been re-transmitter recently
                break

    async def _tick_generator(self):
        while self.ticking:
            self._tick_impl()
            await asyncio.sleep(self.ping * self.ping_retry_mul * 0.32)

    def _start_tick(self):
        assert self.ticking == False
        self.ticking = True
        self.ticktask = asyncio.create_task(self._tick_generator)

    def _stop_tick(self):
        if self.ticking:
            self.ticking = False
            self.ticktask.cancel()
            self.ticktask = None

    def send(self, packet):
        # Chunk logical packet into induvidual frames
        nframes = ((len(packet) - 1 + 3) // (self.mtu - 5)) + 1

        tgt_len = (len(packet) - 1 + 3) // nframes + (5 + 1)

        offset = 0
        while(offset < len(packet)):
            frame, offset = _createFrame(packet, offset, tgt_len)
            # Unsent packets have last sent at time 0 (1970)
            self.send_queue.add((0, Frame(frame)))

        self._tick()

    # Bypasses all encryption and reordering
    def sendRaw(self, pipe, data):
        nframe = bytes(2 + length)
        struct.pack_into('<BBs' + str(len(data)), nframe, 0,
                         ChunkType.TYPE_UDP_PIPE, pipe, data)
        self.udp.sendto(nframe)

    def _receive_mgmt_dc(self, data):
        return (EmittedType.CONN_FAIL)

    def _receive_mgmt_hello(self, data):
        # The login packet
        if(data == self.magic):
            return None
        else:
            return (EmittedType.CONN_FAIL)

    def _receive_mgmt_ping(self, data):
        if(len(self.data) != 64):
            return (EmittedType.CONN_FAIL)
        self._sendPong(data)
        return None

    def _receive_mgmt_pong(self, data):
        self.last_pong = time.time()
        return None

    def _receive_pkt_content(self, data):
        self.recv_queue.setdefault(data)


        
    def _receive_pkt_ack(self, data):
        for packet in self.send_queue:
            #Check seq
            if packet[1].seq == data[1:5]:
                #Time since last re-transmit
                packet_ping = time.time() - packet[0]

                #Update running average
                self.ping = 0.95*self.ping + 0.05 * packet_ping
                self.ping2 = 0.95*self.ping2 + 0.05 * (packet_ping**2)

                #Remove from queue
                self.send_queue.discard(packet)
                break



    def _receive_pkt_fec(self, data):
        return None

    def _receive_udp_pipe(self, data):
        pass

    # Infinte async iterable
    # Returns when connection closed or errored
    async def receive(self):
        try:
            #Wait for magic handshake
            data, addr = await self.udp.recvfrom()
            if(addr == self.addr):
                if data[0] == ChunkType.TYPE_MGMT_HELLO and data[1:] == self.magic:
                    # Activation complete
                    yield (EmittedType.CONN_SUCCESS)
                else:
                    yield (EmittedType.CONN_FAIL)
                    # Exit the generator
                    return

            #Decoder for other packets
            decoder_table = {
                # Management packets
                ChunkType.TYPE_MGMT_DC: TransportUDP._receive_mgmt_dc,
                ChunkType.TYPE_MGMT_HELLO: TransportUDP._receive_mgmt_hello,

                ChunkType.TYPE_MGMT_PING: TransportUDP._receive_mgmt_ping,
                ChunkType.TYPE_MGMT_PONG: TransportUDP._receive_mgmt_pong,

                ChunkType.TYPE_PKT_SINGLE: TransportUDP._receive_pkt_single,
                ChunkType.TYPE_PKT_START: TransportUDP._receive_pkt_start,
                ChunkType.TYPE_PKT_END: TransportUDP._receive_pkt_end,
                ChunkType.TYPE_PKT_MID: TransportUDP._receive_pkt_mid,

                # For TCP over UDP control
                ChunkType.TYPE_PKT_ACK: TransportUDP._receive_pkt_ack,
                ChunkType.TYPE_PKT_FEC: TransportUDP._receive_pkt_fec,

                # For
                ChunkType.TYPE_UDP_PIPE: TransportUDP._receive_udp_pipe,
            }

            #Receive loop
            while True:
                data, addr = await self.udp.recvfrom()
                if(addr == self.addr):
                    pkt_type = data[0]

                    if pkt_type in decoder_table:
                        res = decoder_table[pkt_type](self, data)
                        if res is not None:
                            yield res

                else:
                    print("Unexpected packet")
        finally:
            # Stop tick timer
            self._stop_tick()
            # Close socket
            self.udp.close()
