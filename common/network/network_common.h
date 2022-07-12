#include "../utils/event.h"

namespace NATBuster::Common::Network {
    class TCPS;
    class TCPC;
    class UDP;

    typedef std::shared_ptr<TCPS> TCPSHandle;
    typedef std::shared_ptr<TCPC> TCPCHandle;
    typedef std::shared_ptr<UDP> UDPHandle;

    enum NetworkErrorCodes {
        //Initilaise network subsystem
        NetworkErrorCodeInitalize,

        //Resolve listen address
        NetworkErrorCodeResolveAddress,

        //Create listening socket
        NetworkErrorCodeCreateListenSocket,
        //Bind listening socket
        NetworkErrorCodeBindListenSocket,
        //Listen
        NetworkErrorCodeListen,
        //Accept
        NetworkErrorCodeAccept,

        //Create client socket
        NetworkErrorCodeCreateClientSocket,
        //Connect to server
        NetworkErrorCodeConnect,

        //Send function
        NetworkErrorCodeSendData,
        //Listen function
        NetworkErrorCodeReciveData,

        //Select acceptable socket
        NetworkErrorCodeSelectAccept,
        //Select readable socket
        NetworkErrorCodeSelectRead,

    };
};
