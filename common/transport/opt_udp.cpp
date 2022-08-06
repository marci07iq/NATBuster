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
            Crypto::random(pkt->content.ping.bytes, 64);
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

            packet_decoder* pkt = packet_decoder::view(packet);
            packet_decoder::PacketType type = pkt->type;

            _reassemble_list.push_back(std::move(packet));

            //End of frame
            if (type == packet_decoder::PacketType::PKT_END || type == packet_decoder::PacketType::PKT_SINGLE) {

                uint32_t new_len = 0;
                for (auto& it2 : _reassemble_list) {
                    new_len += it2.size() - 5;
                }

                uint8_t* buffer = Utils::Blob::alloc(new_len);

                //Write progress
                uint32_t progress = 0;

                for (auto& it2 : _reassemble_list) {
                    Utils::Blob::bufcpy(buffer, new_len, progress, it2.getr(), it2.size(), 5, it2.size() - 5);
                    progress += it2.size() - 5;
                }

                assert(progress == new_len);

                Utils::Blob assembled = Utils::Blob::factory_consume(buffer, new_len);

                _callback_packet(assembled);
                _reassemble_list.clear();
            }

            _rx_seq += packet_decoder::seq_rt_cnt;
        } while (true);
    }

    void OPTUDP::on_open() {
        _callback_open();
    }

    void OPTUDP::on_receive(const Utils::ConstBlobView& data_ref) {

        Time::time_type_us now = Time::now();

        if (data_ref.size() < 1) {
            close();
            return;
        }

        const packet_decoder* pkt = packet_decoder::cview(data_ref);


        switch ((pkt->type)) {
            //Shouldnt get hello once connection up, but a few may still be in pipe
            //Ignore
        case packet_decoder::PacketType::MGMT_HELLO:
            break;
            //Ping: reply with pong
        case packet_decoder::PacketType::MGMT_PING:
            if (data_ref.size() == 65) {
                send_pong(data_ref);
            }
            else {
                _callback_error(ErrorCode::OPT_UDP_INVALID_PING);
            }
            break;
            //Pong: update pong timer
        case packet_decoder::PacketType::MGMT_PONG:
            _last_pong_in = now;
            reset_pong_timeout_timer();
            break;
            //ACK: remove from re-transmit queue, update ping time estiamte
        case packet_decoder::PacketType::PKT_ACK:
        {
            if (data_ref.size() != 5) {
                close();
                return;
            }

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
                        _ping += _settings.ping_average_weight * (tof / 1000000.f - _ping);
                        _ping2 += _settings.ping_average_weight * ((tof / 1000000.f) * (tof / 1000000.f) - _ping2);
                        //Remove from re-transmit system
                        auto it2 = it++;
                        _transmit_queue.erase(it2);
                    }
                    else {
                        ++it;
                    }
                }
            }
            reset_retransmit_timer();
        }
        break;
        //Data packet
        case packet_decoder::PacketType::PKT_MID:
        case packet_decoder::PacketType::PKT_START:
        case packet_decoder::PacketType::PKT_END:
        case packet_decoder::PacketType::PKT_SINGLE:
        {
            if (data_ref.size() < 5) {
                close();
                return;
            }

            //Send ack
            {
                Utils::Blob ack_packet = Utils::Blob::factory_empty(5);
                packet_decoder* ack_view = packet_decoder::view(ack_packet);
                ack_view->type = packet_decoder::PKT_ACK;
                ack_view->content.ack.seq = pkt->content.packet.seq;
                _socket->send(ack_packet);
            }

            //Safe even at roll-around
            int32_t advance = pkt->content.packet.seq - _rx_seq;
            if (advance >= 0) {
                Utils::Blob data = Utils::Blob::factory_empty(data_ref.size());
                data.copy_from(data_ref);

                _receive_map.insert({ pkt->content.packet.seq, std::move(data) });
                try_reassemble();
            }
        }
        break;

        case packet_decoder::PacketType::UDP_PIPE:
            _callback_raw(data_ref);
            break;
        }
    }

    void OPTUDP::on_error(ErrorCode code) {
        _callback_error(code);
    }

    void OPTUDP::on_ping_timer() {
        std::cout << "Sending ping" << std::endl;
        //Send a ping
        send_ping();
        //Set next delay
        _emitter->add_delay(new Utils::MemberWCallback<OPTUDP, void>(weak_from_this(), &OPTUDP::on_ping_timer), _settings.ping_interval);
    }

    void OPTUDP::on_close() {
        _callback_close();
    }

    void OPTUDP::reset_retransmit_timer() {
        //Time for the next scheduled re-transmit
        std::lock_guard _lg(_tx_lock);
        _emitter->cancel_timer(_retransmit_timer);

        if (_transmit_queue.size()) {
            packet_decoder* pkt = packet_decoder::view(_transmit_queue.front().packet);
            uint32_t old_rt = (pkt->content.packet.seq & packet_decoder::seq_rt_mask);
            Time::time_type_us next_transmit = _transmit_queue.front().transmits[old_rt] + next_retransmit_delta();
            _retransmit_timer = _emitter->add_timer(new Utils::MemberWCallback<OPTUDP, void>(weak_from_this(), &OPTUDP::on_retransmit_timer), next_transmit);
        }
        
    }
    void OPTUDP::reset_pong_timeout_timer() {
        std::lock_guard _lg(_tx_lock);
        _emitter->cancel_timer(_pong_timeout_timer);
        _pong_timeout_timer = _emitter->add_timer(new Utils::MemberWCallback<OPTUDP, void>(weak_from_this(), &OPTUDP::on_pong_timeout_timer), _last_pong_in + _settings.max_pong);
    }

    void OPTUDP::on_retransmit_timer() {
        //Retransmits
        {
            std::lock_guard _lg(_tx_lock);

            Time::time_type_us now = Time::now();
            Time::time_delta_type_us retransmit_delta = next_retransmit_delta();

            //Retransmit all outdated packets
            if (_transmit_queue.size()) {
                while (true) {
                    packet_decoder* pkt = packet_decoder::view(_transmit_queue.front().packet);
                    uint32_t old_rt = (pkt->content.packet.seq & packet_decoder::seq_rt_mask);
                    if (now < _transmit_queue.front().transmits[old_rt] + retransmit_delta) {
                        break;
                    }
                    //Advance to next re-transmit number
                    uint32_t new_rt = ((old_rt + 1) & packet_decoder::seq_rt_mask);
                    pkt->content.packet.seq = (pkt->content.packet.seq & packet_decoder::seq_num_mask) | new_rt;
                    assert((pkt->content.packet.seq & packet_decoder::seq_rt_mask) == new_rt);

                    //Expand re-transmit time storage
                    if (_transmit_queue.front().transmits.size() <= new_rt) {
                        _transmit_queue.front().transmits.push_back(now);
                        assert(_transmit_queue.front().transmits.size() == new_rt + 1);
                    }
                    else {
                        _transmit_queue.front().transmits[new_rt] = now;
                    }
                    assert(_transmit_queue.front().transmits[new_rt] == now);

                    //Send packet
                    _socket->send(_transmit_queue.front().packet);

                    //Move to back
                    _transmit_queue.push_back(std::move(_transmit_queue.front()));
                    _transmit_queue.pop_front();
                }
            }
        }

        reset_retransmit_timer();
    }
    void OPTUDP::on_pong_timeout_timer() {
        //Pong expired
        if (_last_pong_in + _settings.max_pong < Time::now()) {
            //Time to shut down
            _socket->close();
        }

        reset_pong_timeout_timer();
    }

    OPTUDP::OPTUDP(
        bool is_client,
        std::shared_ptr<Utils::EventEmitter> emitter,
        Network::UDPHandleS socket,
        OPTUDPSettings settings
    ) : OPTBase(is_client),
        _emitter(emitter),
        _socket(socket),
        _settings(settings) {

    }

    void OPTUDP::start() {
        std::cout << "OPT UDP Starting" << std::endl;

        //Set callbacks
        _socket->set_callback_start(new Utils::MemberWCallback<OPTUDP, void>(weak_from_this(), &OPTUDP::on_open));
        _socket->set_callback_packet(new Utils::MemberWCallback<OPTUDP, void, const Utils::ConstBlobView&>(weak_from_this(), &OPTUDP::on_receive));
        _socket->set_callback_error(new Utils::MemberWCallback<OPTUDP, void, ErrorCode>(weak_from_this(), &OPTUDP::on_error));
        _socket->set_callback_close(new Utils::MemberWCallback<OPTUDP, void>(weak_from_this(), &OPTUDP::on_close));

        //Start pinging
        _emitter->add_delay(new Utils::MemberWCallback<OPTUDP, void>(weak_from_this(), &OPTUDP::on_ping_timer), _settings.ping_interval);

        _socket->start();
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
                _socket->send(_transmit_queue.back().packet);
            }
        }
        reset_retransmit_timer();
    }
    void OPTUDP::sendRaw(const Utils::ConstBlobView& data) {
        //1 byte header
        Utils::Blob packet = Utils::Blob::factory_empty(data.size() + 1);

        packet_decoder* pview = packet_decoder::view(packet);

        pview->type = packet_decoder::PacketType::UDP_PIPE;

        packet.copy_from(data, 1);

        std::lock_guard _lg(_tx_lock);

        _socket->send(packet);
    }

    void OPTUDP::close() {
        _socket->close();
    }
};