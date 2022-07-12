#pragma once

#include <map>
#include <memory>


#include "opt_base.h"

namespace NATBuster::Common::Transport {
    class OPTPipes;

    class OPTPipe : public OPTBase, public std::enable_shared_from_this<OPTPipe> {
        std::shared_ptr<OPTPipes> _underlying;
        uint32_t _id;

        friend class OPTPipes;

        OPTPipe(
            bool is_client,
            uint32_t id) : OPTBase(is_client), _id(id) {

        }
    public:
        void start() {
            Utils::Blob req_packet = Utils::Blob::factory_empty(5);

            OPTPipes::packet_decoder* req_header = OPTPipes::packet_decoder::view(req_packet);
            req_header->type = OPTPipes::packet_decoder::MGMT_OPEN_REQ;
            req_header->id = _id;

            _underlying->_underlying->send(req_packet);
        }

        //Send ordered packet
        void send(const Utils::ConstBlobView& packet) {
            Utils::Blob full_packet = Utils::Blob::factory_empty(5 + packet.size());

            OPTPipes::packet_decoder* header = OPTPipes::packet_decoder::view(full_packet);
            header->type = OPTPipes::packet_decoder::DATA;
            header->id = _id;

            full_packet.copy_from(packet, 5);

            _underlying->_underlying->send(full_packet);
        }
        //Send UDP-like packet (no order / arrival guarantee, likely faster)
        void sendRaw(const Utils::ConstBlobView& packet) {
            Utils::Blob full_packet = Utils::Blob::factory_empty(5 + packet.size());

            OPTPipes::packet_decoder* header = OPTPipes::packet_decoder::view(full_packet);
            header->type = OPTPipes::packet_decoder::DATA;
            header->id = _id;

            full_packet.copy_from(packet, 5);

            _underlying->_underlying->sendRaw(full_packet);
        }
        //Close connection (gracefully) if possible
        void close() {
            _underlying->closePipe(_id);
        }
    };

    class OPTPipes : public std::enable_shared_from_this<OPTPipes> {
#pragma pack(push, 1)
        struct packet_decoder : Utils::NonStack {
            static const uint32_t seq_rt_mask = 0x3f;
            static const uint32_t seq_rt_cnt = 64;
            static const uint32_t seq_num_mask = ~seq_rt_mask;

            enum PacketType : uint8_t {
                //Request to open a new pipe with ID
                //Resposne comes via MGMT_OPEN_RESP or MGMT_CLOSE
                MGMT_OPEN_REQ = 1, //Followed by 4 byte pipe ID
                //Request to open a pipe with ID approved
                MGMT_OPEN_RESP = 2, //Followed by 4 byte pipe ID
                //Close the pipe with ID
                MGMT_CLOSE = 3, //Followed by 4 byte pipe ID

                DATA = 16, //Followed by 4 byte pipe ID, and data
            } type;
            uint32_t id;
            union {
                struct { uint8_t data[1]; } data;
            } content;

            //Need as non const ref, so caller must maintain ownership of Packet
            static inline packet_decoder* view(Utils::BlobView& packet) {
                return (packet_decoder*)(packet.getw());
            }

            static inline const packet_decoder* cview(const Utils::ConstBlobView& packet) {
                return (const packet_decoder*)(packet.getr());
            }
        };

        static_assert(offsetof(packet_decoder, type) == 0);
        static_assert(offsetof(packet_decoder, id) == 1);
        static_assert(offsetof(packet_decoder, content.data.data) == 5);
#pragma pack(pop)


        std::shared_ptr<OPTBase> _underlying;

        std::map<uint32_t, std::shared_ptr<OPTPipe>> _pipes;
        bool _is_client;
        uint32_t _next_pipe_id;
        std::mutex _pipe_lock;

        friend class OPTPipe;

