#include "opt_pipes.h"


namespace NATBuster::Common::Transport {
    OPTPipe::OPTPipe(
        bool is_client,
        std::shared_ptr<OPTPipes> underlying,
        uint32_t id) : OPTBase(is_client), _underlying(underlying), _id(id) {

    }

    std::shared_ptr<OPTPipe> OPTPipe::create(
        bool is_client,
        std::shared_ptr<OPTPipes> underlying,
        uint32_t id) {
        return std::shared_ptr<OPTPipe>(new OPTPipe(is_client, underlying, id));
    }

    void OPTPipe::start() {
        if (_open_state == OPTPipe::OpenState::None) {
            Utils::Blob req_packet = Utils::Blob::factory_empty(5);

            OPTPipes::packet_decoder* req_header = OPTPipes::packet_decoder::view(req_packet);
            req_header->type = OPTPipes::packet_decoder::MGMT_OPEN_REQ;
            req_header->id = _id;

            _open_state = OPTPipe::OpenRequsted;

            _underlying->_underlying->send(req_packet);
        }
        else if (_open_state == OPTPipe::OpenState::OpenRequsted) {
            Utils::Blob req_packet = Utils::Blob::factory_empty(5);

            OPTPipes::packet_decoder* req_header = OPTPipes::packet_decoder::view(req_packet);
            req_header->type = OPTPipes::packet_decoder::MGMT_OPEN_RESP;
            req_header->id = _id;

            _open_state = OPTPipe::Opened;

            _underlying->_underlying->send(req_packet);
        }
    }

    //Send ordered packet
    void OPTPipe::send(const Utils::ConstBlobView& packet) {
        Utils::Blob full_packet = Utils::Blob::factory_empty(5 + packet.size());

        OPTPipes::packet_decoder* header = OPTPipes::packet_decoder::view(full_packet);
        header->type = OPTPipes::packet_decoder::DATA;
        header->id = _id;

        full_packet.copy_from(packet, 5);

        _underlying->_underlying->send(full_packet);
    }
    //Send UDP-like packet (no order / arrival guarantee, likely faster)
    void OPTPipe::sendRaw(const Utils::ConstBlobView& packet) {
        Utils::Blob full_packet = Utils::Blob::factory_empty(5 + packet.size());

        OPTPipes::packet_decoder* header = OPTPipes::packet_decoder::view(full_packet);
        header->type = OPTPipes::packet_decoder::DATA;
        header->id = _id;

        full_packet.copy_from(packet, 5);

        _underlying->_underlying->sendRaw(full_packet);
    }

    //Add a callback that will be called in `delta` time, if the emitter is still running
        //There is no way to cancel this call
        //Only call from callbacks, or before start
    void OPTPipe::addDelay(Utils::Timers::TimerCallback::raw_type cb, Time::time_delta_type_us delta) {
        _underlying->addDelay(cb, delta);
    };

    //Add a callback that will be called at time `end`, if the emitter is still running
    //There is no way to cancel this call
    //Only call from callbacks, or before start
    void OPTPipe::addTimer(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end) {
        _underlying->addTimer(cb, end);
    };

    //Overwrite the next time the floating timer is fired
    //There is only one floating timer
    //Only call from callbacks, or before start
    void OPTPipe::updateFloatingNext(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end) {
        _floating.cb = cb;
        _floating.dst = end;

        _underlying->updateFloatingNext();
    };

    //Request closing the connection (gracefully if possible)
    //Close callback will be issued when that succeeded
    void OPTPipe::close() {
        //TODO: Multithreading
        if (_open_state != OPTPipe::CloseRequrested && _open_state != OPTPipe::Closed) {
            _open_state = OPTPipe::CloseRequrested;

            Utils::Blob req_packet = Utils::Blob::factory_empty(5);

            OPTPipes::packet_decoder* req_header = OPTPipes::packet_decoder::view(req_packet);
            req_header->type = OPTPipes::packet_decoder::MGMT_CLOSE;
            req_header->id = _id;

            _underlying->_underlying->send(req_packet);
        }
    }

    std::shared_ptr<Identity::User> OPTPipe::getUser() {
        return _underlying->_underlying->getUser();
    }

