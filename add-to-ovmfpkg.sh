#!/bin/sh
set -e

if [ -z $WORKSPACE ] || [ -z $EDK_TOOLS_PATH ]; then
    echo "Error: Not in EDK2 build environment"
    echo "Run \`source edksetup.sh\` in EDK2 to setup build environment"
    exit 1
else
    echo "EDK_TOOLS_PATH:  $EDK_TOOLS_PATH"
fi

VFIOIGDPKG_PATH=$(cd $(dirname $0) && pwd)
OVMFPKG_PATH=$WORKSPACE/OvmfPkg
OVMFPKG_DSC=$OVMFPKG_PATH/OvmfPkgX64.dsc
OVMFPKG_FDF=$OVMFPKG_PATH/OvmfPkgX64.fdf

if [ ! -f "$OVMFPKG_DSC" ] || [ ! -f "$OVMFPKG_FDF" ]; then
    echo "Error: OvmfPkg not found"
    exit 1
else
    echo "VFIOIGDPKG_PATH: $VFIOIGDPKG_PATH"
    echo "OVMFPKG_PATH:    $OVMFPKG_PATH"
    echo ""
fi

if [ $(realpath "$VFIOIGDPKG_PATH/..") != $(realpath "$OVMFPKG_PATH/..") ]; then
    echo "Error: VfioIgdPkg and OvmfPkg are not in the same folder"
    exit 1
fi

SED_PATTERN="# DXE Phase modules"
DSC_INCLUDE="!include VfioIgdPkg/Include/VfioIgdPkg.dsc.inc"
FDF_INCLUDE="!include VfioIgdPkg/Include/VfioIgdPkg.fdf.inc"


if [ ! $(grep -Fxq "$DSC_INCLUDE" "$OVMFPKG_DSC") ]; then
    sed -i "/$SED_PATTERN/a $DSC_INCLUDE" "$OVMFPKG_DSC"
    echo "Including VfioIgdPkg.dsc.inc"
fi

if [ ! $(grep -Fxq "$FDF_INCLUDE" "$OVMFPKG_FDF") ]; then
    sed -i "/$SED_PATTERN/a $FDF_INCLUDE" "$OVMFPKG_FDF"
    echo "Including VfioIgdPkg.fdf.inc"
fi
