Summary: The Linux kernel (the core of the Linux operating system)

# What parts do we want to build?  We must build at least one kernel.
# These are the kernels that are built IF the architecture allows it.

%define buildup 1
%define buildsmp 1
%define builduml 0
%define buildxen 0
%define builddoc 1


# Versions of various parts

#
# Polite request for people who spin their own kernel rpms:
# please modify the "release" field in a way that identifies
# that the kernel isn't the stock distribution kernel, for example by
# adding some text to the end of the version number.
#
%define sublevel 12
%define kversion 2.6.%{sublevel}
%define rpmversion 2.6.%{sublevel}
%define rhbsys  %([ -r /etc/beehive-root -o -n "%{?__beehive_build}" ] && echo || echo .`whoami`)

%define release 1.1398_FC4.7%{?pldistro:.%{pldistro}}%{?date:.%{date}}

%define signmodules 0
%define make_target bzImage

%define KVERREL %{PACKAGE_VERSION}-%{PACKAGE_RELEASE}

# Override generic defaults with per-arch defaults 

%define image_install_path boot

#
# Three sets of minimum package version requirements in the form of Conflicts:
# to versions below the minimum
#

#
# First the general kernel 2.6 required versions as per
# Documentation/Changes
#
%define kernel_dot_org_conflicts  ppp <= 2.3.15, pcmcia-cs <= 3.1.20, isdn4k-utils <= 3.0, mount < 2.10r-5, nfs-utils < 1.0.3, e2fsprogs < 1.29, util-linux < 2.10, jfsutils < 1.0.14, reiserfsprogs < 3.6.3, xfsprogs < 2.1.0, procps < 2.0.9, oprofile < 0.5.3

# 
# Then a series of requirements that are distribution specific, either 
# because we add patches for something, or the older versions have 
# problems with the newer kernel or lack certain things that make 
# integration in the distro harder than needed.
#
%define package_conflicts  cipe < 1.4.5, tux < 2.1.0, kudzu <= 0.92, initscripts < 7.23, dev < 3.2-7, iptables < 1.2.5-3, bcm5820 < 1.81, nvidia-rh72 <= 1.0

#
# Several packages had bugs in them that became obvious when the NPTL
# threading code got integrated. 
#
%define nptl_conflicts SysVinit < 2.84-13, pam < 0.75-48, vixie-cron < 3.0.1-73, privoxy < 3.0.0-8, spamassassin < 2.44-4.8.x,  cups < 1.1.17-13

#
# The ld.so.conf.d file we install uses syntax older ldconfig's don't grok.
#

# MEF commented out
# %define xen_conflicts glibc < 2.3.5-1

#
# Packages that need to be installed before the kernel is, because the %post
# scripts use them.
#
%define kernel_prereq  fileutils, module-init-tools, initscripts >= 5.83, mkinitrd >= 3.5.5

Vendor: PlanetLab
Packager: PlanetLab Central <support@planet-lab.org>
Distribution: PlanetLab 3.0
URL: http://cvs.planet-lab.org/cvs/linux-2.6

Name: kernel
Group: System Environment/Kernel
License: GPLv2
Version: %{rpmversion}
Release: %{release}
ExclusiveOS: Linux
Provides: kernel = %{version}
Provides: kernel-drm = 4.3.0
Provides: kernel-%{_target_cpu} = %{rpmversion}-%{release}
Prereq: %{kernel_prereq}
Conflicts: %{kernel_dot_org_conflicts}
Conflicts: %{package_conflicts}
Conflicts: %{nptl_conflicts}
# We can't let RPM do the dependencies automatic because it'll then pick up
# a correct but undesirable perl dependency from the module headers which
# isn't required for the kernel proper to function
AutoReqProv: no

#
# List the packages used during the kernel build
#
BuildPreReq: module-init-tools, patch >= 2.5.4, bash >= 2.03, sh-utils, tar
BuildPreReq: bzip2, findutils, gzip, m4, perl, make >= 3.78, gnupg, diffutils
#BuildRequires: gcc >= 3.4.2, binutils >= 2.12, redhat-rpm-config
BuildRequires: gcc >= 2.96-98, binutils >= 2.12, redhat-rpm-config
BuildConflicts: rhbuildsys(DiskFree) < 500Mb
BuildArchitectures: i686



Source0: ftp://ftp.kernel.org/pub/linux/kernel/v2.6/linux-%{kversion}.tar.bz2

BuildRoot: %{_tmppath}/kernel-%{KVERREL}-root

%description 
The kernel package contains the Linux kernel (vmlinuz), the core of any
Linux operating system.  The kernel handles the basic functions
of the operating system:  memory allocation, process allocation, device
input and output, etc.

%package devel
Summary: Development package for building kernel modules to match the kernel.
Group: System Environment/Kernel
AutoReqProv: no
Provides: kernel-devel-%{_target_cpu} = %{rpmversion}-%{release}
Prereq: /usr/sbin/hardlink, /usr/bin/find

%description devel
This package provides kernel headers and makefiles sufficient to build modules
against the kernel package.


%package doc
Summary: Various documentation bits found in the kernel source.
Group: Documentation

%description doc
This package contains documentation files from the kernel
source. Various bits of information about the Linux kernel and the
device drivers shipped with it are documented in these files. 

You'll want to install this package if you need a reference to the
options that can be passed to Linux kernel modules at load time.


%package smp
Summary: The Linux kernel compiled for SMP machines.

Group: System Environment/Kernel
Provides: kernel = %{version}
Provides: kernel-drm = 4.3.0
Provides: kernel-%{_target_cpu} = %{rpmversion}-%{release}smp
Prereq: %{kernel_prereq}
Conflicts: %{kernel_dot_org_conflicts}
Conflicts: %{package_conflicts}
Conflicts: %{nptl_conflicts}
# upto and including kernel 2.4.9 rpms, the 4Gb+ kernel was called kernel-enterprise
# now that the smp kernel offers this capability, obsolete the old kernel
Obsoletes: kernel-enterprise < 2.4.10
# We can't let RPM do the dependencies automatic because it'll then pick up
# a correct but undesirable perl dependency from the module headers which
# isn't required for the kernel proper to function
AutoReqProv: no

%description smp
This package includes a SMP version of the Linux kernel. It is
required only on machines with two or more CPUs as well as machines with
hyperthreading technology.

Install the kernel-smp package if your machine uses two or more CPUs.

%package smp-devel
Summary: Development package for building kernel modules to match the SMP kernel.
Group: System Environment/Kernel
Provides: kernel-smp-devel-%{_target_cpu} = %{rpmversion}-%{release}
Provides: kernel-devel-%{_target_cpu} = %{rpmversion}-%{release}smp
Provides: kernel-devel = %{rpmversion}-%{release}smp
AutoReqProv: no
Prereq: /usr/sbin/hardlink, /usr/bin/find

%description smp-devel
This package provides kernel headers and makefiles sufficient to build modules
against the SMP kernel package.

%package xenU
Summary: The Linux kernel compiled for unprivileged Xen guest VMs

Group: System Environment/Kernel
Provides: kernel = %{version}
Provides: kernel-%{_target_cpu} = %{rpmversion}-%{release}xenU
Prereq: %{kernel_prereq}
Conflicts: %{kernel_dot_org_conflicts}
Conflicts: %{package_conflicts}
Conflicts: %{nptl_conflicts}

# MEF commented out 
# Conflicts: %{xen_conflicts}

# We can't let RPM do the dependencies automatic because it'll then pick up
# a correct but undesirable perl dependency from the module headers which
# isn't required for the kernel proper to function
AutoReqProv: no

%description xenU
This package includes a version of the Linux kernel which
runs in Xen unprivileged guest VMs.  This should be installed
both inside the unprivileged guest (for the modules) and in
the guest0 domain.

%package xenU-devel
Summary: Development package for building kernel modules to match the kernel.
Group: System Environment/Kernel
AutoReqProv: no
Provides: kernel-xenU-devel-%{_target_cpu} = %{rpmversion}-%{release}
Provides: kernel-devel-%{_target_cpu} = %{rpmversion}-%{release}xenU
Provides: kernel-devel = %{rpmversion}-%{release}xenU
Prereq: /usr/sbin/hardlink, /usr/bin/find

%description xenU-devel
This package provides kernel headers and makefiles sufficient to build modules
against the kernel package.

%package uml
Summary: The Linux kernel compiled for use in user mode (User Mode Linux).

Group: System Environment/Kernel

%description uml
This package includes a user mode version of the Linux kernel.

%package uml-devel
Summary: Development package for building kernel modules to match the UML kernel.
Group: System Environment/Kernel
Provides: kernel-uml-devel-%{_target_cpu} = %{rpmversion}-%{release}
Provides: kernel-devel-%{_target_cpu} = %{rpmversion}-%{release}smp
Provides: kernel-devel = %{rpmversion}-%{release}smp
AutoReqProv: no
Prereq: /usr/sbin/hardlink, /usr/bin/find

%description uml-devel
This package provides kernel headers and makefiles sufficient to build modules
against the User Mode Linux kernel package.

%package uml-modules
Summary: The Linux kernel modules compiled for use in user mode (User Mode Linux).

Group: System Environment/Kernel

%description uml-modules
This package includes a user mode version of the Linux kernel modules.

%package vserver
Summary: A placeholder RPM that provides kernel and kernel-drm

Group: System Environment/Kernel
Provides: kernel = %{version}
Provides: kernel-drm = 4.3.0
Provides: kernel-%{_target_cpu} = %{rpmversion}-%{release}

