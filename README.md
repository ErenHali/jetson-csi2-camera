# Jetson GMSL2 Camera Bring-up Guide

**Host PC Requirement:**            Linux (Ubuntu 20.04)  
**Jetson Device:**                  Jetson AGX Xavier Industrial  
**Jetson Linux Version:**           35.6.2  
**Jetpack Version:**                5.1.5    
**Jetson Linux Kernel version:**    5.15  

On this user guide we will focus on creating a custom kernel and a device tree to incorporate _VLS-GM2-AR0234_ camera module from Technexion. The sensor drivers use the V4L2 interface with the Tegra Camera Framework.

## Installation

There are four main files for installing Jetson Linux, for downloading them, check the following URL:  
[https://developer.nvidia.com/embedded/jetson-linux-r3562#](https://developer.nvidia.com/embedded/jetson-linux-r3562#)

-  **aarch64--glibc--stable-final.tar.gz**: Building tools used to build the OS (Bootlin Toolchain gcc 9.3)
-  **Jetson_Linux_R35.6.2_aarch64.tbz2**: BSP file containing Jetson Linux release package (Driver Package BSP)
-  **Tegra_Linux_Sample-Root-Filesystem_R35.6.2_aarch64.tbz2**: Jetson Linux Root File System (Sample Root Filesystem)
-  **public_sources.tbz2**: File for creating and building our custom kernel (Driver Package BSP Sources)

1.  Install Jetson Linux build utilities:  
```
sudo apt install build-essential bc
sudo apt-get install qemu-user-static
sudo apt install libarchive-tools
```

2.  Creating enviroment variables:  
```
export L4T_RELEASE_PACKAGE=Jetson_Linux_R35.3.1_aarch64.tbz2
export SAMPLE_FS_PACKAGE=Tegra_Linux_Sample-Root-Filesystem_R35.3.1_aarch64.tbz2
export TOOLCHAIN=aarch64--glibc--stable-final.tar.gz
export CUSTOM_BSP=public_sources.tbz2
```

3.  Untar the building tools to _$HOME/l4t-gcc_  
```
mkdir $HOME/l4t-gcc
tar xf ${TOOLCHAIN} -C $HOME/l4t-gcc
```

Create enviroment variables for cross-compiling:  
```
export CROSS_COMPILE_AARCH64_PATH=$HOME/l4t-gcc
export CROSS_COMPILE_AARCH64=$HOME/l4t-gcc/bin/aarch64-buildroot-linux-gnu-
```

You can test enviroment variables with below command, this should return _"aarch64--glibc--stable-final.tar.gz"_  
```
echo ${TOOLCHAIN}
```

:memo: **Note:** Be aware not adding extra spaces to the end of the enviroment variables.  

:memo: **Note #2:** Closing the terminal where you created the enviroment variables makes them get lost, be aware of not closing the terminal.

4.  Extract other files:
```
tar xf ${L4T_RELEASE_PACKAGE}
sudo tar xpf ${SAMPLE_FS_PACKAGE} -C Linux_for_Tegra/rootfs/
tar xpf ${CUSTOM_BSP} -C Linux_for_Tegra/source/ --strip-components=2 Linux_for_Tegra/source
```

5.  Assemble the rootfs  
```
cd Linux_for_Tegra/
sudo ./apply_binaries.sh
sudo ./tools/l4t_flash_prerequisites.sh
```

After these steps you can connect an USB-C cable to your Jetson board and choose to flash the board with the un-updated kernel to verify if everything works correctly, if you do not want to do this, skip below step:  
```
cd Linux_for_Tegra
sudo ./flash.sh jetson-agx-xavier-industrial mmcblk0p1
```
## Customizing and Building the Kernel

1.  Extract necessary files  
```
cd Linux_for_Tegra/source/public
tar -xjf kernel_src.tbz2
```

2.  Extract the _deico_drivers_dts_files.tbz2_ into _Linux_for_Tegra_, to achieve this keep the _deico_drivers_dts_files.tbz2_ file and the _Linux_for_Tegra_ in the same folder
```
cd <your top folder>
tar xpf deico_drivers_dts_files.tbz2 --strip-components=1
```

## Updating the Boot Order
```
cd Linux_for_Tegra

kernel/dtc -I dtb -O dts -o kernel/dtb/L4TConfiguration.dts kernel/dtb/L4TConfiguration.dtbo

gedit kernel/dtb/L4TConfiguration.dts
```
Change the _DefaultBootPriority_ parameter with the below:
```
DefaultBootPriority {
	data = "emmc,nvme,sd,ufs,usb";
	locked;
}
```

Compile the updated .dts file to .dtb:
```
kernel/dtc -I dts -O dtb -o kernel/dtb/L4TConfiguration.dtbo kernel/dtb/L4TConfiguration.dts
```

After changing the boot parameter you can build the custom kernel, this step will take some time:
```
cd Linux_for_Tegra/source/public/
mkdir kernel_out
./nvbuild.sh -o $PWD/kernel_out > build.log 2>&1
```

This will create a __kernel_out__ directory where the custom kernel will be built.  
It will also create a __build.log__ file on the same folder. 

## Replace New Files
```
cd Linux_for_Tegra/source/public 
```
1.  Copy with root privilages the _tevs.ko max9296a.ko max96717.ko_ files under _Linux_for_Tegra/source/public/kernel_out/drivers/media_ into
```
Linux_for_Tegra/rootfs/usr/lib/modules/5.10.216-tegra/kernel/drivres/media/i2c/
```

2.  Copy with root privilages the _nvgpu.ko_ under _kernel_out/drivers/gpu/nvgpu/nvgpu.ko_ into
```
Linux_for_Tegra/rootfs/usr/lib/modules/5.10.216-tegra/kernel/drivers/gpu/nvgpu/
```

3.  Run below command, change __~/Desktop/jetson__ with your own top folder:
```
cp kernel_out/arch/arm64/boot/dts/nvidia/* ~/Desktop/jetson/Linux_for_Tegra/kernel/dtb/
```

4.  Run final copying command, change __~/Desktop/jetson__ with your own top folder:
```
cp kernel_out/arch/arm64/boot/Image ~/Desktop/jetson/Linux_for_Tegra/kernel/
```


## Flashing the Board

You need the flash the eMMC first, if you try to flash the NVMe first, Jetson will not boot up.

Before flashing connect Ethernet, USB Mouse/Keyboard to the Jetson.

1.  Connect a reliable USB-C cable to the Jetson's __USB0__ Port

2.  Put Jetson in recovery mode by holding the "recovery" button first then power-up the Jetson, yoy can let go the recovery button after powering the board.

3.  Test it on your Host Machine by writing below command, this should list Jetson as "NVIDIA device"
```
lsusb
```
Start the flashing process:
```
cd Linux_for_Tegra
sudo ./flash.sh jetson-agx-xavier-industrial mmcblk0p1
```

## Jetson Applications

Install necessary tools, make sure you have internet connection.
```
sudo apt-get install nano v4l-utils
```
To show the image to the first monitor connected to the Jetson, run below command:
```
export DISPLAY=:0
```

To test if cameras drivers are probed correctly:
```
media-ctl -p
sudo dmesg | grep -i "max9296"
sudo dmesg | grep -i "max96717"
```

Copy your new dtb file and reconfigure your board:
```
sudo nautilus /boot
```
Copy the _tegra194-p2888-0008-p2822-0000-deico-tevs-0.dtb_ into
```
 /boot/dtb
 ```
Then rename it to _kernel_tegra194-p2888-0008-p2822-0000-deico-tevs-0.dtb_

Afterwards, run below command to configure the __extlinux.conf__ file:
```
sudo nano /boot/extlinux/extlinux.conf
```
Change below line:
```
Change this -> FDT /boot/dtb/kernel_tegra194-p2888-0008-p2822-0000.dtb
To this -> FDT /boot/dtb/kernel_tegra194-p2888-0008-p2822-0000-deico-tevs-0.dtb
```
Shutdown the Jetson board running below command, then reboot your board:
```
sync
shutdown now
```
After probbing everything and a fresh reboot run below commands, this will start a video stream on your device:
```
gst-device-monitor-1.0 Video/Source
```
```
DISPLAY=:0 gst-launch-1.0 v4l2src device=/dev/video0 ! "video/x-raw, format=UYVY, width=1920, height=1080 framerate=30/1" ! nvvidconv ! nv3dsink
```