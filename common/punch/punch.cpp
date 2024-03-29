#include "punch.h"
#include "../endpoint/c2_client.h"
#include "../crypto/random.h"

namespace NATBuster::Punch {
    void Puncher::on_open() {
        if (_is_client) {
            if (_state == S0_New) {
                //Send client hello
                Utils::Blob chello = Utils::Blob::factory_empty(72, 0, 20);
                packet_decoder* chello_view = packet_decoder::view(chello);
                chello_view->type = packet_decoder::PUNCH_DESCRIBE;
                chello_view->content.describe.nat.port_min = 1024;
                chello_view->content.describe.nat.port_max = 65535;
                chello_view->content.describe.nat.rate_limit = 25;
                chello.copy_from(_outbound_magic_content, 8);

                //TODO: Get from IP server
                Utils::Blob ip = Utils::Blob::factory_string("localhost");

                Utils::PackedBlobWriter chello_w(chello);
                chello_w.add_record(ip);

                _underlying->send(chello);

                _state = S1_CHello;
            }
        }
    }
    void Puncher::on_packet(const Utils::ConstBlobView& data) {
        if (data.size() == 0) close();

        const packet_decoder* view = packet_decoder::cview(data);

        packet_decoder::PacketType packet_type = view->type;

        if (_is_client) {
            switch (_state)
            {
                //Client hello sent, expect server hello back
            case S1_CHello:
            {
                //Expect to get Server hello
                if (packet_type != packet_decoder::PUNCH_DESCRIBE) return fail();
                //Must have enough length for NAT descriptor
                if (data.size() < 72) return fail();
                //if(view->content.describe.nat != nat_descriptor::Symmetric) return fail();

                HolepunchSym::HolepunchSymSettings puncher_settings;
                puncher_settings.port_min = view->content.describe.nat.port_min;
                puncher_settings.port_max = view->content.describe.nat.port_max;
                puncher_settings.rate = view->content.describe.nat.rate_limit;

                //Prepend one extra byte to the magic value
                Utils::Blob ib_magic = Utils::Blob::factory_empty(1 + 64);
                *(uint8_t*)ib_magic.getw() = Transport::OPTUDP::MGMT_HELLO;
                ib_magic.copy_from(data.cslice(8, 64), 1);

                Utils::PackedBlobReader data_reader(data.cslice_right(72));
                Utils::ConstBlobSliceView ip;
                if (!data_reader.next_record(ip)) return fail();
                if (!data_reader.eof()) return fail();

                //IP as string
                std::string ip_s((const char*)ip.getr(), ip.size());

                //Outbound blob with paddig
                Utils::Blob ob_magic = Utils::Blob::factory_empty(1 + 64);
                *(uint8_t*)ob_magic.getw() = Transport::OPTUDP::MGMT_HELLO;
                ob_magic.copy_from(_outbound_magic_content, 1);

                _puncher = HolepunchSym::create(ip_s, std::move(ob_magic), std::move(ib_magic), puncher_settings);
                _puncher->set_punch_callback(new Utils::MemberWCallback<Puncher, void, Network::UDPHandleU>(weak_from_this(), &Puncher::on_punch));

                _state = S2_SHello;
            }
            {
                Utils::Blob start = Utils::Blob::factory_empty(1);
                *(uint8_t*)start.getw() = (uint8_t)packet_decoder::PUNCH_START;
                _underlying->send(start);
                _puncher->async_launch();

                _state = S3_CStart;
            }
            break;
            case S0_New:
            case S2_SHello:
            case S3_CStart:
            case SF_Success:
            case SF_Error:
            default:
                break;
            }
        }
        else {
            switch (_state) {
                //New object created
            case S0_New:
            {
                //Expect to get Client hello
                if (packet_type != packet_decoder::PUNCH_DESCRIBE) return fail();
                //Must have enough length for NAT descriptor
                if (data.size() < 72) return fail();
                //if(view->content.describe.nat != nat_descriptor::Symmetric) return fail();

                HolepunchSym::HolepunchSymSettings puncher_settings;
                puncher_settings.port_min = view->content.describe.nat.port_min;
                puncher_settings.port_max = view->content.describe.nat.port_max;
                puncher_settings.rate = view->content.describe.nat.rate_limit;

                //Prepend one extra byte to the magic value
                Utils::Blob ib_magic = Utils::Blob::factory_empty(1 + 64);
                *(uint8_t*)ib_magic.getw() = Transport::OPTUDP::MGMT_HELLO;
                ib_magic.copy_from(data.cslice(8, 64), 1);

                Utils::PackedBlobReader data_reader(data.cslice_right(72));
                Utils::ConstBlobSliceView ip;
                if (!data_reader.next_record(ip)) return fail();
                if (!data_reader.eof()) return fail();

                //IP as string
                std::string ip_s((const char*)ip.getr(), ip.size());

                //Outbound blob with paddig
                Utils::Blob ob_magic = Utils::Blob::factory_empty(1 + 64);
                *(uint8_t*)ob_magic.getw() = Transport::OPTUDP::MGMT_HELLO;
                ob_magic.copy_from(_outbound_magic_content, 1);

                _puncher = HolepunchSym::create(ip_s, std::move(ob_magic), std::move(ib_magic), puncher_settings);
                _puncher->set_punch_callback(new Utils::MemberWCallback<Puncher, void, Network::UDPHandleU>(weak_from_this(), &Puncher::on_punch));

                _state = S1_CHello;
            }
            //Reply with server hello
            {
                Utils::Blob shello = Utils::Blob::factory_empty(72, 0, 20);
                packet_decoder* shello_view = packet_decoder::view(shello);
                shello_view->type = packet_decoder::PUNCH_DESCRIBE;
                shello_view->content.describe.nat.port_min = 1024;
                shello_view->content.describe.nat.port_max = 65535;
                shello_view->content.describe.nat.rate_limit = 25;
                shello.copy_from(_outbound_magic_content, 8);

                //TODO: Get from IP server
                Utils::Blob ip = Utils::Blob::factory_string("localhost");

                Utils::PackedBlobWriter shello_w(shello);
                shello_w.add_record(ip);

                _underlying->send(shello);

                _state = S2_SHello;
            }
            break;
            case S2_SHello:
            {
                if (packet_type != packet_decoder::PUNCH_START) return fail();
                //Must have enough length for NAT descriptor
                if (data.size() != 1) return close();
                //if(view->content.describe.nat != nat_descriptor::Symmetric) return close();

                _puncher->async_launch();
            }
            break;
            case S1_CHello:
            case S3_CStart:
            case SF_Success:
            case SF_Error:
            default:
                break;
            }
        }
    }
    void Puncher::on_error(ErrorCode code) {
        std::cout << "Error " << code << std::endl;
    }
    void Puncher::on_close() {
        {
            std::lock_guard _lg(_c2_client->_open_punchers_lock);
            if (_self != _c2_client->_open_punchers.end()) {
                _c2_client->_open_punchers.erase(_self);
                std::cout << "Puncher self erasing" << std::endl;
            }
        }
    }

