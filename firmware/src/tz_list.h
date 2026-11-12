#pragma once

struct TzEntry {
    const char* name;
    const char* posix;
};

// ~50 named timezones grouped by region.
// POSIX TZ strings: STDoffset[DST[offset][,start[/time],end[/time]]]
static const TzEntry tz_list[] = {
    // UTC
    { "UTC",                    "UTC0" },

    // Americas
    { "America/New_York",       "EST5EDT,M3.2.0,M11.1.0" },
    { "America/Chicago",        "CST6CDT,M3.2.0,M11.1.0" },
    { "America/Denver",         "MST7MDT,M3.2.0,M11.1.0" },
    { "America/Phoenix",        "MST7" },
    { "America/Los_Angeles",    "PST8PDT,M3.2.0,M11.1.0" },
    { "America/Anchorage",      "AKST9AKDT,M3.2.0,M11.1.0" },
    { "Pacific/Honolulu",       "HST10" },
    { "America/Toronto",        "EST5EDT,M3.2.0,M11.1.0" },
    { "America/Vancouver",      "PST8PDT,M3.2.0,M11.1.0" },
    { "America/Sao_Paulo",      "BRT3BRST,M10.3.0/0,M2.3.0/0" },
    { "America/Buenos_Aires",   "ART3" },
    { "America/Bogota",         "COT5" },
    { "America/Lima",           "PET5" },
    { "America/Santiago",       "CLT4CLST,M10.2.6/24,M3.2.6/24" },
    { "America/Mexico_City",    "CST6CDT,M4.1.0,M10.5.0" },
    { "America/Caracas",        "VET4" },

    // Europe
    { "Europe/London",          "GMT0BST,M3.5.0/1,M10.5.0" },
    { "Europe/Dublin",          "GMT0IST,M3.5.0/1,M10.5.0" },
    { "Europe/Lisbon",          "WET0WEST,M3.5.0/1,M10.5.0" },
    { "Europe/Paris",           "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Berlin",          "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Amsterdam",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Rome",            "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Madrid",          "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Stockholm",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Warsaw",          "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Helsinki",        "EET-2EEST,M3.5.0/3,M10.5.0/4" },
    { "Europe/Athens",          "EET-2EEST,M3.5.0/3,M10.5.0/4" },
    { "Europe/Bucharest",       "EET-2EEST,M3.5.0/3,M10.5.0/4" },
    { "Europe/Kiev",            "EET-2EEST,M3.5.0/3,M10.5.0/4" },
    { "Europe/Moscow",          "MSK-3" },
    { "Europe/Istanbul",        "TRT-3" },

    // Africa
    { "Africa/Cairo",           "EET-2" },
    { "Africa/Johannesburg",    "SAST-2" },
    { "Africa/Lagos",           "WAT-1" },
    { "Africa/Nairobi",         "EAT-3" },
    { "Africa/Casablanca",      "WET0" },

    // Asia
    { "Asia/Dubai",             "GST-4" },
    { "Asia/Karachi",           "PKT-5" },
    { "Asia/Kolkata",           "IST-5:30" },
    { "Asia/Dhaka",             "BDT-6" },
    { "Asia/Bangkok",           "ICT-7" },
    { "Asia/Jakarta",           "WIB-7" },
    { "Asia/Singapore",         "SGT-8" },
    { "Asia/Hong_Kong",         "HKT-8" },
    { "Asia/Shanghai",          "CST-8" },
    { "Asia/Taipei",            "CST-8" },
    { "Asia/Seoul",             "KST-9" },
    { "Asia/Tokyo",             "JST-9" },
    { "Asia/Vladivostok",       "VLAT-10" },
    { "Asia/Yekaterinburg",     "YEKT-5" },
    { "Asia/Novosibirsk",       "NOVT-7" },
    { "Asia/Irkutsk",           "IRKT-8" },
    { "Asia/Yakutsk",           "YAKT-9" },

    // Pacific
    { "Australia/Perth",        "AWST-8" },
    { "Australia/Adelaide",     "ACST-9:30ACDT,M10.1.0,M4.1.0/3" },
    { "Australia/Sydney",       "AEST-10AEDT,M10.1.0,M4.1.0/3" },
    { "Australia/Brisbane",     "AEST-10" },
    { "Pacific/Auckland",       "NZST-12NZDT,M9.5.0,M4.1.0/3" },
    { "Pacific/Fiji",           "FJT-12" },
    { "Pacific/Guam",           "ChST-10" },
};

static constexpr int tz_list_count = (int)(sizeof(tz_list) / sizeof(tz_list[0]));