    //Called when a packet can be read
    void OPTPipes::on_open() {
        _open_callback();
    }
    //Called when a packet can be read
    void OPTPipes::on_packet(const Utils::ConstBlobView& data) {
        const OPTPipes::packet_decoder* header = OPTPipes::packet_decoder::cview(data);

        switch (header->type)
        {
        case packet_decoder::MGMT_OPEN_REQ:
            //Check if ID parity is correct
        if((header->id % 2 == 1) != _is_client) {
            std::unique_lock _lg(_pipe_lock);
            auto it = _pipes.find(header->id);
            //Make sure pipe doesn't exist
            if (it == _pipes.end()) {
                std::shared_ptr<OPTPipe> pipe = OPTPipe::create(false, shared_from_this(), header->id);
                _pipes.insert({ header->id, pipe });
                _lg.unlock();
                pipe->_open_state = OPTPipe::OpenRequsted;
                _result_callback(pipe);
            }
        }
        break;
        case packet_decoder::MGMT_OPEN_RESP:
            //Check ID parity
        if ((header->id % 2 == 1) == _is_client) {
            std::shared_lock _lg(_pipe_lock);
            auto it = _pipes.find(header->id);
            if (it != _pipes.end()) {
                if (it->second->_open_state == OPTPipe::OpenRequsted) {
                    it->second->_open_state = OPTPipe::Opened;
                    it->second->_open_callback();
                }
            }
        }
        break;
        case packet_decoder::MGMT_CLOSE:
        {
            std::unique_lock _lg(_pipe_lock);
            auto it = _pipes.find(header->id);
            if (it != _pipes.end()) {
                std::shared_ptr<OPTPipe> pipe = it->second;
                //Response to a close request
                if (pipe->_open_state == OPTPipe::CloseRequrested) {
                    pipe->_open_state = OPTPipe::Closed;
                    _pipes.erase(it);
                    _lg.unlock();
                    pipe->_close_callback();
                }
                //Incoming close request
                else if (pipe->_open_state != OPTPipe::CloseRequrested && pipe->_open_state != OPTPipe::Closed) {
                    pipe->_open_state = OPTPipe::Closed;
                    _pipes.erase(it);
                    _lg.unlock();
                    pipe->_close_callback();

                }
                
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
                if (pipe->_open_state == OPTPipe::Opened) {
                    const Utils::ConstBlobSliceView content = data.cslice_right(5);
                    pipe->_result_callback(content);
                }
            }
        }
        break;
        default:
            _underlying->close();
            break;
        }
    }
    //Called when a packet can be read
    void OPTPipes::on_raw(const Utils::ConstBlobView& data) {
        const OPTPipes::packet_decoder* header = OPTPipes::packet_decoder::cview(data);

        switch (header->type)
        {
        case packet_decoder::DATA:
        {
            std::shared_lock _lg(_pipe_lock);
            auto it = _pipes.find(header->id);
            if (it != _pipes.end()) {
                std::shared_ptr<OPTPipe> pipe = it->second;
                _lg.unlock();
                const Utils::ConstBlobSliceView content = data.cslice_right(5);
                pipe->_raw_callback(content);
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
    void OPTPipes::on_error() {
        std::shared_lock _lg(_pipe_lock);

        for (auto& it : _pipes) {
            it.second->_error_callback();
        }
    }
    //Socket was closed
    void OPTPipes::on_close() {
        std::shared_lock _lg(_pipe_lock);

        for (auto& it : _pipes) {
            it.second->_close_callback();
        }
    }


    OPTPipes::OPTPipes(bool is_client) : _is_client(is_client), _next_pipe_id(_is_client ? 1 : 0) {

    }

    void OPTPipes::start() {
        _underlying->set_open_callback(new Utils::MemberCallback<OPTPipes, void>(weak_from_this(), &OPTPipes::on_open));
        _underlying->set_packet_callback(new Utils::MemberCallback<OPTPipes, void, const Utils::ConstBlobView&>(weak_from_this(), &OPTPipes::on_packet));
        _underlying->set_raw_callback(new Utils::MemberCallback<OPTPipes, void, const Utils::ConstBlobView&>(weak_from_this(), &OPTPipes::on_raw));
        _underlying->set_error_callback(new Utils::MemberCallback<OPTPipes, void>(weak_from_this(), &OPTPipes::on_error));
        _underlying->set_close_callback(new Utils::MemberCallback<OPTPipes, void>(weak_from_this(), &OPTPipes::on_close));

        _underlying->start();
    }

    //Add a callback that will be called in `delta` time, if the emitter is still running
    //There is no way to cancel this call
    //Only call from callbacks, or before start
    inline void OPTPipes::addDelay(Utils::Timers::TimerCallback::raw_type cb, Time::time_delta_type_us delta) {
        _underlying->addDelay(cb, delta);
    }

    //Add a callback that will be called at time `end`, if the emitter is still running
    //There is no way to cancel this call
    //Only call from callbacks, or before start
    inline void OPTPipes::addTimer(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end) {
        _underlying->addTimer(cb, end);
    }

    //Overwrite the next time the floating timer is fired
    //There is only one floating timer
    //Only call from callbacks, or before start
    inline void OPTPipes::updateFloatingNext(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end) {
        _floating.cb = cb;
        _floating.dst = end;

        updateFloatingNext();
    }

    std::shared_ptr<OPTPipe> OPTPipes::openPipe() {
        std::shared_ptr<OPTPipe> pipe;

        {
            std::unique_lock _lg(_pipe_lock);

            uint32_t new_id = _next_pipe_id;
            _next_pipe_id += 2;

            pipe = OPTPipe::create(true, shared_from_this(), new_id);

            _pipes.insert({ new_id, pipe });
        }

        return pipe;
    }

    void OPTPipes::close() {
        _underlying->close();
    }

};