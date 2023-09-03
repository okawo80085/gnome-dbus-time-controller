#ifndef __GNOME_TIME_SERVER_HPP
#define __GNOME_TIME_SERVER_HPP


#include <dbus-c++/dbus.h>
#include "gnome-dbus-time-service-glue.hpp"


static const char *GNOME_TIME_SERVER_NAME = "org.freedesktop.timedate1";
static const char *GNOME_TIME_SERVER_PATH = "/org/freedesktop/timedate1";

class GnomeTimeServer:
	public org::freedesktop::timedate1_adaptor,
	public DBus::IntrospectableAdaptor,
	public DBus::ObjectAdaptor
{
public:

	GnomeTimeServer(DBus::Connection &connection);

    virtual void SetTime(const int64_t& usec_utc, const bool& relative, const bool& interactive) override;
    virtual void SetTimezone(const std::string& timezone, const bool& interactive) override;
    virtual void SetLocalRTC(const bool& local_rtc, const bool& fix_system, const bool& interactive) override;
    virtual void SetNTP(const bool& use_ntp, const bool& interactive) override;
    virtual std::vector< std::string > ListTimezones() override;

};

#endif // #ifndef __GNOME_TIME_SERVER_HPP