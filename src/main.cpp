#include "gnome-time-server.hpp"
#include <signal.h>

DBus::BusDispatcher dispatcher;

void niam(int sig)
{
	dispatcher.leave();
}

int main()
{
	signal(SIGTERM, niam);
	signal(SIGINT, niam);

	DBus::default_dispatcher = &dispatcher;

	DBus::Connection conn = DBus::Connection::SystemBus();
	conn.request_name(GNOME_TIME_SERVER_NAME, true);

	GnomeTimeServer server(conn);

	dispatcher.enter();

	return 0;
}