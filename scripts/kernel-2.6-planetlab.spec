Summary: The Linux kernel (the core of the Linux operating system)

# What parts do we want to build?  We must build at least one kernel.
# These are the kernels that are built IF the architecture allows it.

%define buildup 1
%define buildsmp 0
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
%define sublevel 7
%define kversion 2.6.%{sublevel}
%define rpmversion 2.6.%{sublevel}
%define rhbsys  %([ -r /etc/beehive-root ] && echo  || echo .`whoami`)
%define release 1.planetlab%{?date:.%{date}}
%define signmodules 0

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
BuildPreReq: bzip2, findutils, gzip, m4, perl, make >= 3.78, gnupg, kernel-utils >= 2.4-12.1.139
# temporary req since modutils changed output format 
#BuildPreReq: modutils >= 2.4.26-14
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

%package uml
Summary: The Linux kernel compiled for use in user mode (User Mode Linux).

Group: System Environment/Kernel

%description uml
This package includes a user mode version of the Linux kernel.

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

    # and now to start the build process

    make -s mrproper
    cp configs/$Config .config

    make -s nonint_oldconfig > /dev/null
    make -s include/linux/version.h 

    make -s %{?_smp_mflags} bzImage 
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
    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
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
# change those to values that are more appropriate as defeault for people who build their own kernel.
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

%post 
[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --mkinitrd --depmod --install %{KVERREL}
if [ -x /usr/sbin/hardlink ] ; then
pushd /lib/modules/%{KVERREL}/build > /dev/null ; {
	cd /lib/modules/%{KVERREL}/build
	find . -type f | while read f; do hardlink -c /lib/modules/*/build/$f $f ; done
}
popd
fi

# make some useful links
pushd /boot > /dev/null ; {
	ln -sf System.map-%{KVERREL} System.map
	ln -sf initrd-%{KVERREL}.img initrd-boot
	ln -sf vmlinuz-%{KVERREL} kernel-boot
}
popd

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
popd
fi


%preun 
/sbin/modprobe loop 2> /dev/null > /dev/null  || :
[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --rminitrd --rmmoddep --remove %{KVERREL}

%preun smp
/sbin/modprobe loop 2> /dev/null > /dev/null  || :
[ -x /sbin/new-kernel-pkg ] && /sbin/new-kernel-pkg --rminitrd --rmmoddep --remove %{KVERREL}smp


###
### file lists
###

%if %{buildup}
%files 
%defattr(-,root,root)
/%{image_install_path}/vmlinuz-%{KVERREL}
/boot/System.map-%{KVERREL}
/boot/config-%{KVERREL}
%dir /lib/modules/%{KVERREL}
/lib/modules/%{KVERREL}/kernel
%verify(not mtime) /lib/modules/%{KVERREL}/build

%endif

%if %{buildsmp}
%files smp
%defattr(-,root,root)
/%{image_install_path}/vmlinuz-%{KVERREL}smp
/boot/System.map-%{KVERREL}smp
/boot/config-%{KVERREL}smp
%dir /lib/modules/%{KVERREL}smp
/lib/modules/%{KVERREL}smp/kernel
%verify(not mtime) /lib/modules/%{KVERREL}smp/build

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

%changelog
* Thu Aug  5 2004 Mark Huang <mlhuang@cs.princeton.edu>
- adapt for Fedora Core 2 based PlanetLab 3.0 (remove Source and Patch
  sections, most non-x86 sections, and GPG sections)

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
 
