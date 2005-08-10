Summary: The Linux kernel (the core of the Linux operating system)

# What parts do we want to build?  We must build at least one kernel.
# These are the kernels that are built IF the architecture allows it.

%define buildup 1
%define buildsmp 0
%define buildxenU 1
%define builduml 0
%define buildsource 0
%define builddoc 0


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

%define release 1.1398_FC4.0.planetlab%{?date:.%{date}}

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
BuildPreReq: bzip2, findutils, gzip, m4, perl, make >= 3.78, gnupg
#BuildPreReq: kernel-utils >= 1:2.4-12.1.142
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


%package sourcecode
Summary: The source code for the Linux kernel.
Group: Development/System
Prereq: fileutils
Requires: make >= 3.78
Requires: gcc >= 3.2
Requires: /usr/bin/strip
# for xconfig and gconfig
Requires: qt-devel, gtk2-devel readline-devel ncurses-devel
Provides: kernel-source
Obsoletes: kernel-source <= 2.6.6

%description sourcecode
The kernel-sourcecode package contains the source code files for the Linux
kernel. The source files can be used to build a custom kernel that is
smaller by virtue of only including drivers for your particular hardware, if
you are so inclined (and you know what you're doing). The customisation
guide in the documentation describes in detail how to do this. This package
is neither needed nor usable for building external kernel modules for
linking such modules into the default operating system kernels.

%package doc
Summary: Various documentation bits found in the kernel source.
Group: Documentation
%if !%{buildsource}
Obsoletes: kernel-source <= 2.6.6
Obsoletes: kernel-sourcecode <= 2.6.6
%endif

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

%package xenU
Summary: The Linux kernel compiled for xenU virtual machines.

Group: System Environment/Kernel
Provides: kernel = %{version}
Provides: kernel-drm = 4.3.0
Prereq: %{kernel_prereq}
Conflicts: %{kernel_dot_org_conflicts}
Conflicts: %{package_conflicts}
Conflicts: %{nptl_conflicts}
Obsoletes: kernel-enterprise < 2.4.10
# We can't let RPM do the dependencies automatic because it'll then pick up
# a correct but undesirable perl dependency from the module headers which
# isn't required for the kernel proper to function
AutoReqProv: no

%description xenU
This package includes a xenU version of the Linux kernel. It is used
to run the Linux kernel in an unprivileged Xen domain.

Install the kernel-xenU package if your machine supports the Xen VMM.


%package uml
Summary: The Linux kernel compiled for use in user mode (User Mode Linux).

Group: System Environment/Kernel

%description uml
This package includes a user mode version of the Linux kernel.

%package vserver
Summary: A placeholder RPM that provides kernel and kernel-drm

Group: System Environment/Kernel
Provides: kernel = %{version}
Provides: kernel-drm = 4.3.0

%description vserver
VServers do not require and cannot use kernels, but some RPMs have
implicit or explicit dependencies on the "kernel" package
(e.g. tcpdump). This package installs no files but provides the
necessary dependencies to make rpm and yum happy.

%prep

%setup -n linux-%{kversion}

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

    # Pick the right config file for the kernel we're building
    if [ -n "$1" ] ; then
	Config=kernel-%{kversion}-%{_target_cpu}-$1-planetlab.config
    else
	Config=kernel-%{kversion}-%{_target_cpu}-planetlab.config
    fi

    KernelVer=%{version}-%{release}$1
    echo BUILDING A KERNEL FOR $1 %{_target_cpu}...

    # make sure EXTRAVERSION says what we want it to say
    perl -p -i -e "s/^EXTRAVERSION.*/EXTRAVERSION = -%{release}$1/" Makefile

    # override ARCH in the case of UML
    if [ "$1" = "uml" ] ; then
        export ARCH=um
    fi

    # override ARCH in the case of UML
    if [ "$1" = "xenU" ] ; then
        export ARCH=xen
    fi

    # and now to start the build process

    make -s mrproper
    cp configs/$Config .config

    make -s nonint_oldconfig > /dev/null
    make -s include/linux/version.h 

    make -s %{?_smp_mflags} %{make_target}
    make -s %{?_smp_mflags} modules || exit 1
    make buildcheck
    
    # Start installing the results

    mkdir -p $RPM_BUILD_ROOT/usr/lib/debug/boot
    mkdir -p $RPM_BUILD_ROOT/%{image_install_path}
    install -m 644 System.map $RPM_BUILD_ROOT/usr/lib/debug/boot/System.map-$KernelVer
    objdump -t vmlinux | grep ksymtab | cut -f2 | cut -d" " -f2 | cut -c11- | sort -u  > exported
    echo "_stext" >> exported
    echo "_end" >> exported
    touch $RPM_BUILD_ROOT/boot/System.map-$KernelVer
    for i in `cat exported` 
    do 
	 grep " $i\$" System.map >> $RPM_BUILD_ROOT/boot/System.map-$KernelVer || :
	 grep "tab_$i\$" System.map >> $RPM_BUILD_ROOT/boot/System.map-$KernelVer || :
	 grep "__crc_$i\$" System.map >> $RPM_BUILD_ROOT/boot/System.map-$KernelVer ||:
    done
    rm -f exported
#    install -m 644 init/kerntypes.o $RPM_BUILD_ROOT/boot/Kerntypes-$KernelVer
    install -m 644 .config $RPM_BUILD_ROOT/boot/config-$KernelVer
    rm -f System.map
    cp arch/*/boot/bzImage $RPM_BUILD_ROOT/%{image_install_path}/vmlinuz-$KernelVer

    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer
    make -s INSTALL_MOD_PATH=$RPM_BUILD_ROOT modules_install KERNELRELEASE=$KernelVer
 
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
    cp --parents `find  -type f -name Makefile -o -name "Kconfig*"` $RPM_BUILD_ROOT/lib/modules/$KernelVer/build 
    cp Module.symvers $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    # then drop all but the needed Makefiles/Kconfig files
    rm -rf $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/Documentation
    rm -rf $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/scripts
    rm -rf $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    cp arch/%{_arch}/kernel/asm-offsets.s $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/arch/%{_arch}/kernel || :
    cp .config $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    cp -a scripts $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    cp -a arch/%{_arch}/scripts $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/arch/%{_arch} || :
    cp -a arch/%{_arch}/*lds $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/arch/%{_arch}/ || :
    rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/scripts/*.o
    rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/scripts/*/*.o
    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    cd include
    cp -a acpi config linux math-emu media net pcmcia rxrpc scsi sound video asm asm-generic $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    cp -a `readlink asm` $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    # Make sure the Makefile and version.h have a matching timestamp so that
    # external modules can be built
    touch -r $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/Makefile $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include/linux/version.h
    touch -r $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/.config $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include/linux/autoconf.h
    cd .. 

    #
    # save the vmlinux file for kernel debugging into the kernel-debuginfo rpm
    #
    mkdir -p $RPM_BUILD_ROOT/usr/lib/debug/lib/modules/$KernelVer
    cp vmlinux $RPM_BUILD_ROOT/usr/lib/debug/lib/modules/$KernelVer

    # mark modules executable so that strip-to-file can strip them
    find $RPM_BUILD_ROOT/lib/modules/$KernelVer -name "*.ko" -type f  | xargs chmod u+x

    # detect missing or incorrect license tags
    for i in `find $RPM_BUILD_ROOT/lib/modules/$KernelVer -name "*.ko" ` ; do echo -n "$i " ; /sbin/modinfo -l $i >> modinfo ; done
    cat modinfo | grep -v "^GPL" | grep -v "^Dual BSD/GPL" | grep -v "^Dual MPL/GPL" | grep -v "^GPL and additional rights" | grep -v "^GPL v2" && exit 1 
    rm -f modinfo
    # remove files that will be auto generated by depmod at rpm -i time
    rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/modules.*

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

%if %{buildxenU}
BuildKernel xenU
%endif


###
### install
###

%install

# architectures that don't get kernel-source (i586/i686/athlon) dont need
# much of an install because the build phase already copied the needed files

%if %{builddoc}
mkdir -p $RPM_BUILD_ROOT/usr/share/doc/kernel-doc-%{kversion}/Documentation

# sometimes non-world-readable files sneak into the kernel source tree
chmod -R a+r *
# copy the source over
tar cf - Documentation | tar xf - -C $RPM_BUILD_ROOT/usr/share/doc/kernel-doc-%{kversion}
%endif

%if %{buildsource}

mkdir -p $RPM_BUILD_ROOT/usr/src/linux-%{KVERREL}
chmod -R a+r *

# clean up the source tree so that it is ready for users to build their own
# kernel
make -s mrproper
# copy the source over
tar cf - . | tar xf - -C $RPM_BUILD_ROOT/usr/src/linux-%{KVERREL}

# set the EXTRAVERSION to <version>custom, so that people who follow a kernel building howto
# don't accidentally overwrite their currently working moduleset and hose
# their system
perl -p -i -e "s/^EXTRAVERSION.*/EXTRAVERSION = -%{release}custom/" $RPM_BUILD_ROOT/usr/src/linux-%{KVERREL}/Makefile

# some config options may be appropriate for an rpm kernel build but are less so for custom user builds,
# change those to values that are more appropriate as default for people who build their own kernel.
perl -p -i -e "s/^CONFIG_DEBUG_INFO.*/# CONFIG_DEBUG_INFO is not set/" $RPM_BUILD_ROOT/usr/src/linux-%{KVERREL}/configs/*
perl -p -i -e "s/^.*CONFIG_DEBUG_PAGEALLOC.*/# CONFIG_DEBUG_PAGEALLOC is not set/" $RPM_BUILD_ROOT/usr/src/linux-%{KVERREL}/configs/*
perl -p -i -e "s/^.*CONFIG_DEBUG_SLAB.*/# CONFIG_DEBUG_SLAB is not set/" $RPM_BUILD_ROOT/usr/src/linux-%{KVERREL}/configs/*
perl -p -i -e "s/^.*CONFIG_DEBUG_SPINLOCK.*/# CONFIG_DEBUG_SPINLOCK is not set/" $RPM_BUILD_ROOT/usr/src/linux-%{KVERREL}/configs/*
perl -p -i -e "s/^.*CONFIG_DEBUG_HIGHMEM.*/# CONFIG_DEBUG_HIGHMEM is not set/" $RPM_BUILD_ROOT/usr/src/linux-%{KVERREL}/configs/*
perl -p -i -e "s/^.*CONFIG_MODULE_SIG.*/# CONFIG_MODULE_SIG is not set/" $RPM_BUILD_ROOT/usr/src/linux-%{KVERREL}/configs/*

install -m 644 %{SOURCE10}  $RPM_BUILD_ROOT/usr/src/linux-%{KVERREL}
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

%pre xenU
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
[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --mkinitrd --depmod --install %{KVERREL}
if [ -n "$fake_root_lvm" ]; then
    rm -f $rootdev
fi
if [ -x /usr/sbin/hardlink ] ; then
pushd /lib/modules/%{KVERREL}/build > /dev/null ; {
	cd /lib/modules/%{KVERREL}/build
	find . -type f | while read f; do hardlink -c /lib/modules/*/build/$f $f ; done
}
popd > /dev/null
fi

# make some useful links
pushd /boot > /dev/null ; {
	ln -sf System.map-%{KVERREL} System.map
#	ln -sf Kerntypes-%{KVERREL} Kerntypes
	ln -sf config-%{KVERREL} config
	ln -sf initrd-%{KVERREL}.img initrd-boot
	ln -sf vmlinuz-%{KVERREL} kernel-boot
}
popd > /dev/null

# ask for a reboot
mkdir -p /etc/planetlab
touch /etc/planetlab/update-reboot

%post smp
[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --mkinitrd --depmod --install %{KVERREL}smp
if [ -x /usr/sbin/hardlink ] ; then
pushd /lib/modules/%{KVERREL}smp/build > /dev/null ; {
	cd /lib/modules/%{KVERREL}smp/build
	find . -type f | while read f; do hardlink -c /lib/modules/*/build/$f $f ; done
}
popd > /dev/null
fi

%post xenU
[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --mkinitrd --depmod --install %{KVERREL}xenU
if [ -x /usr/sbin/hardlink ] ; then
pushd /lib/modules/%{KVERREL}xenU/build > /dev/null ; {
	cd /lib/modules/%{KVERREL}xenU/build
	find . -type f | while read f; do hardlink -c /lib/modules/*/build/$f $f ; done
}
popd > /dev/null
fi


%preun 
/sbin/modprobe loop 2> /dev/null > /dev/null  || :
[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --rminitrd --rmmoddep --remove %{KVERREL}

%preun smp
/sbin/modprobe loop 2> /dev/null > /dev/null  || :
[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --rminitrd --rmmoddep --remove %{KVERREL}smp

%preun xenU
/sbin/modprobe loop 2> /dev/null > /dev/null  || :
[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --rminitrd --rmmoddep --remove %{KVERREL}xenU


###
### file lists
###

%if %{buildup}
%files 
%defattr(-,root,root)
/%{image_install_path}/*-%{KVERREL}
#/boot/Kerntypes-%{KVERREL}
/boot/System.map-%{KVERREL}
/boot/config-%{KVERREL}
%dir /lib/modules/%{KVERREL}
/lib/modules/%{KVERREL}/kernel
%verify(not mtime) /lib/modules/%{KVERREL}/build
/lib/modules/%{KVERREL}/source
%endif

%if %{buildsmp}
%files smp
%defattr(-,root,root)
/%{image_install_path}/*-%{KVERREL}smp
#/boot/Kerntypes-%{KVERREL}smp
/boot/System.map-%{KVERREL}smp
/boot/config-%{KVERREL}smp
%dir /lib/modules/%{KVERREL}smp
/lib/modules/%{KVERREL}smp/kernel
%verify(not mtime) /lib/modules/%{KVERREL}smp/build
/lib/modules/%{KVERREL}smp/source
%endif

%if %{buildxenU}
%files xenU
%defattr(-,root,root)
/%{image_install_path}/*-%{KVERREL}xenU
#/boot/Kerntypes-%{KVERREL}xenU
/boot/System.map-%{KVERREL}xenU
/boot/config-%{KVERREL}xenU
%dir /lib/modules/%{KVERREL}xenU
/lib/modules/%{KVERREL}xenU/kernel
%verify(not mtime) /lib/modules/%{KVERREL}xenU/build
/lib/modules/%{KVERREL}xenU/source
%endif

%if %{builduml}
%files uml
%defattr(-,root,root)
%endif

# only some architecture builds need kernel-source and kernel-doc

%if %{buildsource}
%files sourcecode
%defattr(-,root,root)
/usr/src/linux-%{KVERREL}/
%endif


%if %{builddoc}
%files doc
%defattr(-,root,root)
/usr/share/doc/kernel-doc-%{kversion}/Documentation/*
%endif


%files vserver
%defattr(-,root,root)
# no files

%changelog
* Sun Mar 27 2005 Dave Jones <davej@redhat.com>
- Catch up with all recent security issues.
  - CAN-2005-0210 : dst leak
  - CAN-2005-0384 : ppp dos
  - CAN-2005-0531 : Sign handling issues.
  - CAN-2005-0400 : EXT2 information leak.
  - CAN-2005-0449 : Remote oops.
  - CAN-2005-0736 : Epoll overflow
  - CAN-2005-0749 : ELF loader may kfree wrong memory.
  - CAN-2005-0750 : Missing range checking in bluetooth 
  - CAN-2005-0767 : drm race in radeon
  - CAN-2005-0815 : Corrupt isofs images could cause oops.

* Tue Mar 22 2005 Dave Jones <davej@redhat.com>
- Fix swapped parameters to memset in ieee802.11 code.

* Thu Feb 24 2005 Dave Jones <davej@redhat.com>
- Use old scheme first when probing USB. (#145273)

* Wed Feb 23 2005 Dave Jones <davej@redhat.com>
- Try as you may, there's no escape from crap SCSI hardware. (#149402)

* Mon Feb 21 2005 Dave Jones <davej@redhat.com>
- Disable some experimental USB EHCI features.

* Tue Feb 15 2005 Dave Jones <davej@redhat.com>
- Fix bio leak in md layer.

* Wed Feb  9 2005 Dave Jones <davej@redhat.com> [2.6.10-1.766_FC3, 2.6.10-1.14_FC2]
- Backport some exec-shield fixes from devel/ branch.
- Scan all SCSI LUNs by default.
  Theoretically, some devices may hang when being probed, though
  there should be few enough of these that we can blacklist them
  instead of having to whitelist every other device on the planet.

* Tue Feb  8 2005 Dave Jones <davej@redhat.com>
- Use both old-style and new-style for USB initialisation.

* Mon Feb  7 2005 Dave Jones <davej@redhat.com>	[2.6.10-1.762_FC3, 2.6.10-1.13_FC2]
- Update to 2.6.10-ac12

* Tue Feb  1 2005 Dave Jones <davej@redhat.com> [2.6.10-1.760_FC3, 2.6.10-1.12_FC2]
- Disable longhaul driver, it causes random hangs. (#140873)
- Fixup NFSv3 oops when mounting with sec=krb5 (#146703)

* Mon Jan 31 2005 Dave Jones <davej@redhat.com>
- Rebase to 2.6.10-ac11

* Sat Jan 29 2005 Dave Jones <davej@redhat.com>
- Reintegrate Tux. (#144812)

* Thu Jan 20 2005 Dave Jones <davej@redhat.com>	[2.6.10-1.753_FC3, 2.6.10-1.11_FC2]
- Fix x87 fnsave Tag Word emulation when using FXSR (SSE)
- Add multi-card reader of the day to the whitelist. (#145587)

* Tue Jan 18 2005 Dave Jones <davej@redhat.com>
- Reintegrate netdump/netconsole. (#144068)

* Mon Jan 17 2005 Dave Jones <davej@redhat.com>
- Update to 2.6.10-ac10
- Revert module loader patch that caused lots of invalid parameter problems.
- Print more debug info when spinlock code triggers a panic.
- Print tainted information on various mm debug info.

* Fri Jan 14 2005 Dave Jones <davej@redhat.com>
- Enable advansys scsi module on x86. (#141004)

* Thu Jan 13 2005 Dave Jones <davej@redhat.com>
- Reenable CONFIG_PARIDE (#127333)

* Thu Jan 13 2005 Dave Jones <davej@redhat.com>  [2.6.10-1.741_FC3, 2.6.10-1.9_FC2]
- Update to 2.6.10-ac9
- Fix slab corruption in ACPI video code.

* Mon Jan 10 2005 Dave Jones <davej@redhat.com>
- Add another Lexar card reader to the whitelist. (#143600)
- Package asm-m68k for asm-ppc includes. (don't ask). (#144604)

* Mon Jan 10 2005 Dave Jones <davej@redhat.com>  [2.6.10-1.737_FC3, 2.6.10-1.8_FC2]
- Disable slab debugging.

* Sat Jan  8 2005 Dave Jones <davej@redhat.com>
- Periodic slab debug is incompatable with pagealloc debug.
  Disable the latter.
- Update to 2.6.10-ac8

* Fri Jan  7 2005 Dave Jones <davej@redhat.com>
- Bump up to -ac7
- Another new card reader.

* Thu Jan  6 2005 Dave Jones <davej@redhat.com>
- Rebase to 2.6.10-ac5

* Tue Jan  4 2005 Dave Jones <davej@redhat.com>
- Rebase to 2.6.10-ac4
- Add periodic slab debug checker.

* Mon Jan  3 2005 Dave Jones <davej@redhat.com>
- Drop patch which meant we needed a newer gcc. (#144035)
- Rebase to 2.6.10-ac2
- Enable SL82C104 IDE driver as built-in on PPC64 (#131033)

* Sat Jan  1 2005 Dave Jones <davej@redhat.com>
- Fix probing of vesafb. (#125890)
- Enable PCILynx driver. (#142173)

* Fri Dec 31 2004 Dave Jones <davej@redhat.com>
- Drop 4g/4g patch completely.

* Tue Dec 28 2004 Dave Jones <davej@redhat.com>
- Drop bogus ethernet slab cache.

* Thu Dec 23 2004 Dave Jones <davej@redhat.com>
- Fix bio error propagation.
- Clear ebp on sysenter return.
- Extra debugging info on OOM kill.
- exit() race fix.
- Fix refcounting order in sd/sr, fixing cable pulls on USB storage.
- IGMP source filter fixes.
- Fix ext2/3 leak on umount.
- fix missing wakeup in ipc/sem
- Fix another tux corner case bug.

* Wed Dec 22 2004 Dave Jones <davej@redhat.com>
- Add another ipod to the unusual usb devices list. (#142779)

* Tue Dec 21 2004 Dave Jones <davej@redhat.com>
- Fix two silly bugs in the AGP posting fixes.

* Thu Dec 16 2004 Dave Jones <davej@redhat.com>
- Better version of the PCI Posting fixes for agpgart.
- Add missing cache flush to the AGP code.

* Sun Dec 12 2004 Dave Jones <davej@redhat.com>
- fix false ECHILD result from wait* with zombie group leader.

* Sat Dec 11 2004 Dave Jones <davej@redhat.com>
- Workaround broken pci posting in AGPGART.
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
- Disable tiglusb module. (#142102)
- E1000 64k-alignment fix. (#140047)
- Disable tiglusb module. (#142102)
- ID updates for cciss driver.
- Fix overflows in USB Edgeport-IO driver. (#142258)
- Fix wrong TASK_SIZE for 32bit processes on x86-64. (#141737)
- Fix ext2/ext3 xattr/mbcache race. (#138951)
- Fix bug where __getblk_slow can loop forever when pages are partially mapped. (#140424)
- Add missing cache flushes in agpgart code.

* Wed Dec  8 2004 Dave Jones <davej@redhat.com>
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

* Sat Dec  4 2004 Dave Jones <davej@redhat.com>
- Enable both old and new megaraid drivers.
- Add yet another card reader to usb scsi whitelist. (#141367)
- Fix oops in conntrack on rmmod.

* Fri Dec  3 2004 Dave Jones <davej@redhat.com>
- Pull in bits of -ac12
  Should fix the smbfs & visor issues among others.

* Thu Dec  2 2004 Dave Jones <davej@redhat.com>
- Drop the futex debug patch, it served its purpose.
- XFRM layer bug fixes
- ppc64: Convert to using ibm,read-slot-reset-state2 RTAS call
- ide: Make CSB6 driver support configurations.
- ide: Handle early EOF on CDs.
- Fix sx8 device naming in sysfs
- e100/e1000: return -EINVAL when setting rx-mini or rx-jumbo. (#140793)

* Wed Dec  1 2004 Dave Jones <davej@redhat.com>
- Disable 4G/4G for i686.
- Workaround for the E1000 erratum 23 (#140047)
- Remove bogus futex warning. (#138179)
- x86_64: Fix lost edge triggered irqs on UP kernel.
- x86_64: Reenable DRI for MGA.
- Workaround E1000 post-maturely writing back to TX descriptors (#133261)
- 3c59x: add EEPROM_RESET for 3c900 Boomerang
- Fix buffer overrun in arch/x86_64/sys_ia32.c:sys32_ni_syscall()
- ext3: improves ext3's error logging when we encounter an on-disk corruption.
- ext3: improves ext3's ability to deal with corruption on-disk
- ext3: Handle double-delete of indirect blocks.
- Disable SCB2 flash driver for RHEL4. (#141142)

* Tue Nov 30 2004 Dave Jones <davej@redhat.com>
- x86_64: add an option to configure oops stack dump
- x86[64]: display phys_proc_id only when it is initialized
- x86_64: no TIOCSBRK/TIOCCBRK in ia32 emulation
- via-rhine: references __init code during resume
- Add barriers to generic timer code to prevent race. (#128242)
- ppc64: Add PURR and version data to /proc/ppc64/lparcfg
- Prevent xtime value becoming incorrect.
- scsi: return full SCSI status byte in SG_IO
- Fix show_trace() in irq context with CONFIG_4KSTACKS
- Adjust alignment of pagevec structure.
- md: make sure md always uses rdev_dec_pending properly.
- Make proc_pid_status not dereference dead task structs.
- sg: Fix oops of sg_cmd_done and sg_release race (#140648)
- fix bad segment coalescing in blk_recalc_rq_segments()
- fix missing security_*() check in net/compat.c
- ia64/x86_64/s390 overlapping vma fix
- Update Emulex lpfc to 8.0.15

* Mon Nov 29 2004 Dave Jones <davej@redhat.com>
- Add another card reader to whitelist. (#141022)
- Fix possible hang in do_wait() (#140042)
- Fix ps showing wrong ppid. (#132030)
- Print advice to use -hugemem if >=16GB of memory is detected.
- Enable ICOM serial driver. (#136150)
- Enable acpi hotplug driver for IA64.
- SCSI: fix USB forced remove oops.
- ia64: add missing sn2 timer mask in time_interpolator code. (#140580)
- ia64: Fix hang reading /proc/pal/cpu0/tr_info (#139571)
- ia64: bump number of UARTS. (#139100)
- Fix ACPI debug level (#141292)
- Make EDD runtime configurable, and reenable.
- ppc64: IBM VSCSI driver race fix. (#138725)
- ppc64: Ensure PPC64 interrupts don't end up hard-disabled. (#139020, #131590)
- ppc64: Yet more sigsuspend/singlestep fixing. (#140102, #137931)
- x86-64: Implement ACPI based reset mechanism. (#139104)
- Backport 2.6.10rc sysfs changes needed for IBM hotplug driver. (#140372)
- Update Emulex lpfc driver to v8.0.14
- Optimize away the unconditional write to debug registers on signal delivery path.
- Fix up scsi_test_unit_ready() to work correctly with CD-ROMs.
- md: fix two little bugs in raid10
- Remove incorrect ELF check from module loading. (#140954)
- Plug leaks in error paths of aic driver.
- Add refcounting to scsi command allocation.
- Taint oopses on machine checks, bad_page()'s calls and forced rmmod's.
- Share Intel cache descriptors between x86 & x86-64.
- rx checksum support for gige nForce ethernet
- vm: vm_dirty_ratio initialisation fix

* Mon Nov 29 2004 Soeren Sandmann <sandmann@redhat.com>
- Build FC-3 kernel in RHEL build root

* Sun Nov 28 2004 Dave Jones <davej@redhat.com>
- Move 4g/4g kernel into -hugemem.

* Sat Nov 27 2004 Dave Jones <davej@redhat.com>
- Recognise Shuttle SN85G4 card reader. (#139163)

* Tue Nov 23 2004 Dave Jones <davej@redhat.com>
- Add futex debug patch.

* Mon Nov 22 2004 Dave Jones <davej@redhat.com>
- Update -ac patch to 2.6.9-ac11
- make tulip_stop_rxtx() wait for DMA to fully stop. (#138240)
- ACPI: Make LEqual less strict about operand types matching.
- scsi: avoid extra 'put' on devices in __scsi_iterate_device() (#138135)
- Fix bugs with SOCK_SEQPACKET AF_UNIX sockets
- Reenable token ring drivers. (#119345)
- SELinux: Map Unix seqpacket sockets to appropriate security class
- SELinux: destroy avtab node cache in policy load error path.
- AF_UNIX: Serialize dgram read using semaphore just like stream.
- lockd: NLM blocks locks don't sleep
- NFS lock recovery fixes
- Add more MODULE_VERSION tags (#136403)
- Update qlogic driver to 2.6.10rc2 level.
- cciss: fixes for clustering
- ieee802.11 update.
- ipw2100: update to ver 1.0.0
- ipw2200: update to ver 1.0.0
- Enable promisc mode on ipw2100
- 3c59x: reload EEPROM values at rmmod for needy cards
- ppc64: Prevent sigsuspend stomping on r4 and r5
- ppc64: Alternative single-step fix.
- fix for recursive netdump oops on x86_64
- ia64: Fix IRQ routing fix when booted with maxcpus=  (#138236)
- ia64: search the iommu for the correct size
- Deal with fraglists correctly on ipv4/ipv6 output
- Various statm accounting fixes (#139447)
- Reenable CMM /proc interface for s390 (#137397)

* Fri Nov 19 2004 Dave Jones <davej@redhat.com>
- e100: fix improper enabling of interrupts. (#139706)
- autofs4: allow map update recognition
- Various TCP fixes from 2.6.10rc
- Various netlink fixes from 2.6.10rc
- [IPV4]: Do not try to unhash null-netdev nexthops.
- ppc64: Make NUMA map CPU->node before bringing up the CPU (#128063)
- ppc64: sched domains / cpu hotplug cleanup. (#128063)
- ppc64: Add a CPU_DOWN_PREPARE hotplug CPU notifier (#128063)
- ppc64: Register a cpu hotplug notifier to reinitialize the
  scheduler domains hierarchy (#128063)
- ppc64: Introduce CPU_DOWN_FAILED notifier (#128063)
- ppc64: Make arch_destroy_sched_domains() conditional (#128063)
- ppc64: Use CPU_DOWN_FAILED notifier in the sched-domains hotplug code (#128063)
- Various updates to the SCSI midlayer from 2.6.10rc.
- vlan_dev: return 0 on vlan_dev_change_mtu success. (#139760)
- Update Emulex lpfc driver to v8013
- Fix problem with b44 driver and 4g/4g patch. (#118165)
- Prevent oops when loading aic79xx on machine without hardware. (#125982)
- Use correct spinlock functions in token ring net code. (#135462)
- scsi: Add reset ioctl capability to ULDs
- scsi: update ips driver to 7.10.18
- Reenable ACPI hotplug driver. (#139976, #140130, #132691)

* Thu Nov 18 2004 Dave Jones <davej@redhat.com>
- Drop 2.6.9 changes that broke megaraid. (#139723)
- Update to 2.6.9-ac10, fixing the SATA problems (#139674)
- Update the OOM-killer tamer to upstream.
- Implement an RCU scheme for the SELinux AVC
- Improve on the OOM-killer taming patch.
- device-mapper: Remove duplicate kfree in dm_register_target error path.
- Make SHA1 guard against misaligned accesses
- ASPM workaround for PCIe. (#123360)
- Hot-plug driver updates due to MSI change (#134290)
- Workaround for 80332 IOP hot-plug problem (#139041)
- ExpressCard hot-plug support for ICH6M (#131800)
- Fix boot crash on VIA systems (noted on x86-64)
- PPC64: Store correct backtracking info in ppc64 signal frames
- PPC64: Prevent HVSI from oopsing on hangup (#137912)
- Fix poor performance b/c of noncacheable mapping in 4g/4g (#130842)
- Fix PCI-X hotplug issues (#132852, #134290)
- Re-export force_sig() (#139503)
- Various fixes for more security issues from latest -ac patch.
- Fix d_find_alias brokenness (#137791)
- tg3: Fix fiber hw autoneg bounces (#138738)
- diskdump: Fix issue with NMI watchdog. (#138041)
- diskdump: Export disk_dump_state. (#138132)
- diskdump: Tickle NMI watchdog in diskdump_mdelay() (#138036)
- diskdump: Fix mem= for x86-64 (#138139)
- diskdump: Fix missing system_state setting. (#138130)
- diskdump: Fix diskdump completion message (#138028)
- Re-add aic host raid support.
- Take a few more export removal patches from 2.6.10rc
- SATA: Make AHCI work
- SATA: Core updates.
- S390: Fix Incorrect registers in core dumps. (#138206)
- S390: Fix up lcs device state. (#131167)
- S390: Fix possible qeth IP registration failure.
- S390: Support broadcast on z800/z900 HiperSockets
- S390: Allow FCP port to recover after aborted nameserver request.
- Flush error in pci_mmcfg_write (#129338)
- hugetlb_get_unmapped_area fix (#135364, #129525)
- Fix ia64 cyclone timer on ia64 (#137842, #136684)
- Fix ipv6 MTU calculation. (#130397)
- ACPI: Don't display messages about ACPI breakpoints. (#135856)
- Fix x86_64 copy_user_generic (#135655)
- lockd: remove hardcoded maximum NLM cookie length
- Fix SCSI bounce limit
- Disable polling mode on hotplug controllers in favour of interrupt driven. (#138737)

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
