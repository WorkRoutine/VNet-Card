VNet-Card
--------------------------------------------

## Queue test

```
make CROSS_CC=*/aarch64-linux-gnu-gcc
scp Vnet_FPGA root@172.x.x.x:/tmp
```

##### Running on Host

```
sudo ./Vnet_Host
```

##### Running on FPGA

```
sudo ./Vnet_FPGA
```
