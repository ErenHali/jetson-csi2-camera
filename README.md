# Jetson Developing Guide

**Host PC Requirement:**            Linux (Ubuntu 18.04 or Ubuntu 20.04)  
**Jetson Device:**                  Jetson AGX Xavier Industrial  
**Jetson Linux Version:**           35.6.2  
**Jetpack Version:**                5.1.5    
**Jetson Linux Kernel version:**    5.15  

Onthis developing guide we will focus on creating a custom kernel and device tree to incorporate VLS-GM2-AR0234 camera module from Technexion. The sensor drivers use the V4L2 interface with the Tegra Camera Framework.

## Installation

First check if you are using dash or bash. Switch to dash:
```
sudo dpkg-reconfigure dash
```
Say "yes"

Verify it:  
```
ls -l /bin/sh
```
Should show:  
```
/bin/sh -> dash
```

There are four main files for installing Jetson Linux, for downloading them, check the following URL:  
[NVIDIA Jetson Linux 35.6.2](https://developer.nvidia.com/embedded/jetson-linux-r3562#)


-  **aarch64--glibc--stable-final.tar.gz**  -> Building tools used to build the OS (Bootlin Toolchain gcc 9.3)
-  **Jetson_Linux_R35.6.2_aarch64.tbz2**    -> BSP file containing Jetson Linux release package (Driver Package BSP)
-  **Tegra_Linux_Sample-Root-Filesystem_R35.6.2_aarch64.tbz2** -> Jetson Linux Root File System (Sample Root Filesystem)
-  **public_sources.tbz2**  -> File for creating and building our custom kernel (Driver Package BSP Sources)

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

You can test enviroment variables with below command, this should return _aarch64--glibc--stable-final.tar.gz_  
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

After these steps you can connect an USB-C cable to your Jetson board and choose to flash the board with the un-updated kernel to verify if everything works correctly, if you do not want this, skip below step:  
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

2.  Extract the deico_drivers_dts_files.tbz2 into Linux_for_Tegra, to achieve this keep the .tbz2 file and the Linux_for_Tegra in the same folder
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

```
kernel/dtc -I dts -O dtb -o kernel/dtb/L4TConfiguration.dtbo kernel/dtb/L4TConfiguration.dts
```

After changing the boot parameter you can build the custom kernel:
```
cd Linux_for_Tegra/source/public/
mkdir kernel_out
./nvbuild.sh -o $PWD/kernel_out > build.log 2>&1
```

This will create a __kernel_out__ directory where the custom kernel will be built.

## Replace New Files
```
cd Linux_for_Tegra/source/public 
```
1.  Copy with root privilages the __tevs.ko max9296a.ko max96717.ko_ files under _Linux_for_Tegra/source/public/kernel_out/drivers/media_ into _Linux_for_Tegra/rootfs/usr/lib/modules/5.10.216-tegra/kernel/drivres/media/i2c/_

2.  Copy with root privilages the _nvgpu.ko_ under _kernel_out/drivers/gpu/nvgpu/nvgpu.ko_ into _Linux_for_Tegra/rootfs/usr/lib/modules/5.10.216-tegra/kernel/drivers/gpu/nvgpu/_

3.  cp kernel_out/arch/arm64/boot/dts/nvidia/* ~/Desktop/jetson/Linux_for_Tegra/kernel/dtb/

4.  cp kernel_out/arch/arm64/boot/Image ~/Desktop/jetson/Linux_for_Tegra/kernel/


## Flashing the Board

You need the first flash the eMMC, if you try to flash the NVMe first, Jetson will not boot up.

Before flashing connect Ethernet, USB Mouse/Keyboard to the Jetson.

1.  Connect a reliable USB-C cable to the Jetson's USB0 Port

2.  Put Jetson in recovery mode by holding the "recovery" button first then power-up the Jetson

3.  Test it on your Host Machine by writing below command, this should list "Jetson as NVIDIA device"
```
lsusb
```
Start the flashing process:
```
cd Linux_for_Tegra
sudo ./flash.sh jetson-agx-xavier-industrial mmcblk0p1
```

## Jetson Applications

Install necessary tools
```
sudo apt-get install nano v4l-utils
```
To show the image to the first monitor connected to the jetson via:
```
export DISPLAY=:0
```

To test the if cameras probed correctly:
```
media-ctl -p
sudo dmesg | grep -i "max9296"
sudo dmesg | grep -i "max96717"
```

Copy your new dtb file and reconfigure your board:
```
sudo nautilus /boot
```
Copy the _tegra194-p2888-0008-p2822-0000-deico-tevs-0.dtb_ into _/boot/dtb_  
Then rename it to _kernel_tegra194-p2888-0008-p2822-0000-deico-tevs-0.dtb_

```
sudo nano /boot/extlinux/extlinux.conf
```
Change below line:
```
Change this -> FDT /boot/dtb/kernel_tegra194-p2888-0008-p2822-0000.dtb
To this -> FDT /boot/dtb/kernel_tegra194-p2888-0008-p2822-0000-deico-tevs-0.dtb
```
```
sync
shutdown now
```
After probbing everything and a fresh reboot:  
```
gst-device-monitor-1.0 Video/Source
```
```
DISPLAY=:0 gst-launch-1.0 v4l2src device=/dev/video0 ! "video/x-raw, format=UYVY, width=1920, height=1080i framerate=30/1" ! nvvidconv ! nv3dsink
```

## Debugging on Jetson

__MY HOST PC:__ ehali@192.168.55.100

scp a file from your Host to Jetson, example:
```
test.dtb deico@192.168.55.1:~/Desktop/GMSL2
```

Below line compiles .dts into dtb
```
dtc -I dts -O dtb -o tegra194-p2888-0008-p2822-0000-deico-gmsl.dtb decompiled.dts
```
Below line decompiles .dtb into .dts
```
dtc -I dtb -O dts -o decompiled.dts tegra194-p2888-0008-p2822-0000-deico-gmsl.dtb
```

To see a specific module info
```
modinfo ar0234
```
kernel modules location
```
/lib/modules/$(uname -r)/kernel/drivers
```
Attiktan sonra
```
sudo depmod -a
```

Test the I2C bus, listing busses, reading/writing
```
i2cdetect -l
i2cdetect -y -r 3180000.i2c
```
(-y: auto-yes, 2: I2C port number, 0x28: 7bit slave addr, 0x0: reg addr, b: read byte)
```
i2cget -y 2 0x28 0x0 b
```
(-y: auto-yes, 2: I2C port number, 0x28: 7bit slave addr, 0x0: reg addr, 0x50: data written)
```
i2cset -y 2 0x28 0x0 0x50
```
**VERY IMPORTANT:** i2cget & i2cset functions only return meaningful values if the slave I2C device has 8-bit register addressing. For 16-bit register addresing use **i2ctransfer** function. You can still use i2cget & i2cset to validate if the bus is alive.

```
i2ctransfer -y 2 w2@0x28 0x00 0x00 r1@0x28
```
-y: auto-yes, 2: I2C port number, w2: write two bytes of data, 0x00: register high byte (first two number), 0x00: register low byte, r1: read one byte, @0x28: 7-bit slave address

This solved the issue of reading garbage data from i2cget functions.
  
change i2c address of devices for testing  
make the 0x40 device's i2c address as 0x05:
```
i2ctransfer -y 2 w3@0x40 0x00 0x0A //check datasheet for why we write 0x0A
```
We basically send two bytes of register we want read from first. Because checking the datasheet we see that some register are 4 digits (eg. 0x4291) meaning they are 16 bits. Third byte is the data we want to ride to that register.

To read back:
```
i2cdetect -y -r
i2ctransfer -y 2 w2@0x05 0x00 0x00 r1@0x05
```

One shot link reset:
```
i2ctransfer -y 2 w3@0x28 0x00 0x10 0x20
```

### Note to myself

FDT can not be fixed beforehand as this line is generated after ./flash.sh  
You need to manually change the .dtb top level file on jetson

For debugging, switch new drivers to MODULES not BUILT IN (=y) on kernel5.10/..../tegra_defconfig

**I2C PORTS:**
```
aliases {
	ethernet0 = "/bus@0/ethernet@2490000";
	i2c0 = "/bpmp/i2c";
	i2c1 = "/bus@0/i2c@3160000";
	i2c2 = "/bus@0/i2c@c240000";
	i2c3 = "/bus@0/i2c@3180000";
	i2c4 = "/bus@0/i2c@3190000";
	i2c5 = "/bus@0/i2c@31c0000";
	i2c6 = "/bus@0/i2c@c250000";
	i2c7 = "/bus@0/i2c@31e0000";
	mmc0 = "/bus@0/mmc@3460000";
	mmc1 = "/bus@0/mmc@3400000";
	serial0 = &tcu;
};
```
_Location:_  
Linux_for_Tegra\source\public\kernel_src\kernel\kernel-5.10\arch\arm64\boot\dts\nvidia

**MY WHOLE DEBUGGING PROCESS**

Before: Fixed hardware errors. (PWDNB was active low)
```
max9296a 2-0028: read 0 0x0 = 0x50
max9296a 2-0028: update 0 0x10 0x80 = 0x80
max9296a 2-0028: read 0 0x0 = 0x50
max9296a 2-0028: update 0 0x332 0xf0 = 0x00
max9296a 2-0028: update 0 0x2 0xf0 = 0x00
max9296a 2-0028: update 0 0x160 0x01 = 0x00
max9296a 2-0028: update 0 0x10 0x10 = 0x00
max9296a 2-0028: Cannot turn on unconfigured PHY
max9296a 2-0028:  probe of 2-0028 failed with error -22  
```
_Result: The VI and NVCSI remote endpoint bindings should be constructed first._

```
max96717 9-0040:  update 0 0x10 0x10 = 0x00
max96717 9-0040:  Cannot turn on unconfigured PHY
max96717 9-0040:  probe of 9-0040 failed with error -22  
```
_Result: Ser-Des should have same link speeds (3Gbps in this case)_

```
image sensor (actually this is the ISP :) communucation was not succesfull  
```
_Result: image sensor (ISP) should be enabled by the onboard gpio expender_

```
tegra-camrtc-capture-vi tegra-capture-vi: failet to create des_ch_0:0 -> ser_0_ch_0:0 link
ar0234-v4l2 10-0048: failed to register v4l2 subdev -22  
```
_Result: port0 corresponds to output port1 to input -> explained on .yaml documents._

```
tevs 10-0048: i2c bus regbase unavailable
tevs 10-0048: Could not initialize sensor properties.
tevs 10-0048: Failed to initialize
tevs 10-0048: tegra camera driver registration failed
tevs: probe of 10-0048 failed with error -75.  
```
_Result: tevs driver is not compatible with V4L2 Framework_

```
tevs 10-0048: DEBUG: Probe step 1 - About to call tegracam_device_register
tevs 10-0048: DEBUG: Now inside tevs_power_get()
tevs 10-0048: i2c bus regbase unavailable
tevs 10-0048: Could not initialize sensor properties
tevs 10-0048: Failed to initialize
tevs 10-0048: DEBUG: Probe step 2 - tegra camera driver registeration FAILED with error -75
tevs: probe of 10-0048 failed with error -75  
```
_Result: tevs driver does not work with v4l2 ser-des drivers, therefore either adapt the tevs driver to the v4l2 framework or use the technexion ser-des drivers with the original tevs driver._  

_**Conclusion**: Used technexion ser-des and isp drivers. Updated the device tree, and it worked Could have used generic ser-des drivers but we do not have the datasheet and information for the ISP device therefore it wouldş take a lot of time to develop that driver which would incorparete with the ser-des drivers._

**VARDIGIM SONUCLAR**

1. maxim-serdes driver'larini kullanarak serializer, deserializer ve gpio expender entegresine erisip konfigüre edebiliyorum. Fakat bu senaryoda kendi yazdigim sensör driver'ini kullanmaya calistigim icin goruntu alamıyorum, Çünkü ISP entegresini konfigüre etmem lazım ve bunu entegreyi bilmeden yapamam. Bu senaryoda kullandigim device tree dosyasi:
    - tegra194-p2888-0008-p2822-0000-deico-eren.dts

2. tevs sensor (isp) driver'ini maxim-serdes driver'lari ile kullandigimda ise tevs driver'ı hiç bir şekilde kernel'de probe olmuyor. Buna engel olan üç sebep bulabiliyorum.
    - __gpio expender__: bunu elle sürmeme rağmen driver probe olmuyor, fakat sensör driver'i expender'in hiçbir yerde 7. pinini high sürmüyor, normalde bu pin'i high sürdüğümde isp entegresi ayağa kalkıyor

    - __xavier__: tegra camera framework'ü xavier'da farklı çalışıyor olabilir, çünkü bu driver orin için yazılmış.

    - __maxim-serdes:__ tevs driver'i technexion'un kendi ser-des driver'lari ile birlikte kullanıyor. Bunu maxim-serdes driver'lari ile kullanmak bir sorun yaratıyor olabilir

Eger sensör driver'i probe olursa bütün sorunların çözüleceğini düşünüyorum.

Hata tegracam depencies'leri bizim V4L2 yapısındaki ser-des driver'lari ile uyumlu değil. i2c-atr içindeki virtual i2c bus'ına tegracam erişemiyor bu yüzden ya tevs driver'ini v4l2'e framework'a uyumlu hala getiricez, ya da technexion ser-des driver'lari ile birlikte kullanıcam.

### Device Tree Parameter Notes
#### vc-id
This parameter only changes in the same chip but for different PHY  
if it was max9296a first channels output would be vc-id=0  
if it was the second phy's output it woudl be vc-id=1  
for single phy everything is vc_id=0  

#### tegra_sinterface
tegra_sinterface corresponds rto CSI-A, CSI-B and so on. For example:  
If your camera is connected to the CSI-E with 4 lanes, you would change this parameter to seria
l_e.  
_Check the port-binding.png_

#### port-index
port-index correspond to the port number of the nvcsi.  
For the  video input (vi) this correspond to the Stream Number. In addition the channel@ and port@ numbers are just for naming conventions they do not directly represent the port-index of nvcsi or stream-index of vi, for example:

If you are connected to the CSI-F then your port-index for nvcsi should be "6" and your port-index for vi should be "5". You can use any channel or port node for nvcsi and vi. But if you are connecting multiple cameras then intuitively it is better to select these channels which would correspond to your camera index. Check the _port-binding.png_

#### Quad DTS File
When the port-indexes for the 4th channel (CSI-G) is fixed, this .dts file started working for all channels. When DISPLAY parameter is not set, you would stop getting video stream correctly. If this happens reboot the Jetson then first set the enviroment variable for DISPLAY.