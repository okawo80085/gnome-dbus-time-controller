#include "gnome-time-server.hpp"
// #include "valid_tzs.hpp"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/timex.h>
#include <sys/types.h>
#include <unistd.h>
// #include "timespec.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/rtc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <iostream>

typedef uint64_t usec_t;
typedef uint64_t nsec_t;

#define LOG_STREAM std::cout << "log::" << __FUNCTION__ << ": "

#define USEC_INFINITY ((usec_t)UINT64_MAX)
#define NSEC_INFINITY ((nsec_t)UINT64_MAX)

#define MSEC_PER_SEC 1000ULL
#define USEC_PER_SEC ((usec_t)1000000ULL)
#define USEC_PER_MSEC ((usec_t)1000ULL)
#define NSEC_PER_SEC ((nsec_t)1000000000ULL)
#define NSEC_PER_MSEC ((nsec_t)1000000ULL)
#define NSEC_PER_USEC ((nsec_t)1000ULL)

#define USEC_PER_MINUTE ((usec_t)(60ULL * USEC_PER_SEC))
#define NSEC_PER_MINUTE ((nsec_t)(60ULL * NSEC_PER_SEC))
#define USEC_PER_HOUR ((usec_t)(60ULL * USEC_PER_MINUTE))
#define NSEC_PER_HOUR ((nsec_t)(60ULL * NSEC_PER_MINUTE))
#define USEC_PER_DAY ((usec_t)(24ULL * USEC_PER_HOUR))
#define NSEC_PER_DAY ((nsec_t)(24ULL * NSEC_PER_HOUR))
#define USEC_PER_WEEK ((usec_t)(7ULL * USEC_PER_DAY))
#define NSEC_PER_WEEK ((nsec_t)(7ULL * NSEC_PER_DAY))
#define USEC_PER_MONTH ((usec_t)(2629800ULL * USEC_PER_SEC))
#define NSEC_PER_MONTH ((nsec_t)(2629800ULL * NSEC_PER_SEC))
#define USEC_PER_YEAR ((usec_t)(31557600ULL * USEC_PER_SEC))
#define NSEC_PER_YEAR ((nsec_t)(31557600ULL * NSEC_PER_SEC))

/* We assume a maximum timezone length of 6. TZNAME_MAX is not defined on Linux,
 * but glibc internally initializes this to 6. Let's rely on that. */
#define FORMAT_TIMESTAMP_MAX (3U + 1U + 10U + 1U + 8U + 1U + 6U + 1U + 6U + 1U)
#define FORMAT_TIMESTAMP_RELATIVE_MAX 256U
#define FORMAT_TIMESPAN_MAX 64U

#define TIME_T_MAX (time_t)((UINTMAX_C(1) << ((sizeof(time_t) << 3) - 1)) - 1)

struct tm* localtime_or_gmtime_r(const time_t* t, struct tm* tm, bool utc)
{
    assert(t);
    assert(tm);

    return utc ? gmtime_r(t, tm) : localtime_r(t, tm);
}

static clockid_t map_clock_id(clockid_t c)
{

    /* Some more exotic archs (s390, ppc, …) lack the "ALARM" flavour of the
     * clocks. Thus, clock_gettime() will fail for them. Since they are
     * essentially the same as their non-ALARM pendants (their only difference
     * is when timers are set on them), let's just map them accordingly. This
     * way, we can get the correct time even on those archs. */

    switch (c) {

    case CLOCK_BOOTTIME_ALARM:
        return CLOCK_BOOTTIME;

    case CLOCK_REALTIME_ALARM:
        return CLOCK_REALTIME;

    default:
        return c;
    }
}

usec_t timespec_load(const struct timespec* ts)
{
    assert(ts);

    if (ts->tv_sec < 0 || ts->tv_nsec < 0)
        return USEC_INFINITY;

    if ((usec_t)ts->tv_sec
        > (UINT64_MAX - (ts->tv_nsec / NSEC_PER_USEC)) / USEC_PER_SEC)
        return USEC_INFINITY;

    return (usec_t)ts->tv_sec * USEC_PER_SEC
        + (usec_t)ts->tv_nsec / NSEC_PER_USEC;
}

struct timespec* timespec_store(struct timespec* ts, usec_t u)
{
    assert(ts);

    if (u == USEC_INFINITY || u / USEC_PER_SEC >= TIME_T_MAX) {
        ts->tv_sec = (time_t)-1;
        ts->tv_nsec = -1L;
        return ts;
    }

    ts->tv_sec = (time_t)(u / USEC_PER_SEC);
    ts->tv_nsec = (long)((u % USEC_PER_SEC) * NSEC_PER_USEC);

    return ts;
}

usec_t now(clockid_t clock_id)
{
    struct timespec ts;

    assert(clock_gettime(map_clock_id(clock_id), &ts) == 0);

    return timespec_load(&ts);
}

static inline int negative_errno(void)
{
    /* This helper should be used to shut up gcc if you know 'errno' is
     * negative. Instead of "return -errno;", use "return negative_errno();"
     * It will suppress bogus gcc warnings in case it assumes 'errno' might
     * be 0 and thus the caller's error-handling might not be triggered. */
    // assert_return(errno > 0, -EINVAL);
    return -errno;
}

