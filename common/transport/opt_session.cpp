#include "opt_session.h"
#include "kex_v1.h"

namespace NATBuster::Common::Transport {
    //Called when the emitter starts
    void OPTSession::on_open() {
        //Start the key exchange
        _kex->init_kex(this);
    }
    //Called when a packet can be read
    void OPTSession::on_packet(const Utils::ConstBlobView& data) {
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
        case NATBuster::Common::Transport::OPTSession::DATA:
            if (_flags._first_kex_ok) {
                _callback_packet(content);
            }
            else {
                //Error;
                close();
            }
            break;
        case NATBuster::Common::Transport::OPTSession::KEX:
        {
            Proto::KEX::KEX_Event res = _kex->recv(content, this);
            switch (res)
            {
            case NATBuster::Common::Proto::KEX::KEX_Event::OK:
                break;
            case NATBuster::Common::Proto::KEX::KEX_Event::OK_Done:
                if (!_flags._first_kex_ok) {
                    //Let upper levels know that the tunnle is ready
                    _callback_open();
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
        case NATBuster::Common::Transport::OPTSession::ENC_OFF:
            if (_flags._enc_off_enable) {
                _flags._enc_on_ib = false;
            }
            else {
                close();
            }
            break;
        case NATBuster::Common::Transport::OPTSession::CLOSE:
        default:
            close();
            break;
        }
    }
    //Called when a packet can be read
    void OPTSession::on_raw(const Utils::ConstBlobView& data) {
        if (_flags._first_kex_ok) {
            _callback_raw(data);
        }
    }
    //Called when a socket error occurs
    void OPTSession::on_error(ErrorCode code) {
        _callback_error(code);
    }
    //Socket was closed
    void OPTSession::on_close() {
        _callback_close();
    }

    void OPTSession::send_internal(PacketType type, const Utils::ConstBlobView& packet) {
        std::lock_guard _lg(_out_encryption_lock);
        Utils::Blob packet_full = Utils::Blob::factory_empty(1 + packet.size());

        (*((uint8_t*)packet_full.getw())) = type;
        packet_full.copy_from(packet, 1);

        //Decrypt data if there is encyption
        Utils::Blob packet_encryped;
        Utils::Blob no_aad;

        if (_flags._enc_on_ob) {
            _outbound.encrypt(packet_full, packet_encryped, no_aad);
        }

        Utils::BlobView& to_send = (_flags._enc_on_ob) ? packet_encryped : packet_full;

        _underlying->send(to_send);
    }
    void OPTSession::send_kex(const Utils::ConstBlobView& packet) {
        send_internal(PacketType::KEX, packet);
    }

    OPTSession::OPTSession(
        bool is_client,
        std::shared_ptr<OPTBase> underlying,
        Crypto::PKey&& self,
        std::shared_ptr<Identity::UserGroup> known_remotes
    ) :
        OPTBase(is_client),
        _underlying(underlying)
    {
        if (_is_client) {
            _kex = std::make_unique<Proto::KEXV1_A>(std::move(self), known_remotes);
        }
        else {
            _kex = std::make_unique<Proto::KEXV1_B>(std::move(self), known_remotes);
        }
    }

    void OPTSession::start() {
        _underlying->set_callback_open(new Utils::MemberWCallback<OPTSession, void>(weak_from_this(), &OPTSession::on_open));
        _underlying->set_callback_packet(new Utils::MemberWCallback<OPTSession, void, const Utils::ConstBlobView&>(weak_from_this(), &OPTSession::on_packet));
        _underlying->set_callback_raw(new Utils::MemberWCallback<OPTSession, void, const Utils::ConstBlobView&>(weak_from_this(), &OPTSession::on_raw));
        _underlying->set_callback_error(new Utils::MemberWCallback<OPTSession, void, ErrorCode>(weak_from_this(), &OPTSession::on_error));
        _underlying->set_callback_close(new Utils::MemberWCallback<OPTSession, void>(weak_from_this(), &OPTSession::on_close));

        _underlying->start();
    }

    //Send ordered packet
    void OPTSession::send(const Utils::ConstBlobView& packet) {
        send_internal(PacketType::DATA, packet);
    }

    //Send raw fasttrack packet, passed stright to the underlying inderface
    void OPTSession::sendRaw(const Utils::ConstBlobView& packet) {
        _underlying->sendRaw(packet);
    }

    void OPTSession::close() {
        _underlying->close();
    }
}