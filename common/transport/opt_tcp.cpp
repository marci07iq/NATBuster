#include <cassert>
#include <queue>
#include <set>
#include <vector>

#include "opt_tcp.h"
#include "../network/network.h"
#include "../utils/random.h"

namespace NATBuster::Transport {
    void OPTTCP::on_open() {
        std::cout << "ASD" << std::endl;
        _callback_open();
    }

    void OPTTCP::on_receive(const Utils::ConstBlobView& data_ref) {
        Utils::Blob data = Utils::Blob::factory_empty(data_ref.size());
        data.copy_from(data_ref);
        _reassamble_total_len += data.size();
        _reassamble_list.push_back(std::move(data));

        while (_reassamble_list.size() > 0) {
            //Now try to reassamble
            Utils::Blob next_header;

            //Peek at next header
            if (!read_from_reassamble_list(next_header, 5, true)) {
                goto exit_loop;
            }

            const packet_decoder* decode = packet_decoder::cview(next_header);

            Utils::Blob content;

            //Prevent the rx buffer getting too full
            if (decode->len < max_packet_size) {
                switch (decode->type)
                {
                case packet_decoder::PKT_NORMAL:
                    //Read out 5 byte header and content
                    if (read_from_reassamble_list(content, 5 + decode->len, false)) {
                        //Cut off the header, and send to user
                        _callback_packet(content.cslice_right(5));
                    }
                    else {
                        goto exit_loop;
                    }
                    break;
                case packet_decoder::PKT_RAW:
                    //Read out 5 byte header and content
                    if (read_from_reassamble_list(content, 5 + decode->len, false)) {
                        //Cut off the header, and send to user
                        _callback_raw(content.cslice_right(5));
                    }
                    else {
                        goto exit_loop;
                    }
                    break;
                default:
                    close();
                    goto exit_loop;
                    break;
                }
            }
            else {
                close();
                goto exit_loop;
                break;
            }
        }
    exit_loop:
        return;

    }

    void OPTTCP::on_error(ErrorCode code) {
        _callback_error(code);
    }

    void OPTTCP::on_close() {
        _callback_close();
    }

    OPTTCP::OPTTCP(
        bool is_client,
        std::shared_ptr<Utils::EventEmitter> emitter,
        Network::TCPCHandleS socket
    ) : OPTBase(is_client),
        _emitter(emitter),
        _socket(socket)
    {

    }

    void OPTTCP::start() {
        //Set callbacks
        if (_is_client) {
            _socket->set_callback_connect(new Utils::MemberWCallback<OPTTCP, void>(weak_from_this(), &OPTTCP::on_open));
        }
        else {
            _socket->set_callback_start(new Utils::MemberWCallback<OPTTCP, void>(weak_from_this(), &OPTTCP::on_open));
        }
        _socket->set_callback_packet(new Utils::MemberWCallback<OPTTCP, void, const Utils::ConstBlobView&>(weak_from_this(), &OPTTCP::on_receive));
        _socket->set_callback_error(new Utils::MemberWCallback<OPTTCP, void, ErrorCode>(weak_from_this(), &OPTTCP::on_error));
        _socket->set_callback_close(new Utils::MemberWCallback<OPTTCP, void>(weak_from_this(), &OPTTCP::on_close));

        _socket->start();
    }

    void OPTTCP::send(const Utils::ConstBlobView& data) {
        Utils::Blob wrapped = Utils::Blob::factory_empty(5 + data.size());

        packet_decoder* header = packet_decoder::view(wrapped);
        header->type = packet_decoder::PKT_NORMAL;
        header->len = data.size();

        wrapped.copy_from(data, 5);

        std::lock_guard _lg(_out_lock);

        _socket->send(wrapped);
    }
    void OPTTCP::sendRaw(const Utils::ConstBlobView& data) {

        Utils::Blob wrapped = Utils::Blob::factory_empty(5 + data.size());

        packet_decoder* header = packet_decoder::view(wrapped);
        header->type = packet_decoder::PKT_RAW;
        header->len = data.size();
        wrapped.copy_from(data, 5);

        std::lock_guard _lg(_out_lock);

        _socket->send(wrapped);
    }

    void OPTTCP::close() {
        _socket->close();
    }
};