#include <cassert>
#include <queue>
#include <set>
#include <vector>

#include "opt_udp.h"
#include "../network/network.h"
#include "../utils/random.h"

namespace NATBuster::Common::Transport {
    void OPTUDP::send_ping() {
        Utils::Blob ping = Utils::Blob::factory_empty(65);

        packet_decoder* pkt = packet_decoder::view(ping);
        pkt->type = packet_decoder::PacketType::MGMT_PING;
        for (int i = 0; i < 64; i++) {
            Random::fast_fill(pkt->content.ping.bytes, 64);
        }

        _socket->send(ping);
    }

    void OPTUDP::send_pong(const Utils::ConstBlobView& ping_packet) {
        Utils::Blob pong = Utils::Blob::factory_copy(ping_packet, 0, 0);

        packet_decoder* pkt = packet_decoder::view(pong);
        pkt->type = packet_decoder::PacketType::MGMT_PONG;

        _socket->send(pong);
    }

    void OPTUDP::try_reassemble() {
        do {
            auto it = _receive_map.find(_rx_seq);
            if (it == _receive_map.end()) break;

            //Got it, move to reassembly queue
            Utils::Blob packet = std::move(it->second);
            _receive_map.erase(it);

            _reassemble_list.push_back(std::move(packet));

            packet_decoder* pkt = packet_decoder::view(packet);
            //End of frame
            if (pkt->type == packet_decoder::PacketType::PKT_END || pkt->type == packet_decoder::PacketType::PKT_SINGLE) {

                uint32_t new_len = 0;
                for (auto& it : _reassemble_list) {
                    new_len += it.size();
                }

                uint8_t* buffer = Utils::Blob::alloc(new_len);

                //Write progress
                uint32_t progress = 0;

                for (auto& it : _reassemble_list) {
                    Utils::Blob::bufcpy(buffer, new_len, progress, it.getr(), it.size(), 0, it.size());
                    progress += it.size();
                }

                Utils::Blob assembled = Utils::Blob::factory_consume(buffer, new_len);

                _result_callback(assembled);
                _reassemble_list.clear();
            }

            _rx_seq += packet_decoder::seq_rt_cnt;
        } while (true);
    }

    void OPTUDP::on_open() {
        _open_callback();
    }

    void OPTUDP::on_receive(Utils::Void data) {
        Utils::Blob packet;
        //Packet not neccessarily really available
        bool res = _socket->readFilter(packet, _settings._max_mtu * 2);

        if (res) {
            Time::time_type_us now = Time::now();
            packet_decoder* pkt = packet_decoder::view(packet);

            switch ((pkt->type)) {
                //Shouldnt get hello once connection up, but a few may still be in pipe
                //Ignore
            case packet_decoder::PacketType::MGMT_HELLO:
                break;
                //Ping: reply with pong
            case packet_decoder::PacketType::MGMT_PING:
                if (packet.size() == 65) {
                    send_pong(packet);
                }
                else {
                    _error_callback();
                }
                break;
                //Pong: update pong timer
            case packet_decoder::PacketType::MGMT_PONG:
                _last_pong_in = now;
                break;
                //ACK: remove from re-transmit queue, update ping time estiamte
            case packet_decoder::PacketType::PKT_ACK:
            {
                std::lock_guard _lg(_tx_lock);
                uint32_t seq = pkt->content.packet.seq;
                auto it = _transmit_queue.begin();
                while (it != _transmit_queue.end()) {
                    if (
                        //Check correct SEQ number
                        (packet_decoder::view(it->packet)->content.packet.seq & packet_decoder::seq_num_mask) ==
                        (seq & packet_decoder::seq_num_mask) &&
                        //Actually existing re-transmit attempt
                        ((seq & packet_decoder::seq_rt_mask) < it->transmits.size())
                        ) {
                        //Time of flight
                        Time::time_delta_type_us tof = now - it->transmits[seq & packet_decoder::seq_rt_mask];
                        //Update ping
                        _ping += _settings.ping_average_weight * (tof / 1000000. - _ping);
                        _ping2 += _settings.ping_average_weight * ((tof / 1000000.) * (tof / 1000000.) - _ping2);
                        //Remove from re-transmit system
                        auto it2 = it++;
                        _transmit_queue.erase(it2);
                    }
                    else {
                        ++it;
                    }
                }
            }
            //Data packet
            case packet_decoder::PacketType::PKT_MID:
            case packet_decoder::PacketType::PKT_START:
            case packet_decoder::PacketType::PKT_END:
            case packet_decoder::PacketType::PKT_SINGLE:
                //Safe even at roll-around
            {
                int32_t advance = pkt->content.packet.seq - _rx_seq;
                if (advance >= 0) {
                    _receive_map.insert({ pkt->content.packet.seq, std::move(packet) });
                    try_reassemble();
                }
            }
            break;

            case packet_decoder::PacketType::UDP_PIPE:
                _raw_callback(packet);
                break;
            }
        }

        on_housekeeping();
    }

