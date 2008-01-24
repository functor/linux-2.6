rm -fR inst
mkdir inst
make install INSTALL_PATH=inst
make modules_install INSTALL_MOD_PATH=inst
tar cfz inst.tar.gz inst
scp inst.tar.gz root@$1:/tmp
echo "Expanding and installing..."
ssh root@$1 "cd /tmp;tar xvfz inst.tar.gz"
ssh root@$1 "cp -R /tmp/inst/lib/* /lib/"
ssh root@$1 "rm -fR /tmp/inst/lib; mv /tmp/inst/* /boot"
echo "Rebooting in 5 seconds..."
sleep 5
ssh root@$1 reboot