static inline int RET_NERRNO(int ret)
{

    /* Helper to wrap system calls in to make them return negative errno errors.
     * This brings system call error handling in sync with how we usually handle
     * errors in our own code, i.e. with immediate returning of negative errno.
     * Usage is like this:
     *
     *     …
     *     r = RET_NERRNO(unlink(t));
     *     …
     *
     * or
     *
     *     …
     *     fd = RET_NERRNO(open("/etc/fstab", O_RDONLY|O_CLOEXEC));
     *     …
     */

    if (ret < 0)
        return negative_errno();

    return ret;
}

time_t mktime_or_timegm(struct tm* tm, bool utc)
{
    assert(tm);

    return utc ? timegm(tm) : mktime(tm);
}

int clock_get_hwclock(struct tm* tm)
{
    int fd = -EBADF;

    assert(tm);

    fd = open("/dev/rtc", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -errno;

    /* This leaves the timezone fields of struct tm
     * uninitialized! */
    if (ioctl(fd, RTC_RD_TIME, tm) < 0) {
        close(fd);
        return -errno;
    }

    /* We don't know daylight saving, so we reset this in order not
     * to confuse mktime(). */
    tm->tm_isdst = -1;

    return RET_NERRNO(close(fd));
}

int clock_set_hwclock(const struct tm* tm)
{
    int fd = -EBADF;

    assert(tm);

    fd = open("/dev/rtc", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -errno;

    int ret = ioctl(fd, RTC_SET_TIME, tm);
    if (0 != ret) {
        close(fd);
        return -errno;
    }

    return RET_NERRNO(close(fd));
}

int clock_set_timezone(int* ret_minutesdelta)
{
    struct timespec ts;
    struct tm tm;
    int minutesdelta;
    struct timezone tz;

    assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);
    assert(localtime_r(&ts.tv_sec, &tm));
    minutesdelta = tm.tm_gmtoff / 60;

    tz = (struct timezone){
        .tz_minuteswest = -minutesdelta, .tz_dsttime = 0, /* DST_NONE */
    };

    /* If the RTC does not run in UTC but in local time, the very first call to
     * settimeofday() will set the kernel's timezone and will warp the system
     * clock, so that it runs in UTC instead of the local time we have read from
     * the RTC. */
    if (settimeofday(NULL, &tz) < 0)
        return -errno;

    if (ret_minutesdelta)
        *ret_minutesdelta = minutesdelta;

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// actual service class
////////////////////////////////////////////////////////////////////////////////////////

GnomeTimeServer::GnomeTimeServer(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, GNOME_TIME_SERVER_PATH)
{
    getTimezonesData();
    m_current_timezone = getTimezone();
    m_local_rtc = getLocalRTC();
    LOG_STREAM << "Init: timezone=" << m_current_timezone
               << ", local_rtc=" << m_local_rtc
               << ", available_timezones_count=" << m_valid_timezones.size()
               << "\n";
}

void GnomeTimeServer::SetTime(
    const int64_t& usec_utc, const bool& relative, const bool& interactive)
{
    // sd_bus *bus = sd_bus_message_get_bus(m);
    char buf[256];
    int r;
    // Context *c = ASSERT_PTR(userdata);
    int64_t utc = usec_utc;
    struct timespec ts;
    usec_t start;
    struct tm tm;

    // assert(m);

    // if (c->slot_job_removed)
    //         return sd_bus_error_set(error,
    //         BUS_ERROR_AUTOMATIC_TIME_SYNC_ENABLED, "Previous request is not
    //         finished, refusing.");

    // r = context_update_ntp_status(c, bus, m);
    // if (r < 0)
    //         return sd_bus_error_set_errnof(error, r, "Failed to update
    //         context: %m");

    // if (context_ntp_service_is_active(c) > 0)
    //         return sd_bus_error_set(error,
    //         BUS_ERROR_AUTOMATIC_TIME_SYNC_ENABLED, "Automatic time
    //         synchronization is enabled");

    /* this only gets used if dbus does not provide a timestamp */
    start = now(CLOCK_MONOTONIC);

    // r = sd_bus_message_read(m, "xbb", &utc, &relative, &interactive);
    // if (r < 0)
    //         return r;

    if (!relative && utc <= 0)
        throw DBus::Error(
            "org.freedesktop.timedate1.ErrorInvalidArgs",
            "Invalid absolute time");

    if (relative && utc == 0)
        return;

    if (relative) {
        usec_t n, x;

        n = now(CLOCK_REALTIME);
        x = n + utc;

        if ((utc > 0 && x < n) || (utc < 0 && x > n))
            throw DBus::Error(
                "org.freedesktop.timedate1.ErrorInvalidArgs",
                "Time value overflow");

        timespec_store(&ts, x);
    } else
        timespec_store(&ts, (usec_t)utc);

    // r = bus_verify_polkit_async(
    //                 m,
    //                 CAP_SYS_TIME,
    //                 "org.freedesktop.timedate1.set-time",
    //                 NULL,
    //                 interactive,
    //                 UID_INVALID,
    //                 &c->polkit_registry,
    //                 error);
    // if (r < 0)
    //         return r;
    // if (r == 0)
    //         return 1;

    // /* adjust ts for time spent in program */
    // r = sd_bus_message_get_monotonic_usec(m, &start);
    // /* when sd_bus_message_get_monotonic_usec() returns -ENODATA it does not
    // modify &start */ if (r < 0 && r != -ENODATA)
    //         return r;

    timespec_store(&ts, timespec_load(&ts) + (now(CLOCK_MONOTONIC) - start));

    /* Set system clock */
    if (clock_settime(CLOCK_REALTIME, &ts) < 0) {
        LOG_STREAM << "Failed to set local time: " << errno << "\n";
        throw DBus::Error(
            "org.freedesktop.timedate1.Error", "Failed to set local time");
    }

    m_current_time = timespec_load(&ts);
    TimeUSec = m_current_time;

    /* Sync down to RTC */
    localtime_or_gmtime_r(&ts.tv_sec, &tm, !m_local_rtc);

    r = clock_set_hwclock(&tm);
    if (r < 0)
        LOG_STREAM << "Failed to update hardware clock, ignoring: " << r
                   << "\n";

    RTCTimeUSec = timespec_load(&ts);

    LOG_STREAM << "set clock to new time: " << usec_utc << "\n";
}

void GnomeTimeServer::SetTimezone(
    const std::string& timezone, const bool& interactive)
{
    int r;
    if (!isTimezoneValid(timezone)) {
        throw DBus::Error(
            "org.freedesktop.timedate1.ErrorInvalidArgs", "Invalid timezone");
        return;
    }

    if (timezone == m_current_timezone) {
        LOG_STREAM << "Timezone already set: "
                   << "\n";
        return;
    }

    /* 1. Write new configuration file */
    writeDataTimezone();

    /* 2. Make glibc notice the new timezone */
    tzset();

    /* 3. Tell the kernel our timezone */
    r = clock_set_timezone(NULL);
    if (r < 0)
        LOG_STREAM << "Faled to tell kernel about timezone, ignoring: " << r
                   << "\n";

    if (m_local_rtc) {
        struct timespec ts;
        struct tm tm;

        /* 4. Sync RTC from system clock, with the new delta */
        assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);
        assert(localtime_r(&ts.tv_sec, &tm));

        r = clock_set_hwclock(&tm);
        if (r < 0)
            LOG_STREAM << "Failed to sync time to hardware clock, ignoring: "
                       << r << "\n";
    }

    LOG_STREAM << "Timezone set to '" << timezone << "'\n";

    m_current_timezone = timezone;
    Timezone = m_current_timezone;
}

void GnomeTimeServer::SetLocalRTC(
    const bool& local_rtc, const bool& fix_system, const bool& interactive)
{
    struct timespec ts;

    int r;

    if (local_rtc == m_local_rtc && !fix_system)
        return;

    if (local_rtc != m_local_rtc) {
        m_local_rtc = local_rtc;

        /* 1. Write new configuration file */
        writeDataLocalRTC();
    }

    /* 2. Tell the kernel our timezone */
    r = clock_set_timezone(NULL);
    if (r < 0)
        LOG_STREAM << "Failed to tell kernel about timezone, ignoring: " << r
                   << "\n";

    /* 3. Synchronize clocks */
    assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);

    if (fix_system) {
        struct tm tm;

        /* Sync system clock from RTC; first, initialize the timezone fields of
         * struct tm. */
        localtime_or_gmtime_r(&ts.tv_sec, &tm, !m_local_rtc);

        /* Override the main fields of struct tm, but not the timezone fields */
        r = clock_get_hwclock(&tm);
        if (r < 0)
            LOG_STREAM << "Failed to get hardware clock, ignoring: " << r
                       << "\n";
        else {
            /* And set the system clock with this */
            ts.tv_sec = mktime_or_timegm(&tm, !m_local_rtc);

            if (clock_settime(CLOCK_REALTIME, &ts) < 0)
                LOG_STREAM << "Failed to update system clock, ignoring: "
                           << errno << "\n";
        }

    } else {
        struct tm tm;

        /* Sync RTC from system clock */
        localtime_or_gmtime_r(&ts.tv_sec, &tm, !m_local_rtc);

        r = clock_set_hwclock(&tm);
        if (r < 0)
            LOG_STREAM << "Failed to sync time to hardware clock, ignoring: "
                       << r << "\n";
    }

    LOG_STREAM << "RTC configured to " << (m_local_rtc ? "local" : "UTC")
               << "\n";

    LocalRTC = m_local_rtc;
}

void GnomeTimeServer::SetNTP(const bool& use_ntp, const bool& interactive)
{
    LOG_STREAM << "failed\n";
    throw DBus::Error(
        "org.freedesktop.timedate1.ErrorMethodNotSupported",
        "method not implemeted yet");
}

std::vector<std::string> GnomeTimeServer::ListTimezones()
{
    LOG_STREAM << "Returned " << m_valid_timezones.size() << " timezones\n";

    return m_valid_timezones;
}