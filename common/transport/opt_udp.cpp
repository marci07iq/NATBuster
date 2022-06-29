#include <cassert>
#include <queue>
#include <set>
#include <vector>

#include "opt_udp.h"
#include "../utils/random.h"

namespace NATBuster::Common::Transport {
    OPTUDP::OPTUDP(
        OPTPacketCallback packet_callback,
        OPTRawCallback raw_callback,
        OPTErrorCallback error_callback,
        OPTClosedCallback closed_callback,
        Network::UDPHandle socket,
        Network::Packet magic,
        OPTUDPSettings settings
    ) : OPTBase(packet_callback, raw_callback, error_callback, closed_callback),
        _socket(socket),
        _magic(magic),
        _settings(settings) {

    }

    void OPTUDP::loop(std::weak_ptr<OPTUDP> emitter_w) {
        //By the time shutdown callback is called, the object could be dead
        //So copy it out now
        OPTClosedCallback closed_callback_copy;

        Time::time_type_us last_retransmit = Time::now();
        Time::time_type_us last_ping_out = Time::now();
        Time::time_type_us last_pong_in = Time::now();
        {
            std::shared_ptr<OPTUDP> emitter = emitter_w.lock();
            if (!emitter) return;
            closed_callback_copy = emitter->_closed_callback;
            last_retransmit = Time::now();
        }

        while (true) {
            //Check if object still exists
            std::shared_ptr<OPTUDP> emitter = emitter_w.lock();
            if (!emitter) break;

            //Check exit condition
            {
                std::lock_guard lg(emitter->_system_lock);
                //Check exit condition
                if (!emitter->_run) break;
            }

            //Validate socket
            if (!emitter->_socket->valid()) break;

            Time::time_type_us next_ping = last_ping_out + emitter->_settings.ping_interval;
            Time::time_type_us next_pong = last_pong_in + emitter->_settings.max_pong;
            Time::time_delta_type_us retransmit_delta = emitter->next_retransmit_delta();

            Time::time_type_us now = Time::now();

            //Need to send a new ping
            if (next_ping < now) {
                emitter->send_ping();
            }

            //Should have received a pong
            if (next_pong < now) {
                emitter->_socket->close();
                break;
            }

            {
                std::lock_guard _lg(emitter->_tx_lock);
                //Retransmit all outdated packets
                if (emitter->_transmit_queue.size()) {
                    while (emitter->_transmit_queue.front().transmits.back() + retransmit_delta < now) {
                        //Advance to next re-transmit number
                        packet_decoder* pkt = packet_decoder::view(emitter->_transmit_queue.front().packet);
                        uint32_t new_rt = (((pkt->content.packet.seq & packet_decoder::seq_rt_mask) + 1) & packet_decoder::seq_rt_mask);
                        pkt->content.packet.seq = (pkt->content.packet.seq & packet_decoder::seq_num_mask) | new_rt;
                        if (emitter->_transmit_queue.front().transmits.size() <= new_rt) {
                            emitter->_transmit_queue.front().transmits.push_back(now);
                            assert(emitter->_transmit_queue.front().transmits.size() == new_rt);
                        }
                        else {
                            emitter->_transmit_queue.front().transmits[new_rt] = now;
                        }

                        //Send packet
                        emitter->_socket->send(emitter->_transmit_queue.front().packet);

                        //Move to back
                        emitter->_transmit_queue.push_back(emitter->_transmit_queue.front());
                        emitter->_transmit_queue.pop_front();
                    }
                }
            }

            //The above may have taken a while
            now = Time::now();

            Time::time_delta_type_us timeout_us = min(retransmit_delta, next_ping - now);

            Time::Timeout to(
                ((timeout_us < 0) ? 0 : timeout_us));

            //Find server with event
            Utils::EventResponse<Utils::Void> result = emitter->_socket->check(to);

            if (result.error()) {
                emitter->_error_callback();
            }
            if (result.ok()) {
                Network::Packet packet = emitter->_socket->readFilter(emitter->_settings._max_mtu * 2);

                now = Time::now();

                if (packet.size()) {
                    packet_decoder* pkt = packet_decoder::view(packet);

                    switch ((PacketType)(pkt->type)) {
                        //Shouldnt get hello once connection up, but a few may still be in pipe
                        //Ignore
                    case PacketType::MGMT_HELLO:
                        break;
                        //Ping: reply with pong
                    case PacketType::MGMT_PING:
                        if (packet.size() == 65) {
                            emitter->send_pong(packet);
                        }
                        else {
                            emitter->_error_callback();
                        }
                        break;
                        //Pong: update pong timer
                    case PacketType::MGMT_PONG:
                        last_pong_in = now;
                        break;
                        //ACK: remove from re-transmit queue, update ping time estiamte
                    case PacketType::PKT_ACK:
                    {
                        std::lock_guard _lg(emitter->_tx_lock);
                        uint32_t seq = pkt->content.packet.seq;
                        auto it = emitter->_transmit_queue.begin();
                        while (it != emitter->_transmit_queue.end()) {
                            if (
                                //Check correct SEQ number
                                (packet_decoder::view(it->packet)->content.packet.seq & packet_decoder::seq_num_mask) == (seq & packet_decoder::seq_num_mask) &&
                                //Actually existing re-transmit attempt
                                ((seq & packet_decoder::seq_rt_mask) < it->transmits.size())
                                ) {
                                //Time of flight
                                Time::time_delta_type_us tof = now - it->transmits[seq & packet_decoder::seq_rt_mask];
                                //Update ping
                                emitter->_ping += emitter->_settings.ping_average_weight * (tof / 1000000. - emitter->_ping);
                                emitter->_ping2 += emitter->_settings.ping_average_weight * ((tof / 1000000.) * (tof / 1000000.) - emitter->_ping2);
                                //Remove from re-transmit system
                                auto it2 = it++;
                                emitter->_transmit_queue.erase(it2);
                            }
                            else {
                                ++it;
                            }
                        }
                    }
                        //Data packet
                    case PacketType::PKT_MID:
                    case PacketType::PKT_START:
                    case PacketType::PKT_END:
                    case PacketType::PKT_SINGLE:
                        //Safe even at roll-around
                        int32_t advance = pkt->content.packet.seq - emitter->_rx_seq;
                        if (advance >= 0) {
                            emitter->_receive_map.insert({ pkt->content.packet.seq , packet });
                            emitter->try_reassemble();
                        }
                        break;

                    case PacketType::UDP_PIPE:
                        emitter->_raw_callback(packet);
                        break;
                    }
                }
            }

        }

        closed_callback_copy();
    }

