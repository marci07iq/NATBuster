from enum import Enum

class EmittedType(Enum):
    #Connection succeeded
    CONN_SUCCESS = 1,
    #Connection failed
    CONN_FAIL = 2,
    #Message from the UDP pipe
    UDP_PIPE = 3,
    #Message from the TCP transport
    TCP_PACKET = 4