#include "battery_line.h"
#include "dmod.h"
#include <string.h>

/* A time of minutes as people say it: "35 min", "4 h 10 min", "about 2 days" */
static void time_text(unsigned minutes, char *out, size_t size)
{
    if (minutes < 60U)
        Dmod_SnPrintf(out, size, "%u min", minutes);
    else if (minutes < 48U * 60U)
        Dmod_SnPrintf(out, size, "%u h %u min", minutes / 60U, minutes % 60U);
    else
        Dmod_SnPrintf(out, size, "about %u days", (minutes + 12U * 60U) / (24U * 60U));
}

void phoneui_battery_line(const dmdrvi_battery_status_t *battery, char *out, size_t size)
{
    char time[24];
    if ((battery->valid & DMDRVI_BATTERY_VALID_STATE) != 0U && battery->state == DMDRVI_BATTERY_STATE_NOT_PRESENT)
    {
        Dmod_SnPrintf(out, size, "No battery");
        return;
    }
    if ((battery->valid & DMDRVI_BATTERY_VALID_PERCENT) == 0U)
    {
        Dmod_SnPrintf(out, size, "Battery");
        return;
    }
    unsigned percent = battery->percent;
    bool timed = (battery->valid & DMDRVI_BATTERY_VALID_TIME) != 0U;
    switch ((battery->valid & DMDRVI_BATTERY_VALID_STATE) != 0U ? battery->state : DMDRVI_BATTERY_STATE_UNKNOWN)
    {
        case DMDRVI_BATTERY_STATE_DISCHARGING:
            if (timed && battery->minutes_to_empty != 0U)
            {
                time_text(battery->minutes_to_empty, time, sizeof(time));
                Dmod_SnPrintf(out, size, "Battery %u%%  ·  %s left", percent, time);
            }
            else
                Dmod_SnPrintf(out, size, "Battery %u%%", percent);
            break;
        case DMDRVI_BATTERY_STATE_CHARGING:
            if (timed && battery->minutes_to_full != 0U)
            {
                time_text(battery->minutes_to_full, time, sizeof(time));
                Dmod_SnPrintf(out, size, "Battery %u%%  ·  charging, full in %s", percent, time);
            }
            else
                Dmod_SnPrintf(out, size, "Battery %u%%  ·  charging", percent);
            break;
        case DMDRVI_BATTERY_STATE_FULL:
            Dmod_SnPrintf(out, size, "Battery %u%%  ·  full", percent);
            break;
        case DMDRVI_BATTERY_STATE_IDLE:
            Dmod_SnPrintf(out, size, "Battery %u%%  ·  not charging", percent);
            break;
        default:
            Dmod_SnPrintf(out, size, "Battery %u%%", percent);
            break;
    }
}