    void OPTUDP::on_error() {
        _error_callback();
    }

    void OPTUDP::on_ping_timer() {
        //Send a ping
        send_ping();
        //Set next delay
        _source->addDelay(new Utils::MemberCallback<OPTUDP, void>(weak_from_this(), &OPTUDP::on_ping_timer), _settings.ping_interval);
    }

    void OPTUDP::on_floating_timer() {
        if (_internal_floating <= Time::now()) {
            on_housekeeping();
        }

        if (_external_floating.cb.has_function() && _external_floating.dst <= Time::now()) {
            _external_floating.cb.call_and_clear();
        }
    }

    void OPTUDP::on_close() {
        _close_callback();
    }

    void OPTUDP::on_housekeeping() {
        //Retransmits
        {
            std::lock_guard _lg(_tx_lock);

            Time::time_type_us now = Time::now();
            Time::time_delta_type_us retransmit_delta = next_retransmit_delta();

            //Retransmit all outdated packets
            if (_transmit_queue.size()) {
                packet_decoder* pkt = packet_decoder::view(_transmit_queue.front().packet);
                uint32_t old_rt = (pkt->content.packet.seq & packet_decoder::seq_rt_mask);
                while (_transmit_queue.front().transmits[old_rt] + retransmit_delta < now) {
                    //Advance to next re-transmit number
                    uint32_t new_rt = ((old_rt + 1) & packet_decoder::seq_rt_mask);
                    pkt->content.packet.seq = (pkt->content.packet.seq & packet_decoder::seq_num_mask) | new_rt;

                    //Expand re-transmit time storage
                    if (_transmit_queue.front().transmits.size() <= new_rt) {
                        _transmit_queue.front().transmits.push_back(now);
                        assert(_transmit_queue.front().transmits.size() == new_rt);
                    }
                    else {
                        _transmit_queue.front().transmits[new_rt] = now;
                    }

                    //Send packet
                    _socket->send(_transmit_queue.front().packet);

                    //Move to back
                    _transmit_queue.push_back(std::move(_transmit_queue.front()));
                    _transmit_queue.pop_front();
                }
            }
        }

        //Pong expired
        if (_last_pong_in + _settings.max_pong < Time::now()) {
            //Time to shut down
            _source->close();
        }

        _source->updateFloatingNext(new Utils::MemberCallback<OPTUDP, void>(weak_from_this(), &OPTUDP::on_floating_timer), next_floating_time());
    }

    OPTUDP::OPTUDP(
        bool is_client,
        Network::UDPHandle socket,
        OPTUDPSettings settings
    ) : OPTBase(is_client),
        _socket(socket),
        _source(std::make_shared<Utils::PollEventEmitter<Network::UDPHandle, Utils::Void>>(socket)),
        _settings(settings) {

    }

