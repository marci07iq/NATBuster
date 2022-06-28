#include "network.h"

#include <list>

namespace NATBuster::Common::Network {
    template <typename T>
    concept SocketHwnd = requires(const T & t, const std::list<T> &elems, Timeout to)
    {
        //Validity check. If false, the event emitter returns
        { t->valid() } -> std::same_as<bool>;
        //Close function, for when the event emitter needs to exit
        { t->close() } -> std::same_as<void>;
        //Gets the next response
        { T::find(elems, to) } -> std::same_as<T>;
    };

    template<SocketHwnd HWND>
    class SocketHWNDPool {
        std::list<HWND> _sockets;

    public:
        SocketHWNDPool(const std::list<HWND>& sockets) : _sockets(sockets) {

        }

        bool valid() {
            return _sockets.size();
        }

        void close() {
            for (auto&& it : _sockets) {
                it.close();
            }
        }


    };
}