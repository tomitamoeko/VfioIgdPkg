/** @file

  Internal function declarations for determining IGD generation.

  Copyright (c) 2025, Tomita Moeko <tomitamoeko@gmail.com>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
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