    void Puncher::on_punch(Network::UDPHandleU punched) {
        std::cout << "Punch successful" << std::endl;

        Network::UDPHandleS punched_s = punched;
        _c2_client->_client_emitter_provider->associate_socket(std::move(punched));

        //Create the OPT
        std::shared_ptr<Transport::OPTUDP> opt_udp = Transport::OPTUDP::create(_is_client, _c2_client->_client_emitter, punched_s);
        //Create the Session
        std::shared_ptr<Transport::OPTSession> opt_session = Transport::OPTSession::create(_is_client, opt_udp, _self_key, _trusted_users);

        _punch_callback(opt_session);
    }

    Puncher::Puncher(
        std::shared_ptr<Endpoint::C2Client> c2_client,
        bool is_client,
        const std::shared_ptr<const Crypto::PrKey> self_key,
        const std::shared_ptr<const Identity::UserGroup> trusted_users,
        std::shared_ptr<Transport::OPTBase> underlying
    ) : _c2_client(c2_client), _is_client(is_client), _self_key(std::move(self_key)), _trusted_users(trusted_users), _underlying(underlying) {
        _outbound_magic_content = Utils::Blob::factory_empty(64);
        Crypto::random(_outbound_magic_content.getw(), _outbound_magic_content.size());
    }

    void Puncher::start() {
        {
            std::lock_guard _lg(_c2_client->_open_punchers_lock);
            _self = _c2_client->_open_punchers.insert(_c2_client->_open_punchers.end(), shared_from_this());
            std::cout << "Puncher self registering" << std::endl;
        }

        _underlying->set_callback_open(new Utils::MemberWCallback<Puncher, void>(weak_from_this(), &Puncher::on_open));
        _underlying->set_callback_packet(new Utils::MemberWCallback<Puncher, void, const Utils::ConstBlobView&>(weak_from_this(), &Puncher::on_packet));
        _underlying->set_callback_error(new Utils::MemberWCallback<Puncher, void, ErrorCode>(weak_from_this(), &Puncher::on_error));
        _underlying->set_callback_close(new Utils::MemberWCallback<Puncher, void>(weak_from_this(), &Puncher::on_close));

        _underlying->start();
    }


    void Puncher::fail() {
        _state = SF_Error;
        close();
    }

    std::shared_ptr<Puncher> Puncher::create(
        std::shared_ptr<Endpoint::C2Client> c2_client,
        bool is_client,
        const std::shared_ptr<const Crypto::PrKey> self_key,
        const std::shared_ptr<const Identity::UserGroup> trusted_users,
        std::shared_ptr<Transport::OPTBase> underlying
    ) {
        return std::shared_ptr<Puncher>(new Puncher(c2_client, is_client, std::move(self_key), trusted_users, underlying));
    }

    void Puncher::close() {
        _underlying->close();
    }

}