/** @file
  This driver enables Intel Graphics Device (IGD) assignment with vfio-pci
  according to QEMU's "docs/igd-assign.txt" specification.

  Copyright (C) 2018, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <IndustryStandard/Pci22.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/QemuFwCfgLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/PciIo.h>

#include <IndustryStandard/AssignedIgd.h>
#include <IndustryStandard/IgdOpRegion.h>

#include "IgdGeneration.h"

//
// structure that collects information from PCI config space that is needed to
// evaluate whether IGD assignment applies to the device
//
typedef struct {
  UINT16 VendorId;
  UINT16 DeviceId;
  UINT8  ClassCode[3];
  UINTN  Segment;
  UINTN  Bus;
  UINTN  Device;
  UINTN  Function;
  INTN   Generation;
  CHAR8  Name[sizeof "0000:00:02.0"];
} CANDIDATE_PCI_INFO;

//
// selector and size of ASSIGNED_IGD_FW_CFG_OPREGION
//
STATIC FIRMWARE_CONFIG_ITEM mOpRegionItem;
STATIC UINTN                mOpRegionSize;
//
// value read from ASSIGNED_IGD_FW_CFG_BDSM_SIZE, converted to UINTN
//
STATIC UINTN                mBdsmSize;
//
// gBS->LocateProtocol() helper for finding the next unhandled PciIo instance
//
STATIC VOID                 *mPciIoTracker;


/**
  Populate the CANDIDATE_PCI_INFO structure for a PciIo protocol instance.

  @param[in] PciIo     EFI_PCI_IO_PROTOCOL instance to interrogate.

  @param[out] PciInfo  CANDIDATE_PCI_INFO structure to fill.

  @retval EFI_SUCCESS  PciInfo has been filled in. PciInfo->Name has been set
                       to the empty string.

  @return              Error codes from PciIo->Pci.Read() and
                       PciIo->GetLocation(). The contents of PciInfo are
                       indeterminate.
**/
STATIC
EFI_STATUS
InitPciInfo (
  IN  EFI_PCI_IO_PROTOCOL *PciIo,
  OUT CANDIDATE_PCI_INFO  *PciInfo
  )
{
  EFI_STATUS Status;

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint16,
                        PCI_VENDOR_ID_OFFSET,
                        1,                    // Count
                        &PciInfo->VendorId
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint16,
                        PCI_DEVICE_ID_OFFSET,
                        1,                    // Count
                        &PciInfo->DeviceId
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint8,
                        PCI_CLASSCODE_OFFSET,
                        sizeof PciInfo->ClassCode,
                        PciInfo->ClassCode
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PciIo->GetLocation (
                    PciIo,
                    &PciInfo->Segment,
                    &PciInfo->Bus,
                    &PciInfo->Device,
                    &PciInfo->Function
                    );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PciInfo->Name[0] = '\0';
  return EFI_SUCCESS;
}


/**
  Format and get the debug name of a CANDIDATE_PCI_INFO structure.

  param[in,out] PciInfo  If the PciInfo->Name member is an empty string, format
                         the PCI bus address of PciInfo into PciInfo->Name.
                         Otherwise, don't modify PciInfo.

  @return                PciInfo->Name
**/
#if !defined(MDEPKG_NDEBUG)
STATIC
CONST CHAR8 *
GetPciName (
  IN OUT CANDIDATE_PCI_INFO *PciInfo
  )
{
  if (PciInfo->Name[0] == '\0') {
    AsciiSPrint (
      PciInfo->Name,
      sizeof PciInfo->Name,
      "%04x:%02x:%02x.%x",
      (UINT16)PciInfo->Segment,
      (UINT8)PciInfo->Bus,
      (UINT8)PciInfo->Device,
      (UINT32)PciInfo->Function & 0xf
      );
  }
  return PciInfo->Name;
}
#endif

