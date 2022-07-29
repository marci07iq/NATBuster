#include "opt_pipes.h"


namespace NATBuster::Common::Transport {
    void OPTPipe::on_open_expired() {
        std::cout << "Pipe expiry CB" << std::endl;
        if (_open_state == OpenRequstSent) {
            std::cout << "Pipe expired" << std::endl;
            close();
        }
    }

    OPTPipe::OPTPipe(
        bool is_client,
        std::shared_ptr<OPTPipes> underlying,
        uint32_t id) : OPTBase(is_client), _underlying(underlying), _id(id), _open_state(OPTPipe::OpenState::None) {

    }

    std::shared_ptr<OPTPipe> OPTPipe::create(
        bool is_client,
        std::shared_ptr<OPTPipes> underlying,
        uint32_t id) {
        return std::shared_ptr<OPTPipe>(new OPTPipe(is_client, underlying, id));
    }

    void OPTPipe::erase() {
        //Remove form list
        {
            std::unique_lock _lg(_underlying->_pipe_lock);
            auto it = _underlying->_pipes.find(_id);
            if (it != _underlying->_pipes.end()) {
                _underlying->_pipes.erase(it);
            }
        }
    }

    void OPTPipe::start() {
        //An unopened client pipe
        if (_open_state == OPTPipe::OpenState::None) {
            Utils::Blob empty_data;
            start_client(empty_data);
        }
        //A server pipe, which received a request
        else if (_open_state == OPTPipe::OpenState::OpenRequstRecv) {
            Utils::Blob req_packet = Utils::Blob::factory_empty(5);

            OPTPipes::packet_decoder* req_header = OPTPipes::packet_decoder::view(req_packet);
            req_header->type = OPTPipes::packet_decoder::PIPE_OPEN_RESP;
            req_header->content.pipe.id = _id;

            _open_state = OPTPipe::Opened;

            _underlying->_underlying->send(req_packet);
        }
    }

    //Start a client pipe with additional data
    void OPTPipe::start_client(const Utils::ConstBlobView& data) {
        if (_open_state == OPTPipe::OpenState::None) {
            Utils::Blob req_packet = Utils::Blob::factory_empty(5 + data.size());

            //Create header
            OPTPipes::packet_decoder* req_header = OPTPipes::packet_decoder::view(req_packet);
            req_header->type = OPTPipes::packet_decoder::PIPE_OPEN_REQ;
            req_header->content.pipe.id = _id;

            //Additional request data
            req_packet.copy_from(data, 5);

            _open_state = OPTPipe::OpenRequstSent;

            _underlying->_underlying->send(req_packet);

            //Add a callback to close the connection if the open still hasnt been accepted
            addDelay(
                new Utils::MemberSCallback<OPTPipe, void>(
                    shared_from_this(), &OPTPipe::on_open_expired),
                10000000
            );
        }
    }

    //Send ordered packet
    void OPTPipe::send(const Utils::ConstBlobView& packet) {
        Utils::Blob full_packet = Utils::Blob::factory_empty(5 + packet.size());

        OPTPipes::packet_decoder* header = OPTPipes::packet_decoder::view(full_packet);
        header->type = OPTPipes::packet_decoder::PIPE_DATA;
        header->content.pipe.id = _id;

        full_packet.copy_from(packet, 5);

        std::lock_guard _lg(_underlying->_tx_lock);

        _underlying->_underlying->send(full_packet);
    }
    //Send UDP-like packet (no order / arrival guarantee, likely faster)
    void OPTPipe::sendRaw(const Utils::ConstBlobView& packet) {
        Utils::Blob full_packet = Utils::Blob::factory_empty(5 + packet.size());

        OPTPipes::packet_decoder* header = OPTPipes::packet_decoder::view(full_packet);
        header->type = OPTPipes::packet_decoder::PIPE_DATA;
        header->content.pipe.id = _id;

        full_packet.copy_from(packet, 5);

        std::lock_guard _lg(_underlying->_tx_lock);

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
        erase();

        if (_open_state != OPTPipe::Closed) {
            _open_state = OPTPipe::Closed;

            Utils::Blob req_packet = Utils::Blob::factory_empty(5);

            OPTPipes::packet_decoder* req_header = OPTPipes::packet_decoder::view(req_packet);
            req_header->type = OPTPipes::packet_decoder::PIPE_CLOSE;
            req_header->content.pipe.id = _id;

            _underlying->_underlying->send(req_packet);

            _close_callback();
        }
    }

