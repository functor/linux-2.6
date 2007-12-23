rm -fR inst
mkdir inst
make install INSTALL_PATH=inst
make modules_install INSTALL_MOD_PATH=inst
tar cfz inst.tar.gz inst
scp -i ~/newvici inst.tar.gz root@$1:/tmp
ssh -i ~/newvici root@$1 "cd /tmp;tar xvfz inst.tar.gz"
ssh -i ~/newvici root@$1 "wget www/~sapanb/vgup;sh vgup"
ssh -i ~/newvici root@$1 "cp -R /tmp/inst/lib/* /mnt/lib/"
ssh -i ~/newvici root@$1 "rm -fR /tmp/inst/lib; mv /tmp/inst/* /mnt/boot"
sleep 5
ssh -i ~/newvici root@$1 reboot