/**
  Allocate memory in the 32-bit address space, with the requested UEFI memory
  type and the requested alignment.

  @param[in] MemoryType        Assign MemoryType to the allocated pages as
                               memory type.

  @param[in] NumberOfPages     The number of pages to allocate.

  @param[in] AlignmentInPages  On output, Address will be a whole multiple of
                               EFI_PAGES_TO_SIZE (AlignmentInPages).
                               AlignmentInPages must be a power of two.

  @param[out] Address          Base address of the allocated area.

  @retval EFI_SUCCESS            Allocation successful.

  @retval EFI_INVALID_PARAMETER  AlignmentInPages is not a power of two (a
                                 special case of which is when AlignmentInPages
                                 is zero).

  @retval EFI_OUT_OF_RESOURCES   Integer overflow detected.

  @return                        Error codes from gBS->AllocatePages().
**/
STATIC
EFI_STATUS
Allocate32BitAlignedPagesWithType (
  IN  EFI_MEMORY_TYPE      MemoryType,
  IN  UINTN                NumberOfPages,
  IN  UINTN                AlignmentInPages,
  OUT EFI_PHYSICAL_ADDRESS *Address
  )
{
  EFI_STATUS           Status;
  EFI_PHYSICAL_ADDRESS PageAlignedAddress;
  EFI_PHYSICAL_ADDRESS FullyAlignedAddress;
  UINTN                BottomPages;
  UINTN                TopPages;

  //
  // AlignmentInPages must be a power of two.
  //
  if (AlignmentInPages == 0 ||
      (AlignmentInPages & (AlignmentInPages - 1)) != 0) {
    return EFI_INVALID_PARAMETER;
  }
  //
  // (NumberOfPages + (AlignmentInPages - 1)) must not overflow UINTN.
  //
  if (AlignmentInPages - 1 > MAX_UINTN - NumberOfPages) {
    return EFI_OUT_OF_RESOURCES;
  }
  //
  // EFI_PAGES_TO_SIZE (AlignmentInPages) must not overflow UINTN.
  //
  if (AlignmentInPages > (MAX_UINTN >> EFI_PAGE_SHIFT)) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Allocate with sufficient padding for alignment.
  //
  PageAlignedAddress = BASE_4GB - 1;
  Status = gBS->AllocatePages (
                  AllocateMaxAddress,
                  MemoryType,
                  NumberOfPages + (AlignmentInPages - 1),
                  &PageAlignedAddress
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  FullyAlignedAddress = ALIGN_VALUE (
                          PageAlignedAddress,
                          (UINT64)EFI_PAGES_TO_SIZE (AlignmentInPages)
                          );

  //
  // Release bottom and/or top padding.
  //
  BottomPages = EFI_SIZE_TO_PAGES (
                  (UINTN)(FullyAlignedAddress - PageAlignedAddress)
                  );
  TopPages = (AlignmentInPages - 1) - BottomPages;
  if (BottomPages > 0) {
    Status = gBS->FreePages (PageAlignedAddress, BottomPages);
    ASSERT_EFI_ERROR (Status);
  }
  if (TopPages > 0) {
    Status = gBS->FreePages (
                    FullyAlignedAddress + EFI_PAGES_TO_SIZE (NumberOfPages),
                    TopPages
                    );
    ASSERT_EFI_ERROR (Status);
  }

  *Address = FullyAlignedAddress;
  return EFI_SUCCESS;
}


/**
  Set up the OpRegion for the device identified by PciIo.

  @param[in] PciIo        The device to set up the OpRegion for.

  @param[in,out] PciInfo  On input, PciInfo must have been initialized from
                          PciIo with InitPciInfo(). SetupOpRegion() may call
                          GetPciName() on PciInfo, possibly modifying it.

  @retval EFI_SUCCESS            OpRegion setup successful.

  @retval EFI_INVALID_PARAMETER  mOpRegionSize is zero.

  @return                        Error codes propagated from underlying
                                 functions.
**/
STATIC
EFI_STATUS
SetupOpRegion (
  IN     EFI_PCI_IO_PROTOCOL *PciIo,
  IN OUT CANDIDATE_PCI_INFO  *PciInfo
  )
{
  UINTN                OpRegionPages;
  UINTN                OpRegionResidual;
  EFI_STATUS           Status;
  EFI_PHYSICAL_ADDRESS Address;
  UINT8                *BytePointer;

  if (mOpRegionSize == 0) {
    return EFI_INVALID_PARAMETER;
  }
  OpRegionPages = EFI_SIZE_TO_PAGES (mOpRegionSize);
  OpRegionResidual = EFI_PAGES_TO_SIZE (OpRegionPages) - mOpRegionSize;

  //
  // While QEMU's "docs/igd-assign.txt" specifies reserved memory, Intel's IGD
  // OpRegion spec refers to ACPI NVS.
  //
  Status = Allocate32BitAlignedPagesWithType (
             EfiACPIMemoryNVS,
             OpRegionPages,
             1,                // AlignmentInPages
             &Address
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %a: failed to allocate OpRegion: %r\n",
      __FUNCTION__, GetPciName (PciInfo), Status));
    return Status;
  }

  //
  // Download OpRegion contents from fw_cfg, zero out trailing portion.
  //
  BytePointer = (UINT8 *)(UINTN)Address;
  QemuFwCfgSelectItem (mOpRegionItem);
  QemuFwCfgReadBytes (mOpRegionSize, BytePointer);
  ZeroMem (BytePointer + mOpRegionSize, OpRegionResidual);

  //
  // Write address of OpRegion to PCI config space.
  //
  Status = PciIo->Pci.Write (
                        PciIo,
                        EfiPciIoWidthUint32,
                        ASSIGNED_IGD_PCI_ASLS_OFFSET,
                        1,                            // Count
                        &Address
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %a: failed to write OpRegion address: %r\n",
      __FUNCTION__, GetPciName (PciInfo), Status));
    goto FreeOpRegion;
  }

  DEBUG ((DEBUG_INFO, "%a: %a: OpRegion @ 0x%Lx size 0x%Lx version %d.%d.%d\n",
    __FUNCTION__,
    GetPciName (PciInfo), Address, (UINT64)mOpRegionSize,
    ((IGD_OPREGION_HEADER*)BytePointer)->OVER >> 24,
    ((IGD_OPREGION_HEADER*)BytePointer)->OVER >> 16 & 0xff,
    ((IGD_OPREGION_HEADER*)BytePointer)->OVER >> 8 & 0xff));
  return EFI_SUCCESS;

