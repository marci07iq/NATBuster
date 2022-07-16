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
                _result_callback(content);
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
            _raw_callback(data);
        }
    }
    //Called when a socket error occurs
    void OPTSession::on_error() {
        _error_callback();
    }
    //Socket was closed
    void OPTSession::on_close() {
        _close_callback();
    }

    void OPTSession::send_internal(PacketType type, const Utils::ConstBlobView& packet) {
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
        _underlying->set_open_callback(new Utils::MemberCallback<OPTSession, void>(weak_from_this(), &OPTSession::on_open));
        _underlying->set_packet_callback(new Utils::MemberCallback<OPTSession, void, const Utils::ConstBlobView&>(weak_from_this(), &OPTSession::on_packet));
        _underlying->set_raw_callback(new Utils::MemberCallback<OPTSession, void, const Utils::ConstBlobView&>(weak_from_this(), &OPTSession::on_raw));
        _underlying->set_error_callback(new Utils::MemberCallback<OPTSession, void>(weak_from_this(), &OPTSession::on_error));
        _underlying->set_close_callback(new Utils::MemberCallback<OPTSession, void>(weak_from_this(), &OPTSession::on_close));

        _underlying->start();
    }

    //Add a callback that will be called in `delta` time, if the emitter is still running
    //There is no way to cancel this call
    //Only call from callbacks, or before start
    inline void OPTSession::addDelay(Utils::Timers::TimerCallback::raw_type cb, Time::time_delta_type_us delta) {
        _underlying->addDelay(cb, delta);
    }

    //Add a callback that will be called at time `end`, if the emitter is still running
    //There is no way to cancel this call
    //Only call from callbacks, or before start
    inline void OPTSession::addTimer(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end) {
        _underlying->addTimer(cb, end);
    }

    void OPTSession::updateFloatingNext(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end) {
        _underlying->updateFloatingNext(cb, end);
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