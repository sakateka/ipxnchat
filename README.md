# ipxnchat
ncurses chat over ipx network (course work)

## network part
Get tools from https://pkgs.org/download/ipx 
or compile from source http://pkgs.fedoraproject.org/repo/pkgs/ncpfs/

Add ipx interface on local and remote machine
```
sudo ./ipx_interface add -p wlp1s0 802.2 55
```


Chat over ipx network ;-)
```
./ipxnchat -h
Usage: ./ipxnchat [-n network] [-r] [network:]remote_addr
   -n NETWORK    - ipx network number like 0x1 or 0x33234 (hex)
   -r [NET]:ADDR - remote network and addr (addr is MAC add in hex format)
```
![2017-12-05_18 53 18](https://user-images.githubusercontent.com/2256154/33605873-08cf45c2-d9ee-11e7-9b03-afbe4ae07b93.png)
