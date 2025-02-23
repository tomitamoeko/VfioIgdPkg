/** @file
  Macros corresponding to QEMU's "Intel Graphics Device (IGD) assignment with
  vfio-pci" specification, located at "docs/igd-assign.txt" in the QEMU source
  tree.

  Copyright (C) 2018, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef _ASSIGNED_IGD_H_
#define _ASSIGNED_IGD_H_

#include <Base.h>

//
// names of fw_cfg files
//
#define ASSIGNED_IGD_FW_CFG_OPREGION  "etc/igd-opregion"
#define ASSIGNED_IGD_FW_CFG_BDSM_SIZE "etc/igd-bdsm-size"

//
// Alignment constants. UEFI page allocation automatically satisfies the
// requirements for the OpRegion, thus we only need to define an alignment
// constant for IGD stolen memory.
//
#define ASSIGNED_IGD_BDSM_ALIGN SIZE_1MB

//
// PCI config space registers. The naming follows the PCI_*_OFFSET pattern seen
// in MdePkg/Include/IndustryStandard/Pci*.h.
//
#define ASSIGNED_IGD_PCI_GSM_SIZE_OFFSET 0x51
#define ASSIGNED_IGD_PCI_BDSM_OFFSET 0x5C
#define ASSIGNED_IGD_PCI_ASLS_OFFSET 0xFC

//
// PCI location and vendor
//
#define ASSIGNED_IGD_PCI_BUS       0x00
#define ASSIGNED_IGD_PCI_DEVICE    0x02
#define ASSIGNED_IGD_PCI_FUNCTION  0x0
#define ASSIGNED_IGD_PCI_VENDOR_ID 0x8086

#endif // _ASSIGNED_IGD_H_
