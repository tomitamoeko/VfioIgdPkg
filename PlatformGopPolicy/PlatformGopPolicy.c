/*++

Copyright (c)  1999  - 2019, Intel Corporation. All rights reserved
                                                                                   
  This program and the accompanying materials are licensed and made available under
  the terms and conditions of the BSD License that accompanies this distribution.  
  The full text of the license may be found at                                     
  http://opensource.org/licenses/bsd-license.php.                                  
                                                                                   
  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,            
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.    
                                                                                   

--*/

/** @file
**/

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Protocol/FirmwareVolume2.h>
#include <Protocol/PlatformGopPolicy.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/PciLib.h>

#include <IndustryStandard/AssignedIgd.h>
#include <IndustryStandard/IgdOpRegion.h>

PLATFORM_GOP_POLICY_PROTOCOL  mPlatformGopPolicy;
EFI_PHYSICAL_ADDRESS mVbt;

//
// Function implementations
//

/**
  The function will execute with as the platform policy, and gives
  the Platform Lid Status. IBV/OEM can customize this code for their specific
  policy action.

  @param CurrentLidStatus  Gives the current LID Status

  @retval EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
GetPlatformLidStatus (
   OUT LID_STATUS *CurrentLidStatus
)
{
  return EFI_UNSUPPORTED;
}

/**
  The function will execute and gives the Video Bios Table Size and Address.

  @param VbtAddress  Gives the Physical Address of Video BIOS Table

  @param VbtSize     Gives the Size of Video BIOS Table

  @retval EFI_STATUS.

**/