FreeOpRegion:
  gBS->FreePages (Address, OpRegionPages);
  return Status;
}


/**
  Set up stolen memory for the device identified by PciIo.

  @param[in] PciIo        The device to set up stolen memory for.

  @param[in,out] PciInfo  On input, PciInfo must have been initialized from
                          PciIo with InitPciInfo(). SetupStolenMemory() may
                          call GetPciName() on PciInfo, possibly modifying it.

  @retval EFI_SUCCESS            Stolen memory setup successful.

  @retval EFI_INVALID_PARAMETER  mBdsmSize is zero.

  @return                        Error codes propagated from underlying
                                 functions.
**/
STATIC
EFI_STATUS
SetupStolenMemory (
  IN     EFI_PCI_IO_PROTOCOL *PciIo,
  IN OUT CANDIDATE_PCI_INFO  *PciInfo
  )
{
  UINTN                BdsmPages;
  EFI_STATUS           Status;
  EFI_PHYSICAL_ADDRESS Address;

  if (mBdsmSize == 0) {
    return EFI_INVALID_PARAMETER;
  }
  BdsmPages = EFI_SIZE_TO_PAGES (mBdsmSize);

  Status = Allocate32BitAlignedPagesWithType (
             EfiReservedMemoryType,
             BdsmPages,
             EFI_SIZE_TO_PAGES ((UINTN)ASSIGNED_IGD_BDSM_ALIGN),
             &Address
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %a: failed to allocate stolen memory: %r\n",
      __FUNCTION__, GetPciName (PciInfo), Status));
    return Status;
  }

  //
  // Zero out stolen memory.
  //
  ZeroMem ((VOID *)(UINTN)Address, EFI_PAGES_TO_SIZE (BdsmPages));

  //
  // Write address of stolen memory to PCI config space.
  //
  if (PciInfo->Generation < 11) {
    Status = PciIo->Pci.Write (
                          PciIo,
                          EfiPciIoWidthUint32,
                          ASSIGNED_IGD_PCI_BDSM_OFFSET,
                          1,                            // Count
                          &Address
                          );
  } else {
    Status = PciIo->Pci.Write (
                          PciIo,
                          EfiPciIoWidthUint64,
                          ASSIGNED_IGD_PCI_BDSM64_OFFSET,
                          1,                            // Count
                          &Address
                          );
  }
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %a: failed to write stolen memory address: %r\n",
      __FUNCTION__, GetPciName (PciInfo), Status));
    goto FreeStolenMemory;
  }

  DEBUG ((DEBUG_INFO, "%a: %a: stolen memory @ 0x%Lx size 0x%Lx\n",
    __FUNCTION__, GetPciName (PciInfo), Address, (UINT64)mBdsmSize));
  return EFI_SUCCESS;

FreeStolenMemory:
  gBS->FreePages (Address, BdsmPages);
  return Status;
}


/**
  Process any PciIo protocol instances that may have been installed since the
  last invocation.

  @param[in] Event    Event whose notification function is being invoked.

  @param[in] Context  The pointer to the notification function's context.
**/
STATIC
VOID
EFIAPI
PciIoNotify (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  EFI_PCI_IO_PROTOCOL *PciIo;

  while (!EFI_ERROR (gBS->LocateProtocol (
                            &gEfiPciIoProtocolGuid,
                            mPciIoTracker,
                            (VOID **)&PciIo
                            ))) {
    EFI_STATUS         Status;
    CANDIDATE_PCI_INFO PciInfo;

    Status = InitPciInfo (PciIo, &PciInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: InitPciInfo (PciIo@%p): %r\n", __FUNCTION__,
        (VOID *)PciIo, Status));
      continue;
    }

    //
    // Check VendorId and ClassCode. These checks are necessary for both
    // OpRegion and stolen memory setup.
    //
    if (PciInfo.VendorId != ASSIGNED_IGD_PCI_VENDOR_ID ||
        PciInfo.ClassCode[2] != PCI_CLASS_DISPLAY ||
        PciInfo.ClassCode[1] != PCI_CLASS_DISPLAY_VGA ||
        PciInfo.ClassCode[0] != PCI_IF_VGA_VGA) {
      continue;
    }

    //
    // Check device generation
    //
    PciInfo.Generation = GetIgdGeneration (PciInfo.DeviceId);
    if (PciInfo.Generation == -1) {
      continue;
    }

    if (mOpRegionSize > 0) {
      SetupOpRegion (PciIo, &PciInfo);
    }

    //
    // Check Bus:Device.Function (Segment is ignored). This is necessary before
    // stolen memory setup.
    //
    if (PciInfo.Bus != ASSIGNED_IGD_PCI_BUS ||
        PciInfo.Device != ASSIGNED_IGD_PCI_DEVICE ||
        PciInfo.Function != ASSIGNED_IGD_PCI_FUNCTION) {
      continue;
    }

    if (mBdsmSize > 0) {
      SetupStolenMemory (PciIo, &PciInfo);
    }
  }
}


