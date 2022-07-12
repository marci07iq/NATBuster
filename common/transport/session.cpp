#include "session.h"
#include "kex_v1.h"

namespace NATBuster::Common::Transport {
    //Called when the emitter starts
    void Session::on_open() {
        //Start the key exchange
        _kex->init_kex(this);
    }
    //Called when a packet can be read
    void Session::on_packet(const Utils::ConstBlobView& data) {
        //Decrypt data if there is encyption
        Utils::Blob data_decrypted;
        Utils::Blob no_aad;

        if (_flags._enc_on_ib) {
            _inbound.decrypt(data, data_decrypted, no_aad);
        }

        //Data to use in the rest of the process
        const Utils::ConstBlobView& data_dc = _flags._enc_on_ib ? data_decrypted : data;


        PacketType type = (PacketType)(*((uint8_t*)data_dc.getr()));

        Utils::ConstBlobSliceView content = data_dc.cslice_right(1);

        switch (type)
        {
        case NATBuster::Common::Transport::Session::DATA:
            if (_flags._first_kex_ok) {
                _result_callback(data);
            }
            else {
                //Error;
                close();
            }
            break;
        case NATBuster::Common::Transport::Session::KEX:
        {
            Proto::KEX::KEX_Event res = _kex->recv(content, this);
            switch (res)
            {
            case NATBuster::Common::Proto::KEX::KEX_Event::OK:
                break;
            case NATBuster::Common::Proto::KEX::KEX_Event::OK_Done:
                if (!_flags._first_kex_ok) {
                    //Let upper levels know that the tunnle is ready
                    _open_callback();
                }
                _flags._first_kex_ok = true;
                break;
            case NATBuster::Common::Proto::KEX::KEX_Event::ErrorGeneric:
            case NATBuster::Common::Proto::KEX::KEX_Event::ErrorMalformed:
            case NATBuster::Common::Proto::KEX::KEX_Event::ErrorNotrust:
            case NATBuster::Common::Proto::KEX::KEX_Event::ErrorCrypto:
            case NATBuster::Common::Proto::KEX::KEX_Event::ErrorSend:
            case NATBuster::Common::Proto::KEX::KEX_Event::ErrorState:
            default:
                close();
                break;
            }
        }
        break;
        case NATBuster::Common::Transport::Session::ENC_OFF:
            if (_flags._enc_off_enable) {
                _flags._enc_on_ib = false;
            }
            else {
                close();
            }
            break;
        case NATBuster::Common::Transport::Session::CLOSE:
            break;
        default:
            break;
        }
    }
    //Called when a packet can be read
    void Session::on_raw(const Utils::ConstBlobView& data) {
        if (_flags._first_kex_ok) {
            _raw_callback(data);
        }
    }
    //Called when a socket error occurs
    void Session::on_error() {
        _error_callback();
    }
    //Socket was closed
    void Session::on_close() {
        _close_callback();
    }

    void Session::send_internal(PacketType type, const Utils::ConstBlobView& packet) {
        Utils::Blob packet_full = Utils::Blob::factory_empty(1 + packet.size());

        (*((uint8_t*)packet_full.getw())) = type;
        packet_full.copy_from(packet, 1);

        //Decrypt data if there is encyption
        Utils::Blob packet_encryped;
        Utils::Blob no_aad;

        //TODO: Thread protection!!
        if (_flags._enc_on_ob) {
            _outbound.encrypt(packet_full, packet_encryped, no_aad);
        }

        Utils::BlobView& to_send = (_flags._enc_on_ob) ? packet_encryped : packet_full;

        _underlying->send(to_send);
    }
    void Session::send_kex(const Utils::ConstBlobView& packet) {
        send_internal(PacketType::KEX, packet);
    }

    Session::Session(
        bool is_client,
        std::shared_ptr<OPTBase> underlying,
        Crypto::PKey&& my_private,
        Crypto::PKey&& remote_public
    ) :
        OPTBase(is_client)
    {
        if (_is_client) {
            _kex = std::make_unique<Proto::KEXV1_A>(std::move(my_private), std::move(remote_public));
        }
        else {
            _kex = std::make_unique<Proto::KEXV1_B>(std::move(my_private), std::move(remote_public));
        }
    }

    void Session::start() {
        _underlying->set_packet_callback(new Utils::MemberCallback<Session, void, const Utils::ConstBlobView&>(weak_from_this(), &Session::on_packet));
        _underlying->set_raw_callback(new Utils::MemberCallback<Session, void, const Utils::ConstBlobView&>(weak_from_this(), &Session::on_raw));
        _underlying->set_error_callback(new Utils::MemberCallback<Session, void>(weak_from_this(), &Session::on_error));
        _underlying->set_close_callback(new Utils::MemberCallback<Session, void>(weak_from_this(), &Session::on_close));

        _underlying->start();
    }

    //Send ordered packet
    void Session::send(const Utils::ConstBlobView& packet) {
        send_internal(PacketType::DATA, packet);
    }

    //Send raw fasttrack packet, passed stright to the underlying inderface
    void Session::sendRaw(const Utils::ConstBlobView& packet) {
        _underlying->sendRaw(packet);
    }

    void Session::close() {
        _underlying->close();
    }
}