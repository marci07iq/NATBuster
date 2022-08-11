#include <string>

#ifdef WIN32
#include <Windows.h>
#endif

namespace NATBuster::Utils {
    //UTF8 strings used in the app
    //std::u8string is not currently used
    
#ifdef WIN32
    //On windows, OS APIs use UTF16 wchar strings
    typedef std::wstring string_os;

    inline std::string convert_os2app(const string_os& data) {
        if (data.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, data.data(), (int)data.size(), NULL, 0, NULL, NULL);
        std::string res(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, data.data(), (int)data.size(), (char*)&res[0], size_needed, NULL, NULL);
        return res;
    }

    inline string_os convert_app2os(const std::string& data) {
        if (data.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, (const char*)data.data(), (int)data.size(), NULL, 0);
        std::wstring res(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, (const char*)data.data(), (int)data.size(), &res[0], size_needed);
        return res;
    }
#elif __linux__
    //On linux, everything is utf8
    typedef std::string string_os;

    inline std::u8string convert_os2app(const string_os& data) {
        return data;
}

    inline string_os convert_app2os(const std::u8string& data) {
        return data;
    }
#else
#error "Unsupported OS"
#endif

    
}