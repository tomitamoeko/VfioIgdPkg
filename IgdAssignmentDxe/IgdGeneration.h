/** @file

  Internal function declarations for determining IGD generation.

  Copyright (c) 2025, Tomita Moeko <tomitamoeko@gmail.com>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef IGD_GENERATION_H_
#define IGD_GENERATION_H_

#include <Uefi.h>

/**
  Get the generation of IGD device based on Device ID

  @param  DeviceId  IGD Device ID

  @retval  >0     IGD device generation
  @retval  -1     IGD device is unsupported
**/
INTN
GetIgdGeneration (
  IN UINT16 DeviceId
  );

#endif