%description vserver
VServers do not require and cannot use kernels, but some RPMs have
implicit or explicit dependencies on the "kernel" package
(e.g. tcpdump). This package installs no files but provides the
necessary dependencies to make rpm and yum happy.



%prep
if [ ! -d kernel-%{kversion}/vanilla ]; then
%setup -q -n %{name}-%{version} -c
rm -f pax_global_header
mv linux-%{kversion} vanilla
else
 cd kernel-%{kversion}
fi

cd vanilla

# make sure the kernel has the sublevel we know it has. This looks weird
# but for -pre and -rc versions we need it since we only want to use
# the higher version when the final kernel is released.
perl -p -i -e "s/^SUBLEVEL.*/SUBLEVEL = %{sublevel}/" Makefile
perl -p -i -e "s/^EXTRAVERSION.*/EXTRAVERSION = -prep/" Makefile

# get rid of unwanted files resulting from patch fuzz
find . -name "*.orig" -exec rm -fv {} \;
find . -name "*~" -exec rm -fv {} \;

###
### build
###
%build

BuildKernel() {
    # create a clean copy in BUILD/ (for backward compatibility with
    # other RPMs that bootstrap off of the kernel build)
    cd $RPM_BUILD_DIR
    rm -rf linux-%{kversion}$1
    cp -rl kernel-%{kversion}/vanilla linux-%{kversion}$1
    cd linux-%{kversion}$1

    # Pick the right config file for the kernel we're building
    Arch=i386
    Target=%{make_target}
    if [ -n "$1" ] ; then
	Config=kernel-%{kversion}-%{_target_cpu}-$1-planetlab.config
	DevelDir=/usr/src/kernels/%{KVERREL}-$1-%{_target_cpu}
	DevelLink=/usr/src/kernels/%{KVERREL}$1-%{_target_cpu}
    	# override ARCH in the case of UML or Xen
    	if [ "$1" = "uml" ] ; then
	    Arch=um
	    Target=linux
	elif [ "$1" = "xenU" ] ; then
	    Arch=xen
    	fi
    else
	Config=kernel-%{kversion}-%{_target_cpu}-planetlab.config
	DevelDir=/usr/src/kernels/%{KVERREL}-%{_target_cpu}
	DevelLink=
    fi

    KernelVer=%{version}-%{release}$1
    echo BUILDING A KERNEL FOR $1 %{_target_cpu}...

    # make sure EXTRAVERSION says what we want it to say
    perl -p -i -e "s/^EXTRAVERSION.*/EXTRAVERSION = -%{release}$1/" Makefile

    # and now to start the build process

    CC=gcc
    gccversion=$(gcc -v 2>&1 | grep "gcc version" | awk '{print $3'} | awk -F . '{print $1}')
    if [ "$gccversion" == "4" ] ; then
	echo "Currently not compiling kernel with gcc 4.x"
	echo "Trying to find a recent gcc 3.x based compiler"
	CC=
	gcc3=$(which gcc32 2>/dev/null || /bin/true)
	[ "$gcc3" != "" ] && CC=gcc32
	echo "gcc3 = $gcc3; CC=${CC}"
	gcc3=$(which gcc33 2>/dev/null || /bin/true)
	[ "$gcc3" != "" ] && CC=gcc33
	echo "gcc3 = $gcc3; CC=${CC}"
	gcc3=$(which gcc34 2>/dev/null || /bin/true)
	[ "$gcc3" != "" ] && CC=gcc34
	echo "gcc3 = $gcc3; CC=${CC}"
	if [ -z "$CC" ]; then
	    echo "Could not find a gcc 3.x based compiler!"
	    echo "Trying to compile with gcc $gccversion anyway"
	    CC=gcc
	    #echo "Aborting kernel compilation!"
	    #exit -1
	fi
    fi
    HOSTCC=${CC}

    make -s CC=${CC} HOSTCC=${HOSTCC} ARCH=$Arch mrproper
    cp configs/$Config .config
    echo "USING ARCH=$Arch CC=${CC} HOSTCC=${HOSTCC}"

    make -s CC=${CC} HOSTCC=${HOSTCC} ARCH=$Arch nonint_oldconfig > /dev/null
    make -s CC=${CC} HOSTCC=${HOSTCC} ARCH=$Arch include/linux/version.h 
    make -s CC=${CC} HOSTCC=${HOSTCC} ARCH=$Arch %{?_smp_mflags} $Target
    make -s CC=${CC} HOSTCC=${HOSTCC} ARCH=$Arch %{?_smp_mflags} modules || exit 1
    make CC=${CC} HOSTCC=${HOSTCC} ARCH=$Arch buildcheck
    
    # Start installing the results

%if "%{_enable_debug_packages}" == "1"
    mkdir -p $RPM_BUILD_ROOT/usr/lib/debug/boot
%endif
    mkdir -p $RPM_BUILD_ROOT/%{image_install_path}
    install -m 644 .config $RPM_BUILD_ROOT/boot/config-$KernelVer
    install -m 644 System.map $RPM_BUILD_ROOT/boot/System.map-$KernelVer
    if [ -f arch/$Arch/boot/bzImage ]; then
	cp arch/$Arch/boot/bzImage $RPM_BUILD_ROOT/%{image_install_path}/vmlinuz-$KernelVer
    fi
    if [ -f arch/$Arch/boot/zImage.stub ]; then
	cp arch/$Arch/boot/zImage.stub $RPM_BUILD_ROOT/%{image_install_path}/zImage.stub-$KernelVer
    fi
    if [ "$1" = "uml" ] ; then
	install -D -m 755 linux $RPM_BUILD_ROOT/%{_bindir}/linux
    fi
    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer
    make -s ARCH=$Arch INSTALL_MOD_PATH=$RPM_BUILD_ROOT modules_install KERNELRELEASE=$KernelVer
 
    # And save the headers/makefiles etc for building modules against
    #
    # This all looks scary, but the end result is supposed to be:
    # * all arch relevant include/ files
    # * all Makefile/Kconfig files
    # * all script/ files 

    rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/source
    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    (cd $RPM_BUILD_ROOT/lib/modules/$KernelVer ; ln -s build source)
    # first copy everything
    cp --parents `find  -type f -name "Makefile*" -o -name "Kconfig*"` $RPM_BUILD_ROOT/lib/modules/$KernelVer/build 
    cp Module.symvers $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    if [ "$1" = "uml" ] ; then
      cp --parents -a `find arch/um -name include` $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    fi
    # then drop all but the needed Makefiles/Kconfig files
    rm -rf $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/Documentation
    rm -rf $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/scripts
    rm -rf $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    cp arch/%{_arch}/kernel/asm-offsets.s $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/arch/%{_arch}/kernel || :
    cp .config $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    cp -a scripts $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    if [ -d arch/%{_arch}/scripts ]; then
      cp -a arch/%{_arch}/scripts $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/arch/%{_arch} || :
    fi
    if [ -f arch/%{_arch}/*lds ]; then
      cp -a arch/%{_arch}/*lds $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/arch/%{_arch}/ || :
    fi
    rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/scripts/*.o
    rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/scripts/*/*.o
    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    cd include
    cp -a acpi config linux math-emu media net pcmcia rxrpc scsi sound video asm asm-generic $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
%if %{buildxen}
    cp -a asm-xen $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
%endif
    if [ "$1" = "uml" ] ; then
      cd asm	
      cp -a `readlink arch` $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
      cd ..
    fi
    cp -a `readlink asm` $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    # Make sure the Makefile and version.h have a matching timestamp so that
    # external modules can be built
    touch -r $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/Makefile $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include/linux/version.h
    touch -r $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/.config $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include/linux/autoconf.h
    cd .. 

    #
    # save the vmlinux file for kernel debugging into the kernel-debuginfo rpm
    #
%if "%{_enable_debug_packages}" == "1"
    mkdir -p $RPM_BUILD_ROOT/usr/lib/debug/lib/modules/$KernelVer
    cp vmlinux $RPM_BUILD_ROOT/usr/lib/debug/lib/modules/$KernelVer
%endif

    # mark modules executable so that strip-to-file can strip them
    find $RPM_BUILD_ROOT/lib/modules/$KernelVer -name "*.ko" -type f  | xargs chmod u+x

    # remove files that will be auto generated by depmod at rpm -i time
    rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/modules.*

    # Move the devel headers out of the root file system
    mkdir -p $RPM_BUILD_ROOT/usr/src/kernels
    mv $RPM_BUILD_ROOT/lib/modules/$KernelVer/build $RPM_BUILD_ROOT/$DevelDir
    ln -sf ../../..$DevelDir $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    [ -z "$DevelLink" ] || ln -sf `basename $DevelDir` $RPM_BUILD_ROOT/$DevelLink
}

###
# DO it...
###

# prepare directories
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/boot

%if %{buildup}
BuildKernel
%endif

%if %{buildsmp}
BuildKernel smp
%endif

%if %{builduml}
BuildKernel uml
%endif

%if %{buildxen}
BuildKernel xenU
%endif


###
### install
###

%install

cd vanilla

%if %{buildxen}
mkdir -p $RPM_BUILD_ROOT/etc/ld.so.conf.d
rm -f $RPM_BUILD_ROOT/etc/ld.so.conf.d/kernelcap-%{KVERREL}.conf
cat > $RPM_BUILD_ROOT/etc/ld.so.conf.d/kernelcap-%{KVERREL}.conf <<\EOF
# This directive teaches ldconfig to search in nosegneg subdirectories
# and cache the DSOs there with extra bit 0 set in their hwcap match
# fields.  In Xen guest kernels, the vDSO tells the dynamic linker to
# search in nosegneg subdirectories and to match this extra hwcap bit
# in the ld.so.cache file.
hwcap 0 nosegneg
EOF
chmod 444 $RPM_BUILD_ROOT/etc/ld.so.conf.d/kernelcap-%{KVERREL}.conf
%endif

