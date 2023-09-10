#ifndef __GNOME_TIME_SERVER_HPP
#define __GNOME_TIME_SERVER_HPP

#include "gnome-dbus-time-service-glue.hpp"

#include <dbus-c++/dbus.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
namespace fs = std::filesystem;

#define NULL_ADJTIME_UTC "0.0 0 0\n0\nUTC\n"
#define NULL_ADJTIME_LOCAL "0.0 0 0\n0\nLOCAL\n"

inline bool file_exists(const std::string& name)
{
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

static const char* GNOME_TIME_SERVER_NAME = "org.freedesktop.timedate1";
static const char* GNOME_TIME_SERVER_PATH = "/org/freedesktop/timedate1";

class GnomeTimeServer : public org::freedesktop::timedate1_adaptor,
                        public DBus::IntrospectableAdaptor,
                        public DBus::ObjectAdaptor {
public:
    GnomeTimeServer(DBus::Connection& connection);

    virtual void SetTime(
        const int64_t& usec_utc,
        const bool& relative,
        const bool& interactive) override;
    virtual void
    SetTimezone(const std::string& timezone, const bool& interactive) override;
    virtual void SetLocalRTC(
        const bool& local_rtc,
        const bool& fix_system,
        const bool& interactive) override;
    virtual void SetNTP(const bool& use_ntp, const bool& interactive) override;
    virtual std::vector<std::string> ListTimezones() override;

private:
    inline bool isTimezoneValid(const std::string& value) const
    {
        return std::find(
                   m_valid_timezones.begin(), m_valid_timezones.end(), value)
            != m_valid_timezones.end();
    }

    inline void getTimezonesData()
    {
        std::ifstream file("/usr/share/zoneinfo/tzdata.zi");

        if (!file.good())
            return;

        std::string tmp;

        while (std::getline(file, tmp, '\n')) {
            std::stringstream line(tmp);
            std::string type, f1, f2;

            line >> type >> f1 >> f2;

            if ("Z" == type || "z" == type)
                m_valid_timezones.push_back(f1);
            else if ("L" == type || "L" == type)
                m_valid_timezones.push_back(f2);
        }
    }

    inline std::string getTimezone() const
    {
        auto lct = fs::path("/etc/localtime");

        if (fs::exists(lct))
            if (fs::is_symlink(lct)) {
                auto tzf = fs::read_symlink(lct);
                std::string tbc = tzf.string();
                std::string rmv_path = "/usr/share/zoneinfo/";

                std::size_t ind = tbc.find(rmv_path);
                if (ind != std::string::npos) {
                    tbc.erase(ind, rmv_path.length());
                    if (isTimezoneValid(tbc))
                        return tbc;
                }
            }

        return std::string("UTC");
    }

    inline bool getLocalRTC() const
    {
        std::ifstream file("/etc/adjtime");

        if (!file.good())
            return false;

        std::string tmp;

        while (std::getline(file, tmp, '\n')) {
            if (tmp == "LOCAL")
                return true;
        }

        return false;
    }

    inline void writeDataTimezone()
    {
        fs::path source;

        /* No timezone is very similar to UTC. Hence in either of these cases
         * link the UTC file in. Except if it isn't installed, in which case we
         * remove the symlink altogether. Since glibc defaults to an internal
         * version of UTC in that case behaviour is mostly equivalent. We still
         * prefer creating the symlink though, since things are more self
         * explanatory then. */

        if (m_current_timezone.empty() || m_current_timezone == "UTC") {

            if (access("/usr/share/zoneinfo/UTC", F_OK) < 0) {

                if (unlink("/etc/localtime") < 0 && errno != ENOENT) {
                    std::string err
                        = "Failed to unlink /etc/localtime, reason: ";
                    err += std::to_string(errno);
                    throw DBus::Error(
                        "org.freedesktop.timedate1.Error", err.c_str());
                }

                return;
            }

            source = "/usr/share/zoneinfo/UTC";
        } else {
            fs::path pre("/usr/share/zoneinfo");
            pre /= m_current_timezone;
            source = pre;
            if (source.empty())
                throw DBus::Error(
                    "org.freedesktop.timedate1.Error",
                    "Failed, no memory left");
        }

        if (file_exists("/etc/localtime"))
            if (unlink("/etc/localtime") < 0 && errno != ENOENT) {
                std::string err = "Failed to unlink /etc/localtime, reason: ";
                err += std::to_string(errno);
                throw DBus::Error(
                    "org.freedesktop.timedate1.Error", err.c_str());
            }

        try {
            fs::create_symlink(source, fs::path("/etc/localtime"));
        } catch (...) {
            throw DBus::Error(
                "org.freedesktop.timedate1.Error",
                "Failed to create /etc/localtime symlink");
        }
    }

    inline void writeDataLocalRTC()
    {
        std::string s, w;
        s.resize(256);
        w.resize(256);

        int r;

        std::ifstream t("/etc/adjtime");
        std::stringstream buffer;
        buffer << t.rdbuf();
        if (!(buffer.good())) {
            if (t.is_open()) {
                throw DBus::Error(
                    "org.freedesktop.timedate1.Error",
                    "Failed to open /etc/adjtime");
            }

            if (!m_local_rtc)
                return;

            w = NULL_ADJTIME_LOCAL;
            if (!w.capacity())
                throw DBus::Error(
                    "org.freedesktop.timedate1.Error",
                    "Failed, no memory left");

        } else {

            char* p;
            const char* e = "\n"; /* default if there is less than 3 lines */
            const char* prepend = "";
            size_t a, b;

            p = strchrnul(s.data(), '\n');
            if (*p == '\0')
                /* only one line, no \n terminator */
                prepend = "\n0\n";
            else if (p[1] == '\0') {
                /* only one line, with \n terminator */
                ++p;
                prepend = "0\n";
            } else {
                p = strchr(p + 1, '\n');
                if (!p) {
                    /* only two lines, no \n terminator */
                    prepend = "\n";
                    p = s.data() + strlen(s.data());
                } else {
                    char* end;
                    /* third line might have a \n terminator or not */
                    p++;
                    end = strchr(p, '\n');
                    /* if we actually have a fourth line, use that as suffix
                     * "e", otherwise the default \n */
                    if (end)
                        e = end;
                }
            }

            a = p - s.data();
            b = strlen(e);

            w = new char(a + (m_local_rtc ? 5 : 3) + strlen(prepend) + b + 1);
            if (!w.capacity())
                throw DBus::Error(
                    "org.freedesktop.timedate1.Error",
                    "Failed, no memory left");

            *(char*)mempcpy(
                stpcpy(
                    stpcpy(
                        static_cast<char*>(mempcpy(w.data(), s.data(), a)),
                        prepend),
                    m_local_rtc ? "LOCAL" : "UTC"),
                e,
                b)
                = 0;

            if (w == NULL_ADJTIME_UTC) {
                if (unlink("/etc/adjtime") < 0)
                    if (errno != ENOENT)
                        throw DBus::Error(
                            "org.freedesktop.timedate1.Error",
                            "Failed to unlink /etc/adjtime");
                return;
            }
        }

        // r = mac_init();
        // if (r < 0)
        // 	throw DBus::Error("org.freedesktop.timedate1.Error", "Failed mac
        // init");

        // equivalent of atomic write file

        std::ofstream o("/etc/adjtime_tmp");
        if (!o.good())
            throw DBus::Error(
                "org.freedesktop.timedate1.Error", "Failed atomic write");

        o << w;
        o.flush();
        o.close();
        r = rename("/etc/adjtime_tmp", "/etc/adjtime");
        if (r < 0)
            throw DBus::Error(
                "org.freedesktop.timedate1.Error", "Failed atomic write");
    }

    int64_t m_current_time;
    std::string m_current_timezone;
    bool m_local_rtc;

    std::vector<std::string> m_valid_timezones;
};

#endif // #ifndef __GNOME_TIME_SERVER_HPP