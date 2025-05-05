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

#ifndef _IGD_PRIVATE_H_
#define _IGD_PRIVATE_H_

#include <Uefi.h>
#include <Protocol/PciIo.h>

#define IGD_FLAG_BDSM_32BIT BIT0
#define IGD_FLAG_BDSM_64BIT BIT1

typedef UINTN (*GET_STOLEN_SIZE_FUNC)(EFI_PCI_IO_PROTOCOL *PciIo);

typedef struct {
  UINT32                Flags;
  GET_STOLEN_SIZE_FUNC  GetStolenSize;
} IGD_PRIVATE_DATA;

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
  );

#endif