    void OPTPipe::drop() {
        if (_open_state == OPTPipe::Opened || _open_state == OPTPipe::OpenRequstSent) {
            close();
        }
        else {
            erase();
        }
    }

    std::shared_ptr<Identity::User> OPTPipe::getUser() {
        return _underlying->getUser();
    }

    OPTPipe::~OPTPipe() {
        drop();
    }



    //Called when the underlying opens
    void OPTPipes::on_open() {
        _open_callback();
    }
    //Called when a packet can be read
    void OPTPipes::on_packet(const Utils::ConstBlobView& data) {
        const OPTPipes::packet_decoder* header = OPTPipes::packet_decoder::cview(data);
        const Utils::ConstBlobSliceView content = data.cslice_right(5);

        switch (header->type)
        {
            //Incoming request to open a new packet
        case packet_decoder::PIPE_OPEN_REQ:
            //Check if ID parity is correct
            if ((header->content.pipe.id % 2 == 1) != _is_client) {
                std::unique_lock _lg(_pipe_lock);
                auto it = _pipes.find(header->content.pipe.id);
                //Make sure pipe doesn't exist
                if (it == _pipes.end()) {
                    //Create a new pipe
                    std::shared_ptr<OPTPipe> pipe = OPTPipe::create(false, shared_from_this(), header->content.pipe.id);
                    pipe->_open_state = OPTPipe::OpenRequstRecv;
                    _pipes.insert({ header->content.pipe.id, pipe });
                    _lg.unlock();

                    std::cout << "Pipe request " << header->content.pipe.id << std::endl;

                    //Issue callback about new incoming pipe
                    _pipe_callback(OPTPipeOpenData{
                        .pipe = pipe,
                        .content = content
                        });
                }
            }
            break;
            //Response to a pipe open request
        case packet_decoder::PIPE_OPEN_RESP:
            //Check ID parity
            if ((header->content.pipe.id % 2 == 1) == _is_client) {
                std::shared_lock _lg(_pipe_lock);
                auto it = _pipes.find(header->content.pipe.id);
                if (it != _pipes.end()) {
                    std::shared_ptr pipe = it->second.lock();
                    _lg.unlock();
                    if (pipe) {
                        std::cout << "Pipe response " << header->content.pipe.id << std::endl;

                        if (pipe->_open_state == OPTPipe::OpenRequstSent) {
                            pipe->_open_state = OPTPipe::Opened;
                            pipe->_open_callback();
                        }
                    }
                }
            }
            break;
            //A pipe closure was requested
        case packet_decoder::PIPE_CLOSE:
        {
            std::unique_lock _lg(_pipe_lock);
            auto it = _pipes.find(header->content.pipe.id);
            if (it != _pipes.end()) {
                std::shared_ptr<OPTPipe> pipe = it->second.lock();
                _pipes.erase(it);
                _lg.unlock();

                if (pipe) {
                    std::cout << "Pipe close " << header->content.pipe.id << std::endl;

                    if (pipe->_open_state != OPTPipe::Closed) {
                        pipe->_open_state = OPTPipe::Closed;
                        //Issue close callback
                        pipe->_close_callback();
                    }
                }
            }
        }
        break;
        //Data received on a pipe
        case packet_decoder::PIPE_DATA:
        {
            std::unique_lock _lg(_pipe_lock);
            auto it = _pipes.find(header->content.pipe.id);
            if (it != _pipes.end()) {
                std::shared_ptr<OPTPipe> pipe = it->second.lock();
                _lg.unlock();

                if (pipe) {
                    if (pipe->_open_state == OPTPipe::Opened) {
                        pipe->_result_callback(content);
                    }
                }
            }
        }
        break;
        case packet_decoder::SELF_DATA:
            _result_callback(content);
            break;
        default:
            close();
            break;
        }
    }
    //Called when a packet can be read
    void OPTPipes::on_raw(const Utils::ConstBlobView& data) {
        const OPTPipes::packet_decoder* header = OPTPipes::packet_decoder::cview(data);
        const Utils::ConstBlobSliceView content = data.cslice_right(5);

        switch (header->type)
        {
        case packet_decoder::PIPE_DATA:
        {
            std::shared_lock _lg(_pipe_lock);
            auto it = _pipes.find(header->content.pipe.id);
            //If pipe not found: open/close still in progress
            //Can safely drop
            if (it != _pipes.end()) {
                std::shared_ptr<OPTPipe> pipe = it->second.lock();
                _lg.unlock();
                if (pipe) {
                    if (pipe->_open_state == OPTPipe::Opened) {
                        pipe->_raw_callback(content);
                    }
                }
            }
        }
        break;
        case packet_decoder::SELF_DATA:
            _raw_callback(content);
            break;
        case packet_decoder::PIPE_OPEN_REQ:
        case packet_decoder::PIPE_OPEN_RESP:
        case packet_decoder::PIPE_CLOSE:
        default:
            close();
            break;
        }
    }
    //Called when a socket error occurs
    void OPTPipes::on_error() {
        std::shared_lock _lg(_pipe_lock);

        for (auto& it : _pipes) {
            std::shared_ptr<OPTPipe> pipe = it.second.lock();
            if (pipe) {
                pipe->_error_callback();
            }
        }
    }
    //Timer
    void OPTPipes::on_floating() {
        std::shared_lock _lg(_pipe_lock);

        //Find earliest
        for (auto& it : _pipes) {
            std::shared_ptr<OPTPipe> pipe = it.second.lock();
            if (pipe) {
                if (pipe->_floating.dst < Time::now()) {
                    pipe->_floating.cb.call_and_clear();
                }
            }
        }

        if (_floating.dst < Time::now()) {
            _floating.cb.call_and_clear();
        }


        updateFloatingNext();
    }
    //Socket was closed
    void OPTPipes::on_close() {
        std::shared_lock _lg(_pipe_lock);

        for (auto& it : _pipes) {
            std::shared_ptr<OPTPipe> pipe = it.second.lock();
            if (pipe) {
                pipe->_close_callback();
            }
        }
    }

