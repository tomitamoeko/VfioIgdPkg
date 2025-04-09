#!/bin/sh
set -e

if [ -z $WORKSPACE ] || [ -z $EDK_TOOLS_PATH ]; then
    echo "Error: Not in EDK2 build environment"
    echo "Run \`source edksetup.sh\` in EDK2 to setup build environment"
    exit 1
else
    echo "EDK_TOOLS_PATH: $EDK_TOOLS_PATH"
fi

SCRIPT_DIR=$(cd $(dirname $0) && pwd)
DSC_PATH=$SCRIPT_DIR/VfioIgdPkg.dsc

BUILD_TARGET=DEBUG
BUILD_ARCH=X64
BUILD_TOOLCHAIN=GCC
BUILD_OUTPUT_DIR=$WORKSPACE/Build/VfioIgdPkg/"$BUILD_TARGET"_"$BUILD_TOOLCHAIN"/$BUILD_ARCH

# EDK2 MdeModulePkg/Bus/Pci/PciBusDxe/PciOptionRomSupport.c does not check Device ID when loading
# PCI Option ROM, any value is okay here
device_id=0xffff
output_file=
gop_file=

help() {
    echo "Usage: $0 [options] <output>"
    echo "Options:"
    echo "  -h, --help                    Print this help"
    echo "  -i, --device <device id>      Device ID for Option ROM, default is 0xffff"
    echo "  -g, --gop <file>              Path to Intel GOP driver"
    echo "  -r, --release                 Trigger a release build"
}

while [ $# -gt 0 ]; do
    case $1 in
        -h|--help)
            help
            exit 0
            ;;
        -i|--device_id)
            shift
            device_id=$1
            ;;
        -g|--gop)
            shift
            if [ -f $1 ]; then
                gop_file=$1
            else
                echo "File not found: $1"
                exit 1
            fi
            ;;
        -r|--release)
            BUILD_TARGET=RELEASE
            ;;
        -)
            echo "Unknown option: $1"
            exit 1
            ;;
        *)
            if [ -z $output_file ]; then
                output_file=$1
            else
                echo "Unexpected argument: $1"
            fi
            ;;
    esac
    shift
done

if [ -z $output_file ]; then
    echo "Error: output file is required."
    echo ""
    help
    exit 1
fi

build -b $BUILD_TARGET -a $BUILD_ARCH -t $BUILD_TOOLCHAIN -p $DSC_PATH

if [ -z $gop_file ]; then
    echo "Generating non-GOP IGD Option ROM with IgdAssignmentDxe..."
    EfiRom -f 0x8086 -i $device_id -e $BUILD_OUTPUT_DIR/IgdAssignmentDxe.efi -o $output_file
else
    echo "IntelGopDriver: $(pwd)/$gop_file"
    echo "enerating GOP IGD Option ROM with IgdAssignmentDxe, PlatformGopPolicy and IntelGopDriver..."
    EfiRom -f 0x8086 -i $device_id -e $BUILD_OUTPUT_DIR/IgdAssignmentDxe.efi $BUILD_OUTPUT_DIR/PlatformGopPolicy.efi $gop_file -o $output_file
fi

echo ""
echo "Generated IGD Option ROM at $(pwd)/$output_file"