%if %{builddoc}
mkdir -p $RPM_BUILD_ROOT/usr/share/doc/kernel-doc-%{kversion}/Documentation

# sometimes non-world-readable files sneak into the kernel source tree
chmod -R a+r *
# copy the source over
tar cf - Documentation | tar xf - -C $RPM_BUILD_ROOT/usr/share/doc/kernel-doc-%{kversion}
%endif

###
### clean
###

%clean
rm -rf $RPM_BUILD_ROOT

###
### scripts
###

# load the loop module for upgrades...in case the old modules get removed we have
# loopback in the kernel so that mkinitrd will work.
%pre 
/sbin/modprobe loop 2> /dev/null > /dev/null  || :
exit 0

%pre smp
/sbin/modprobe loop 2> /dev/null > /dev/null  || :
exit 0

%post 
# trick mkinitrd in case the current environment does not have device mapper
rootdev=$(awk '/^[ \t]*[^#]/ { if ($2 == "/") { print $1; }}' /etc/fstab)
if echo $rootdev |grep -q /dev/mapper 2>/dev/null ; then
    if [ ! -f $rootdev ]; then
	fake_root_lvm=1
	mkdir -p $(dirname $rootdev)
	touch $rootdev
    fi
fi

[ ! -x /usr/sbin/module_upgrade ] || /usr/sbin/module_upgrade
#[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --package kernel --mkinitrd --depmod --install %{KVERREL}
# Older modutils do not support --package option
[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --mkinitrd --depmod --install %{KVERREL}

# remove fake handle
if [ -n "$fake_root_lvm" ]; then
    rm -f $rootdev
fi

# make some useful links
pushd /boot > /dev/null ; {
	ln -sf config-%{KVERREL} config
	ln -sf initrd-%{KVERREL}.img initrd-boot
	ln -sf vmlinuz-%{KVERREL} kernel-boot
}
popd > /dev/null

# ask for a reboot
mkdir -p /etc/planetlab
touch /etc/planetlab/update-reboot

%post devel
if [ -x /usr/sbin/hardlink ] ; then
pushd /usr/src/kernels/%{KVERREL}-%{_target_cpu} > /dev/null
/usr/bin/find . -type f | while read f; do hardlink -c /usr/src/kernels/*FC*/$f $f ; done
popd > /dev/null
fi

%post smp
# trick mkinitrd in case the current environment does not have device mapper
rootdev=$(awk '/^[ \t]*[^#]/ { if ($2 == "/") { print $1; }}' /etc/fstab)
if echo $rootdev |grep -q /dev/mapper 2>/dev/null ; then
    if [ ! -f $rootdev ]; then
	fake_root_lvm=1
	mkdir -p $(dirname $rootdev)
	touch $rootdev
    fi
fi

[ ! -x /usr/sbin/module_upgrade ] || /usr/sbin/module_upgrade
#[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --package kernel-smp --mkinitrd --depmod --install %{KVERREL}smp
# Older modutils do not support --package option
[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --mkinitrd --depmod --install %{KVERREL}smp

# remove fake handle
if [ -n "$fake_root_lvm" ]; then
    rm -f $rootdev
fi

# make some useful links
pushd /boot > /dev/null ; {
	ln -sf config-%{KVERREL}smp configsmp
	ln -sf initrd-%{KVERREL}smp.img initrd-bootsmp
	ln -sf vmlinuz-%{KVERREL}smp kernel-bootsmp
}
popd > /dev/null

# ask for a reboot
mkdir -p /etc/planetlab
touch /etc/planetlab/update-reboot

%post smp-devel
if [ -x /usr/sbin/hardlink ] ; then
pushd /usr/src/kernels/%{KVERREL}-smp-%{_target_cpu} > /dev/null
/usr/bin/find . -type f | while read f; do hardlink -c /usr/src/kernels/*FC*/$f $f ; done
popd > /dev/null
fi

%post xenU
[ ! -x /usr/sbin/module_upgrade ] || /usr/sbin/module_upgrade
[ ! -x /sbin/ldconfig ] || /sbin/ldconfig -X

%post xenU-devel
if [ -x /usr/sbin/hardlink ] ; then
pushd /usr/src/kernels/%{KVERREL}-xenU-%{_target_cpu} > /dev/null
/usr/bin/find . -type f | while read f; do hardlink -c /usr/src/kernels/*FC*/$f $f ; done
popd > /dev/null
fi

%post uml-modules
depmod -ae %{KVERREL}uml


%preun 
/sbin/modprobe loop 2> /dev/null > /dev/null  || :
[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --rminitrd --rmmoddep --remove %{KVERREL}

%preun smp
/sbin/modprobe loop 2> /dev/null > /dev/null  || :
[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --rminitrd --rmmoddep --remove %{KVERREL}smp

%preun xenU
/sbin/modprobe loop 2> /dev/null > /dev/null  || :
[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --rmmoddep --remove %{KVERREL}xenU

%preun uml-modules
rm -f /lib/modules/%{KVERREL}uml/modules.*


###
### file lists
###

%if %{buildup}
%files 
%defattr(-,root,root)
/%{image_install_path}/*-%{KVERREL}
/boot/System.map-%{KVERREL}
/boot/config-%{KVERREL}
%dir /lib/modules/%{KVERREL}
/lib/modules/%{KVERREL}/kernel
/lib/modules/%{KVERREL}/build
/lib/modules/%{KVERREL}/source

%files devel
%defattr(-,root,root)
%verify(not mtime) /usr/src/kernels/%{KVERREL}-%{_target_cpu}
%endif

%if %{buildsmp}
%files smp
%defattr(-,root,root)
/%{image_install_path}/*-%{KVERREL}smp
/boot/System.map-%{KVERREL}smp
/boot/config-%{KVERREL}smp
%dir /lib/modules/%{KVERREL}smp
/lib/modules/%{KVERREL}smp/kernel
/lib/modules/%{KVERREL}smp/build
/lib/modules/%{KVERREL}smp/source

%files smp-devel
%defattr(-,root,root)
%verify(not mtime) /usr/src/kernels/%{KVERREL}-smp-%{_target_cpu}
/usr/src/kernels/%{KVERREL}smp-%{_target_cpu}
%endif

%if %{builduml}
%files uml
%defattr(-,root,root)
%{_bindir}/linux

%files uml-devel
%defattr(-,root,root)
%verify(not mtime) /usr/src/kernels/%{KVERREL}-uml-%{_target_cpu}
/usr/src/kernels/%{KVERREL}uml-%{_target_cpu}

%files uml-modules
%defattr(-,root,root)
/boot/System.map-%{KVERREL}uml
/boot/config-%{KVERREL}uml
%dir /lib/modules/%{KVERREL}uml
/lib/modules/%{KVERREL}uml/kernel
%verify(not mtime) /lib/modules/%{KVERREL}uml/build
/lib/modules/%{KVERREL}uml/source
%endif

%if %{buildxen}
%files xenU
%defattr(-,root,root)
/%{image_install_path}/*-%{KVERREL}xenU
/boot/System.map-%{KVERREL}xenU
/boot/config-%{KVERREL}xenU
%dir /lib/modules/%{KVERREL}xenU
/lib/modules/%{KVERREL}xenU/kernel
%verify(not mtime) /lib/modules/%{KVERREL}xenU/build
/lib/modules/%{KVERREL}xenU/source
/etc/ld.so.conf.d/kernelcap-%{KVERREL}.conf

%files xenU-devel
%defattr(-,root,root)
%verify(not mtime) /usr/src/kernels/%{KVERREL}-xenU-%{_target_cpu}
/usr/src/kernels/%{KVERREL}xenU-%{_target_cpu}
%endif

%files vserver
%defattr(-,root,root)
# no files


# only some architecture builds need kernel-doc

%if %{builddoc}
%files doc
%defattr(-,root,root)
%{_datadir}/doc/kernel-doc-%{kversion}/Documentation/*
%dir %{_datadir}/doc/kernel-doc-%{kversion}/Documentation
%dir %{_datadir}/doc/kernel-doc-%{kversion}
%endif

%changelog
* Fri Jul 15 2005 Dave Jones <davej@redhat.com>
- Include a number of patches likely to show up in 2.6.12.3

* Thu Jul 14 2005 Dave Jones <davej@redhat.com>
- Add Appletouch support.

* Wed Jul 13 2005 David Woodhouse <dwmw2@redhat.com>
- Audit updates. In particular, don't printk audit messages that 
  are passed from userspace when auditing is disabled.

* Tue Jul 12 2005 Dave Jones <davej@redhat.com>
- Fix up several reports of CD's causing crashes.
- Make -p port arg of rpc.nfsd work.
- Work around a usbmon deficiency.
- Fix connection tracking bug with bridging. (#162438)

* Mon Jul 11 2005 Dave Jones <davej@redhat.com>
- Fix up locking in piix IDE driver whilst tuning chipset.

* Tue Jul  5 2005 Dave Jones <davej@redhat.com>
- Fixup ACPI IRQ routing bug that prevented booting for some folks.
- Reenable ISA I2C drivers for x86-64.
- Bump requirement on mkinitrd to something newer (#160492)

* Wed Jun 29 2005 Dave Jones <davej@redhat.com>
- 2.6.12.2

* Mon Jun 27 2005 Dave Jones <davej@redhat.com>
- Disable multipath caches. (#161168)
- Reenable AMD756 I2C driver for x86-64. (#159609)
- Add more IBM r40e BIOS's to the C2/C3 blacklist.

* Thu Jun 23 2005 Dave Jones <davej@redhat.com>
- Make orinoco driver suck less.
  (Scanning/roaming/ethtool support).
- Exec-shield randomisation fix.
- pwc driver warning fix.
- Prevent potential oops in tux with symlinks. (#160219)

* Wed Jun 22 2005 Dave Jones <davej@redhat.com>
- 2.6.12.1
  - Clean up subthread exec (CAN-2005-1913)
  - ia64 ptrace + sigrestore_context (CAN-2005-1761)

* Wed Jun 22 2005 David Woodhouse <dwmw2@redhat.com>
- Update audit support

* Mon Jun 20 2005 Dave Jones <davej@redhat.com>
- Rebase to 2.6.12
  - Temporarily drop Alans IDE fixes whilst they get redone.
- Enable userspace queueing of ipv6 packets.

* Tue Jun  7 2005 Dave Jones <davej@redhat.com>
- Drop recent b44 changes which broke some setups.

* Wed Jun  1 2005 Dave Jones <davej@redhat.com>
- Fix up ALI IDE regression. (#157175)

* Mon May 30 2005 Dave Jones <davej@redhat.com>
- Fix up VIA IRQ quirk.

* Sun May 29 2005 Dave Jones <davej@redhat.com>
- Fix slab corruption in firewire (#158424)

* Fri May 27 2005 Dave Jones <davej@redhat.com>
- remove non-cleanroom pwc driver compression.
- Fix unintialised value in single bit error detector. (#158825)

* Wed May 25 2005 Dave Jones <davej@redhat.com>
- Disable TPM driver, it breaks 8139 driver.
- Revert to previous version of ipw2x00 drivers.
  The newer ones sadly brought too many problems this close to
  the release. I'll look at updating them again for an update.
- Update to 2.6.12rc5
  Fix potential local DoS. 1-2 other small fixes.
- Tweak to fix up some vdso arithmetic.
- Disable sysenter again for now.

* Wed May 25 2005 David Woodhouse <dwmw2@redhat.com>
- Turn off CONFIG_ISA on PPC again. It makes some Macs unhappy (#149200)
- Make Speedtouch DSL modem resync automatically

* Tue May 24 2005 Dave Jones <davej@redhat.com>
- Update various cpufreq drivers.
- 2.6.12-rc4-git8
  kobject ordering, tg3 fixes, ppc32 ipic fix,
  ppc64 powermac smp fix. token-ring fixes,
  TCP fix. ipv6 fix.
- Disable slab debugging.

* Mon May 23 2005 Dave Jones <davej@redhat.com>
- Add extra id to SATA Sil driver. (#155748)
- Fix oops on rmmod of lanai & ms558 drivers when no hardware present.

* Mon May 23 2005 Dave Jones <davej@redhat.com>
- Fix double unlock of spinlock on tulip. (#158522)

* Mon May 23 2005 David Woodhouse <dwmw2@redhat.com>
- audit updates: log serial # in user messages, escape comm= in syscalls

* Mon May 23 2005 Dave Jones <davej@redhat.com>
- 2.6.12-rc4-git6
  MMC update, reiserfs fixes, AIO fix.
- Fix absolute symlink in -devel (#158582)
- 2.6.12-rc4-git7
  PPC64 & i2c fixes
- Fix another divide by zero in ipw2100 (#158406)
- Fix dir ownership in kernel-doc rpm (#158478)

* Sun May 22 2005 Dave Jones <davej@redhat.com>
- Fix divide by zero in ipw2100 driver. (#158406)
- 2.6.12-rc4-git5
  More x86-64 updates, Further pktcdvd frobbing,
  yet more dvb updates, x86(64) ioremap fixes,
  ppc updates, IPMI sysfs support (reverted for now due to breakage),
  various SCSI fixes (aix7xxx, spi transport), vmalloc improvements

* Sat May 21 2005 David Woodhouse <dwmw2@redhat.com>
- Fix oops in avc_audit() (#158377)
- Include serial numbers in non-syscall audit messages

* Sat May 21 2005 Bill Nottingham <notting@redhat.com>
- bump ipw2200 conflict

* Sat May 21 2005 Dave Jones <davej@redhat.com> [2.6.11-1.1334_FC4]
- driver core: restore event order for device_add()

* Sat May 21 2005 David Woodhouse <dwmw2@redhat.com>
- More audit updates. Including a fix for AVC_USER messages.

* Fri May 20 2005 Dave Jones <davej@redhat.com>
- 2.6.12-rc4-git4
  networking fixes (netlink, pkt_sched, ipsec, netfilter,
  ip_vs, af_unix, ipv4/6, xfrm). TG3 driver improvements.

* Thu May 19 2005 Dave Jones <davej@redhat.com> [2.6.11-1.1327_FC4]
- 2.6.12-rc4-git3
  Further fixing to raw driver. More DVB updates,
  driver model updates, power management improvements,
  ext3 fixes.  
- Radeon on thinkpad backlight power-management goodness.
  (Peter Jones owes me two tacos).
- Fix ieee1394 smp init.

* Thu May 19 2005 Rik van Riel <riel@redhat.com>
- Xen: disable TLS warning (#156414)

* Thu May 19 2005 David Woodhouse <dwmw2@redhat.com>
- Update audit patches

* Thu May 19 2005 Dave Jones <davej@redhat.com> [2.6.11-1.1325_FC4]
- Fix up missing symbols in ipw2200 driver.
- Reenable debugfs / usbmon. SELinux seems to cope ok now.
  (Needs selinux-targeted-policy >= 1.23.16-1)

* Wed May 18 2005 Dave Jones <davej@redhat.com>
- Fix up some warnings in the IDE patches.
- 2.6.12-rc4-git2
  Further pktcdvd fixing, DVB update, Lots of x86-64 updates,
  ptrace fixes, ieee1394 changes, input layer tweaks,
  md layer fixes, PCI hotplug improvements, PCMCIA fixes,
  libata fixes, serial layer, usb core, usbnet, VM fixes,
  SELinux tweaks.
- Update ipw2100 driver to 1.1.0
- Update ipw2200 driver to 1.0.4 (#158073)

* Tue May 17 2005 Dave Jones <davej@redhat.com>
- 2.6.12-rc4-git1
  ARM, ioctl security fixes, mmc driver update,
  ibm_emac & tulip netdriver fixes, serial updates
  ELF loader security fix.

* Mon May 16 2005 Rik van Riel <riel@redhat.com>
- enable Xen again (not tested yet)
- fix a typo in the EXPORT_SYMBOL patch

* Sat May 14 2005 Dave Jones <davej@redhat.com>
- Update E1000 driver from netdev-2.6 tree. 
- Add some missing EXPORT_SYMBOLs.

* Fri May 13 2005 Dave Jones <davej@redhat.com>
- Bump maximum supported CPUs on x86-64 to 32.
- Tickle the NMI watchdog when we're doing serial writes.
- SCSI CAM geometry fix.
- Slab debug single-bit error improvement.

* Thu May 12 2005 David Woodhouse <dwmw2@redhat.com>
- Enable CONFIG_ISA on ppc32 to make the RS/6000 user happy.
- Update audit patches

* Wed May 11 2005 Dave Jones <davej@redhat.com>
- Add Ingo's patch to detect soft lockups.
- Thread exits silently via __RESTORE_ALL exception for iret. (#154369)

* Wed May 11 2005 David Woodhouse <dwmw2@redhat.com>
- Import post-rc4 audit fixes from git, including ppc syscall auditing

* Wed May 11 2005 Dave Jones <davej@redhat.com>
- Revert NMI watchdog changes.

* Tue May 10 2005 Dave Jones <davej@redhat.com>
- Enable PNP on x86-64

* Tue May 10 2005 Jeremy Katz <katzj@redhat.com>
- make other -devel packages provide kernel-devel so they get 
  installed instead of upgraded (#155988)

* Mon May  9 2005 Dave Jones <davej@redhat.com>
- Rebase to 2.6.12-rc4
  | Xen builds are temporarily disabled again.
- Conflict if old version of ipw firmware is present.

* Fri May  6 2005 Dave Jones <davej@redhat.com>
- Add PCI ID for new sundance driver. (#156859)

* Thu May  5 2005 David Woodhouse <dwmw2@redhat.com>
- Import audit fixes from upstream

* Wed May  4 2005 Jeremy Katz <katzj@redhat.com>
- enable radeonfb and agp on ppc64 to fix X on the G5

* Tue May  3 2005 Dave Jones <davej@redhat.com>
- Disable usbmon/debugfs again for now until SELinux policy is fixed.

* Mon May  2 2005 David Woodhouse <dwmw2@redhat.com>
- Make kallsyms include platform-specific symbols
- Fix might_sleep warning in pbook clock-spreading fix

* Sun May  1 2005 Dave Jones <davej@redhat.com>
- Fix yesterdays IDE fixes.
- Blacklist another brainless SCSI scanner. (#155457)

* Sun May  1 2005 David Woodhouse <dwmw2@redhat.com>
- Fix EHCI port power switching

* Sun May  1 2005 Dave Jones <davej@redhat.com>
- Enable usbmon & debugfs. (#156489)

* Sat Apr 30 2005 Dave Jones <davej@redhat.com>
- Numerous IDE layer fixes from Alan Cox.
- Kill off some stupid messages from the input layer.

* Fri Apr 29 2005 Roland McGrath <roland@redhat.com>
- Fix the 32bit emulation on x86-64 segfaults.

* Wed Apr 27 2005 Dave Jones <davej@redhat.com>
- Hopefully fix the random reboots some folks saw on x86-64.

* Wed Apr 27 2005 Jeremy Katz <katzj@redhat.com>
- fix prereqs for -devel packages

* Wed Apr 27 2005 Rik van Riel <riel@redhat.com>
- Fix up the vdso stuff so kernel-xen* compile again
- Import upstream bugfix so xenU domains can be started again

* Tue Apr 26 2005 Dave Jones <davej@redhat.com>
- Fix up the vdso again, which broke on the last rebase to -rc3
- Fix the put_user() fix. (#155999)

* Mon Apr 25 2005 Dave Jones <davej@redhat.com>
- Fix x86-64 put_user()
- Fix serio oops.
- Fix ipv6_skip_exthdr() invocation causing OOPS.
- Fix up some permissions on some /proc files.
- Support PATA drives on Promise SATA. (#147303)

* Mon Apr 25 2005 Rik van Riel <riel@redhat.com>
- upgrade to the latest version of xenolinux patches
- reenable xen (it boots, ship it!)

* Sat Apr 23 2005 David Woodhouse <dwmw2@redhat.com>
- Enable adt746x and windtunnel thermal modules
- Disable clock spreading on certain pbooks before sleep
- Sound support for Mac Mini

* Fri Apr 22 2005 Dave Jones <davej@redhat.com>
- Reenable i2c-viapro on x86-64.

* Fri Apr 22 2005 Dave Jones <davej@redhat.com>
- Don't build powernow-k6 on anything other than 586 kernels.
- Temporarily disable Xen again.

* Wed Apr 20 2005 Dave Jones <davej@redhat.com>
- 2.6.12rc3

* Wed Apr 20 2005 Dave Jones <davej@redhat.com>
- Adjust struct dentry 'padding' based on 64bit'ness.

* Tue Apr 19 2005 Dave Jones <davej@redhat.com>
- Print stack trace when we panic.
  Might give more clues for some of the wierd panics being seen right now.
- Blacklist another 'No C2/C3 states' Thinkpad R40e BIOS. (#155236)

* Mon Apr 18 2005 Dave Jones <davej@redhat.com>
- Make ISDN ICN driver not oops when probed with no hardware present.
- Add missing MODULE_LICENSE to mac_modes.ko

* Sat Apr 16 2005 Dave Jones <davej@redhat.com>
- Make some i2c drivers arch dependant.
- Make multimedia buttons on Dell inspiron 8200 work. (#126148)
- Add diffutils buildreq (#155121)

* Thu Apr 14 2005 Dave Jones <davej@redhat.com>
- Build DRM modular. (#154769)

* Wed Apr 13 2005 Rik van Riel <riel@redhat.com>
- fix up Xen for 2.6.12-rc2
- drop arch/xen/i386/signal.c, thanks to Roland's vdso patch (yay!)
- reenable xen compile - this kernel test boots on my system

* Tue Apr 12 2005 Dave Jones <davej@redhat.com>
- Further vdso work from Roland.

* Mon Apr 11 2005 David Woodhouse <dwmw2@redhat.com>
- Disable PPC cpufreq/sleep patches which make sleep less reliable
- Add TIMEOUT to hotplug environment when requesting firmware (#153993)

* Sun Apr 10 2005 Dave Jones <davej@redhat.com>
- Integrate Roland McGrath's changes to make exec-shield
  and vdso play nicely together.

* Fri Apr  8 2005 Dave Jones <davej@redhat.com>
- Disable Longhaul driver (again).

* Wed Apr  6 2005 Dave Jones <davej@redhat.com>
- 2.6.12rc2
  - netdump/netconsole currently broken.
  - Xen temporarily disabled.

* Fri Apr  1 2005 Dave Jones <davej@redhat.com>
- Make the CFQ elevator the default again.

* Thu Mar 31 2005 Rik van Riel <riel@redhat.com>
- upgrade to new upstream Xen code, twice 
- for performance reasons, disable CONFIG_DEBUG_PAGEALLOC for FC4t2

* Wed Mar 30 2005 Rik van Riel <riel@redhat.com>
- fix Xen kernel compilation (pci, page table, put_user, execshield, ...)
- reenable Xen kernel compilation

* Tue Mar 29 2005 Rik van Riel <riel@redhat.com>
- apply Xen patches again (they don't compile yet, though)
- Use uname in kernel-devel directories (#145914)
- add uname-based kernel-devel provisions (#152357)
- make sure /usr/share/doc/kernel-doc-%%{kversion} is owned by a
  package, so it will get removed again on uninstall/upgrade (#130667)

* Mon Mar 28 2005 Dave Jones <davej@redhat.com>
- Don't generate debuginfo files if %%_enable_debug_packages isnt set. (#152268)

* Sun Mar 27 2005 Dave Jones <davej@redhat.com>
- 2.6.12rc1-bk2
- Disable NVidia FB driver for time being, it isn't stable.

* Thu Mar 24 2005 Dave Jones <davej@redhat.com>
- rebuild

* Tue Mar 22 2005 Dave Jones <davej@redhat.com>
- Fix several instances of swapped arguments to memset()
- 2.6.12rc1-bk1

* Fri Mar 18 2005 Dave Jones <davej@redhat.com>
- kjournald release race. (#146344)
- 2.6.12rc1

* Thu Mar 17 2005 Rik van Riel <riel@redhat.com>
- upgrade to latest upstream Xen code

* Tue Mar 15 2005 Rik van Riel <riel@redhat.com>
- add Provides: headers for external kernel modules (#149249)
- move build & source symlinks from kernel-*-devel to kernel-* (#149210)
- fix xen0 and xenU devel %%post scripts to use /usr/src/kernels (#149210)

* Thu Mar 10 2005 Dave Jones <davej@redhat.com>
- Reenable advansys driver for x86

* Tue Mar  8 2005 Dave Jones <davej@redhat.com>
- Change SELinux execute-related permission checking. (#149819)

* Sun Mar  6 2005 Dave Jones <davej@redhat.com>
- Forward port some FC3 patches that got lost.

* Fri Mar  4 2005 Dave Jones <davej@redhat.com>
- Fix up ACPI vs keyboard controller problem.
- Fix up Altivec usage on PPC/PPC64.

* Fri Mar  4 2005 Dave Jones <davej@redhat.com>
- Finger the programs that try to read from /dev/mem.
- Improve spinlock debugging a little.

* Thu Mar  3 2005 Dave Jones <davej@redhat.com>
- Fix up the unresolved symbols problem.

* Thu Mar  3 2005 Rik van Riel <riel@redhat.com>
- upgrade to new Xen snapshot (requires new xen RPM, too)

* Wed Mar  2 2005 Dave Jones <davej@redhat.com>
- 2.6.11

* Tue Mar 1 2005 David Woodhouse <dwmw2@redhat.com>
- Building is nice. Booting would be better. Work around GCC -Os bug which
  which makes the PPC kernel die when extracting its initramfs. (#150020)
- Update include/linux/compiler-gcc+.h

* Tue Mar 1 2005 Dave Jones <davej@redhat.com>
- 802.11b/ipw2100/ipw2200 update.
- 2.6.11-rc5-bk4

* Tue Mar 1 2005 David Woodhouse <dwmw2@redhat.com>
- Fix ppc/ppc64/ppc64iseries builds for gcc 4.0
- Fix Xen build too

* Mon Feb 28 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc5-bk3
- Various compile fixes for building with gcc-4.0

* Sat Feb 26 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc5-bk1

* Fri Feb 25 2005 Dave Jones <davej@redhat.com>
- Hopefully fix the zillion unresolved symbols. (#149758)

* Thu Feb 24 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc5

* Wed Feb 23 2005 Rik van Riel <riel@redhat.com>
- get rid of unknown symbols in kernel-xen0 (#149495)

* Wed Feb 23 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc4-bk11

* Mon Feb 21 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc4-bk9

* Sat Feb 19 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc4-bk7

* Sat Feb 19 2005 Rik van Riel <riel@redhat.com>
- upgrade to newer Xen code, needs xen-20050218 to run

* Sat Feb 19 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc4-bk6

* Fri Feb 18 2005 David Woodhouse <dwmw2@redhat.com>
- Add SMP kernel for PPC32

* Fri Feb 18 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc4-bk5

* Tue Feb 15 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc4-bk3

* Mon Feb 14 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc4-bk2

* Sun Feb 13 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc4-bk1

* Sat Feb 12 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc4

* Fri Feb 11 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc3-bk8

* Thu Feb 10 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc3-bk7

* Wed Feb  9 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc3-bk6

* Tue Feb  8 2005 Dave Jones <davej@redhat.com>
- Enable old style and new style USB initialisation.
- More PPC jiggery pokery hackery.
- 2.6.11-rc3-bk5

* Mon Feb  7 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc3-bk4
- Various patches to unbork PPC.
- Display taint bits on VM error.

* Mon Feb  7 2005 Rik van Riel <riel@redhat.com>
- upgrade to latest upstream Xen bits, upgrade those to 2.6.11-rc3-bk2

* Sat Feb  5 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc3-bk2

* Fri Feb  4 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc3-bk1

* Wed Feb  2 2005 Dave Jones <davej@redhat.com>
- Stop the input layer spamming the console. (#146906)
- 2.6.11-rc3

* Tue Feb  1 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc2-bk10
- Reenable periodic slab checker.

* Tue Feb  1 2005 Rik van Riel <riel@redhat.com>
- update to latest xen-unstable source snapshot
- add agpgart patch from upstream xen tree
- port Ingo's latest execshield updates to Xen

* Mon Jan 31 2005 Rik van Riel <riel@redhat.com>
- enable SMP support in xenU kernel, use the xen0 kernel for the
  unprivileged domains if the SMP xenU breaks on your system

* Thu Jan 27 2005 Dave Jones <davej@redhat.com>
- Drop VM hack that broke in yesterdays rebase.

* Wed Jan 26 2005 Dave Jones <davej@redhat.com>
- Drop 586-SMP kernels.  These are a good candidate for
  fedora-extras when it appears. The number of people
  actually using this variant is likely to be very very small.
- 2.6.11-rc2-bk4

* Tue Jan 25 2005 Dave Jones <davej@redhat.com>
- 2.6.11-rc2-bk3

* Sun Jan 23 2005 Dave Jones <davej@redhat.com>
- Updated periodic slab debug check from Manfred.
- Enable PAGE_ALLOC debugging again, it should now be fixed.
- 2.6.11-rc2-bk1

* Fri Jan 21 2005 Dave Jones <davej@redhat.com>
- Rebase to 2.6.11-rc2

* Fri Jan 21 2005 Rik van Riel <riel@redhat.com>
- make exec-shield segment limits work inside the xen kernels

* Thu Jan 20 2005 Dave Jones <davej@redhat.com>
- Rebase to -bk8

* Wed Jan 19 2005 Dave Jones <davej@redhat.com>
- Re-add diskdump/netdump based on Jeff Moyers patches.
- Rebase to -bk7

* Tue Jan 18 2005 Jeremy Katz <katzj@redhat.com>
- fixup xen0 %%post to use new grubby features for multiboot kernels
- conflict with older mkinitrd for kernel-xen0

* Tue Jan 18 2005 Dave Jones <davej@redhat.com>
- -bk6

* Mon Jan 17 2005 Dave Jones <davej@redhat.com>
- First stab at kernel-devel packages. (David Woodhouse).

* Mon Jan 17 2005 Rik van Riel <riel@redhat.com>
- apply dmi fix, now xenU boots again

* Fri Jan 14 2005 Dave Jones <davej@redhat.com>
- Rebase to 2.6.11-bk2

* Thu Jan 13 2005 Dave Jones <davej@redhat.com>
- Rebase to 2.6.11-bk1

* Wed Jan 12 2005 Dave Jones <davej@redhat.com>
- Rebase to 2.6.11rc1

* Tue Jan 11 2005 Rik van Riel <riel@redhat.com>
- fix Xen compile with -bk14

* Tue Jan 11 2005 Dave Jones <davej@redhat.com>
- Update to -bk14
- Print tainted information in slab corruption messages.

* Tue Jan 11 2005 Rik van Riel <riel@redhat.com>
- merge fix for the Xen TLS segment fixup issue

* Tue Jan 11 2005 Dave Jones <davej@redhat.com>
- Depend on hardlink, not kernel-utils.

* Mon Jan 10 2005 Dave Jones <davej@redhat.com>
- Update to -bk13, reinstate GFP_ZERO patch which hopefully
  is now fixed.
- Add another Lexar card reader to the whitelist. (#143600)
- Package asm-m68k for asm-ppc includes. (don't ask). (#144604)

* Sat Jan  8 2005 Dave Jones <davej@redhat.com>
- Periodic slab debug is incompatable with pagealloc debug.
  Disable the latter.

* Fri Jan  7 2005 Dave Jones <davej@redhat.com>
- Santa came to Notting's house too. (another new card reader)
- Rebase to 2.6.10-bk10

* Thu Jan  6 2005 Rik van Riel <riel@redhat.com>
- update to latest xen-unstable tree
- fix up Xen compile with -bk9, mostly pudding

* Thu Jan  6 2005 Dave Jones <davej@redhat.com>
- Rebase to 2.6.10-bk9

* Tue Jan  4 2005 Dave Jones <davej@redhat.com>
- Rebase to 2.6.10-bk7
- Add periodic slab debug checker.

* Sun Jan  2 2005 Dave Jones <davej@redhat.com>
- Rebase to 2.6.10-bk5

* Sat Jan  1 2005 Dave Jones <davej@redhat.com>
- Fix probing of vesafb. (#125890)
- Reenable EDD.
- Don't assume existance of ~/.gnupg (#142201)

* Fri Dec 31 2004 Dave Jones <davej@redhat.com>
- Rebase to 2.6.10-bk4

* Thu Dec 30 2004 Dave Jones <davej@redhat.com>
- Rebase to 2.6.10-bk3

* Tue Dec 28 2004 Dave Jones <davej@redhat.com>
- Drop bogus ethernet slab cache.

* Sun Dec 26 2004 Dave Jones <davej@redhat.com>
- Santa brought a new card reader that needs whitelisting.

* Fri Dec 24 2004 Dave Jones <davej@redhat.com>
- Rebase to 2.6.10

* Wed Dec 22 2004 Dave Jones <davej@redhat.com>
- Re-add missing part of the exit() race fix. (#142505, #141896)

* Tue Dec 21 2004 Dave Jones <davej@redhat.com>
- Fix two silly bugs in the AGP posting fixes.

* Fri Dec 17 2004 Dave Jones <davej@redhat.com>
- Fix bio error propagation.
- Clear ebp on sysenter return.
- Extra debugging info on OOM kill.
- exit() race fix.
- Fix refcounting order in sd/sr, fixing cable pulls on USB storage.
- IGMP source filter fixes.
- Fix ext2/3 leak on umount.
- fix missing wakeup in ipc/sem
- Fix another tux corner case bug.
- NULL out ptrs in airo driver after kfree'ing them.

* Thu Dec 16 2004 Dave Jones <davej@redhat.com>
- Better version of the PCI Posting fixes for AGPGART.
- Add missing cache flush to the AGP code.
- Drop netdump and common crashdump code.

* Mon Dec 13 2004 Dave Jones <davej@redhat.com>
- Drop diskdump. Aiming for a better kexec based solution for FC4.

* Sun Dec 12 2004 Dave Jones <davej@redhat.com>
- fix false ECHILD result from wait* with zombie group leader.

* Sat Dec 11 2004 Dave Jones <davej@redhat.com>
- Workaround broken pci posting in AGPGART.
- Compile 686 kernel tuned for pentium4.
  | Needs benchmarking across various CPUs under
  | various workloads to find out if its worth keeping.
- Make sure VC resizing fits in s16.

* Fri Dec 10 2004 Dave Jones <davej@redhat.com>
- Prevent block device queues from being shared in viocd. (#139018)
- Libata updates. (#132848, #138405)
- aacraid: remove aac_handle_aif (#135527)
- fix uninitialized variable in waitid(2). (#142505)
- Fix CMSG validation checks wrt. signedness.
- Fix memory leak in ip_conntrack_ftp
- [IPV4]: Do not leak IP options.
- ppc64: Align PACA buffer for hypervisor's use. (#141817)
- ppc64: Indicate that the veth link is always up. (#135402)
- ppc64: Quiesce OpenFirmware stdin device at boot. (#142009)
- SELinux: Fix avc_node_update oops. (#142353)
- Fix CCISS ioctl return code.
- Make ppc64's pci_alloc_consistent() conform to documentation. (#140047)
- Enable EDD
- Enable ETH1394. (#138497)
- Workaround E1000 post-maturely writing back to TX descriptors. (#133261)
- Fix the previous E1000 errata workaround.
- Several IDE fixes from 2.6.9-ac
- vm pageout throttling. (#133858)
- Fix Tux from oopsing. (#140918)
- Fix Tux/SELinux incompatability (#140916)
- Fix Tux/IPV6 problem. (#140916)
- ide: Fix possible oops on boot.
- Make spinlock debugging panic instead of printk.
- Update Emulex lpfc driver to 8.0.16
- Selected patches from 2.6.9-ac12
- ppc64: Fix inability to find space for TCE table (#138844)
- Fix compat fcntl F_GETLK{,64} (#141680)
- blkdev_get_blocks(): handle eof
- Another card reader for the whitelist. (#134094)
- Disable tiglusb module. (#142102)
- E1000 64k-alignment fix. (#140047)
- Disable tiglusb module. (#142102)
- ID updates for cciss driver.
- Fix overflows in USB Edgeport-IO driver. (#142258)
- Fix wrong TASK_SIZE for 32bit processes on x86-64. (#141737)
- Fix ext2/ext3 xattr/mbcache race. (#138951)
- Fix bug where __getblk_slow can loop forever when pages are partially mapped. (#140424)
- Add missing cache flushes in agpgart code.

* Thu Dec  9 2004 Dave Jones <davej@redhat.com>
- Drop the 4g/4g hugemem kernel completely.

* Wed Dec  8 2004 Rik van Riel <riel@redhat.com>
- make Xen inherit config options from x86

* Mon Dec  6 2004 Rik van Riel <riel@redhat.com>
- apparently Xen works better without serial drivers in domain0 (#141497)

* Sun Dec  5 2004 Rik van Riel <riel@redhat.com>
- Fix up and reenable Xen compile.
- Fix bug in install part of BuildKernel. 

* Sat Dec  4 2004 Dave Jones <davej@redhat.com>
- Enable both old and new megaraid drivers.
- Add yet another card reader to usb scsi whitelist. (#141367)

* Fri Dec  3 2004 Dave Jones <davej@redhat.com>
- Sync all patches with latest updates in FC3.
- Fix up xen0/xenU uninstall.
- Temporarily disable xen builds.

* Wed Dec  1 2004 Rik van Riel <riel@redhat.com>
- replace VM hack with the upstream version
- more Xen bugfixes

* Tue Nov 30 2004 Rik van Riel <riel@redhat.com>
- upgrade to later Xen sources, with upstream bugfixes
- export direct_remap_area_pages for Xen

* Mon Nov 29 2004 Dave Jones <davej@redhat.com>
- Add another card reader to whitelist. (#141022)

* Fri Nov 26 2004 Rik van Riel <riel@redhat.com>
- add Xen kernels for i686, plus various bits and pieces to make them work

* Mon Nov 15 2004 Dave Jones <davej@redhat.com>
- Rebase to 2.6.9-ac9

* Sat Nov 13 2004 Dave Jones <davej@redhat.com>
- Drop some bogus patches.

* Thu Nov 11 2004 Dave Jones <davej@redhat.com>
- NFS: Fix dentry refcount accounting error
- Fix single-stepping on PPC64
- Integrate kernel-devel changes
- SELinux: netif fixes.
- SELinux: add DAC check to setxattr() hook.
- SELinux: sidtab locking fix.
- SELinux: mediate send_sigurg().
- SELinux: fix setscheduler hook deadlock.
- ide-floppy: Supresses error messages resulting from Medium not present
- Various IA64 updates from 2.6.10rc1
- nfsd: make sure getxattr inode op is non-NULL before calling it.
- Handle NULL dev->dev_addr in SIOCGIFHWADDR correctly. (#137648)
- Fix NFSD domainname size limit.
- nfsd4: nfsd oopsed when encountering a conflict with a local lock
- nfsd4: fix putrootfh return
- nfsd: Insecure port warning shows decimal IPv4 address
- Disable sw irqbalance/irqaffinity for e7520/e7320/e7525 (#136419)
- Fix exec-shield non-PIE/non-prelinked bug
- ext3 reservations: fix goal hit accounting.
- Fix problems with non-power-of-two sector size discs. (#135094)
- Fix possible oops in netpoll (#132153)
- Add missing MODULE_VERSION tags to various modules. (#136399)
- Add USB card reader de jour. (#124048)
- Remove SG_IO deprecation warning (#136179)
- Make sure that modules get signed with the right key.
- Remove SG_IO deprecation warning (#136179)
- s390: Fix fake_ll for qeth device. (#136175)
- s390: zfcp: Kernel stack frame for zfcp_cfdc_dev_ioctl() is too big
- s390: Use slab allocator for DASD I/O pages.
- PPC64: HVSI udbg support
- PPC64: Make HVSI console survive FSP reset
- PPC64: Make PCI hostbridge hotplugging work
- PPC64: Fix IBM VSCSI problems (#138124)
- Rebase -ac patch to 2.6.9-ac8.

* Wed Nov  3 2004 Dave Jones <davej@redhat.com>
- Reenable token-ring drivers (#122602)

* Tue Nov  2 2004 Dave Jones <davej@redhat.com>
- Reenable SLIP. (#124223)
- Add USB card reader de jour. (#124048)

* Mon Nov  1 2004 Dave Jones <davej@redhat.com>
- Fix memory leak on x86-64 in mixed 32/64 mode. (#132947)
- Yet another USB card reader for the whitelist. (#137722)

* Fri Oct 29 2004 Dave Jones <davej@redhat.com>
- Fix raid5 oops (#127862)
- Stop E820 BIOS entries being corrupted by EDID info. (#137510)

* Thu Oct 28 2004 Dave Jones <davej@redhat.com>
- Remove the possibility of some false OOM kills. (#131251)
- Add more USB card readers to SCSI whitelist (#131546)
- Disable CONFIG_SCHED_SMT for iseries.

* Wed Oct 27 2004 Dave Jones <davej@redhat.com>
- Reenable ISA NIC support (#136569)

* Tue Oct 26 2004 Dave Jones <davej@redhat.com>
- Reenable Initio 9100U(W) SCSI driver. (#137153)

* Mon Oct 25 2004 Dave Jones <davej@redhat.com>
- Add another USB card reader to SCSI whitelist (#132923)

* Fri Oct 22 2004 Dave Jones <davej@redhat.com>
- Fix PPC NUMA (#130716).
- Fix autoraid for S390 (#123842/#130339)
- Selected bits from 2.6.9-ac3
  - Fix syncppp/async ppp problems with new hangup
  - Fix broken parport_pc unload
  - Stop i8xx_tco making some boxes reboot on load
  - Fix cpia/module tools deadlock
  - Security fix for smbfs leak/overrun

* Thu Oct 21 2004 Dave Jones <davej@redhat.com>
- Misc security fixes from 2.6.9-ac2

* Wed Oct 20 2004 Dave Jones <davej@redhat.com>
- Fix ia64 module loading. (#136365)
- Enable discontigmem for PPC64
- Disable a bunch of useless PPC config options
- Enable PACK_STACK on s390.

* Tue Oct 19 2004 Dave Jones <davej@redhat.com>
- Fix NFS badness (#132726)
- Drop bogus USB workaround. (#131127)

* Mon Oct 18 2004 Dave Jones <davej@redhat.com>
- Rebase to 2.6.9
- Speedtouch USB DSL modem driver update.
- Cleanup some iseries config options.

* Fri Oct 15 2004 Dave Jones <davej@redhat.com>
- 2.6.9-rc4-bk3
- Fix up a bunch of unresolved symbols that crept in recently.
- Remove bogus O_NONBLOCK patch which broke lots of userspace.
- Fix booting on PPC64 by reserving initrd pages.

* Thu Oct 14 2004 Dave Jones <davej@redhat.com>
- Rebase to 2.6.9-rc4-bk2
- librtas needs to work around the /dev/mem restrictions.
- EXT3 reservations use-before-initialised bugfix.
- support O_NONBLOCK for read,pread,readv of regular files.
- EDD blows up some x86-64's. Disable again.

* Wed Oct 13 2004 Dave Jones <davej@redhat.com>
- Make EDD driver modular on x86-64 too.
- Various mkinitrd spec changes (Jeremy Katz)
- Enable a bunch more PPC64 config options. (Dave Howells)
- Enable ACPI cpufreq driver for x86-64 too.

* Tue Oct 12 2004 Dave Jones <davej@redhat.com>
- Rebase to 2.6.9-rc4-bk1
- Tux update.
- Update netdump/diskdump patches
- PowerPC 64 netboot changes.
- Various CONFIG_ option diddling.
- Fix up the find_isa_irq_pin() oops on reboot for x86-64 too. 

* Mon Oct 11 2004 Dave Jones <davej@redhat.com>
- Rebase to 2.6.9-rc4
- Enable CONFIG_MICROCODE for x86-64

* Fri Oct  8 2004 Dave Jones <davej@redhat.com>
- Rebase to 2.6.9-rc3-bk8

* Thu Oct  7 2004 Dave Jones <davej@redhat.com>
- Rebase to 2.6.9-rc3-bk7
- Fix up PPC/PPC64 compilation failures due to new binutils. (David Woodhouse)

* Wed Oct  6 2004 Dave Jones <davej@redhat.com>
- Rebase to 2.6.9-rc3-bk6
- Add xattr support for tmpfs.

* Mon Oct  4 2004 Stephen C. Tweedie <sct@redhat.com>
- Update ext3 online resize to 2.6.9-rc3-mm2 upstream
- Reenable ext3 online resize in .spec

* Tue Sep 28 2004 Jeremy Katz <katzj@redhat.com>
- add patch from Roland McGrath/James Morris to fix mprotect hook bug (#133505)

* Mon Sep 20 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.9-rc2-bk5

* Thu Sep 16 2004 Arjan van de Ven <arjanv@redhat.com>
- fix tux for x86-64 and ppc64

* Tue Sep 14 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.9-rc2
- add diskdump

* Fri Sep 10 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.9-rc1-bk17 ; make ppc32 build

* Tue Sep 07 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.9-rc1-bk13

* Mon Sep 06 2004 Arjan van de Ven <arjanv@redhat.com>
- disable online resize again
- hopefully fix Quake3 interaction with execshield
- add Alan's borken-bios-IRQ workaround patch

* Sat Sep 04 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.9-rc1-bk11

* Tue Aug 31 2004 Arjan van de Ven <arjanv@redhat.com>
- fix execshield buglet with legacy binaries
- 2.6.9-rc1-bk7

* Mon Aug 30 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.9-rc1-bk6

* Sat Aug 28 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.9-rc1-bk4, now with i915 DRM driver

* Fri Aug 27 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.9-rc1-bk2 

* Mon Aug 23 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.8.1-bk2

* Sat Aug 21 2004 Arjan van de Ven <arjanv@redhat.com>
- attempt to fix early-udev bug

* Fri Aug 13 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.8-rc4-bk3
- split execshield up some more

* Fri Aug 13 2004 Dave Jones <davej@redhat.com>
- Update SCSI whitelist again with some more card readers.

* Mon Aug 9 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.8-rc3-bk3

* Wed Aug 4 2004 Arjan van de Ven <arjanv@redhat.com>
- Add the flex-mmap bits for s390/s390x (Pete Zaitcev)
- Add flex-mmap for x86-64 32 bit emulation
- 2.6.8-rc3

* Mon Aug 2 2004 Arjan van de Ven <arjanv@redhat.com>
- Add Rik's token trashing control patch

* Sun Aug 1 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.8-rc2-bk11

* Fri Jul 30 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.8-rc2-bk8

* Wed Jul 28 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.8-rc2-bk6
- make a start at splitting up the execshield patchkit

* Fri Jul 16 2004 Arjan van de Ven <arjanv@redhat.com>
- ppc32 embedded updates

* Thu Jul 15 2004 Arjan van de Ven <arjanv@redhat.com>
- make USB modules again and add Alan's real fix for the SMM-meets-USB bug
- 2.6.8-rc1-bk4

* Wed Jul 14 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.8-rc1-bk3

* Tue Jul 13 2004 Arjan van de Ven <arjanv@redhat.com>
- add "enforcemodulesig" boot option to make the kernel load signed modules only

* Mon Jul 12 2004 Arjan van de Ven <arjanv@redhat.com>
- updated voluntary preempt
- 2.6.8-rc1

* Wed Jul 7 2004 Arjan van de Ven <arjanv@redhat.com>
- fix boot breakage that was hitting lots of people (Dave Jones)

* Tue Jul 6 2004 Arjan van de Ven <arjanv@redhat.com>
- add voluntary preemption patch from Ingo
- 2.6.7-bk19

* Tue Jun 29 2004 Arjan van de Ven <arjanv@redhat.com>
- make a start at gpg signed modules support

* Sat Jun 27 2004 Arjan van de Ven <arjanv@redhat.com>
- experiment with making the hardlink call in post more efficient
- 2.6.7-bk9

* Thu Jun 24 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.7-bk7
- Add wli's patch to allocate memory bottom up not top down
- change some config options in the kernel-sourcecode package that are
  good for rpm kernel builds but not for custom user builds to more appropriate 
  default values.
- reenable kernel-sourcecode again for a few builds 

* Wed Jun 23 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.7-bk5
- fix tux unresolved symbols (#126532)

* Mon Jun 21 2004 Arjan van de Ven <arjanv@redhat.com>
- make kernel-doc and kernel-sourcecode builds independent of eachother
- disable kernel-sourcecode builds entirely, we'll be replacing it with documentation
  on how to use the src.rpm instead for building your own kernel.

* Sat Jun 19 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.7-bk2

* Sun Jun 13 2004 Arjan van de Ven <arjanv@redhat.com>
- add patch from DaveM to fix the ppp-keeps-iface-busy bug

* Sat Jun 12 2004 Arjan van de Ven <arjanv@redhat.com>
- add fix from Andi Kleen/Linus for the fpu-DoS

* Thu Jun 10 2004 Arjan van de Ven <arjanv@redhat.com>
- disable mlock-uses-rlimit patch, it has a security hole and needs more thought
- revert airo driver to the FC2 one since the new one breaks

* Tue Jun 8 2004 Dave Jones <davej@redhat.com>
- Update to 2.6.7rc3

* Fri Jun 4 2004 Arjan van de Ven <arjanv@redhat.com>
- fix the mlock-uses-rlimit patch

* Wed Jun 2 2004 David Woodhouse <dwmw2@redhat.com>
- Add ppc64 (Mac G5)

* Wed Jun 2 2004 Arjan van de Ven <arjanv@redhat.com>
- add a forward port of the mlock-uses-rlimit patch
- add NX support for x86 (Intel, Ingo)

* Tue Jun 1 2004 Arjan van de Ven <arjanv@redhat.com>
- refresh ext3 reservation patch

* Sun May 30 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.7-rc2
- set the ACPI OS name to "Microsoft Windows XP" for better compatibility

* Thu May 27 2004 Pete Zaitcev <zaitcev@redhat.com>
- Fix qeth and zfcp (s390 drivers): align qib by 256, embedded into qdio_irq.

* Thu May 27 2004 Dave Jones <davej@redhat.com>
- Fix the crashes on boot on Asus P4P800 boards. (#121819)

* Wed May 26 2004 Dave Jones <davej@redhat.com>
- Lots more updates to the SCSI whitelist for various
  USB card readers. (#112778, among others..)

* Wed May 26 2004 Arjan van de Ven <arjanv@redhat.com>
- back out ehci suspend/resume patch, it breaks
- add fix for 3c59x-meets-kudzu bug from Alan

* Tue May 25 2004 Arjan van de Ven <arjanv@redhat.com>
- try improving suspend/resume by restoring more PCI state
- 2.6.7-rc1-bk1

* Mon May 24 2004 Dave Jones <davej@redhat.com>
- Add yet another multi-card reader to the whitelist (#85851)

* Sun May 23 2004 Dave Jones <davej@redhat.com>
- Add another multi-card reader to the whitelist (#124048)

* Wed May 19 2004 Arjan van de Ven <arjanv@redhat.com>
- put firewire race fix in (datacorruptor)

* Tue May 18 2004 Dave Jones <davej@redhat.com>
- Fix typo in ibmtr driver preventing compile (#123391)

* Mon May 17 2004 Arjan van de Ven <arjanv@redhat.com>
- update to 2.6.6-bk3
- made kernel-source and kernel-doc noarch.rpm's since they are not
  architecture specific.

* Sat May 08 2004 Arjan van de Ven <arjanv@redhat.com>
- fix non-booting on Transmeta cpus (Peter Anvin)
- fix count leak in message queues

* Fri May 07 2004 Arjan van de Ven <arjanv@redhat.com>
- more ide cache flush work
- patch from scsi-bk to fix sd refcounting

* Thu May 06 2004 Arjan van de Ven <arjanv@redhat.com>
- some more ide cache flush fixes 

* Wed May 05 2004 Arjan van de Ven <arjanv@redhat.com>
- fix bug 122504
- convert b44 to ethtool ops (jgarzik)
- make IDE do a cache-flush on shutdown (me/Alan)

* Tue May 04 2004 Arjan van de Ven <arjanv@redhat.com>
- work around i810/i830 DRM issue

* Fri Apr 30 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.6-rc3-bk1
- make amd64 boot again
- fix vm86-vs-4g4g interaction (Ingo)

* Thu Apr 22 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.6-rc2
 
* Tue Apr 20 2004 Arjan van de Ven <arjanv@redhat.com>
- add the ext3 online resize patch

* Mon Apr 19 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.6-rc1-bk3
- add the objrmap vm from the -mm tree; it needs testing

* Thu Apr 15 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.5-bk2
- disable DISCONTIGMEM on ia64 for performance
- fix sleep_on use in reiserfs (Chris Mason)

* Tue Apr 13 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.5-mc4
- reenable sg driver for scsi tape changers and such
- the sk98lin driver oopses on module unload, preven that

* Mon Apr 12 2004 Arjan van de Ven <arjanv@redhat.com>
- fix "bad pmd" bug with patch from Ingo

* Fri Apr 09 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.5-mc3
- finish up the -mc2 merge
- latest 4g/4g patch from Ingo
- latest execshield patch from Ingo
- fix a few framebuffer bugs

* Thu Apr 08 2004 Arjan van de Ven <arjanv@redhat.com>
- first attempt at a 2.6.5-mc2 merge

* Thu Apr 08 2004 Dave Jones <davej@redhat.com>
- Add in missing SiS AGP fix.

* Tue Apr 06 2004 Dave Jones <davej@redhat.com>
- More agpgart fixes.

* Fri Apr 02 2004 Arjan van de Ven <arjanv@redhat.com>
- fix another 4g/4g-vs-resume bug

* Tue Mar 30 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.5-rc3
- fix PCI posting bug in i830 DRM

* Mon Mar 29 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.5-rc2-bk8

* Mon Mar 29 2004 Dave Jones <davej@redhat.com>
- Include latest agpgart fixes.

* Thu Mar 25 2004 Arjan van de Ven <arjanv@redhat.com>
- more DRM fixes
- add the fsync patches from akpm

* Tue Mar 23 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.5-rc2-bk3
- fix direct userspace memory access in i830 drm driver

* Mon Mar 22 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.5-rc2-bk2
- some stackbloat reductions from Dave and me

* Sat Mar 20 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.5-rc2

* Tue Mar 16 2004 Dave Jones <davej@redhat.com>
- 2.6.5-rc1

* Mon Mar 15 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.4-bk3
- fix oops in toshiba_acpi (Barry K. Nathan)

* Sat Mar 13 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.4-bk2 merge 

* Thu Mar 11 2004 Arjan van de Ven <arjanv@redhat.com>
- renable sonypi driver that was off accidentally
- 2.6.4-final 
- fix the oops on alsa module unloads

* Wed Mar 10 2004 Arjan van de Ven <arjanv@redhat.com>
- add ppc64/iseries, ppc32 (powermac/ibook) and ia64 architectures
- 2.6.4-rc3 

* Tue Mar 09 2004 Arjan van de Ven <arjanv@redhat.com>
- 2.6.4-rc2-bk5
- fix nfs-vs-selinux issue
- fix typo in URL as per #117849

* Mon Mar 08 2004 Arjan van de Ven <arjanv@redhat.com>
- fix race in lp.c (#117710)
- 2.6.4-rc2-bk3
- attempt to fix S3 suspend-to-ram with 4g/4g split

* Sat Mar 06 2004 Arjan van de Ven <arjanv@redhat.com>
- fix reiserfs
- set HZ to 1000 again for some tests

* Wed Feb 25 2004 Arjan van de Ven <arjanv@redhat.com>
- merge back a bunch of fedora fixes
- disable audit

* Tue Feb 24 2004 Arjan van de Ven <arjanv@redhat.com>
- audit bugfixes
- update tux to a working version
- 2.6.3-bk5 merge

* Fri Feb 20 2004 Arjan van de Ven <arjanv@redhat.com>
- re-add and enable the Auditing patch
- switch several cpufreq modules to built in since detecting in userspace
  which to use is unpleasant

* Thu Jul 03 2003 Arjan van de Ven <arjanv@redhat.com>
- 2.6 start