        //Called when a packet can be read
        void on_packet(const Utils::ConstBlobView& data) {
            const OPTPipes::packet_decoder* header = OPTPipes::packet_decoder::cview(data);

            switch (header->type)
            {
            case packet_decoder::MGMT_OPEN_REQ:
            {
                std::unique_lock _lg(_pipe_lock);
                auto it = _pipes.find(header->id);
                if (it == _pipes.end()) {
                    Utils::Blob resp_packet = Utils::Blob::factory_copy(data);

                    OPTPipes::packet_decoder* resp_header = OPTPipes::packet_decoder::view(resp_packet);
                    resp_header->type = packet_decoder::MGMT_OPEN_RESP;

                    _underlying->send(resp_packet);
                }
            }
            break;
            case packet_decoder::MGMT_OPEN_RESP:
            {
                std::unique_lock _lg(_pipe_lock);
                auto it = _pipes.find(header->id);
                if (it != _pipes.end()) {
                    //TODO:
                }
            }
            break;
            case packet_decoder::MGMT_CLOSE:
            {
                std::unique_lock _lg(_pipe_lock);
                auto it = _pipes.find(header->id);
                if (it != _pipes.end()) {
                    std::shared_ptr<OPTPipe> pipe = it->second;
                    _lg.unlock();
                    pipe->close();
                }
            }
            break;
            case packet_decoder::DATA:
            {
                std::unique_lock _lg(_pipe_lock);
                auto it = _pipes.find(header->id);
                if (it != _pipes.end()) {
                    std::shared_ptr<OPTPipe> pipe = it->second;
                    _lg.unlock();
                    const Utils::ConstBlobSliceView data = data.cslice_right(5);
                    pipe->_result_callback(data);
                }
            }
            break;
            default:
                _underlying->close();
                break;
            }
        }
        //Called when a packet can be read
        void on_raw(const Utils::ConstBlobView& data) {
            const OPTPipes::packet_decoder* header = OPTPipes::packet_decoder::cview(data);

            switch (header->type)
            {
            case packet_decoder::DATA:
            {
                std::unique_lock _lg(_pipe_lock);
                auto it = _pipes.find(header->id);
                if (it != _pipes.end()) {
                    std::shared_ptr<OPTPipe> pipe = it->second;
                    _lg.unlock();
                    const Utils::ConstBlobSliceView data = data.cslice_right(5);
                    pipe->_raw_callback(data);
                }
            }
            break;
            case packet_decoder::MGMT_OPEN_REQ:
            case packet_decoder::MGMT_OPEN_RESP:
            case packet_decoder::MGMT_CLOSE:
            default:
                _underlying->close();
                break;
            }
        }
        //Called when a socket error occurs
        void on_error() {
            std::lock_guard _lg(_pipe_lock);

            for (auto& it : _pipes) {
                it.second->_error_callback();
            }
        }
        //Socket was closed
        void on_close() {
            std::lock_guard _lg(_pipe_lock);

            for (auto& it : _pipes) {
                it.second->_close_callback();
            }
        }

    public:
        OPTPipes(bool is_client) : _is_client(is_client), _next_pipe_id(_is_client ? 1 : 0) {

        }

        void start() {
            _underlying->set_packet_callback(new Utils::MemberCallback<OPTPipes, void, const Utils::ConstBlobView&>(weak_from_this(), &OPTPipes::on_packet));
            _underlying->set_raw_callback(new Utils::MemberCallback<OPTPipes, void, const Utils::ConstBlobView&>(weak_from_this(), &OPTPipes::on_raw));
            _underlying->set_error_callback(new Utils::MemberCallback<OPTPipes, void>(weak_from_this(), &OPTPipes::on_error));
            _underlying->set_close_callback(new Utils::MemberCallback<OPTPipes, void>(weak_from_this(), &OPTPipes::on_close));

            _underlying->start();
        }

        void closePipe(uint32_t id) {
            std::unique_lock _lg(_pipe_lock);

            auto it = _pipes.find(id);
            if (it != _pipes.end()) {
                _pipes.erase(it);

                _lg.unlock();

                Utils::Blob close_packet = Utils::Blob::factory_empty(5);

                OPTPipes::packet_decoder* close_header = OPTPipes::packet_decoder::view(close_packet);
                close_header->type = OPTPipes::packet_decoder::MGMT_CLOSE;
                close_header->id = id;

                _underlying->send(close_packet);
            }
        }

        void closePipe(std::shared_ptr<OPTPipe> pipe) {
            closePipe(pipe->_id);
        }

        std::shared_ptr<OPTPipe> openPipe() {
            std::lock_guard _lg(_pipe_lock);

            uint32_t new_id = _next_pipe_id;
            _next_pipe_id += 2;

            std::shared_ptr<OPTPipe> pipe = std::make_shared<OPTPipe>(true, new_id);

            _pipes.insert({ new_id, pipe });

            return pipe;
        }
    };
};