EFI_STATUS
EFIAPI
GetVbtData (
   OUT EFI_PHYSICAL_ADDRESS *VbtAddress,
   OUT UINT32 *VbtSize
)
{
  IGD_OPREGION_STRUCTURE *OpRegion;
  EFI_STATUS Status = EFI_INVALID_PARAMETER;
  UINT16 VerMajor, VerMinor = 0;
  UINT32 VbtSizeMax = 0;

  OpRegion = (IGD_OPREGION_STRUCTURE*)(UINTN)PciRead32 (
    PCI_LIB_ADDRESS (
      ASSIGNED_IGD_PCI_BUS,
      ASSIGNED_IGD_PCI_DEVICE,
      ASSIGNED_IGD_PCI_FUNCTION,
      ASSIGNED_IGD_PCI_ASLS_OFFSET));

  /* Validate IGD OpRegion signature and version */
  if (OpRegion) {
    if (CompareMem (OpRegion->Header.SIGN, IGD_OPREGION_HEADER_SIGN, sizeof(OpRegion->Header.SIGN)) != 0) {
      DEBUG ((EFI_D_ERROR, "%a: Invalid OpRegion signature, expect %s\n",
        __FUNCTION__, IGD_OPREGION_HEADER_SIGN));
      return EFI_INVALID_PARAMETER;
    } else {
      VerMajor = OpRegion->Header.OVER >> 24;
      VerMinor = OpRegion->Header.OVER >> 16 & 0xff;
      /*
       * OpRegion version and VBT size:
       * Before 2.0: VBT is stored in OpRegion Mailbox 4 and the size won't exceed 6K.
       * For 2.0 and 2.0+:
       *   If VBT raw data size doesn't exceeds 6K, VBT is stored in Mailbox 4.
       *   If exceeds 6K, VBT is stored in extended VBT region, the address and
       *     size are stored in OpRegion head RVDA and RVDS.
       *   - 2.0, RVDA holds the absolute physical address.
       *   - 2.0+, RVDA holds the relative address OpRegion base, >= OpRegion size
       * vfio-pci allocates a contigious memory to hold both OpRegion and VBT for
       *   OpRegion 2.0 with >6K VBT and fake it to 2.1. So from OVMF perspective,
       *   it shouldn't see OpRegion 2.0 with valid RVDA/RVDS. Otherwise the
       *   vfio-pci driver needs updated.
       */
      if (VerMajor < 2) {
        VbtSizeMax = IGD_OPREGION_VBT_SIZE_6K;
        if (((VBT_HEADER*)&OpRegion->MBox4)->Table_Size > IGD_OPREGION_VBT_SIZE_6K) {
          DEBUG ((EFI_D_ERROR, "%a: VBT Header reports larger size (0x%x) than OpRegion VBT Mailbox (0x%x)\n",
            __FUNCTION__,
            ((VBT_HEADER*)&OpRegion->MBox4)->Table_Size, IGD_OPREGION_VBT_SIZE_6K));
          VbtSizeMax = 0;
          return EFI_INVALID_PARAMETER;
        }
      } else if (VerMajor == 2 && VerMinor == 0 && OpRegion->MBox3.RVDA && OpRegion->MBox3.RVDS){
        DEBUG ((EFI_D_ERROR, "%a: Unsupported OpRegion version %d.%d with VBT larger than 0x%x\n",
          __FUNCTION__, VerMajor, VerMinor, IGD_OPREGION_VBT_SIZE_6K));
        VbtSizeMax = 0;
        return EFI_UNSUPPORTED;
      } else {
        VbtSizeMax = IGD_OPREGION_VBT_SIZE_6K;
        if (OpRegion->MBox3.RVDA && OpRegion->MBox3.RVDS) {
          VbtSizeMax = OpRegion->MBox3.RVDS;
        }
      }
    }
  }

  if (mVbt) {
    Status = gBS->FreePages (
                    mVbt,
                    EFI_SIZE_TO_PAGES (VbtSizeMax)
                    );
  }

  /* Only operates VBT on support OpRegion */
  if (VbtSizeMax) {
    mVbt = SIZE_4GB - 1;
    Status = gBS->AllocatePages (
                    AllocateMaxAddress,
                    EfiReservedMemoryType,
                    EFI_SIZE_TO_PAGES (VbtSizeMax),
                    &mVbt
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: AllocatePages failed for VBT size 0x%x status %d\n",
        __FUNCTION__, VbtSizeMax, Status));
      return EFI_OUT_OF_RESOURCES;
    } else {
      UINT8 CheckSum = 0;

      /* Zero-out first*/
      ZeroMem ((VOID*)mVbt, VbtSizeMax);
      /* Only copy with size as specified in VBT table */
      if (VerMajor < 2 || !OpRegion->MBox3.RVDA || !OpRegion->MBox3.RVDS) {
        CopyMem((VOID*)mVbt, (VOID*)OpRegion->MBox4.RVBT, ((VBT_HEADER*)&OpRegion->MBox4)->Table_Size);
      } else {
        CopyMem((VOID*)mVbt, (VOID*)OpRegion + OpRegion->MBox3.RVDA, OpRegion->MBox3.RVDS);
      }

      /* Fix the checksum */
      for (UINT32 i = 0; i < ((VBT_HEADER*)mVbt)->Table_Size; i++) {
        CheckSum = (CheckSum + ((UINT8*)mVbt)[i]) & 0xFF;
      }
      ((VBT_HEADER*)mVbt)->Checksum += (0x100 - CheckSum);

      *VbtAddress = mVbt;
      *VbtSize = ((VBT_HEADER*)mVbt)->Table_Size;
      DEBUG ((DEBUG_INFO, "%a: VBT Version %d size 0x%x\n", __FUNCTION__,
        ((VBT_BIOS_DATA_HEADER*)(mVbt + ((VBT_HEADER*)mVbt)->Bios_Data_Offset))->BDB_Version,
        ((VBT_HEADER*)mVbt)->Table_Size));
      return EFI_SUCCESS;
    }
  }

  return EFI_UNSUPPORTED;
}

/**
  Entry point for the Platform GOP Policy Driver.

  @param ImageHandle       Image handle of this driver.
  @param SystemTable       Global system service table.

  @retval EFI_SUCCESS           Initialization complete.
  @retval EFI_OUT_OF_RESOURCES  Do not have enough resources to initialize the driver.

**/

EFI_STATUS
EFIAPI
PlatformGopPolicyEntryPoint (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )

{
  EFI_STATUS  Status = EFI_SUCCESS;

  gBS = SystemTable->BootServices;

  gBS->SetMem (
         &mPlatformGopPolicy,
         sizeof (PLATFORM_GOP_POLICY_PROTOCOL),
         0
         );

  mPlatformGopPolicy.Revision                = PLATFORM_GOP_POLICY_PROTOCOL_REVISION_01;
  mPlatformGopPolicy.GetPlatformLidStatus    = GetPlatformLidStatus;
  mPlatformGopPolicy.GetVbtData              = GetVbtData;

  //
  // Install protocol to allow access to this Policy.
  //  
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gPlatformGopPolicyGuid,
                  &mPlatformGopPolicy,
                  NULL
                  );

  return Status;
}
