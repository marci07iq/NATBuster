#include "punch.h"
#include "c2_client.h"

namespace NATBuster::Client {
    class C2Client;


    void Puncher::on_open() {

    }
    void Puncher::on_packet(const Common::Utils::ConstBlobView& data) {
        /*if (packet.size() == 0) return fail(KEX_Event::ErrorMalformed);

        packet_decoder* view = packet_decoder::cview(data);

        packet_decoder::PacketType packet_type = view->type;*/
    }
    void Puncher::on_error() {

    }
    void Puncher::on_close() {

    }

    void Puncher::on_punch(Common::Network::UDPHandle punched) {
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

    }

    void Puncher::start() {
        {
            std::lock_guard _lg(_c2_client->_open_punchers_lock);
            _self = _c2_client->_open_punchers.insert(_c2_client->_open_punchers.end(), shared_from_this());
        }

        _underlying->set_open_callback(new Common::Utils::MemberWCallback<Puncher, void>(weak_from_this(), &Puncher::on_open));
        _underlying->set_packet_callback(new Common::Utils::MemberWCallback<Puncher, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &Puncher::on_packet));
        _underlying->set_error_callback(new Common::Utils::MemberWCallback<Puncher, void>(weak_from_this(), &Puncher::on_error));
        _underlying->set_close_callback(new Common::Utils::MemberWCallback<Puncher, void>(weak_from_this(), &Puncher::on_close));

        _underlying->start();
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