    /*void OPTUDP::send_hello() {
        Network::Packet hello = Network::Packet::create_empty(_magic.size() + 1);

        packet_decoder* pkt = packet_decoder::view(hello);
        pkt->type = (uint8_t)PacketType::MGMT_HELLO;
        memcpy(pkt->content.ping.bytes, _magic.get(), _magic.size());

        _socket->send(hello);
    }*/

    void OPTUDP::send_ping() {
        Network::Packet ping = Network::Packet::create_empty(65);

        packet_decoder* pkt = packet_decoder::view(ping);
        pkt->type = (uint8_t)PacketType::MGMT_PING;
        for (int i = 0; i < 64; i++) {
            Random::fast_fill(pkt->content.ping.bytes, 64);
        }

        _socket->send(ping);
    }

    void OPTUDP::send_pong(Network::Packet ping) {
        Network::Packet pong = Network::Packet::copy_from(ping);

        packet_decoder* pkt = packet_decoder::view(pong);
        pkt->type = (uint8_t)PacketType::MGMT_PONG;

        _socket->send(pong);
    }

    void OPTUDP::try_reassemble() {

        do {
            auto it = _receive_map.find(_rx_seq);
            if (it == _receive_map.end()) break;

            //Got it, move to reassembly queue
            Network::Packet packet = it->second;
            _receive_map.erase(it);

            _reassamble_list.push_back(packet);

            packet_decoder* pkt = packet_decoder::view(packet);
            //End of frame
            if (pkt->type == (uint8_t)PacketType::PKT_END || pkt->type == (uint8_t)PacketType::PKT_SINGLE) {
                //Merge entries in the list
                uint32_t size = 0;
                for (auto&& it : _reassamble_list) {
                    size += (it.size() - 5); //1 type + 4 seq
                }
                Network::Packet assembled = Network::Packet::create_empty(size);
                size = 0;
                //Copy together
                for (auto&& it : _reassamble_list) {
                    memcpy(assembled.get() + size, it.get() + 5, it.size() - 5);
                    size += (it.size() - 5); //1 type + 4 seq
                }
                _packet_callback(assembled);
                _reassamble_list.clear();
            }

            _rx_seq += packet_decoder::seq_rt_cnt;
        } while (true);
    }

    Time::time_delta_type_us OPTUDP::next_retransmit_delta() {
        Time::time_delta_type_us res = 1000000 * (
            _settings.ping_rt_mul * _ping +
            _settings.jitter_rt_mul * sqrt(_ping2 - _ping * _ping)
            );
        //Sum 3ms re-transmit is just spamming
        return (res < 3000) ? 3000 : res;
    }

    //std::cout << "Listen collector exited" << std::endl;

    void OPTUDP::stop() {
        std::lock_guard _lg(_system_lock);
        _run = false;
        _socket->close();
    }

    void OPTUDP::send(Network::Packet packet) {
        std::lock_guard _lg(_tx_lock);

        
    }
    void OPTUDP::sendRaw(Network::Packet packet) {
        std::lock_guard _lg(_tx_lock);
    }
};