    OPTUDPHandle OPTUDP::create(
        bool is_client,
        Network::UDPHandle socket,
        OPTUDPSettings settings
    ) {
        return std::shared_ptr<OPTUDP>(new OPTUDP(is_client, socket, settings));
    }

    void OPTUDP::start() {
        //Set callbacks
        _source->set_open_callback(new Utils::MemberCallback<OPTUDP, void>(weak_from_this(), &OPTUDP::on_open));
        _source->set_result_callback(new Utils::MemberCallback<OPTUDP, void, Utils::Void>(weak_from_this(), &OPTUDP::on_receive));
        _source->set_error_callback(new Utils::MemberCallback<OPTUDP, void>(weak_from_this(), &OPTUDP::on_error));
        _source->set_close_callback(new Utils::MemberCallback<OPTUDP, void>(weak_from_this(), &OPTUDP::on_close));

        //Start pinging
        _source->addDelay(new Utils::MemberCallback<OPTUDP, void>(weak_from_this(), &OPTUDP::on_ping_timer), _settings.ping_interval);

        _source->start();
    }

    //Add a callback that will be called in `delta` time, if the emitter is still running
    //There is no way to cancel this call
    //Only call from callbacks, or before start
    inline void OPTUDP::addDelay(Utils::Timers::TimerCallback::raw_type cb, Time::time_delta_type_us delta) {
        _source->addDelay(cb, delta);
    }

    //Add a callback that will be called at time `end`, if the emitter is still running
    //There is no way to cancel this call
    //Only call from callbacks, or before start
    inline void OPTUDP::addTimer(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end) {
        _source->addTimer(cb, end);
    }

    void OPTUDP::updateFloatingNext(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end) {
        _external_floating.cb = cb;
        _external_floating.dst = end;

        _source->updateFloatingNext(new Utils::MemberCallback<OPTUDP, void>(weak_from_this(), &OPTUDP::on_floating_timer), next_floating_time());
    }

    void OPTUDP::send(const Utils::ConstBlobView& data) {
        //Type and seq
        uint32_t max_packet_length = _settings._max_mtu - 5;

        uint32_t packets_left = (data.size() - 1) / max_packet_length + 1;

        uint32_t progress = 0;

        uint32_t seq = next_seq(packets_left);

        while (progress < data.size()) {
            uint32_t data_left = data.size() - progress;
            uint32_t packet_data = (data_left - 1) / (packets_left)+1;

            Utils::Blob packet = Utils::Blob::factory_empty(5 + packet_data);

            packet.copy_from(data.cslice(progress, packet_data), 5);

            packet_decoder* pview = packet_decoder::view(packet);

            pview->type = ((progress == 0) ?
                ((packet_data == data_left) ? packet_decoder::PacketType::PKT_SINGLE : packet_decoder::PacketType::PKT_START) :
                ((packet_data == data_left) ? packet_decoder::PacketType::PKT_END : packet_decoder::PacketType::PKT_MID));

            pview->content.packet.seq = seq;
            ++seq;
            packets_left--;
            progress += packet_data;

            transmit_packet tx_packet;
            tx_packet.transmits = std::vector<Time::time_type_us>({ Time::now() });
            tx_packet.packet = std::move(packet);

            //Can't have ACK arrive before the packet was added to the queue
            {
                std::lock_guard _lg(_tx_lock);
                _transmit_queue.push_back(std::move(tx_packet));
            }
            _socket->send(packet);
        }

    }
    void OPTUDP::sendRaw(const Utils::ConstBlobView& data) {
        //1 byte header
        Utils::Blob packet = Utils::Blob::factory_empty(data.size() + 1);

        packet_decoder* pview = packet_decoder::view(packet);

        pview->type = packet_decoder::PacketType::UDP_PIPE;

        packet.copy_from(data, 1);

        _socket->send(packet);
    }

    void OPTUDP::close() {
        _source->close();
    }
};