/**
  Entry point for this driver.

  @param[in] ImageHandle  Image handle of this driver.

  @param[in] SystemTable  Pointer to SystemTable.

  @retval EFI_SUCESS         Driver has loaded successfully.

  @retval EFI_UNSUPPORTED    No IGD assigned.

  @retval EFI_PROTOCOL_ERROR Invalid fw_cfg contents.

  @return                    Error codes propagated from underlying functions.
**/
EFI_STATUS
EFIAPI
IgdAssignmentEntry (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS           OpRegionStatus;
  EFI_STATUS           BdsmStatus;
  FIRMWARE_CONFIG_ITEM BdsmItem;
  UINTN                BdsmItemSize;
  EFI_STATUS           Status;
  EFI_EVENT            PciIoEvent;

  OpRegionStatus = QemuFwCfgFindFile (
                     ASSIGNED_IGD_FW_CFG_OPREGION,
                     &mOpRegionItem,
                     &mOpRegionSize
                     );
  BdsmStatus = QemuFwCfgFindFile (
                 ASSIGNED_IGD_FW_CFG_BDSM_SIZE,
                 &BdsmItem,
                 &BdsmItemSize
                 );
  //
  // If neither fw_cfg file is available, assume no IGD is assigned.
  //
  if (EFI_ERROR (OpRegionStatus) && EFI_ERROR (BdsmStatus)) {
    return EFI_UNSUPPORTED;
  }

  //
  // Require all fw_cfg files that are present to be well-formed.
  //
  if (!EFI_ERROR (OpRegionStatus) && mOpRegionSize == 0)  {
    DEBUG ((DEBUG_ERROR, "%a: %a: zero size\n", __FUNCTION__,
      ASSIGNED_IGD_FW_CFG_OPREGION));
    return EFI_PROTOCOL_ERROR;
  }

  if (!EFI_ERROR (BdsmStatus)) {
    UINT64 BdsmSize;

    if (BdsmItemSize != sizeof BdsmSize) {
      DEBUG ((DEBUG_ERROR, "%a: %a: invalid fw_cfg size: %Lu\n", __FUNCTION__,
        ASSIGNED_IGD_FW_CFG_BDSM_SIZE, (UINT64)BdsmItemSize));
      return EFI_PROTOCOL_ERROR;
    }
    QemuFwCfgSelectItem (BdsmItem);
    QemuFwCfgReadBytes (BdsmItemSize, &BdsmSize);

    if (BdsmSize == 0 || BdsmSize > MAX_UINTN) {
      DEBUG ((DEBUG_ERROR, "%a: %a: invalid value: %Lu\n", __FUNCTION__,
        ASSIGNED_IGD_FW_CFG_BDSM_SIZE, BdsmSize));
      return EFI_PROTOCOL_ERROR;
    }
    mBdsmSize = (UINTN)BdsmSize;
  }

  //
  // At least one valid fw_cfg file has been found.
  //
  ASSERT (mOpRegionSize > 0 || mBdsmSize > 0);

  //
  // Register PciIo protocol installation callback.
  //
  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  PciIoNotify,
                  NULL,              // Context
                  &PciIoEvent
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = gBS->RegisterProtocolNotify (
                  &gEfiPciIoProtocolGuid,
                  PciIoEvent,
                  &mPciIoTracker
                  );
  if (EFI_ERROR (Status)) {
    goto ClosePciIoEvent;
  }

  //
  // Kick the event for any existent PciIo protocol instances.
  //
  Status = gBS->SignalEvent (PciIoEvent);
  if (EFI_ERROR (Status)) {
    goto ClosePciIoEvent;
  }

  return EFI_SUCCESS;

ClosePciIoEvent:
  gBS->CloseEvent (PciIoEvent);

  return Status;
}
