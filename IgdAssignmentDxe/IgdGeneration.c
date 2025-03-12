/** @file

  Internal functions for determining IGD generation.

  Copyright (c) 2025, Tomita Moeko <tomitamoeko@gmail.com>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "IgdGeneration.h"

/**
  Get the generation of IGD device based on Device ID

  @param  DeviceId  IGD Device ID

  @retval  >0     IGD device generation
  @retval  -1     IGD device is unsupported
**/
INTN
GetIgdGeneration (
  IN UINT16 DeviceId
  )
{
    /*
     * Device IDs for Broxton/Apollo Lake are 0x0a84, 0x1a84, 0x1a85, 0x5a84
     * and 0x5a85, match bit 11:1 here
     * Prefix 0x0a is taken by Haswell, this rule should be matched first.
     */
    if ((DeviceId & 0xffe) == 0xa84) {
        return 9;
    }

    switch (DeviceId & 0xff00) {
    case 0x0100:    /* SandyBridge, IvyBridge */
        return 6;
    case 0x0400:    /* Haswell */
    case 0x0a00:    /* Haswell */
    case 0x0c00:    /* Haswell */
    case 0x0d00:    /* Haswell */
    case 0x0f00:    /* Valleyview/Bay Trail */
        return 7;
    case 0x1600:    /* Broadwell */
    case 0x2200:    /* Cherryview */
        return 8;
    case 0x1900:    /* Skylake */
    case 0x3100:    /* Gemini Lake */
    case 0x5900:    /* Kaby Lake */
    case 0x3e00:    /* Coffee Lake */
    case 0x9B00:    /* Comet Lake */
        return 9;
    case 0x8A00:    /* Ice Lake */
    case 0x4500:    /* Elkhart Lake */
    case 0x4E00:    /* Jasper Lake */
        return 11;
    case 0x9A00:    /* Tiger Lake */
    case 0x4C00:    /* Rocket Lake */
    case 0x4600:    /* Alder Lake */
    case 0xA700:    /* Raptor Lake */
        return 12;
    }

    /* Unknown device */
    return -1;
}
