Open VnetCard [中文教程](https://biscuitos.github.io/blog/VnetCard/)
--------------------------------------------

![](https://raw.githubusercontent.com/EmulateSpace/PictureSet/master/BiscuitOS/boot/BOOT000139.png)

**Open VnetCard** is virtual network card base on Tun/Tap. It's speed up
to 4.2Gbps/sec ~ 1.2Gbps/sec. The VnetCard contains above function for
a real network card.

### Usage

The Vnetcard running on X86 and FPGA/ARM that base on DMA to transfer
data and message. U can obtian execute file from source code as follow:

```
git clone https://github.com/WorkRoutine/VNet-Card.git
export CROSS_CC=$CRCC
make clean
make
```

The CRCC points to the path of cross-compiler for FPGA/ARM platform.
When compile success, U can obtain two execute file `Vnet_Host` and 
`Vnet_FPGA`, the `Vnet_Host` running on X86 and `Vnet_FPGA` running 
on FPGA/ARM. When it running success, you can use `ifconfig` to check
information for VnetCard, as follow:

![](https://raw.githubusercontent.com/EmulateSpace/PictureSet/master/BiscuitOS/boot/BOOT000142.png)

And more fun, U can establish a NAT or Gatway to alloc VnetCard access
outside network. As follow:

![](https://raw.githubusercontent.com/EmulateSpace/PictureSet/master/BiscuitOS/boot/BOOT000191.png)

More usemanual of `Vnetcard` see [Usermanual for VnetCard](https://biscuitos.github.io/blog/VnetCard/#B0)

### Test

The VnetCard supports `ssh`, `scp`, `iperf`, or `ping` to test general
network function. 

![](https://raw.githubusercontent.com/EmulateSpace/PictureSet/master/BiscuitOS/boot/BOOT000155.png)

The result for speed as follow:

![](https://raw.githubusercontent.com/EmulateSpace/PictureSet/master/BiscuitOS/boot/BOOT000157.png)

And more test result see [VnetCard Test](https://biscuitos.github.io/blog/VnetCard/#D0)
