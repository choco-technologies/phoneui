#ifndef BATTERY_LINE_H
#define BATTERY_LINE_H

#include <stddef.h>
#include "dmdrvi_ioctl.h"

/**
 * @brief The line under the quick settings tiles: how full the battery is and what it does, in words a person reads
 *        ("Battery 82%  ·  4 h 10 min left", "Battery 40%  ·  charging, full in 1 h 20 min", "Battery 100%  ·  full").
 *        What the battery does not say is left out.
 */
void phoneui_battery_line(const dmdrvi_battery_status_t *battery, char *out, size_t size);

#endif
