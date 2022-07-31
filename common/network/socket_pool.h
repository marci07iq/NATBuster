#pragma once

#include "network.h"

#include "../utils/event.h"

#include <assert.h>
#include <list>

namespace NATBuster::Common::Network {
    /*template<SocketHwnd HWND>
    class SocketHWNDPool {
        std::list<HWND> _sockets;

        //Remove all dead sockets
        void prune() {
            auto it = _sockets.begin();

            while (it != _sockets.end()) {
                if ((*it)->is_valid()) {
                    ++it;
                }
                else {
                    auto it2 = it++;
                    _sockets.erase(it2);
                }
            }
        }
    public:
        SocketHWNDPool(const std::list<HWND>& sockets) : _sockets(sockets) {

        }

        //For only a single socket
        SocketHWNDPool(HWND socket) : _sockets(std::list<HWND>({ socket })) {

        }

        bool is_valid() {
            return _sockets.size();
        }

        void close() {
            for (auto&& it : _sockets) {
                it->close();
            }
            prune();
            assert(!is_valid());
        }

        HWND check(Time::Timeout to) {
            prune();
            return HWND::element_type::find(_sockets, to);
        }
    };*/
}