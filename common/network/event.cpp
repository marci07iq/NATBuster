#include "event.h"

namespace NATBuster::Common::Network {

    void TCPSEventEmitter::loop(std::weak_ptr<TCPSEventEmitter> emitter_w) {
        while (true) {
            //Check if object still exists
            std::shared_ptr<TCPSEventEmitter> emitter = emitter_w.lock();
            if (!emitter) break;

            //Check exit condition
            {
                std::lock_guard lg(emitter->_lock);
                //Check exit condition
                if (!emitter->_run) break;
            }

            //Filter for dead sockets
            emitter->_sockets.remove_if(
                [emitter](TCPSHandle handle) {
                    if (!handle->valid()) {
                        emitter->_error_callback(handle);
                        return true;
                    }
                    return false;
                });

            if (emitter->_sockets.size() == 0) break;

            //Find server with event
            TCPSHandle server = TCPS::findConnection(emitter->_sockets, true);

            if (server) {
                emitter->_listen_callback(server);
            }
        }
        std::cout << "Listen collector exited" << std::endl;
    }

    TCPSEventEmitter::TCPSEventEmitter(
        const std::list<TCPSHandle>& sockets,
        TCPSEventCallback listen_callback,
        TCPSEventCallback error_callback) :
        _sockets(sockets),
        _listen_callback(listen_callback),
        _error_callback(error_callback) {
        _run = true;
    }

    void TCPSEventEmitter::stop() {
        std::lock_guard lg(_lock);
        _run = false;
    }

    std::shared_ptr<TCPSEventEmitter> TCPSEventEmitter::create(const std::vector<TCPSHandle>& sockets) {
        std::shared_ptr<TCPSEventEmitter> obj = make_shared< TCPSEventEmitter>(sockets);

        obj->_recv_thread = std::thread(TCPSEventEmitter::loop, obj);

        return obj;
    }



    void TCPCEventEmitter::loop(std::weak_ptr<TCPCEventEmitter> emitter_w) {
        while (true) {
            //Check if object still exists
            std::shared_ptr<TCPCEventEmitter> emitter = emitter_w.lock();
            if (!emitter) break;

            //Check exit condition
            {
                std::lock_guard lg(emitter->_lock);
                //Check exit condition
                if (!emitter->_run) break;
            }

            //Filter for dead sockets
            emitter->_sockets.remove_if(
                [emitter](TCPCHandle handle) {
                    if (!handle->valid()) {
                        emitter->_error_callback(handle);
                        return true;
                    }
                    return false;
                });

            if (emitter->_sockets.size() == 0) break;

            //Find server with event
            TCPCHandle server = TCPC::findReadable(emitter->_sockets, true);

            if (server) {
                emitter->_read_callback(server);
            }
        }
        std::cout << "TCP Read collector exited" << std::endl;
    }

    TCPCEventEmitter::TCPCEventEmitter(
        const std::list<TCPCHandle>& sockets,
        TCPCEventCallback listen_callback,
        TCPCEventCallback error_callback) :
        _sockets(sockets),
        _read_callback(listen_callback),
        _error_callback(error_callback) {
        _run = true;
    }

    void TCPCEventEmitter::stop() {
        std::lock_guard lg(_lock);
        _run = false;
    }

    std::shared_ptr<TCPCEventEmitter> TCPCEventEmitter::create(const std::vector<TCPCHandle>& sockets) {
        std::shared_ptr<TCPCEventEmitter> obj = make_shared< TCPCEventEmitter>(sockets);

        obj->_recv_thread = std::thread(TCPCEventEmitter::loop, obj);

        return obj;
    }



    void UDPEventEmitter::loop(std::weak_ptr<UDPEventEmitter> emitter_w) {
        while (true) {
            //Check if object still exists
            std::shared_ptr<UDPEventEmitter> emitter = emitter_w.lock();
            if (!emitter) break;

            //Check exit condition
            {
                std::lock_guard lg(emitter->_lock);
                //Check exit condition
                if (!emitter->_run) break;
            }

            //Filter for dead sockets
            emitter->_sockets.remove_if(
                [emitter](UDPHandle handle) {
                    if (!handle->valid()) {
                        emitter->_error_callback(handle);
                        return true;
                    }
                    return false;
                });

            if (emitter->_sockets.size() == 0) break;

            //Find server with event
            UDPHandle server = UDP::findReadable(emitter->_sockets, true);

            if (server) {
                emitter->_read_callback(server);
            }
        }
        std::cout << "UDP Read collector exited" << std::endl;
    }

    UDPEventEmitter::UDPEventEmitter(
        const std::list<UDPHandle>& sockets,
        UDPEventCallback listen_callback,
        UDPEventCallback error_callback) :
        _sockets(sockets),
        _read_callback(listen_callback),
        _error_callback(error_callback) {
        _run = true;
    }

    void UDPEventEmitter::stop() {
        std::lock_guard lg(_lock);
        _run = false;
    }

    std::shared_ptr<UDPEventEmitter> UDPEventEmitter::create(const std::vector<UDPHandle>& sockets) {
        std::shared_ptr<UDPEventEmitter> obj = make_shared< UDPEventEmitter>(sockets);

        obj->_recv_thread = std::thread(UDPEventEmitter::loop, obj);

        return obj;
    }
};