    OPTPipes::OPTPipes(
        bool is_client,
        std::shared_ptr<OPTBase> underlying) : OPTBase(is_client), _underlying(underlying), _next_pipe_id(_is_client ? 1 : 0) {

    }

    std::shared_ptr<OPTPipes> OPTPipes::create(
        bool is_client,
        std::shared_ptr<OPTBase> underlying) {
        return std::shared_ptr<OPTPipes>(new OPTPipes(is_client, underlying));
    }

    void OPTPipes::start() {
        _underlying->set_open_callback(new Utils::MemberWCallback<OPTPipes, void>(weak_from_this(), &OPTPipes::on_open));
        _underlying->set_packet_callback(new Utils::MemberWCallback<OPTPipes, void, const Utils::ConstBlobView&>(weak_from_this(), &OPTPipes::on_packet));
        _underlying->set_raw_callback(new Utils::MemberWCallback<OPTPipes, void, const Utils::ConstBlobView&>(weak_from_this(), &OPTPipes::on_raw));
        _underlying->set_error_callback(new Utils::MemberWCallback<OPTPipes, void>(weak_from_this(), &OPTPipes::on_error));
        _underlying->set_close_callback(new Utils::MemberWCallback<OPTPipes, void>(weak_from_this(), &OPTPipes::on_close));

        _underlying->start();
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

    std::shared_ptr<Identity::User> OPTPipes::getUser() {
        return _underlying->getUser();
    }

    void OPTPipes::close() {
        _underlying->close();
    }

};