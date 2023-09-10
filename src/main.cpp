#include "gnome-time-server.hpp"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

DBus::BusDispatcher dispatcher;

void niam(int sig)
{
    dispatcher.leave();
}

// static void skeleton_daemon()
// {
// 	pid_t pid;

// 	/* Fork off the parent process */
// 	pid = fork();

// 	/* An error occurred */
// 	if (pid < 0)
// 		exit(EXIT_FAILURE);

// 	/* Success: Let the parent terminate */
// 	if (pid > 0)
// 		exit(EXIT_SUCCESS);

// 	/* On success: The child process becomes session leader */
// 	if (setsid() < 0)
// 		exit(EXIT_FAILURE);

// 	/* Catch, ignore and handle signals */
// 	signal(SIGTERM, niam);
// 	signal(SIGINT, niam);

// 	/* Fork off for the second time*/
// 	pid = fork();

// 	/* An error occurred */
// 	if (pid < 0)
// 		exit(EXIT_FAILURE);

// 	/* Success: Let the parent terminate */
// 	if (pid > 0)
// 		exit(EXIT_SUCCESS);

// 	/* Set new file permissions */
// 	umask(0);

// 	/* Change the working directory to the root directory */
// 	/* or another appropriated directory */
// 	chdir("/");

//     /* Close all open file descriptors */
//     int x;
//     for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
//     {
//         close (x);
//     }
// }

int main()
{
    // skeleton_daemon();
    /* Catch, ignore and handle signals */
    signal(SIGTERM, niam);
    signal(SIGINT, niam);

    DBus::default_dispatcher = &dispatcher;

    DBus::Connection conn = DBus::Connection::SystemBus();
    conn.request_name(GNOME_TIME_SERVER_NAME, true);

    GnomeTimeServer server(conn);

    dispatcher.enter();

    return 0;
}