/** @file

  Internal functions for determining IGD generation.

  Copyright (c) 2025, Tomita Moeko <tomitamoeko@gmail.com>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <Library/DebugLib.h>

#include "IgdGeneration.h"
#include "IgdPciIds.h"

typedef struct {
  UINT16  DeviceId;
  UINTN   Gen;
} IGD_DEVICE_INFO;

#define IGD_DEVICE(_id, _gen) { \
  .DeviceId = _id, \
  .Gen = _gen \
}

STATIC CONST IGD_DEVICE_INFO IgdDeviceTable[] = {
  INTEL_SNB_IDS(IGD_DEVICE, 6),
  INTEL_IVB_IDS(IGD_DEVICE, 6),
  INTEL_HSW_IDS(IGD_DEVICE, 7),
  INTEL_VLV_IDS(IGD_DEVICE, 7),
  INTEL_BDW_IDS(IGD_DEVICE, 8),
  INTEL_CHV_IDS(IGD_DEVICE, 8),
  INTEL_SKL_IDS(IGD_DEVICE, 9),
  INTEL_BXT_IDS(IGD_DEVICE, 9),
  INTEL_KBL_IDS(IGD_DEVICE, 9),
  INTEL_CFL_IDS(IGD_DEVICE, 9),
  INTEL_WHL_IDS(IGD_DEVICE, 9),
  INTEL_CML_IDS(IGD_DEVICE, 9),
  INTEL_GLK_IDS(IGD_DEVICE, 9),
  INTEL_ICL_IDS(IGD_DEVICE, 11),
  INTEL_EHL_IDS(IGD_DEVICE, 11),
  INTEL_JSL_IDS(IGD_DEVICE, 11),
  INTEL_TGL_IDS(IGD_DEVICE, 12),
  INTEL_RKL_IDS(IGD_DEVICE, 12),
  INTEL_ADLS_IDS(IGD_DEVICE, 12),
  INTEL_ADLP_IDS(IGD_DEVICE, 12),
  INTEL_ADLN_IDS(IGD_DEVICE, 12),
  INTEL_RPLS_IDS(IGD_DEVICE, 12),
  INTEL_RPLU_IDS(IGD_DEVICE, 12),
  INTEL_RPLP_IDS(IGD_DEVICE, 12),
};

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
  INTN    Gen = -1;
  UINTN   Index;

  for (Index = 0; Index < ARRAY_SIZE(IgdDeviceTable); Index++) {
    if (DeviceId == IgdDeviceTable[Index].DeviceId) {
      Gen = IgdDeviceTable[Index].Gen;
    }
  }

  DEBUG ((DEBUG_INFO, "%a: Device: %x, Generation: %d\n", __FUNCTION__, DeviceId, Gen));

  /* Unknown device */
  return Gen;
}
