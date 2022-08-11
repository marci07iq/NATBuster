#include "../utils/text.h"
#include "identity.h"

#ifdef WIN32
#include <Windows.h>
#include <Lmcons.h>
#elif __linux__
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#else
#error "Unsupported OS"
#endif

namespace NATBuster::Identity {
    std::shared_ptr<User> User::Anonymous = std::make_shared<User>("anonymous", std::make_shared<Crypto::PrKey>());

    std::ostream& operator<<(std::ostream& os, const User& user)
    {
        Utils::Blob fingerprint;
        user.key->fingerprint(fingerprint);

        os << user.name << "[";
        Utils::print_hex(fingerprint.cslice_left(8), os);
        os << "...]";
        return os;
    }

#ifdef WIN32
    std::string get_os_machine_name() {
        DWORD res_len = MAX_COMPUTERNAME_LENGTH + 1;
        Utils::string_os res;
        res.resize(res_len);

        // Get and display the name of the computer.
        if (0 != GetComputerNameW(res.data(), &res_len)) {
            res.resize(res_len);
            return Utils::convert_os2app(res);
        }

        return std::string();
    }
    std::string get_os_user_name() {
        DWORD res_len = UNLEN + 1;
        Utils::string_os res;
        res.resize(res_len);

        // Get and display the name of the computer.
        if (0 != GetUserNameW(res.data(), &res_len)) {
            res.resize(res_len);
            return Utils::convert_os2app(res);
        }

        return std::string();
    }
#elif __linux__
    std::string get_os_machine_name() {
        size_t res_len = HOST_NAME_MAX + 1;
        Utils::string_os res;
        res.resize(res_len);

        // Get and display the name of the computer.
        int code = gethostname(res.data(), res_len);
        if (0 == code) {
            res.resize(strlen(res.c_str()));
            return Utils::convert_os2app(res);
        }

        return std::string();
}
    std::string get_os_user_name() {
        /*size_t res_len = LOGIN_NAME_MAX + 1;
        Utils::string_os res;
        res.resize(res_len);


        // Get and display the name of the computer.
        int code = getlogin_r(res.data(), res_len);
        if (0 == code) {
            res.resize(strlen(res.c_str()));
            return Utils::convert_os2app(res);
        }*/

        passwd* res = getpwuid(getuid());

        std::string res_str(res->pw_name);

        //Do NOT need to free res.

        return res_str;
    }
#endif
};