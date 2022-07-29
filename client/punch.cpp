#include "punch.h"
#include "c2_client.h"
#include "../common/crypto/random.h"

namespace NATBuster::Client {
    void Puncher::on_open() {
        if (_is_client) {
            if (_state == S0_New) {
                //Send client hello
                Common::Utils::Blob chello = Common::Utils::Blob::factory_empty(72, 0, 20);
                packet_decoder* chello_view = packet_decoder::view(chello);
                chello_view->type = packet_decoder::PUNCH_DESCRIBE;
                chello_view->content.describe.nat.port_min = 1024;
                chello_view->content.describe.nat.port_max = 65535;
                chello_view->content.describe.nat.rate_limit = 25;
                chello.copy_from(_outbound_magic_content, 8);

                //TODO: Get from IP server
                Common::Utils::Blob ip = Common::Utils::Blob::factory_string("localhost");

                Common::Utils::PackedBlobWriter chello_w(chello);
                chello_w.add_record(ip);

                _underlying->send(chello);

                _state = S1_CHello;
            }
        }
    }
    void Puncher::on_packet(const Common::Utils::ConstBlobView& data) {
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
                Common::Utils::Blob ib_magic = Common::Utils::Blob::factory_empty(1 + 64);
                *(uint8_t*)ib_magic.getw() = Common::Transport::OPTUDP::MGMT_HELLO;
                ib_magic.copy_from(data.cslice(8, 64), 1);

                Common::Utils::PackedBlobReader data_reader(data.cslice_right(72));
                Common::Utils::ConstBlobSliceView ip;
                if (!data_reader.next_record(ip)) return fail();
                if (!data_reader.eof()) return fail();

                //IP as string
                std::string ip_s((const char*)ip.getr(), ip.size());

                //Outbound blob with paddig
                Common::Utils::Blob ob_magic = Common::Utils::Blob::factory_empty(1 + 64);
                *(uint8_t*)ob_magic.getw() = Common::Transport::OPTUDP::MGMT_HELLO;
                ob_magic.copy_from(_outbound_magic_content, 1);

                _puncher = HolepunchSym::create(ip_s, std::move(ob_magic), std::move(ib_magic), puncher_settings);
                _puncher->set_punch_callback(new Common::Utils::MemberWCallback<Puncher, void, Common::Network::UDPHandle>(weak_from_this(), &Puncher::on_punch));

                _state = S2_SHello;
            }
            {
                Common::Utils::Blob start = Common::Utils::Blob::factory_empty(1);
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
                Common::Utils::Blob ib_magic = Common::Utils::Blob::factory_empty(1 + 64);
                *(uint8_t*)ib_magic.getw() = Common::Transport::OPTUDP::MGMT_HELLO;
                ib_magic.copy_from(data.cslice(8, 64), 1);

                Common::Utils::PackedBlobReader data_reader(data.cslice_right(72));
                Common::Utils::ConstBlobSliceView ip;
                if (!data_reader.next_record(ip)) return fail();
                if (!data_reader.eof()) return fail();

                //IP as string
                std::string ip_s((const char*)ip.getr(), ip.size());

                //Outbound blob with paddig
                Common::Utils::Blob ob_magic = Common::Utils::Blob::factory_empty(1 + 64);
                *(uint8_t*)ob_magic.getw() = Common::Transport::OPTUDP::MGMT_HELLO;
                ob_magic.copy_from(_outbound_magic_content, 1);

                _puncher = HolepunchSym::create(ip_s, std::move(ob_magic), std::move(ib_magic), puncher_settings);
                _puncher->set_punch_callback(new Common::Utils::MemberWCallback<Puncher, void, Common::Network::UDPHandle>(weak_from_this(), &Puncher::on_punch));

                _state = S1_CHello;
            }
            //Reply with server hello
            {
                Common::Utils::Blob shello = Common::Utils::Blob::factory_empty(72, 0, 20);
                packet_decoder* shello_view = packet_decoder::view(shello);
                shello_view->type = packet_decoder::PUNCH_DESCRIBE;
                shello_view->content.describe.nat.port_min = 1024;
                shello_view->content.describe.nat.port_max = 65535;
                shello_view->content.describe.nat.rate_limit = 25;
                shello.copy_from(_outbound_magic_content, 8);
                
                //TODO: Get from IP server
                Common::Utils::Blob ip = Common::Utils::Blob::factory_string("localhost");

                Common::Utils::PackedBlobWriter shello_w(shello);
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
    void Puncher::on_error() {

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

    void Puncher::on_punch(Common::Network::UDPHandle punched) {
        std::cout << "Punch successful" << std::endl;

        //Create the OPT
        std::shared_ptr<Common::Transport::OPTUDP> opt_udp = Common::Transport::OPTUDP::create(_is_client, punched);
        //Create the Session
        Common::Crypto::PKey self_key;
        self_key.copy_public_from(_self_key);
        std::shared_ptr<Common::Transport::OPTSession> opt_session = Common::Transport::OPTSession::create(_is_client, opt_udp, std::move(self_key), _trusted_users);

        _punch_callback(opt_session);
    }

    Puncher::Puncher(
        std::shared_ptr<C2Client> c2_client,
        bool is_client,
        Common::Crypto::PKey&& self_key,
        std::shared_ptr<Common::Identity::UserGroup> trusted_users,
        std::shared_ptr<Common::Transport::OPTBase> underlying
    ) : _c2_client(c2_client), _is_client(is_client), _self_key(std::move(self_key)), _trusted_users(trusted_users), _underlying(underlying) {
        _outbound_magic_content = Common::Utils::Blob::factory_empty(64);
        Common::Crypto::random(_outbound_magic_content.getw(), _outbound_magic_content.size());
    }

    void Puncher::start() {
        {
            std::lock_guard _lg(_c2_client->_open_punchers_lock);
            _self = _c2_client->_open_punchers.insert(_c2_client->_open_punchers.end(), shared_from_this());
            std::cout << "Puncher self registering" << std::endl;
        }

        _underlying->set_open_callback(new Common::Utils::MemberWCallback<Puncher, void>(weak_from_this(), &Puncher::on_open));
        _underlying->set_packet_callback(new Common::Utils::MemberWCallback<Puncher, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &Puncher::on_packet));
        _underlying->set_error_callback(new Common::Utils::MemberWCallback<Puncher, void>(weak_from_this(), &Puncher::on_error));
        _underlying->set_close_callback(new Common::Utils::MemberWCallback<Puncher, void>(weak_from_this(), &Puncher::on_close));

        _underlying->start();
    }


    void Puncher::fail() {
        _state = SF_Error;
        close();
    }

    std::shared_ptr<Puncher> Puncher::create(
        std::shared_ptr<C2Client> c2_client,
        bool is_client,
        Common::Crypto::PKey&& self_key,
        std::shared_ptr<Common::Identity::UserGroup> trusted_users,
        std::shared_ptr<Common::Transport::OPTBase> underlying
    ) {
        return std::shared_ptr<Puncher>(new Puncher(c2_client, is_client, std::move(self_key), trusted_users, underlying));
    }

    void Puncher::close() {
        _underlying->close();
    }

}