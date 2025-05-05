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

#include "IgdPrivate.h"
#include "IgdPciIds.h"

typedef struct {
  UINT16                  DeviceId;
  CONST IGD_PRIVATE_DATA  *Private;
} IGD_DEVICE_INFO;

#define IGD_DEVICE(Id, Priv) { \
  .DeviceId = Id, \
  .Private = Priv \
}

#define SNB_GMCH_CTRL           0x50
#define    SNB_GMCH_GMS_SHIFT   3 /* Graphics Mode Select */
#define    SNB_GMCH_GMS_MASK    0x1f
#define    BDW_GMCH_GMS_SHIFT   8
#define    BDW_GMCH_GMS_MASK    0xff

STATIC
UINTN
Gen6StolenSize (
  IN EFI_PCI_IO_PROTOCOL *PciIo
)
{
  UINT16 Gmch;
  UINT16 Gms;

  PciIo->Pci.Read (PciIo, EfiPciIoWidthUint16, SNB_GMCH_CTRL, 1, &Gmch);
  Gms = (Gmch >> SNB_GMCH_GMS_SHIFT) & SNB_GMCH_GMS_MASK;

  return Gms * SIZE_32MB;
}

STATIC
UINTN
Gen8StolenSize (
  IN EFI_PCI_IO_PROTOCOL *PciIo
)
{
  UINT16 Gmch;
  UINT16 Gms;

  PciIo->Pci.Read (PciIo, EfiPciIoWidthUint16, SNB_GMCH_CTRL, 1, &Gmch);
  Gms = (Gmch >> BDW_GMCH_GMS_SHIFT) & BDW_GMCH_GMS_MASK;

  return Gms * SIZE_32MB;
}

STATIC
UINTN
ChvStolenSize (
  IN EFI_PCI_IO_PROTOCOL *PciIo
)
{
  UINT16 Gmch;
  UINT16 Gms;

  PciIo->Pci.Read (PciIo, EfiPciIoWidthUint16, SNB_GMCH_CTRL, 1, &Gmch);
  Gms = (Gmch >> SNB_GMCH_GMS_SHIFT) & SNB_GMCH_GMS_MASK;

  /*
   * 0x0  to 0x10: 32MB increments starting at 0MB
   * 0x11 to 0x16: 4MB increments starting at 8MB
   * 0x17 to 0x1d: 4MB increments start at 36MB
   */
  if (Gms < 0x11) {
    return Gms * SIZE_32MB;
  } else if (Gms < 0x17) {
    return (Gms - 0x11) * SIZE_4MB + SIZE_8MB;
  } else {
    return (Gms - 0x17) * SIZE_4MB + SIZE_4MB + SIZE_32MB;
  }   
}

STATIC
UINTN
Gen9StolenSize (
  IN EFI_PCI_IO_PROTOCOL *PciIo
)
{
  UINT16 Gmch;
  UINT16 Gms;

  PciIo->Pci.Read (PciIo, EfiPciIoWidthUint16, SNB_GMCH_CTRL, 1, &Gmch);
  Gms = (Gmch >> BDW_GMCH_GMS_SHIFT) & BDW_GMCH_GMS_MASK;

  /* 0x0  to 0xef: 32MB increments starting at 0MB */
  /* 0xf0 to 0xfe: 4MB increments starting at 4MB */
  if (Gms < 0xf0) {
    return Gms * SIZE_32MB;
  } else {
    return (Gms - 0xf0) * SIZE_4MB + SIZE_4MB;
  }
}

STATIC CONST IGD_PRIVATE_DATA Gen6Private = {
  .Flags = IGD_FLAG_BDSM_32BIT,
  .GetStolenSize = Gen6StolenSize,
};

STATIC CONST IGD_PRIVATE_DATA Gen8Private = {
  .Flags = IGD_FLAG_BDSM_32BIT,
  .GetStolenSize = Gen8StolenSize,
};

STATIC CONST IGD_PRIVATE_DATA ChvPrivate = {
  .Flags = IGD_FLAG_BDSM_32BIT,
  .GetStolenSize = ChvStolenSize,
};

STATIC CONST IGD_PRIVATE_DATA Gen9Private = {
  .Flags = IGD_FLAG_BDSM_32BIT,
  .GetStolenSize = Gen9StolenSize,
};

STATIC CONST IGD_PRIVATE_DATA Gen11Private = {
  .Flags = IGD_FLAG_BDSM_64BIT,
  .GetStolenSize = Gen9StolenSize,
};

STATIC CONST IGD_DEVICE_INFO IgdDeviceTable[] = {
  INTEL_SNB_IDS(IGD_DEVICE, &Gen6Private),
  INTEL_IVB_IDS(IGD_DEVICE, &Gen6Private),
  INTEL_HSW_IDS(IGD_DEVICE, &Gen6Private),
  INTEL_VLV_IDS(IGD_DEVICE, &Gen6Private),
  INTEL_BDW_IDS(IGD_DEVICE, &Gen8Private),
  INTEL_CHV_IDS(IGD_DEVICE, &ChvPrivate),
  INTEL_SKL_IDS(IGD_DEVICE, &Gen9Private),
  INTEL_BXT_IDS(IGD_DEVICE, &Gen9Private),
  INTEL_KBL_IDS(IGD_DEVICE, &Gen9Private),
  INTEL_CFL_IDS(IGD_DEVICE, &Gen9Private),
  INTEL_WHL_IDS(IGD_DEVICE, &Gen9Private),
  INTEL_CML_IDS(IGD_DEVICE, &Gen9Private),
  INTEL_GLK_IDS(IGD_DEVICE, &Gen9Private),
  INTEL_ICL_IDS(IGD_DEVICE, &Gen11Private),
  INTEL_EHL_IDS(IGD_DEVICE, &Gen11Private),
  INTEL_JSL_IDS(IGD_DEVICE, &Gen11Private),
  INTEL_TGL_IDS(IGD_DEVICE, &Gen11Private),
  INTEL_RKL_IDS(IGD_DEVICE, &Gen11Private),
  INTEL_ADLS_IDS(IGD_DEVICE, &Gen11Private),
  INTEL_ADLP_IDS(IGD_DEVICE, &Gen11Private),
  INTEL_ADLN_IDS(IGD_DEVICE, &Gen11Private),
  INTEL_RPLS_IDS(IGD_DEVICE, &Gen11Private),
  INTEL_RPLU_IDS(IGD_DEVICE, &Gen11Private),
  INTEL_RPLP_IDS(IGD_DEVICE, &Gen11Private),
};

/**
  Get IGD device private data based on Device ID

  @param[in]  DeviceId      IGD Device ID
  @param[out] PrivateData   IGD Device private data

  @retval  EFI_SUCCESS      IGD device generation
  @retval  EFI_UNSUPPORTED  IGD device is unsupported
**/
EFI_STATUS
EFIAPI
GetIgdPrivateData (
  IN  UINT16                  DeviceId,
  OUT CONST IGD_PRIVATE_DATA  **PrivateData
  )
{
  UINTN   Index;

  for (Index = 0; Index < ARRAY_SIZE(IgdDeviceTable); Index++) {
    if (DeviceId == IgdDeviceTable[Index].DeviceId) {
      *PrivateData = IgdDeviceTable[Index].Private;
      DEBUG ((DEBUG_INFO, "%a: Device: %x, Flag: %x\n", __FUNCTION__,
              DeviceId, (*PrivateData)->Flags));
      return EFI_SUCCESS;
    }
  }

  return EFI_UNSUPPORTED;
}
