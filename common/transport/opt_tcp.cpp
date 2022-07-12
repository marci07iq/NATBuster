#include <cassert>
#include <queue>
#include <set>
#include <vector>

#include "opt_tcp.h"
#include "../network/network.h"
#include "../utils/random.h"

namespace NATBuster::Common::Transport {
    void OPTTCP::on_open() {
        _open_callback();
    }

    void OPTTCP::on_receive(Utils::Void data) {
        Utils::Blob packet;
        //Packet not neccessarily really available
        bool res = _socket->read(packet, 1, 8000);

        if (res) {
            _reassamble_list.push_back(std::move(packet));
            _reassamble_total_len += packet.size();

            while (_reassamble_list.size() > 0) {
                //Now try to reassamble
                Utils::Blob next_header;

                //Peek at next header
                read_from_reassamble_list(next_header, 5, true);

                const packet_decoder* decode = packet_decoder::cview(next_header);
                
                Utils::Blob content;

                switch (decode->type)
                {
                case packet_decoder::PKT_NORMAL:
                    //Read out 5 byte header and content
                    if (read_from_reassamble_list(content, 5 + decode->len, false)) {
                        //Cut off the header, and send to user
                        _result_callback(content.cslice_right(5));
                    }
                    break;
                case packet_decoder::PKT_RAW:
                    //Read out 5 byte header and content
                    if (read_from_reassamble_list(content, 5 + decode->len, false)) {
                        //Cut off the header, and send to user
                        _raw_callback(content.cslice_right(5));
                    }
                    break;
                default:
                    close();
                    break;
                }
            }
        }
    }

    void OPTTCP::on_error() {
        _error_callback();
    }

    void OPTTCP::on_close() {
        _close_callback();
    }

    OPTTCP::OPTTCP(
        bool is_client,
        Network::TCPCHandle socket
    ) : OPTBase(is_client),
        _socket(socket),
        _source(std::make_shared<Utils::PollEventEmitter<Network::TCPCHandle, Utils::Void>>(socket))
        {

    }

    OPTTCPHandle OPTTCP::create(
        bool is_client,
        Network::TCPCHandle socket) {
        return std::shared_ptr<OPTTCP>(new OPTTCP(is_client, socket));
    }

    void OPTTCP::start() {
        //Set callbacks
        _source->set_open_callback(new Utils::MemberCallback<OPTTCP, void>(weak_from_this(), &OPTTCP::on_open));
        _source->set_result_callback(new Utils::MemberCallback<OPTTCP, void, Utils::Void>(weak_from_this(), &OPTTCP::on_receive));
        _source->set_error_callback(new Utils::MemberCallback<OPTTCP, void>(weak_from_this(), &OPTTCP::on_error));
        _source->set_close_callback(new Utils::MemberCallback<OPTTCP, void>(weak_from_this(), &OPTTCP::on_close));

        _source->start();
    }
    
    //Add a callback that will be called in `delta` time, if the emitter is still running
    //There is no way to cancel this call
    //Only call from callbacks, or before start
    inline void OPTTCP::addDelay(Utils::Timers::TimerCallback::raw_type cb, Time::time_delta_type_us delta) {
        _source->addDelay(cb, delta);
    }

    //Add a callback that will be called at time `end`, if the emitter is still running
    //There is no way to cancel this call
    //Only call from callbacks, or before start
    inline void OPTTCP::addTimer(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end) {
        _source->addTimer(cb, end);
    }

    void OPTTCP::updateFloatingNext(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end) {
        _source->updateFloatingNext(cb, end);
    }

    void OPTTCP::send(const Utils::ConstBlobView& data) {
        Utils::Blob wrapped = Utils::Blob::factory_empty(5 + data.size());

        packet_decoder* header = packet_decoder::view(wrapped);
        header->type = packet_decoder::PKT_NORMAL;
        header->len = data.size();

        wrapped.copy_from(data, 5);

        _socket->send(wrapped);
    }
    void OPTTCP::sendRaw(const Utils::ConstBlobView& data) {
        Utils::Blob wrapped = Utils::Blob::factory_empty(5 + data.size());

        packet_decoder* header = packet_decoder::view(wrapped);
        header->type = packet_decoder::PKT_RAW;
        header->len = data.size();

        wrapped.copy_from(data, 5);

        _socket->send(wrapped);
    }

    void OPTTCP::close() {
        _source->close();
    }
};