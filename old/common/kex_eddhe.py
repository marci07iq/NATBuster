# Forward secret key exchange, with S->C authentication

# KEX:
# EDwards curve Diffie Hellman with Ephemeral keys
# Using Ed25519

# Running encryption:
# AES-CBC-256

# Strongly inspired by SSH key exchange

#a: Initiator, b: Target
#Pk: Public key, Sk: (Secret) Private key
#N: Nonces
#g: Curve
#X: Ephemeral keys

# Assume public keys pre-shared (or must trust)
# Curve pre-shared

# A->B: M1: Version
# A<-B: M2: Version and capabilities (respond with a Vb <= Va)
# A->B: M3: Selected system

# A->B: g^Xa, Na
# B: K=g^Xa^Xb, H=hash(K, g^Xa, g^Xb, Na, Nb)
# A<-B: H_Skb, Nb
# A: Calculate K, Check H signature
# A<-B: Encryption ON
# A->B: Encryption ON



from enum import Enum
import OpenSSL

class KexEdDHEStateC(Enum):
    INITIATED = 1,
    CHALLENGE = 2,


class AuthFlowPKI:
    def __init__(self, source, dest):
        self.client_src = source
        self.client_dst = dest

    def recv(message):
        pass
