#include "network.h"

#include <assert.h>
#include <list>

namespace NATBuster::Common::Network {
    template <typename T>
    concept SocketHwnd = requires(const T & t, const std::list<T> &elems, Time::Timeout to)
    {
        //Validity check. If false, the event emitter returns
        { t->valid() } -> std::same_as<bool>;
        //Close function, for when the event emitter needs to exit
        { t->close() } -> std::same_as<void>;
        //Gets the next response
        { T::element_type::find(elems, to) } -> std::same_as<T>;
    };

    template<SocketHwnd HWND>
    class SocketHWNDPool {
        std::list<HWND> _sockets;

        void prune() {
            auto it = _sockets.begin();

            while (it != _sockets.end()) {
                if ((*it)->valid()) {
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

        bool valid() {
            return _sockets.size();
        }

        void close() {
            for (auto&& it : _sockets) {
                it->close();
            }
            prune();
            assert(!valid());
        }

        HWND check(Time::Timeout to) {
            prune();
            return HWND::element_type::find(_sockets, to);
        }
    };
}