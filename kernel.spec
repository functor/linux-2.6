#
# $Id$
# $URL$
#
# We have to override the new %%install behavior because, well... the kernel is special.
%global __spec_install_pre %{___build_pre}

Summary: The Linux kernel

# For a stable, released kernel, released_kernel should be 1. For rawhide
# and/or a kernel built from an rc or git snapshot, released_kernel should
# be 0.
%global released_kernel 1

# Versions of various parts

# Polite request for people who spin their own kernel rpms:
# please modify the "buildid" define in a way that identifies
# that the kernel isn't the stock distribution kernel, for example,
# by setting the define to ".local" or ".bz123456"
#
###-vs-
%define buildid .vs2.3.0.36.27

# fedora_build defines which build revision of this kernel version we're
# building. Rather than incrementing forever, as with the prior versioning
# setup, we set fedora_cvs_origin to the current cvs revision s/1.// of the
# kernel spec when the kernel is rebased, so fedora_build automatically
# works out to the offset from the rebase, so it doesn't get too ginormous.
#
# If you're building on a branch, the RCS revision will be something like
# 1.1205.1.1.  In this case we drop the initial 1, subtract fedora_cvs_origin
# from the second number, and then append the rest of the RCS string as is.
# Don't stare at the awk too long, you'll go blind.
%define fedora_cvs_origin   1786
%define fedora_cvs_revision() %2
%global fedora_build %(echo %{fedora_cvs_origin}.%{fedora_cvs_revision $Revision: 1.1948 $} | awk -F . '{ OFS = "."; ORS = ""; print $3 - $1 ; i = 4 ; OFS = ""; while (i <= NF) { print ".", $i ; i++} }')

# base_sublevel is the kernel version we're starting with and patching
# on top of -- for example, 2.6.22-rc7-git1 starts with a 2.6.21 base,
# which yields a base_sublevel of 21.
%define base_sublevel 31

## If this is a released kernel ##
%if 0%{?released_kernel}

# Do we have a -stable update to apply?
%define stable_update 6
# Is it a -stable RC?
%define stable_rc 0
# Set rpm version accordingly
%if 0%{?stable_update}
%define stablerev .%{stable_update}
%define stable_base %{stable_update}
%if 0%{?stable_rc}
# stable RCs are incremental patches, so we need the previous stable patch
%define stable_base %(echo $((%{stable_update} - 1)))
%endif
%endif
%define rpmversion 2.6.%{base_sublevel}%{?stablerev}

## The not-released-kernel case ##
%else
# The next upstream release sublevel (base_sublevel+1)
%define upstream_sublevel %(echo $((%{base_sublevel} + 1)))
# The rc snapshot level
%define rcrev 9
# The git snapshot level
%define gitrev 2
# Set rpm version accordingly
%define rpmversion 2.6.%{upstream_sublevel}
%endif
# Nb: The above rcrev and gitrev values automagically define Patch00 and Patch01 below.

# What parts do we want to build?  We must build at least one kernel.
# These are the kernels that are built IF the architecture allows it.
# All should default to 1 (enabled) and be flipped to 0 (disabled)
# by later arch-specific checks.

# The following build options are enabled by default.
# Use either --without <opt> in your rpmbuild command or force values
# to 0 in here to disable them.
#
# standard kernel
%define with_up        %{?_without_up:        0} %{?!_without_up:        1}
# kernel-smp (only valid for ppc 32-bit)
%define with_smp       %{?_without_smp:       0} %{?!_without_smp:       1}
# kernel-kdump
%define with_kdump     %{?_without_kdump:     0} %{?!_without_kdump:     1}
# kernel-debug
%define with_debug     %{?_with_debug:        1} %{?!_with_debug:        0}
# kernel-doc
%define with_doc       %{?_with_doc:          1} %{?!_with_doc:          0}
# kernel-headers
%define with_headers   %{?_without_headers:   0} %{?!_without_headers:   1}
# kernel-firmware
%define with_firmware  %{?_without_firmware:  0} %{?!_without_firmware:  1}
# tools/perf
%define with_perftool  %{?_without_perftool:  0} %{?!_without_perftool:  1}
# perf noarch subpkg
%define with_perf      %{?_without_perf:      0} %{?!_without_perf:      1}
# kernel-debuginfo
%define with_debuginfo %{?_without_debuginfo: 0} %{?!_without_debuginfo: 1}
# kernel-bootwrapper (for creating zImages from kernel + initrd)
%define with_bootwrapper %{?_without_bootwrapper: 0} %{?!_without_bootwrapper: 1}
# Want to build a the vsdo directories installed
%define with_vdso_install %{?_without_vdso_install: 0} %{?!_without_vdso_install: 1}
# Use dracut instead of mkinitrd for initrd image generation
%define with_dracut       %{?_without_dracut:       0} %{?!_without_dracut:       1}

# Build the kernel-doc package, but don't fail the build if it botches.
# Here "true" means "continue" and "false" means "fail the build".
%if 0%{?released_kernel}
%define doc_build_fail false
%else
%define doc_build_fail true
%endif

%define rawhide_skip_docs 0
%if 0%{?rawhide_skip_docs}
%define with_doc 0
%endif

# Additional options for user-friendly one-off kernel building:
#
# Only build the base kernel (--with baseonly):
%define with_baseonly  %{?_with_baseonly:     1} %{?!_with_baseonly:     0}
# Only build the smp kernel (--with smponly):
%define with_smponly   %{?_with_smponly:      1} %{?!_with_smponly:      0}
# Only build the debug kernel (--with dbgonly):
%define with_dbgonly   %{?_with_dbgonly:      1} %{?!_with_dbgonly:      0}

# should we do C=1 builds with sparse
%define with_sparse	%{?_with_sparse:      1} %{?!_with_sparse:      0}

# Set debugbuildsenabled to 1 for production (build separate debug kernels)
#  and 0 for rawhide (all kernels are debug kernels).
# See also 'make debug' and 'make release'.
%define debugbuildsenabled 1

# Want to build a vanilla kernel build without any non-upstream patches?
# (well, almost none, we need nonintconfig for build purposes). Default to 0 (off).
%define with_vanilla %{?_with_vanilla: 1} %{?!_with_vanilla: 0}

# pkg_release is what we'll fill in for the rpm Release: field
%if 0%{?released_kernel}

%if 0%{?stable_rc}
%define stable_rctag .rc%{stable_rc}
%endif
%define pkg_release %{fedora_build}%{?stable_rctag}%{?buildid}%{?dist}

%else

# non-released_kernel
%if 0%{?rcrev}
%define rctag .rc%rcrev
%endif
%if 0%{?gitrev}
%define gittag .git%gitrev
%if !0%{?rcrev}
%define rctag .rc0
%endif
%endif
%define pkg_release 0.%{fedora_build}%{?rctag}%{?gittag}%{?buildid}%{?dist}

%endif

# The kernel tarball/base version
%define kversion 2.6.%{base_sublevel}

%define make_target bzImage

%define KVERREL %{PACKAGE_VERSION}-%{PACKAGE_RELEASE}.%{_target_cpu}
%define hdrarch %_target_cpu
%define asmarch %_target_cpu

%if 0%{!?nopatches:1}
%define nopatches 0
%endif

%if %{with_vanilla}
%define nopatches 1
%endif

%if %{nopatches}
%define with_bootwrapper 0
%define variant -vanilla
%else
%define variant_fedora -fedora
%endif

%define using_upstream_branch 0
%if 0%{?upstream_branch:1}
%define stable_update 0
%define using_upstream_branch 1
%define variant -%{upstream_branch}%{?variant_fedora}
%define pkg_release 0.%{fedora_build}%{upstream_branch_tag}%{?buildid}%{?dist}
%endif

%if !%{debugbuildsenabled}
%define with_debug 0
%endif

%if !%{with_debuginfo}
%define _enable_debug_packages 0
%endif
%define debuginfodir /usr/lib/debug

# kernel-PAE is only built on i686.
%ifarch i686
%define with_pae 1
%else
%define with_pae 0
%endif

# if requested, only build base kernel
%if %{with_baseonly}
%define with_smp 0
%define with_kdump 0
%define with_debug 0
%endif

# if requested, only build smp kernel
%if %{with_smponly}
%define with_up 0
%define with_kdump 0
%define with_debug 0
%endif

# if requested, only build debug kernel
%if %{with_dbgonly}
%if %{debugbuildsenabled}
%define with_up 0
%endif
%define with_smp 0
%define with_pae 0
%define with_xen 0
%define with_kdump 0
%define with_perftool 0
%endif

%define all_x86 i386 i686

%if %{with_vdso_install}
# These arches install vdso/ directories.
%define vdso_arches %{all_x86} x86_64 ppc ppc64
%endif

# Overrides for generic default options

# only ppc and alphav56 need separate smp kernels
%ifnarch ppc alphaev56
%define with_smp 0
%endif

# only build kernel-kdump on ppc64
# (no relocatable kernel support upstream yet)
#FIXME: Temporarily disabled to speed up builds.
#ifnarch ppc64
%define with_kdump 0
#endif

# don't do debug builds on anything but i686 and x86_64
%ifnarch i686 x86_64
%define with_debug 0
%endif

# only package docs noarch
%ifnarch noarch
%define with_doc 0
%define with_perf 0
%endif

# don't build noarch kernels or headers (duh)
%ifarch noarch
%define with_up 0
%define with_headers 0
%define all_arch_configs kernel-%{version}-*.config
%define with_firmware  %{?_without_firmware:  0} %{?!_without_firmware:  1}
%endif

# bootwrapper is only on ppc
%ifnarch ppc ppc64
%define with_bootwrapper 0
%endif

# sparse blows up on ppc64 alpha and sparc64
%ifarch ppc64 ppc alpha sparc64
%define with_sparse 0
%endif

# Per-arch tweaks

%ifarch %{all_x86}
%define asmarch x86
%define hdrarch i386
%define all_arch_configs kernel-%{version}-i?86*.config
%define image_install_path boot
%define kernel_image arch/x86/boot/bzImage
%endif

%ifarch x86_64
%define asmarch x86
%define all_arch_configs kernel-%{version}-x86_64*.config
%define image_install_path boot
%define kernel_image arch/x86/boot/bzImage
%endif

%ifarch ppc64
%define asmarch powerpc
%define hdrarch powerpc
%define all_arch_configs kernel-%{version}-ppc64*.config
%define image_install_path boot
%define make_target vmlinux
%define kernel_image vmlinux
%define kernel_image_elf 1
%endif

%ifarch s390x
%define asmarch s390
%define hdrarch s390
%define all_arch_configs kernel-%{version}-s390x.config
%define image_install_path boot
%define make_target image
%define kernel_image arch/s390/boot/image
%endif

%ifarch sparc
# We only build sparc headers since we dont support sparc32 hardware
%endif

%ifarch sparc64
%define asmarch sparc
%define all_arch_configs kernel-%{version}-sparc64*.config
%define make_target image
%define kernel_image arch/sparc/boot/image
%define image_install_path boot
%define with_perftool 0
%endif

%ifarch ppc
%define asmarch powerpc
%define hdrarch powerpc
%define all_arch_configs kernel-%{version}-ppc{-,.}*config
%define image_install_path boot
%define make_target vmlinux
%define kernel_image vmlinux
%define kernel_image_elf 1
%endif

%ifarch ia64
%define all_arch_configs kernel-%{version}-ia64*.config
%define image_install_path boot/efi/EFI/redhat
%define make_target compressed
%define kernel_image vmlinux.gz
%endif

%ifarch alpha alphaev56
%define all_arch_configs kernel-%{version}-alpha*.config
%define image_install_path boot
%define make_target vmlinux
%define kernel_image vmlinux
%endif

%ifarch %{arm}
%define all_arch_configs kernel-%{version}-arm*.config
%define image_install_path boot
%define hdrarch arm
%define make_target vmlinux
%define kernel_image vmlinux
%endif

%if %{nopatches}
# XXX temporary until last vdso patches are upstream
%define vdso_arches ppc ppc64
%endif

%if %{nopatches}%{using_upstream_branch}
# Ignore unknown options in our config-* files.
# Some options go with patches we're not applying.
%define oldconfig_target loose_nonint_oldconfig
%else
%define oldconfig_target nonint_oldconfig
%endif

# To temporarily exclude an architecture from being built, add it to
# %nobuildarches. Do _NOT_ use the ExclusiveArch: line, because if we
# don't build kernel-headers then the new build system will no longer let
# us use the previous build of that package -- it'll just be completely AWOL.
# Which is a BadThing(tm).

# We don't build a kernel on i386; we only do kernel-headers there,
# and we no longer build for 31bit S390. Same for 32bit sparc and arm.
%define nobuildarches i386 s390 sparc %{arm}

%ifarch %nobuildarches
%define with_up 0
%define with_smp 0
%define with_pae 0
%define with_kdump 0
%define with_debuginfo 0
%define with_perftool 0
%define _enable_debug_packages 0
%endif

%define with_pae_debug 0
%if %{with_pae}
%define with_pae_debug %{with_debug}
%endif

#
# Three sets of minimum package version requirements in the form of Conflicts:
# to versions below the minimum
#

#
# First the general kernel 2.6 required versions as per
# Documentation/Changes
#
%define kernel_dot_org_conflicts  ppp < 2.4.3-3, isdn4k-utils < 3.2-32, nfs-utils < 1.0.7-12, e2fsprogs < 1.37-4, util-linux < 2.12, jfsutils < 1.1.7-2, reiserfs-utils < 3.6.19-2, xfsprogs < 2.6.13-4, procps < 3.2.5-6.3, oprofile < 0.9.1-2

#
# Then a series of requirements that are distribution specific, either
# because we add patches for something, or the older versions have
# problems with the newer kernel or lack certain things that make
# integration in the distro harder than needed.
#
%define package_conflicts initscripts < 7.23, udev < 063-6, iptables < 1.3.2-1, ipw2200-firmware < 2.4, iwl4965-firmware < 228.57.2, selinux-policy-targeted < 1.25.3-14, squashfs-tools < 4.0, wireless-tools < 29-3

#
# The ld.so.conf.d file we install uses syntax older ldconfig's don't grok.
#
%define kernel_xen_conflicts glibc < 2.3.5-1, xen < 3.0.1

%define kernel_PAE_obsoletes kernel-smp < 2.6.17, kernel-xen <= 2.6.27-0.2.rc0.git6.fc10
%define kernel_PAE_provides kernel-xen = %{rpmversion}-%{pkg_release}

%ifarch x86_64
%define kernel_obsoletes kernel-xen <= 2.6.27-0.2.rc0.git6.fc10
%define kernel_provides kernel-xen = %{rpmversion}-%{pkg_release}
%endif

# We moved the drm include files into kernel-headers, make sure there's
# a recent enough libdrm-devel on the system that doesn't have those.
%define kernel_headers_conflicts libdrm-devel < 2.4.0-0.15

#
# Packages that need to be installed before the kernel is, because the %post
# scripts use them.
#
%define kernel_prereq  fileutils, module-init-tools, initscripts >= 8.11.1-1, kernel-firmware >= %{rpmversion}-%{fedora_build}, grubby >= 7.0.4-1
%if %{with_dracut}
%define initrd_prereq  dracut >= 001-7
%else
%define initrd_prereq  mkinitrd >= 6.0.61-1
%endif

#
# This macro does requires, provides, conflicts, obsoletes for a kernel package.
#	%%kernel_reqprovconf <subpackage>
# It uses any kernel_<subpackage>_conflicts and kernel_<subpackage>_obsoletes
# macros defined above.
#
%define kernel_reqprovconf \
Provides: kernel = %{rpmversion}-%{pkg_release}\
Provides: kernel-%{_target_cpu} = %{rpmversion}-%{pkg_release}%{?1:.%{1}}\
Provides: kernel-drm = 4.3.0\
Provides: kernel-drm-nouveau = 15\
Provides: kernel-modeset = 1\
Provides: kernel-uname-r = %{KVERREL}%{?1:.%{1}}\
Requires(pre): %{kernel_prereq}\
Requires(pre): %{initrd_prereq}\
Requires(post): /sbin/new-kernel-pkg\
Requires(preun): /sbin/new-kernel-pkg\
Conflicts: %{kernel_dot_org_conflicts}\
Conflicts: %{package_conflicts}\
%{expand:%%{?kernel%{?1:_%{1}}_conflicts:Conflicts: %%{kernel%{?1:_%{1}}_conflicts}}}\
%{expand:%%{?kernel%{?1:_%{1}}_obsoletes:Obsoletes: %%{kernel%{?1:_%{1}}_obsoletes}}}\
%{expand:%%{?kernel%{?1:_%{1}}_provides:Provides: %%{kernel%{?1:_%{1}}_provides}}}\
# We can't let RPM do the dependencies automatic because it'll then pick up\
# a correct but undesirable perl dependency from the module headers which\
# isn't required for the kernel proper to function\
AutoReq: no\
AutoProv: yes\
%{nil}

Name: kernel%{?variant}
Group: System Environment/Kernel
License: GPLv2
URL: http://www.kernel.org/
Version: %{rpmversion}
Release: %{pkg_release}
# DO NOT CHANGE THE 'ExclusiveArch' LINE TO TEMPORARILY EXCLUDE AN ARCHITECTURE BUILD.
# SET %%nobuildarches (ABOVE) INSTEAD
ExclusiveArch: noarch %{all_x86} x86_64 ppc ppc64 ia64 sparc sparc64 s390x alpha alphaev56 %{arm}
ExclusiveOS: Linux

%kernel_reqprovconf
%ifarch x86_64 sparc64
Obsoletes: kernel-smp
%endif


#
# List the packages used during the kernel build
#
BuildRequires: module-init-tools, patch >= 2.5.4, bash >= 2.03, sh-utils, tar
BuildRequires: bzip2, findutils, gzip, m4, perl, make >= 3.78, diffutils, gawk
BuildRequires: gcc >= 3.4.2, binutils >= 2.12, redhat-rpm-config
BuildRequires: net-tools
BuildRequires: xmlto, asciidoc
%if %{with_sparse}
BuildRequires: sparse >= 0.4.1
%endif
%if %{with_perftool}
BuildRequires: elfutils-libelf-devel zlib-devel binutils-devel
%endif
BuildConflicts: rhbuildsys(DiskFree) < 500Mb

%define fancy_debuginfo 0
%if %{with_debuginfo}
%if 0%{?fedora} >= 8 || 0%{?rhel} >= 6
%define fancy_debuginfo 1
%endif
%endif

%if %{fancy_debuginfo}
# Fancy new debuginfo generation introduced in Fedora 8.
BuildRequires: rpm-build >= 4.4.2.1-4
%define debuginfo_args --strict-build-id
%endif

Source0: ftp://ftp.kernel.org/pub/linux/kernel/v2.6/linux-%{kversion}.tar.bz2

Source11: genkey
Source14: find-provides
Source15: merge.pl

Source20: Makefile.config
Source21: config-debug
Source22: config-nodebug
Source23: config-generic
Source24: config-rhel-generic

Source30: config-x86-generic
Source31: config-i686-PAE

Source40: config-x86_64-generic

Source50: config-powerpc-generic
Source51: config-powerpc32-generic
Source52: config-powerpc32-smp
Source53: config-powerpc64

Source60: config-ia64-generic

Source70: config-s390x

Source90: config-sparc64-generic

Source100: config-arm

Source110: config-vserver

Source200: perf

# Here should be only the patches up to the upstream canonical Linus tree.

# For a stable release kernel
%if 0%{?stable_update}
%if 0%{?stable_base}
%define    stable_patch_00  patch-2.6.%{base_sublevel}.%{stable_base}.bz2
Patch00: %{stable_patch_00}
%endif
%if 0%{?stable_rc}
%define    stable_patch_01  patch-2.6.%{base_sublevel}.%{stable_update}-rc%{stable_rc}.bz2
Patch01: %{stable_patch_01}
%endif

# non-released_kernel case
# These are automagically defined by the rcrev and gitrev values set up
# near the top of this spec file.
%else
%if 0%{?rcrev}
Patch00: patch-2.6.%{upstream_sublevel}-rc%{rcrev}.bz2
%if 0%{?gitrev}
Patch01: patch-2.6.%{upstream_sublevel}-rc%{rcrev}-git%{gitrev}.bz2
%endif
%else
# pre-{base_sublevel+1}-rc1 case
%if 0%{?gitrev}
Patch00: patch-2.6.%{base_sublevel}-git%{gitrev}.bz2
%endif
%endif
%endif

%if %{using_upstream_branch}
### BRANCH PATCH ###
%endif

Patch02: git-linus.diff

# we always need nonintconfig, even for -vanilla kernels
Patch03: linux-2.6-build-nonintconfig.patch

# we also need compile fixes for -vanilla
Patch04: linux-2.6-compile-fixes.patch

# build tweak for build ID magic, even for -vanilla
Patch05: linux-2.6-makefile-after_link.patch

###-vs- http://vserver.13thfloor.at/ExperimentalT/patch-2.6.31.6-vs2.3.0.36.27.diff
Patch06: patch-2.6.31.6-vs2.3.0.36.27.diff

%if !%{nopatches}

# revert upstream patches we get via other methods
Patch09: linux-2.6-upstream-reverts.patch
# Git trees.
Patch10: git-cpufreq.patch
Patch11: git-bluetooth.patch

# Standalone patches
Patch20: linux-2.6-hotfixes.patch

Patch21: linux-2.6-tracehook.patch
Patch22: linux-2.6-utrace.patch

Patch30: sched-introduce-SCHED_RESET_ON_FORK-scheduling-policy-flag.patch

Patch31: disable-stackprotector-all.patch

# Intel IOMMU fixes/workarounds
Patch100: linux-2.6-die-closed-source-bios-muppets-die.patch
Patch101: linux-2.6-intel-iommu-updates.patch
Patch102: linux-2.6-iommu-at-zero.patch
Patch103: linux-2.6-iommu-dmar-all-1s.patch
Patch104: linux-2.6-iommu-another-hp-screwup.patch
Patch105: linux-2.6-iommu-sanity-checks-for-intr-remap-too.patch
Patch106: linux-2.6-iommu-hp-cantiga-resume.patch

Patch141: linux-2.6-ps3-storage-alias.patch
Patch143: linux-2.6-g5-therm-shutdown.patch
Patch144: linux-2.6-vio-modalias.patch
Patch147: linux-2.6-imac-transparent-bridge.patch

Patch150: linux-2.6.29-sparc-IOC_TYPECHECK.patch

Patch160: linux-2.6-execshield.patch

Patch250: linux-2.6-debug-sizeof-structs.patch
Patch260: linux-2.6-debug-nmi-timeout.patch
Patch270: linux-2.6-debug-taint-vm.patch
Patch280: linux-2.6-debug-spinlock-taint.patch
Patch300: linux-2.6-driver-level-usb-autosuspend.diff
Patch302: linux-2.6-qcserial-autosuspend.diff
Patch303: linux-2.6-bluetooth-autosuspend.diff
Patch304: linux-2.6-usb-uvc-autosuspend.diff
Patch340: linux-2.6-debug-vm-would-have-oomkilled.patch
Patch360: linux-2.6-debug-always-inline-kzalloc.patch
Patch380: linux-2.6-defaults-pci_no_msi.patch
Patch381: linux-2.6-pciehp-update.patch
Patch382: linux-2.6-defaults-pciehp.patch
Patch383: linux-2.6-defaults-aspm.patch
Patch390: linux-2.6-defaults-acpi-video.patch
Patch391: linux-2.6-acpi-video-dos.patch
Patch450: linux-2.6-input-kill-stupid-messages.patch
Patch451: linux-2.6-input-fix-toshiba-hotkeys.patch
Patch452: linux-2.6.30-no-pcspkr-modalias.patch

Patch460: linux-2.6-serial-460800.patch

Patch470: die-floppy-die.patch

Patch500: linux-2.6.31-copy_from_user-bounds.patch

Patch510: linux-2.6-silence-noise.patch
Patch520: linux-2.6.30-hush-rom-warning.patch
Patch530: linux-2.6-silence-fbcon-logo.patch
Patch570: linux-2.6-selinux-mprotect-checks.patch
Patch580: linux-2.6-sparc-selinux-mprotect-checks.patch

Patch600: linux-2.6-defaults-alsa-hda-beep-off.patch
Patch601: linux-2.6-alsa-improve-hda-powerdown.patch
Patch610: hda_intel-prealloc-4mb-dmabuffer.patch
Patch611: alsa-tell-user-that-stream-to-be-rewound-is-suspended.patch

Patch670: linux-2.6-ata-quirk.patch
Patch671: linux-2.6-ahci-export-capabilities.patch

Patch680: prism54-remove-pci-dev-table.patch
Patch681: linux-2.6-ath9k-fixes.patch

Patch800: linux-2.6-crash-driver.patch

Patch900: linux-2.6-pci-cacheline-sizing.patch

# ACPI
Patch1100: linux-2.6.31-cpuidle-faster-io.patch
# EC fixes from 2.6.32 (#492699, #525681)
Patch1110: acpi-ec-merge-irq-and-poll-modes.patch
Patch1120: acpi-ec-use-burst-mode-only-for-msi-notebooks.patch
Patch1130: acpi-ec-restart-command-even-if-no-interrupts-from-ec.patch

Patch1515: lirc-2.6.31.patch
Patch1517: hdpvr-ir-enable.patch
Patch1518: hid-ignore-all-recent-imon-devices.patch

# virt + ksm patches
Patch1550: linux-2.6-ksm.patch
Patch1551: linux-2.6-ksm-kvm.patch
Patch1552: linux-2.6-ksm-updates.patch
Patch1553: linux-2.6-ksm-fix-munlock.patch
Patch1554: linux-2.6-ksm-updates-from-32.patch
Patch1579: linux-2.6-virtio_blk-revert-QUEUE_FLAG_VIRT-addition.patch
Patch1583: linux-2.6-xen-fix-is_disconnected_device-exists_disconnected_device.patch
Patch1584: linux-2.6-xen-improvement-to-wait_for_devices.patch
Patch1585: linux-2.6-xen-increase-device-connection-timeout.patch
Patch1586: linux-2.6-virtio_blk-add-support-for-cache-flush.patch

# nouveau + drm fixes
Patch1810: kms-offb-handoff.patch
Patch1812: drm-next-b390f944.patch
Patch1813: drm-radeon-pm.patch
Patch1814: drm-nouveau.patch
Patch1818: drm-i915-resume-force-mode.patch
# intel drm is all merged upstream
Patch1824: drm-intel-next.patch
Patch1825: drm-intel-pm.patch
Patch1826: drm-intel-no-tv-hotplug.patch
Patch1827: drm-i915-fix-tvmode-oops.patch
Patch1831: drm-conservative-fallback-modes.patch
Patch1832: drm-edid-retry.patch
Patch1834: drm-edid-header-fixup.patch
Patch1835: drm-default-mode.patch
Patch1837: drm-i915-fix-sync-to-vbl-when-vga-is-off.patch
Patch1839: drm-radeon-misc-fixes.patch
Patch1840: drm-radeon-rv410-test-fix.patch

# vga arb
Patch1900: linux-2.6-vga-arb.patch
Patch1901: drm-vga-arb.patch
Patch1902: drm-radeon-kms-arbiter-return-ignore.patch

# make harmless fbcon debug less loud
Patch1903: fbcon-lower-debug.patch

# kludge to make ich9 e1000 work
Patch2000: linux-2.6-e1000-ich9.patch

# linux1394 git patches
Patch2200: linux-2.6-firewire-git-update.patch
Patch2201: linux-2.6-firewire-git-pending.patch

# Quiet boot fixes
# silence the ACPI blacklist code
Patch2802: linux-2.6-silence-acpi-blacklist.patch

Patch2899: linux-2.6-v4l-dvb-fixes.patch
Patch2900: linux-2.6-v4l-dvb-update.patch
Patch2901: linux-2.6-v4l-dvb-experimental.patch
Patch2904: v4l-dvb-fix-cx25840-firmware-loading.patch

# fs fixes

#btrfs
Patch3000: linux-2.6-btrfs-upstream.patch

# NFSv4
Patch3050: linux-2.6-nfsd4-proots.patch
Patch3060: linux-2.6-nfs4-ver4opt.patch
Patch3061: linux-2.6-nfs4-callback-hidden.patch

# VIA Nano / VX8xx updates
Patch11010: via-hwmon-temp-sensor.patch

# patches headed upstream
Patch12010: linux-2.6-dell-laptop-rfkill-fix.patch
Patch12011: linux-2.6-block-silently-error-unsupported-empty-barriers-too.patch
Patch12012: linux-2.6-rtc-show-hctosys.patch
Patch12013: linux-2.6-rfkill-all.patch
Patch12014: linux-2.6-selinux-module-load-perms.patch

# sched fixes cherry-picked from 2.6.32
Patch13100: sched-deal-with-low-load-in-wake-affine.patch
Patch13101: sched-ensure-child-cant-gain-time-over-its-parent-after-fork.patch
Patch13102: sched-remove-shortcut-from-select-task-rq-fair.patch
# latency defaults from 2.6.32
Patch13110: sched-retune-scheduler-latency-defaults.patch
# Fix huge wakeup latencies
Patch13120: sched-update-the-clock-of-runqueue-select-task-rq-selected.patch

# patches headed for -stable

# make perf counter API available to userspace (#527264)
Patch14010: perf-make-perf-counter-h-available-to-userspace.patch

# fix resource counter issues on *big* machines
Patch14101: improve-resource-counter-scalability.patch

# fix perf for sysprof
Patch14420: perf-events-fix-swevent-hrtimer-sampling.patch
Patch14421: perf-events-dont-generate-events-for-the-idle-task.patch

Patch14430: crypto-via-padlock-fix-nano-aes.patch

# tg3 fixes (#527209)
Patch14451: tg3-01-delay-mdio-bus-init-until-fw-finishes.patch
Patch14452: tg3-02-fix-tso-test-against-wrong-flags-var.patch
Patch14453: tg3-03-fix-57780-asic-rev-pcie-link-receiver-errors.patch
Patch14454: tg3-04-prevent-tx-bd-corruption.patch
Patch14455: tg3-05-assign-flags-to-fixes-in-start_xmit_dma_bug.patch
Patch14456: tg3-06-fix-5906-transmit-hangs.patch

Patch14460: highmem-Fix-debug_kmap_atomic-to-also-handle-KM_IRQ_.patch
Patch14461: highmem-Fix-race-in-debug_kmap_atomic-which-could-ca.patch
Patch14462: highmem-fix-arm-powerpc-kmap_types.patch

Patch14463: dlm-fix-connection-close-handling.patch

# rhbz#544144 [bbf31bf18d34caa87dd01f08bf713635593697f2]
Patch14464: ipv4-fix-null-ptr-deref-in-ip_fragment.patch

%endif

BuildRoot: %{_tmppath}/kernel-%{KVERREL}-root

%description
The kernel package contains the Linux kernel (vmlinuz), the core of any
Linux operating system.  The kernel handles the basic functions
of the operating system: memory allocation, process allocation, device
input and output, etc.


%package doc
Summary: Various documentation bits found in the kernel source
Group: Documentation
%description doc
This package contains documentation files from the kernel
source. Various bits of information about the Linux kernel and the
device drivers shipped with it are documented in these files.

You'll want to install this package if you need a reference to the
options that can be passed to Linux kernel modules at load time.


%package headers
Summary: Header files for the Linux kernel for use by glibc
Group: Development/System
Obsoletes: glibc-kernheaders
Provides: glibc-kernheaders = 3.0-46
%description headers
Kernel-headers includes the C header files that specify the interface
between the Linux kernel and userspace libraries and programs.  The
header files define structures and constants that are needed for
building most standard programs and are also needed for rebuilding the
glibc package.

%package firmware
Summary: Firmware files used by the Linux kernel
Group: Development/System
# This is... complicated.
# Look at the WHENCE file.
License: GPL+ and GPLv2+ and MIT and Redistributable, no modification permitted
%if "x%{?variant}" != "x"
Provides: kernel-firmware = %{rpmversion}-%{pkg_release}
%endif
%description firmware
Kernel-firmware includes firmware files required for some devices to
operate.

%package bootwrapper
Summary: Boot wrapper files for generating combined kernel + initrd images
Group: Development/System
Requires: gzip
%description bootwrapper
Kernel-bootwrapper contains the wrapper code which makes bootable "zImage"
files combining both kernel and initial ramdisk.

%package debuginfo-common-%{_target_cpu}
Summary: Kernel source files used by %{name}-debuginfo packages
Group: Development/Debug
%description debuginfo-common-%{_target_cpu}
This package is required by %{name}-debuginfo subpackages.
It provides the kernel source files common to all builds.

%package -n perf
Summary: Performance monitoring for the Linux kernel
Group: Development/System
License: GPLv2
%description -n perf
This package provides the supporting documentation for the perf tool
shipped in each kernel image subpackage.

#
# This macro creates a kernel-<subpackage>-debuginfo package.
#	%%kernel_debuginfo_package <subpackage>
#
%define kernel_debuginfo_package() \
%package %{?1:%{1}-}debuginfo\
Summary: Debug information for package %{name}%{?1:-%{1}}\
Group: Development/Debug\
Requires: %{name}-debuginfo-common-%{_target_cpu} = %{version}-%{release}\
Provides: %{name}%{?1:-%{1}}-debuginfo-%{_target_cpu} = %{version}-%{release}\
AutoReqProv: no\
%description -n %{name}%{?1:-%{1}}-debuginfo\
This package provides debug information for package %{name}%{?1:-%{1}}.\
This is required to use SystemTap with %{name}%{?1:-%{1}}-%{KVERREL}.\
%{expand:%%global debuginfo_args %{?debuginfo_args} -p '/.*/%%{KVERREL}%{?1:\.%{1}}/.*|/.*%%{KVERREL}%{?1:\.%{1}}(\.debug)?' -o debuginfo%{?1}.list}\
%{nil}

#
# This macro creates a kernel-<subpackage>-devel package.
#	%%kernel_devel_package <subpackage> <pretty-name>
#
%define kernel_devel_package() \
%package %{?1:%{1}-}devel\
Summary: Development package for building kernel modules to match the %{?2:%{2} }kernel\
Group: System Environment/Kernel\
Provides: kernel%{?1:-%{1}}-devel-%{_target_cpu} = %{version}-%{release}\
Provides: kernel-devel-%{_target_cpu} = %{version}-%{release}%{?1:.%{1}}\
Provides: kernel-devel = %{version}-%{release}%{?1:.%{1}}\
Provides: kernel-devel-uname-r = %{KVERREL}%{?1:.%{1}}\
AutoReqProv: no\
Requires(pre): /usr/bin/find\
%description -n kernel%{?variant}%{?1:-%{1}}-devel\
This package provides kernel headers and makefiles sufficient to build modules\
against the %{?2:%{2} }kernel package.\
%{nil}

#
# This macro creates a kernel-<subpackage> and its -devel and -debuginfo too.
#	%%define variant_summary The Linux kernel compiled for <configuration>
#	%%kernel_variant_package [-n <pretty-name>] <subpackage>
#
%define kernel_variant_package(n:) \
%package %1\
Summary: %{variant_summary}\
Group: System Environment/Kernel\
%kernel_reqprovconf\
%{expand:%%kernel_devel_package %1 %{!?-n:%1}%{?-n:%{-n*}}}\
%{expand:%%kernel_debuginfo_package %1}\
%{nil}


# First the auxiliary packages of the main kernel package.
%kernel_devel_package
%kernel_debuginfo_package


# Now, each variant package.

%define variant_summary The Linux kernel compiled for SMP machines
%kernel_variant_package -n SMP smp
%description smp
This package includes a SMP version of the Linux kernel. It is
required only on machines with two or more CPUs as well as machines with
hyperthreading technology.

Install the kernel-smp package if your machine uses two or more CPUs.


%define variant_summary The Linux kernel compiled for PAE capable machines
%kernel_variant_package PAE
%description PAE
This package includes a version of the Linux kernel with support for up to
64GB of high memory. It requires a CPU with Physical Address Extensions (PAE).
The non-PAE kernel can only address up to 4GB of memory.
Install the kernel-PAE package if your machine has more than 4GB of memory.


%define variant_summary The Linux kernel compiled with extra debugging enabled for PAE capable machines
%kernel_variant_package PAEdebug
Obsoletes: kernel-PAE-debug
%description PAEdebug
This package includes a version of the Linux kernel with support for up to
64GB of high memory. It requires a CPU with Physical Address Extensions (PAE).
The non-PAE kernel can only address up to 4GB of memory.
Install the kernel-PAE package if your machine has more than 4GB of memory.

This variant of the kernel has numerous debugging options enabled.
It should only be installed when trying to gather additional information
on kernel bugs, as some of these options impact performance noticably.


%define variant_summary The Linux kernel compiled with extra debugging enabled
%kernel_variant_package debug
%description debug
The kernel package contains the Linux kernel (vmlinuz), the core of any
Linux operating system.  The kernel handles the basic functions
of the operating system:  memory allocation, process allocation, device
input and output, etc.

This variant of the kernel has numerous debugging options enabled.
It should only be installed when trying to gather additional information
on kernel bugs, as some of these options impact performance noticably.


%define variant_summary A minimal Linux kernel compiled for crash dumps
%kernel_variant_package kdump
%description kdump
This package includes a kdump version of the Linux kernel. It is
required only on machines which will use the kexec-based kernel crash dump
mechanism.


%prep
# do a few sanity-checks for --with *only builds
%if %{with_baseonly}
%if !%{with_up}%{with_pae}
echo "Cannot build --with baseonly, up build is disabled"
exit 1
%endif
%endif

%if %{with_smponly}
%if !%{with_smp}
echo "Cannot build --with smponly, smp build is disabled"
exit 1
%endif
%endif

# more sanity checking; do it quietly
if [ "%{patches}" != "%%{patches}" ] ; then
  for patch in %{patches} ; do
    if [ ! -f $patch ] ; then
      echo "ERROR: Patch  ${patch##/*/}  listed in specfile but is missing"
      exit 1
    fi
  done
fi 2>/dev/null

patch_command='patch -p1 -F1 -s'
ApplyPatch()
{
  local patch=$1
  shift
  if [ ! -f $RPM_SOURCE_DIR/$patch ]; then
    exit 1
  fi
  if ! egrep "^Patch[0-9]+: $patch\$" %{_specdir}/${RPM_PACKAGE_NAME%%%%%{?variant}}.spec ; then
    if [ "${patch:0:10}" != "patch-2.6." ] ; then
      echo "ERROR: Patch  $patch  not listed as a source patch in specfile"
      exit 1
    fi
  fi 2>/dev/null
  case "$patch" in
  *.bz2) bunzip2 < "$RPM_SOURCE_DIR/$patch" | $patch_command ${1+"$@"} ;;
  *.gz) gunzip < "$RPM_SOURCE_DIR/$patch" | $patch_command ${1+"$@"} ;;
  *) $patch_command ${1+"$@"} < "$RPM_SOURCE_DIR/$patch" ;;
  esac
}

# don't apply patch if it's empty
ApplyOptionalPatch()
{
  local patch=$1
  shift
  if [ ! -f $RPM_SOURCE_DIR/$patch ]; then
    exit 1
  fi
  local C=$(wc -l $RPM_SOURCE_DIR/$patch | awk '{print $1}')
  if [ "$C" -gt 9 ]; then
    ApplyPatch $patch ${1+"$@"}
  fi
}

# we don't want a .config file when building firmware: it just confuses the build system
%define build_firmware \
   mv .config .config.firmware_save \
   make INSTALL_FW_PATH=$RPM_BUILD_ROOT/lib/firmware firmware_install \
   mv .config.firmware_save .config

# First we unpack the kernel tarball.
# If this isn't the first make prep, we use links to the existing clean tarball
# which speeds things up quite a bit.

# Update to latest upstream.
%if 0%{?released_kernel}
%define vanillaversion 2.6.%{base_sublevel}
# non-released_kernel case
%else
%if 0%{?rcrev}
%define vanillaversion 2.6.%{upstream_sublevel}-rc%{rcrev}
%if 0%{?gitrev}
%define vanillaversion 2.6.%{upstream_sublevel}-rc%{rcrev}-git%{gitrev}
%endif
%else
# pre-{base_sublevel+1}-rc1 case
%if 0%{?gitrev}
%define vanillaversion 2.6.%{base_sublevel}-git%{gitrev}
%endif
%endif
%endif

# We can share hardlinked source trees by putting a list of
# directory names of the CVS checkouts that we want to share
# with in .shared-srctree. (Full pathnames are required.)
[ -f .shared-srctree ] && sharedirs=$(cat .shared-srctree)

if [ ! -d kernel-%{kversion}/vanilla-%{vanillaversion} ]; then

  if [ -d kernel-%{kversion}/vanilla-%{kversion} ]; then

    cd kernel-%{kversion}

    # Any vanilla-* directories other than the base one are stale.
    for dir in vanilla-*; do
      [ "$dir" = vanilla-%{kversion} ] || rm -rf $dir &
    done

  else

    # Ok, first time we do a make prep.
    rm -f pax_global_header
    for sharedir in $sharedirs ; do
      if [[ ! -z $sharedir  &&  -d $sharedir/kernel-%{kversion}/vanilla-%{kversion} ]] ; then
        break
      fi
    done
    if [[ ! -z $sharedir  &&  -d $sharedir/kernel-%{kversion}/vanilla-%{kversion} ]] ; then
%setup -q -n kernel-%{kversion} -c -T
      cp -rl $sharedir/kernel-%{kversion}/vanilla-%{kversion} .
    else
%setup -q -n kernel-%{kversion} -c
      mv linux-%{kversion} vanilla-%{kversion}
    fi

  fi

%if "%{kversion}" != "%{vanillaversion}"

  for sharedir in $sharedirs ; do
    if [[ ! -z $sharedir  &&  -d $sharedir/kernel-%{kversion}/vanilla-%{vanillaversion} ]] ; then
      break
    fi
  done
  if [[ ! -z $sharedir  &&  -d $sharedir/kernel-%{kversion}/vanilla-%{vanillaversion} ]] ; then

    cp -rl $sharedir/kernel-%{kversion}/vanilla-%{vanillaversion} .

  else

    cp -rl vanilla-%{kversion} vanilla-%{vanillaversion}
    cd vanilla-%{vanillaversion}

# Update vanilla to the latest upstream.
# (non-released_kernel case only)
%if 0%{?rcrev}
    ApplyPatch patch-2.6.%{upstream_sublevel}-rc%{rcrev}.bz2
%if 0%{?gitrev}
    ApplyPatch patch-2.6.%{upstream_sublevel}-rc%{rcrev}-git%{gitrev}.bz2
%endif
%else
# pre-{base_sublevel+1}-rc1 case
%if 0%{?gitrev}
    ApplyPatch patch-2.6.%{base_sublevel}-git%{gitrev}.bz2
%endif
%endif

    cd ..

  fi

%endif

else
  # We already have a vanilla dir.
  cd kernel-%{kversion}
fi

if [ -d linux-%{kversion}.%{_target_cpu} ]; then
  # Just in case we ctrl-c'd a prep already
  rm -rf deleteme.%{_target_cpu}
  # Move away the stale away, and delete in background.
  mv linux-%{kversion}.%{_target_cpu} deleteme.%{_target_cpu}
  rm -rf deleteme.%{_target_cpu} &
fi

cp -rl vanilla-%{vanillaversion} linux-%{kversion}.%{_target_cpu}

cd linux-%{kversion}.%{_target_cpu}

# released_kernel with possible stable updates
%if 0%{?stable_base}
ApplyPatch %{stable_patch_00}
%endif
%if 0%{?stable_rc}
ApplyPatch %{stable_patch_01}
%endif

%if %{using_upstream_branch}
### BRANCH APPLY ###
%endif

# Drop some necessary files from the source dir into the buildroot
cp $RPM_SOURCE_DIR/config-* .
cp %{SOURCE15} .

# Dynamically generate kernel .config files from config-* files
make -f %{SOURCE20} VERSION=%{version} configs

#if a rhel kernel, apply the rhel config options
%if 0%{?rhel}
  for i in %{all_arch_configs}
  do
    mv $i $i.tmp
    ./merge.pl config-rhel-generic $i.tmp > $i
    rm $i.tmp
  done
%endif

#ApplyOptionalPatch git-linus.diff

# This patch adds a "make nonint_oldconfig" which is non-interactive and
# also gives a list of missing options at the end. Useful for automated
# builds (as used in the buildsystem).
ApplyPatch linux-2.6-build-nonintconfig.patch

ApplyPatch linux-2.6-makefile-after_link.patch

###-vs-
ApplyPatch patch-2.6.31.6-vs2.3.0.36.27.diff

#
# misc small stuff to make things compile
#
ApplyOptionalPatch linux-2.6-compile-fixes.patch

%if !%{nopatches}

# revert patches from upstream that conflict or that we get via other means
ApplyOptionalPatch linux-2.6-upstream-reverts.patch -R

ApplyOptionalPatch git-cpufreq.patch
#ApplyOptionalPatch git-bluetooth.patch

ApplyPatch linux-2.6-hotfixes.patch

# Roland's utrace ptrace replacement.
ApplyPatch linux-2.6-tracehook.patch
###-vs-
ApplyPatch linux-2.6-utrace.patch -F3

ApplyPatch sched-introduce-SCHED_RESET_ON_FORK-scheduling-policy-flag.patch

ApplyPatch disable-stackprotector-all.patch

# Architecture patches
# x86(-64)
ApplyPatch via-hwmon-temp-sensor.patch
ApplyPatch linux-2.6-dell-laptop-rfkill-fix.patch

#
# Intel IOMMU
#
# Quiesce USB host controllers before setting up the IOMMU
ApplyPatch linux-2.6-die-closed-source-bios-muppets-die.patch
# Some performance fixes, unify hardware/software passthrough support, and
# most importantly: notice when the BIOS points us to a region that returns
# all 0xFF, and claims that there's an IOMMU there.
ApplyPatch linux-2.6-intel-iommu-updates.patch
ApplyPatch linux-2.6-iommu-at-zero.patch
ApplyPatch linux-2.6-iommu-dmar-all-1s.patch
# Check for RMRRs which end before they start
ApplyPatch linux-2.6-iommu-another-hp-screwup.patch
# Apply the 'at zero' and 'all 0xFF' sanity checks for intr_remap too
ApplyPatch linux-2.6-iommu-sanity-checks-for-intr-remap-too.patch
# Fix up MMIO BAR for integrated graphics on HP laptops on resume (#536675)
ApplyPatch linux-2.6-iommu-hp-cantiga-resume.patch

#
# PowerPC
#
### NOT (YET) UPSTREAM:
# The storage alias patch is Fedora-local, and allows the old 'ps3_storage'
# module name to work on upgrades. Otherwise, I believe mkinitrd will fail
# to pull the module in,
ApplyPatch linux-2.6-ps3-storage-alias.patch
# Alleviate G5 thermal shutdown problems
ApplyPatch linux-2.6-g5-therm-shutdown.patch
# Provide modalias in sysfs for vio devices
ApplyPatch linux-2.6-vio-modalias.patch
# Work around PCIe bridge setup on iSight
ApplyPatch linux-2.6-imac-transparent-bridge.patch

#
# SPARC64
#
ApplyPatch linux-2.6.29-sparc-IOC_TYPECHECK.patch

#
# Exec shield
#
###-vs- 
ApplyPatch linux-2.6-execshield.patch -F3

#
# bugfixes to drivers and filesystems
#

# ext4

# xfs

# btrfs
###-vs- 
ApplyPatch linux-2.6-btrfs-upstream.patch

# eCryptfs

# NFSv4
ApplyPatch linux-2.6-nfsd4-proots.patch
ApplyPatch linux-2.6-nfs4-ver4opt.patch
ApplyPatch linux-2.6-nfs4-callback-hidden.patch

# USB
ApplyPatch linux-2.6-driver-level-usb-autosuspend.diff
ApplyPatch linux-2.6-qcserial-autosuspend.diff
ApplyPatch linux-2.6-bluetooth-autosuspend.diff
ApplyPatch linux-2.6-usb-uvc-autosuspend.diff

# ACPI
ApplyPatch linux-2.6-defaults-acpi-video.patch
ApplyPatch linux-2.6-acpi-video-dos.patch
# cpuidle: Fix the menu governor to boost IO performance
ApplyPatch linux-2.6.31-cpuidle-faster-io.patch
# EC fixes from 2.6.32 (#492699, #525681)
ApplyPatch acpi-ec-merge-irq-and-poll-modes.patch
ApplyPatch acpi-ec-use-burst-mode-only-for-msi-notebooks.patch
ApplyPatch acpi-ec-restart-command-even-if-no-interrupts-from-ec.patch

# Various low-impact patches to aid debugging.
ApplyPatch linux-2.6-debug-sizeof-structs.patch
ApplyPatch linux-2.6-debug-nmi-timeout.patch
ApplyPatch linux-2.6-debug-taint-vm.patch
ApplyPatch linux-2.6-debug-spinlock-taint.patch
###-vs- 
ApplyPatch linux-2.6-debug-vm-would-have-oomkilled.patch
ApplyPatch linux-2.6-debug-always-inline-kzalloc.patch

#
# PCI
#
# disable message signaled interrupts
ApplyPatch linux-2.6-defaults-pci_no_msi.patch
# update the pciehp driver
#ApplyPatch linux-2.6-pciehp-update.patch
# default to enabling passively listening for hotplug events
#ApplyPatch linux-2.6-defaults-pciehp.patch
# enable ASPM by default on hardware we expect to work
ApplyPatch linux-2.6-defaults-aspm.patch

#
# SCSI Bits.
#

# ALSA
# squelch hda_beep by default
ApplyPatch linux-2.6-defaults-alsa-hda-beep-off.patch
ApplyPatch linux-2.6-alsa-improve-hda-powerdown.patch
ApplyPatch hda_intel-prealloc-4mb-dmabuffer.patch
ApplyPatch alsa-tell-user-that-stream-to-be-rewound-is-suspended.patch

# Networking

# Misc fixes
# The input layer spews crap no-one cares about.
ApplyPatch linux-2.6-input-kill-stupid-messages.patch

# stop floppy.ko from autoloading during udev...
ApplyPatch die-floppy-die.patch

# make copy_from_user to a stack slot provable right
# hosed stuff, just drop this close to beta
#ApplyPatch linux-2.6.31-copy_from_user-bounds.patch

# Get away from having to poll Toshibas
#ApplyPatch linux-2.6-input-fix-toshiba-hotkeys.patch

ApplyPatch linux-2.6.30-no-pcspkr-modalias.patch

# Allow to use 480600 baud on 16C950 UARTs
ApplyPatch linux-2.6-serial-460800.patch

# Silence some useless messages that still get printed with 'quiet'
ApplyPatch linux-2.6-silence-noise.patch
ApplyPatch linux-2.6.30-hush-rom-warning.patch

# Make fbcon not show the penguins with 'quiet'
ApplyPatch linux-2.6-silence-fbcon-logo.patch

# Fix the SELinux mprotect checks on executable mappings
#ApplyPatch linux-2.6-selinux-mprotect-checks.patch
# Fix SELinux for sparc
#ApplyPatch linux-2.6-sparc-selinux-mprotect-checks.patch

# Changes to upstream defaults.


# ia64 ata quirk
ApplyPatch linux-2.6-ata-quirk.patch

# Make it possible to identify non-hotplug SATA ports
ApplyPatch linux-2.6-ahci-export-capabilities.patch

# prism54: remove pci modinfo device table
ApplyPatch prism54-remove-pci-dev-table.patch

# ath9k: add fixes suggested by upstream maintainer
ApplyPatch linux-2.6-ath9k-fixes.patch

# /dev/crash driver.
ApplyPatch linux-2.6-crash-driver.patch

# Determine cacheline sizes in a generic manner.
ApplyPatch linux-2.6-pci-cacheline-sizing.patch

# http://www.lirc.org/
ApplyPatch lirc-2.6.31.patch
# enable IR receiver on Hauppauge HD PVR (v4l-dvb merge pending)
ApplyPatch hdpvr-ir-enable.patch
# tell usbhid to ignore all imon devices (sent upstream 2009.07.31)
ApplyPatch hid-ignore-all-recent-imon-devices.patch

# Add kernel KSM support
ApplyPatch linux-2.6-ksm.patch
ApplyPatch linux-2.6-ksm-updates.patch
ApplyPatch linux-2.6-ksm-fix-munlock.patch
ApplyPatch linux-2.6-ksm-updates-from-32.patch
# Optimize KVM for KSM support
ApplyPatch linux-2.6-ksm-kvm.patch

# Assorted Virt Fixes
ApplyPatch linux-2.6-virtio_blk-revert-QUEUE_FLAG_VIRT-addition.patch
ApplyPatch linux-2.6-xen-fix-is_disconnected_device-exists_disconnected_device.patch
ApplyPatch linux-2.6-xen-improvement-to-wait_for_devices.patch
ApplyPatch linux-2.6-xen-increase-device-connection-timeout.patch
ApplyPatch linux-2.6-virtio_blk-add-support-for-cache-flush.patch

# Fix block I/O errors in KVM
ApplyPatch linux-2.6-block-silently-error-unsupported-empty-barriers-too.patch

ApplyPatch linux-2.6-e1000-ich9.patch

# Nouveau DRM + drm fixes
ApplyPatch kms-offb-handoff.patch
ApplyPatch drm-next-b390f944.patch
ApplyPatch drm-radeon-misc-fixes.patch
ApplyPatch drm-radeon-rv410-test-fix.patch
ApplyPatch drm-conservative-fallback-modes.patch
ApplyPatch drm-edid-retry.patch
ApplyPatch drm-edid-header-fixup.patch
ApplyPatch drm-default-mode.patch

ApplyPatch drm-nouveau.patch
# pm broken on my thinkpad t60p - airlied
#ApplyPatch drm-radeon-pm.patch
ApplyPatch drm-i915-resume-force-mode.patch
ApplyOptionalPatch drm-intel-next.patch
#this appears to be upstream - mjg59?
#ApplyPatch drm-intel-pm.patch
ApplyPatch drm-intel-no-tv-hotplug.patch
ApplyPatch drm-i915-fix-tvmode-oops.patch
ApplyPatch drm-i915-fix-sync-to-vbl-when-vga-is-off.patch
#ApplyPatch drm-disable-r600-aspm.patch

# VGA arb + drm
ApplyPatch linux-2.6-vga-arb.patch
ApplyPatch drm-vga-arb.patch
ApplyPatch drm-radeon-kms-arbiter-return-ignore.patch

# Lower debug level of fbcon handover messages (rh#538526)
ApplyPatch fbcon-lower-debug.patch

# linux1394 git patches
# apply if non-empty
ApplyOptionalPatch linux-2.6-firewire-git-update.patch
ApplyOptionalPatch linux-2.6-firewire-git-pending.patch

# silence the ACPI blacklist code
ApplyPatch linux-2.6-silence-acpi-blacklist.patch

# V4L/DVB updates/fixes/experimental drivers
# apply if non-empty
ApplyOptionalPatch linux-2.6-v4l-dvb-fixes.patch
ApplyOptionalPatch linux-2.6-v4l-dvb-update.patch
ApplyOptionalPatch linux-2.6-v4l-dvb-experimental.patch

ApplyPatch v4l-dvb-fix-cx25840-firmware-loading.patch

# Patches headed upstream
ApplyPatch linux-2.6-rtc-show-hctosys.patch
ApplyPatch linux-2.6-rfkill-all.patch
ApplyPatch linux-2.6-selinux-module-load-perms.patch

# patches headed for -stable

# make perf counter API available to userspace (#527264)
ApplyPatch perf-make-perf-counter-h-available-to-userspace.patch

ApplyPatch improve-resource-counter-scalability.patch

# fix perf for sysprof
ApplyPatch perf-events-fix-swevent-hrtimer-sampling.patch
ApplyPatch perf-events-dont-generate-events-for-the-idle-task.patch

# Fix oops in padlock
ApplyPatch crypto-via-padlock-fix-nano-aes.patch

# tg3 fixes (#527209)
ApplyPatch tg3-01-delay-mdio-bus-init-until-fw-finishes.patch
ApplyPatch tg3-02-fix-tso-test-against-wrong-flags-var.patch
ApplyPatch tg3-03-fix-57780-asic-rev-pcie-link-receiver-errors.patch
ApplyPatch tg3-04-prevent-tx-bd-corruption.patch
ApplyPatch tg3-05-assign-flags-to-fixes-in-start_xmit_dma_bug.patch
ApplyPatch tg3-06-fix-5906-transmit-hangs.patch

# sched fixes cherry-picked from 2.6.32
ApplyPatch sched-deal-with-low-load-in-wake-affine.patch
ApplyPatch sched-ensure-child-cant-gain-time-over-its-parent-after-fork.patch
ApplyPatch sched-remove-shortcut-from-select-task-rq-fair.patch
# latency defaults from 2.6.32
ApplyPatch sched-retune-scheduler-latency-defaults.patch
# fix wakeup latency
ApplyPatch sched-update-the-clock-of-runqueue-select-task-rq-selected.patch

ApplyPatch highmem-Fix-debug_kmap_atomic-to-also-handle-KM_IRQ_.patch
ApplyPatch highmem-Fix-race-in-debug_kmap_atomic-which-could-ca.patch
ApplyPatch highmem-fix-arm-powerpc-kmap_types.patch

ApplyPatch dlm-fix-connection-close-handling.patch

# rhbz#544144
ApplyPatch ipv4-fix-null-ptr-deref-in-ip_fragment.patch

# END OF PATCH APPLICATIONS

%endif

# Any further pre-build tree manipulations happen here.

chmod +x scripts/checkpatch.pl

# only deal with configs if we are going to build for the arch
%ifnarch %nobuildarches

mkdir configs

# Remove configs not for the buildarch
for cfg in kernel-%{version}-*.config; do
  if [ `echo %{all_arch_configs} | grep -c $cfg` -eq 0 ]; then
    rm -f $cfg
  fi
done

%if !%{debugbuildsenabled}
rm -f kernel-%{version}-*debug.config
%endif

# now run oldconfig over all the config files
for i in *.config
do
  mv $i .config
  Arch=`head -1 .config | cut -b 3-`
###-vs- ignore the warnings, due to IPV6 being set to 'm'
  make ARCH=$Arch %{oldconfig_target} || :
  echo "# $Arch" > configs/$i
  cat .config >> configs/$i
done
# end of kernel config
%endif

# get rid of unwanted files resulting from patch fuzz
find . \( -name "*.orig" -o -name "*~" \) -exec rm -f {} \; >/dev/null

cd ..

###
### build
###
%build

%if %{with_sparse}
%define sparse_mflags	C=1
%endif

%if %{fancy_debuginfo}
# This override tweaks the kernel makefiles so that we run debugedit on an
# object before embedding it.  When we later run find-debuginfo.sh, it will
# run debugedit again.  The edits it does change the build ID bits embedded
# in the stripped object, but repeating debugedit is a no-op.  We do it
# beforehand to get the proper final build ID bits into the embedded image.
# This affects the vDSO images in vmlinux, and the vmlinux image in bzImage.
export AFTER_LINK=\
'sh -xc "/usr/lib/rpm/debugedit -b $$RPM_BUILD_DIR -d /usr/src/debug -i $@"'
%endif

cp_vmlinux()
{
  eu-strip --remove-comment -o "$2" "$1"
}

BuildKernel() {
    MakeTarget=$1
    KernelImage=$2
    Flavour=$3
    InstallName=${4:-vmlinuz}

    # Pick the right config file for the kernel we're building
    Config=kernel-%{version}-%{_target_cpu}${Flavour:+-${Flavour}}.config
    DevelDir=/usr/src/kernels/%{KVERREL}${Flavour:+.${Flavour}}

    # When the bootable image is just the ELF kernel, strip it.
    # We already copy the unstripped file into the debuginfo package.
    if [ "$KernelImage" = vmlinux ]; then
      CopyKernel=cp_vmlinux
    else
      CopyKernel=cp
    fi

    KernelVer=%{version}-%{release}.%{_target_cpu}${Flavour:+.${Flavour}}
    echo BUILDING A KERNEL FOR ${Flavour} %{_target_cpu}...

    # make sure EXTRAVERSION says what we want it to say
    perl -p -i -e "s/^EXTRAVERSION.*/EXTRAVERSION = %{?stablerev}-%{release}.%{_target_cpu}${Flavour:+.${Flavour}}/" Makefile

    # if pre-rc1 devel kernel, must fix up SUBLEVEL for our versioning scheme
    %if !0%{?rcrev}
    %if 0%{?gitrev}
    perl -p -i -e 's/^SUBLEVEL.*/SUBLEVEL = %{upstream_sublevel}/' Makefile
    %endif
    %endif

    # and now to start the build process

    make -s mrproper
    cp configs/$Config .config

    Arch=`head -1 .config | cut -b 3-`
    echo USING ARCH=$Arch

    make -s ARCH=$Arch %{oldconfig_target} > /dev/null
    make -s ARCH=$Arch V=1 %{?_smp_mflags} $MakeTarget %{?sparse_mflags}
    make -s ARCH=$Arch V=1 %{?_smp_mflags} modules %{?sparse_mflags} || exit 1

%if %{with_perftool}
    pushd tools/perf
# make sure the scripts are executable... won't be in tarball until 2.6.31 :/
    chmod +x util/generate-cmdlist.sh util/PERF-VERSION-GEN
    make -s V=1 %{?_smp_mflags} perf
    mkdir -p $RPM_BUILD_ROOT/usr/libexec/
    install -m 755 perf $RPM_BUILD_ROOT/usr/libexec/perf.$KernelVer
    popd
%endif

    # Start installing the results
%if %{with_debuginfo}
    mkdir -p $RPM_BUILD_ROOT%{debuginfodir}/boot
    mkdir -p $RPM_BUILD_ROOT%{debuginfodir}/%{image_install_path}
%endif
    mkdir -p $RPM_BUILD_ROOT/%{image_install_path}
    install -m 644 .config $RPM_BUILD_ROOT/boot/config-$KernelVer
    install -m 644 System.map $RPM_BUILD_ROOT/boot/System.map-$KernelVer
%if %{with_dracut}
    # We estimate the size of the initramfs because rpm needs to take this size
    # into consideration when performing disk space calculations. (See bz #530778)
    dd if=/dev/zero of=$RPM_BUILD_ROOT/boot/initramfs-$KernelVer.img bs=1M count=20
%else
    dd if=/dev/zero of=$RPM_BUILD_ROOT/boot/initrd-$KernelVer.img bs=1M count=5
%endif
    if [ -f arch/$Arch/boot/zImage.stub ]; then
      cp arch/$Arch/boot/zImage.stub $RPM_BUILD_ROOT/%{image_install_path}/zImage.stub-$KernelVer || :
    fi
    $CopyKernel $KernelImage \
    		$RPM_BUILD_ROOT/%{image_install_path}/$InstallName-$KernelVer
    chmod 755 $RPM_BUILD_ROOT/%{image_install_path}/$InstallName-$KernelVer

    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer
    # Override $(mod-fw) because we don't want it to install any firmware
    # We'll do that ourselves with 'make firmware_install'
    make -s ARCH=$Arch INSTALL_MOD_PATH=$RPM_BUILD_ROOT modules_install KERNELRELEASE=$KernelVer mod-fw=
%ifarch %{vdso_arches}
    make -s ARCH=$Arch INSTALL_MOD_PATH=$RPM_BUILD_ROOT vdso_install KERNELRELEASE=$KernelVer
    if grep '^CONFIG_XEN=y$' .config >/dev/null; then
      echo > ldconfig-kernel.conf "\
# This directive teaches ldconfig to search in nosegneg subdirectories
# and cache the DSOs there with extra bit 0 set in their hwcap match
# fields.  In Xen guest kernels, the vDSO tells the dynamic linker to
# search in nosegneg subdirectories and to match this extra hwcap bit
# in the ld.so.cache file.
hwcap 0 nosegneg"
    fi
    if [ ! -s ldconfig-kernel.conf ]; then
      echo > ldconfig-kernel.conf "\
# Placeholder file, no vDSO hwcap entries used in this kernel."
    fi
    %{__install} -D -m 444 ldconfig-kernel.conf \
        $RPM_BUILD_ROOT/etc/ld.so.conf.d/kernel-$KernelVer.conf
%endif

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
    # dirs for additional modules per module-init-tools, kbuild/modules.txt
    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/extra
    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/updates
    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/weak-updates
    # first copy everything
    cp --parents `find  -type f -name "Makefile*" -o -name "Kconfig*"` $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    cp Module.symvers $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    cp System.map $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    if [ -s Module.markers ]; then
      cp Module.markers $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    fi
    # then drop all but the needed Makefiles/Kconfig files
    rm -rf $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/Documentation
    rm -rf $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/scripts
    rm -rf $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    cp .config $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    cp -a scripts $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    if [ -d arch/$Arch/scripts ]; then
      cp -a arch/$Arch/scripts $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/arch/%{_arch} || :
    fi
    if [ -f arch/$Arch/*lds ]; then
      cp -a arch/$Arch/*lds $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/arch/%{_arch}/ || :
    fi
    rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/scripts/*.o
    rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/scripts/*/*.o
%ifarch ppc
    cp -a --parents arch/powerpc/lib/crtsavres.[So] $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/
%endif
    if [ -d arch/%{asmarch}/include ]; then
      cp -a --parents arch/%{asmarch}/include $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/
    fi
    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    cd include
    cp -a acpi config crypto keys linux math-emu media mtd net pcmcia rdma rxrpc scsi sound trace video drm asm-generic $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    asmdir=$(readlink asm)
    cp -a $asmdir $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include/
    pushd $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    ln -s $asmdir asm
    popd
    # Make sure the Makefile and version.h have a matching timestamp so that
    # external modules can be built
    touch -r $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/Makefile $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include/linux/version.h
    touch -r $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/.config $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include/linux/autoconf.h
    # Copy .config to include/config/auto.conf so "make prepare" is unnecessary.
    cp $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/.config $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include/config/auto.conf
    cd ..

    #
    # save the vmlinux file for kernel debugging into the kernel-debuginfo rpm
    #
%if %{with_debuginfo}
    mkdir -p $RPM_BUILD_ROOT%{debuginfodir}/lib/modules/$KernelVer
    cp vmlinux $RPM_BUILD_ROOT%{debuginfodir}/lib/modules/$KernelVer
%endif

    find $RPM_BUILD_ROOT/lib/modules/$KernelVer -name "*.ko" -type f >modnames

    # mark modules executable so that strip-to-file can strip them
    xargs --no-run-if-empty chmod u+x < modnames

    # Generate a list of modules for block and networking.

    fgrep /drivers/ modnames | xargs --no-run-if-empty nm -upA |
    sed -n 's,^.*/\([^/]*\.ko\):  *U \(.*\)$,\1 \2,p' > drivers.undef

    collect_modules_list()
    {
      sed -r -n -e "s/^([^ ]+) \\.?($2)\$/\\1/p" drivers.undef |
      LC_ALL=C sort -u > $RPM_BUILD_ROOT/lib/modules/$KernelVer/modules.$1
    }

    collect_modules_list networking \
    			 'register_netdev|ieee80211_register_hw|usbnet_probe'
    collect_modules_list block \
    			 'ata_scsi_ioctl|scsi_add_host|blk_init_queue|register_mtd_blktrans|scsi_esp_register|scsi_register_device_handler'
    collect_modules_list drm \
    			 'drm_open|drm_init'
    collect_modules_list modesetting \
    			 'drm_crtc_init'

    # detect missing or incorrect license tags
    rm -f modinfo
    while read i
    do
      echo -n "${i#$RPM_BUILD_ROOT/lib/modules/$KernelVer/} " >> modinfo
      /sbin/modinfo -l $i >> modinfo
    done < modnames

    egrep -v \
    	  'GPL( v2)?$|Dual BSD/GPL$|Dual MPL/GPL$|GPL and additional rights$' \
	  modinfo && exit 1

    rm -f modinfo modnames

    # remove files that will be auto generated by depmod at rpm -i time
    for i in alias alias.bin ccwmap dep dep.bin ieee1394map inputmap isapnpmap ofmap pcimap seriomap symbols symbols.bin usbmap
    do
      rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/modules.$i
    done

    # Move the devel headers out of the root file system
    mkdir -p $RPM_BUILD_ROOT/usr/src/kernels
    mv $RPM_BUILD_ROOT/lib/modules/$KernelVer/build $RPM_BUILD_ROOT/$DevelDir
    ln -sf ../../..$DevelDir $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
}

###
# DO it...
###

# prepare directories
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/boot

cd linux-%{kversion}.%{_target_cpu}

%if %{with_debug}
BuildKernel %make_target %kernel_image debug
%endif

%if %{with_pae_debug}
BuildKernel %make_target %kernel_image PAEdebug
%endif

%if %{with_pae}
BuildKernel %make_target %kernel_image PAE
%endif

%if %{with_up}
BuildKernel %make_target %kernel_image
%endif

%if %{with_smp}
BuildKernel %make_target %kernel_image smp
%endif

%if %{with_kdump}
BuildKernel vmlinux vmlinux kdump vmlinux
%endif

%if %{with_doc}
# Make the HTML and man pages.
# XXX nix %{?_smp_mflags} here, buggy Documentation/*/Makefile!
make htmldocs mandocs || %{doc_build_fail}

# sometimes non-world-readable files sneak into the kernel source tree
chmod -R a=rX Documentation
find Documentation -type d | xargs chmod u+w
%endif

%if %{with_perf}
pushd tools/perf
make %{?_smp_mflags} man || %{doc_build_fail}
popd
%endif

###
### Special hacks for debuginfo subpackages.
###

# This macro is used by %%install, so we must redefine it before that.
%define debug_package %{nil}

%if %{fancy_debuginfo}
%define __debug_install_post \
  /usr/lib/rpm/find-debuginfo.sh %{debuginfo_args} %{_builddir}/%{?buildsubdir}\
%{nil}
%endif

%if %{with_debuginfo}
%ifnarch noarch
%global __debug_package 1
%files -f debugfiles.list debuginfo-common-%{_target_cpu}
%defattr(-,root,root)
%endif
%endif

###
### install
###

%install

cd linux-%{kversion}.%{_target_cpu}

%if %{with_doc}
docdir=$RPM_BUILD_ROOT%{_datadir}/doc/kernel-doc-%{rpmversion}
man9dir=$RPM_BUILD_ROOT%{_datadir}/man/man9

# copy the source over
mkdir -p $docdir
tar -f - --exclude=man --exclude='.*' -c Documentation | tar xf - -C $docdir

# Install man pages for the kernel API.
mkdir -p $man9dir
find Documentation/DocBook/man -name '*.9.gz' -print0 |
xargs -0 --no-run-if-empty %{__install} -m 444 -t $man9dir $m
ls $man9dir | grep -q '' || > $man9dir/BROKEN
%endif # with_doc

# perf docs
%if %{with_perf}
mandir=$RPM_BUILD_ROOT%{_datadir}/man
man1dir=$mandir/man1
pushd tools/perf/Documentation
make install-man mandir=$mandir
popd

pushd $man1dir
for d in *.1; do
 gzip $d;
done
popd
%endif # with_perf

# perf shell wrapper
%if %{with_perf}
mkdir -p $RPM_BUILD_ROOT/usr/sbin/
cp $RPM_SOURCE_DIR/perf $RPM_BUILD_ROOT/usr/sbin/perf
chmod 0755 $RPM_BUILD_ROOT/usr/sbin/perf
mkdir -p $RPM_BUILD_ROOT%{_datadir}/doc/perf
%endif

%if %{with_headers}
# Install kernel headers
make ARCH=%{hdrarch} INSTALL_HDR_PATH=$RPM_BUILD_ROOT/usr headers_install

# Do headers_check but don't die if it fails.
make ARCH=%{hdrarch} INSTALL_HDR_PATH=$RPM_BUILD_ROOT/usr headers_check \
     > hdrwarnings.txt || :
if grep -q exist hdrwarnings.txt; then
   sed s:^$RPM_BUILD_ROOT/usr/include/:: hdrwarnings.txt
   # Temporarily cause a build failure if header inconsistencies.
   # exit 1
fi

find $RPM_BUILD_ROOT/usr/include \
     \( -name .install -o -name .check -o \
     	-name ..install.cmd -o -name ..check.cmd \) | xargs rm -f

# glibc provides scsi headers for itself, for now
rm -rf $RPM_BUILD_ROOT/usr/include/scsi
rm -f $RPM_BUILD_ROOT/usr/include/asm*/atomic.h
rm -f $RPM_BUILD_ROOT/usr/include/asm*/io.h
rm -f $RPM_BUILD_ROOT/usr/include/asm*/irq.h
%endif

%if %{with_firmware}
%{build_firmware}
%endif

%if %{with_bootwrapper}
make DESTDIR=$RPM_BUILD_ROOT bootwrapper_install WRAPPER_OBJDIR=%{_libdir}/kernel-wrapper WRAPPER_DTSDIR=%{_libdir}/kernel-wrapper/dts
%endif


###
### clean
###

%clean
rm -rf $RPM_BUILD_ROOT

###
### scripts
###

#
# This macro defines a %%post script for a kernel*-devel package.
#	%%kernel_devel_post [<subpackage>]
#
%define kernel_devel_post() \
%{expand:%%post %{?1:%{1}-}devel}\
if [ -f /etc/sysconfig/kernel ]\
then\
    . /etc/sysconfig/kernel || exit $?\
fi\
if [ "$HARDLINK" != "no" -a -x /usr/sbin/hardlink ]\
then\
    (cd /usr/src/kernels/%{KVERREL}%{?1:.%{1}} &&\
     /usr/bin/find . -type f | while read f; do\
       hardlink -c /usr/src/kernels/*.fc*.*/$f $f\
     done)\
fi\
%{nil}

# This macro defines a %%posttrans script for a kernel package.
#	%%kernel_variant_posttrans [<subpackage>]
# More text can follow to go at the end of this variant's %%post.
#
%define kernel_variant_posttrans() \
%{expand:%%posttrans %{?1}}\
/sbin/new-kernel-pkg --package kernel%{?1:-%{1}} --rpmposttrans %{KVERREL}%{?1:.%{1}} || exit $?\
%{nil}

#
# This macro defines a %%post script for a kernel package and its devel package.
#	%%kernel_variant_post [-v <subpackage>] [-r <replace>]
# More text can follow to go at the end of this variant's %%post.
#
%define kernel_variant_post(v:r:) \
%{expand:%%kernel_devel_post %{?-v*}}\
%{expand:%%kernel_variant_posttrans %{?-v*}}\
%{expand:%%post %{?-v*}}\
%{-r:\
if [ `uname -i` == "x86_64" -o `uname -i` == "i386" ] &&\
   [ -f /etc/sysconfig/kernel ]; then\
  /bin/sed -r -i -e 's/^DEFAULTKERNEL=%{-r*}$/DEFAULTKERNEL=kernel%{?-v:-%{-v*}}/' /etc/sysconfig/kernel || exit $?\
fi}\
%{expand:\
%if %{with_dracut}\
/sbin/new-kernel-pkg --package kernel%{?-v:-%{-v*}} --mkinitrd --dracut --depmod --install %{KVERREL}%{?-v:.%{-v*}} || exit $?\
%else\
/sbin/new-kernel-pkg --package kernel%{?-v:-%{-v*}} --mkinitrd --depmod --install %{KVERREL}%{?-v:.%{-v*}} || exit $?\
%endif}\
#if [ -x /sbin/weak-modules ]\
#then\
#    /sbin/weak-modules --add-kernel %{KVERREL}%{?-v*} || exit $?\
#fi\
%{nil}

#
# This macro defines a %%preun script for a kernel package.
#	%%kernel_variant_preun <subpackage>
#
%define kernel_variant_preun() \
%{expand:%%preun %{?1}}\
/sbin/new-kernel-pkg --rminitrd --rmmoddep --remove %{KVERREL}%{?1:.%{1}} || exit $?\
#if [ -x /sbin/weak-modules ]\
#then\
#    /sbin/weak-modules --remove-kernel %{KVERREL}%{?1} || exit $?\
#fi\
%{nil}

%kernel_variant_preun
%ifarch x86_64
%kernel_variant_post -r (kernel-smp|kernel-xen)
%else
%kernel_variant_post -r kernel-smp
%endif

%kernel_variant_preun smp
%kernel_variant_post -v smp

%kernel_variant_preun PAE
%kernel_variant_post -v PAE -r (kernel|kernel-smp|kernel-xen)

%kernel_variant_preun debug
%kernel_variant_post -v debug

%kernel_variant_post -v PAEdebug -r (kernel|kernel-smp|kernel-xen)
%kernel_variant_preun PAEdebug

if [ -x /sbin/ldconfig ]
then
    /sbin/ldconfig -X || exit $?
fi

###
### file lists
###

%if %{with_headers}
%files headers
%defattr(-,root,root)
/usr/include/*
%endif

%if %{with_firmware}
%files firmware
%defattr(-,root,root)
/lib/firmware/*
%doc linux-%{kversion}.%{_target_cpu}/firmware/WHENCE
%endif

%if %{with_bootwrapper}
%files bootwrapper
%defattr(-,root,root)
/usr/sbin/*
%{_libdir}/kernel-wrapper
%endif

# only some architecture builds need kernel-doc
%if %{with_doc}
%files doc
%defattr(-,root,root)
%{_datadir}/doc/kernel-doc-%{rpmversion}/Documentation/*
%dir %{_datadir}/doc/kernel-doc-%{rpmversion}/Documentation
%dir %{_datadir}/doc/kernel-doc-%{rpmversion}
%{_datadir}/man/man9/*
%endif

%if %{with_perf}
%files -n perf
%defattr(-,root,root)
%{_datadir}/doc/perf
/usr/sbin/perf
%{_datadir}/man/man1/*
%endif

# This is %{image_install_path} on an arch where that includes ELF files,
# or empty otherwise.
%define elf_image_install_path %{?kernel_image_elf:%{image_install_path}}

#
# This macro defines the %%files sections for a kernel package
# and its devel and debuginfo packages.
#	%%kernel_variant_files [-k vmlinux] <condition> <subpackage>
#
%define kernel_variant_files(k:) \
%if %{1}\
%{expand:%%files %{?2}}\
%defattr(-,root,root)\
/%{image_install_path}/%{?-k:%{-k*}}%{!?-k:vmlinuz}-%{KVERREL}%{?2:.%{2}}\
/boot/System.map-%{KVERREL}%{?2:.%{2}}\
%if %{with_perftool}\
/usr/libexec/perf.%{KVERREL}%{?2:.%{2}}\
%endif\
#/boot/symvers-%{KVERREL}%{?2:.%{2}}.gz\
/boot/config-%{KVERREL}%{?2:.%{2}}\
%dir /lib/modules/%{KVERREL}%{?2:.%{2}}\
/lib/modules/%{KVERREL}%{?2:.%{2}}/kernel\
/lib/modules/%{KVERREL}%{?2:.%{2}}/build\
/lib/modules/%{KVERREL}%{?2:.%{2}}/source\
/lib/modules/%{KVERREL}%{?2:.%{2}}/extra\
/lib/modules/%{KVERREL}%{?2:.%{2}}/updates\
/lib/modules/%{KVERREL}%{?2:.%{2}}/weak-updates\
%ifarch %{vdso_arches}\
/lib/modules/%{KVERREL}%{?2:.%{2}}/vdso\
/etc/ld.so.conf.d/kernel-%{KVERREL}%{?2:.%{2}}.conf\
%endif\
/lib/modules/%{KVERREL}%{?2:.%{2}}/modules.*\
%if %{with_dracut}\
/boot/initramfs-%{KVERREL}%{?2:.%{2}}.img\
%else\
/boot/initrd-%{KVERREL}%{?2:.%{2}}.img\
%endif\
%{expand:%%files %{?2:%{2}-}devel}\
%defattr(-,root,root)\
%dir /usr/src/kernels\
%verify(not mtime) /usr/src/kernels/%{KVERREL}%{?2:.%{2}}\
/usr/src/kernels/%{KVERREL}%{?2:.%{2}}\
%if %{with_debuginfo}\
%ifnarch noarch\
%if %{fancy_debuginfo}\
%{expand:%%files -f debuginfo%{?2}.list %{?2:%{2}-}debuginfo}\
%else\
%{expand:%%files %{?2:%{2}-}debuginfo}\
%endif\
%defattr(-,root,root)\
%if !%{fancy_debuginfo}\
%if "%{elf_image_install_path}" != ""\
%{debuginfodir}/%{elf_image_install_path}/*-%{KVERREL}%{?2:.%{2}}.debug\
%endif\
%{debuginfodir}/lib/modules/%{KVERREL}%{?2:.%{2}}\
%{debuginfodir}/usr/src/kernels/%{KVERREL}%{?2:.%{2}}\
%endif\
%endif\
%endif\
%endif\
%{nil}


%kernel_variant_files %{with_up}
%kernel_variant_files %{with_smp} smp
%kernel_variant_files %{with_debug} debug
%kernel_variant_files %{with_pae} PAE
%kernel_variant_files %{with_pae_debug} PAEdebug
%kernel_variant_files -k vmlinux %{with_kdump} kdump

# plz don't put in a version string unless you're going to tag
# and build.

%changelog
* Thu Dec 03 2009 Kyle McMartin <kyle@redhat.com> 2.6.31.6-162
- ipv4-fix-null-ptr-deref-in-ip_fragment.patch: null ptr deref
  bug fix.

* Thu Dec 03 2009 Dave Airlie <airlied@redhat.com> 2.6.31.6-161
- rv410 LVDS on resume test fix from AMD (#541562)

* Wed Dec 02 2009 John W. Linville <linville@redhat.com> 2.6.31.6-160
- ath9k: add fixes suggested by upstream maintainer

* Wed Dec 02 2009 Dave Airlie <airlied@redhat.com> 2.6.31.6-159
- drm-radeon-misc-fixes.patch: r400 LVDS, r600 digital dpms, cursor fix, tv property

* Wed Dec 02 2009 Ben Skeggs <bskeggs@redhat.com> 2.6.31.6-158
- nouveau: more complete lvds script selection on >=G80 (rh#522690, rh#529859)
- nouveau: more complete tmds script selection on >=G80 (rh#537853)
- nouveau: TV detection fixes

* Tue Dec 01 2009 Dave Airlie <airlied@redhat.com> 2.6.31.6-157
- div/0 fix harder (#540593) - also ignore unposted GPUs with no BIOS

* Tue Dec 01 2009 Dave Airlie <airlied@redhat.com> 2.6.31.6-156
- drm-next: fixes LVDS resume on r4xx, div/0 on no bios (#540593)
  lockup on tv-out only startup.

* Mon Nov 30 2009 Kyle McMartin <kyle@redhat.com>
- drm-i915-fix-sync-to-vbl-when-vga-is-off.patch: add (rhbz#541670)

* Sun Nov 29 2009 Kyle McMartin <kyle@redhat.com>
- Drop linux-2.6-sysrq-c.patch, made consistent upstream.

* Fri Nov 27 2009 Jarod Wilson <jarod@redhat.com> 2.6.31.6-153
- add device name to lirc_zilog, fixes issues w/multiple target devices
- add lirc_imon pure input mode support for onboard decode devices

* Wed Nov 26 2009 David Woodhouse <David.Woodhouse@intel.com> 2.6.31.6-152
- Fix intel_tv_mode_set oops (#540218)

* Wed Nov 26 2009 David Woodhouse <David.Woodhouse@intel.com> 2.6.31.6-151
- VT-d: Work around yet more HP BIOS brokenness (#536675)

* Wed Nov 25 2009 Kyle McMartin <kyle@redhat.com>
- dlm: fix connection close handling.
  Fix by lmb, requested by fabio.

* Wed Nov 25 2009 David Woodhouse <David.Woodhouse@intel.com> 2.6.31.6-149
- VT-d: Work around more HP BIOS brokenness.

* Tue Nov 24 2009 Dave Airlie <airlied@redhat.com> 2.6.31.6-148
- radeon: flush HDP cache on rendering wait - fixes r600 rendercheck failure

* Mon Nov 23 2009 Adam Jackson <ajax@redhat.com>
- drm-default-mode.patch: Default to 1024x768 to match UMS. (#538761)

* Mon Nov 23 2009 Roland McGrath <roland@redhat.com> 2.6.31.6-146
- Fix oops in x86-32 kernel's iret handling for bogus user %cs. (#540580)

* Fri Nov 21 2009 Kyle McMartin <kyle@redhat.com>
- Fix up ssp' highmem fixes with fixes for arm & ppc.

* Thu Nov 20 2009 Chris Wright <chrisw@redhat.com> 2.6.31.6-144
- VT-d: another fallback for another BIOS bug (#524808)

* Thu Nov 19 2009 Ben Skeggs <bskeggs@redhat.com> 2.6.31.6-142
- Oops, add new patch to spec file

* Thu Nov 19 2009 Ben Skeggs <bskeggs@redhat.com> 2.6.31.6-141
- Lower debug level of fbcon handover messages (rh#538526)

* Thu Nov 19 2009 Dave Airlie <airlied@redhat.com> 2.6.31.6-140
- drm-next-44c83571.patch: oops pulled the wrong tree into my f12 tree

* Thu Nov 19 2009 Ben Skeggs <bskeggs@redhat.com> 2.6.31.6-139
- nouveau: s/r fixes on chipsets using bios opcode 0x87
- nouveau: fixes to bios opcode 0x8e
- nouveau: hopefully fix nv1x context switching issues (rh#526577)
- nouveau: support for NVA5 (GeForce G220)
- nouveau: fixes for NVAA support

* Thu Nov 19 2009 Dave Airlie <airlied@redhat.com> 2.6.31.6-138
- drm-next-d56672a9.patch: fix some rn50 cloning issues

* Wed Nov 18 2009 David Woodhouse <David.Woodhouse@intel.com> 2.6.31.6-137
- Actually force the IOMMU not to be used when we detect the HP/Acer bug.

* Tue Nov 17 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31.6-136
- ACPI embedded controller fixes from Fedora 11.

* Tue Nov 17 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31.6-135
- Scheduler fixes and latency tuning patches from F-11.

* Tue Nov 17 2009 Dave Airlie <airlied@redhat.com> 2.6.31.6-134
- glad to see edid retry patch was compiled.

* Tue Nov 17 2009 Dave Airlie <airlied@redhat.com> 2.6.31.6-133
- drm-next-984d1f3c.patch: rebase with upstream fixes - drop all merged

* Thu Nov 12 2009 Adam Jackson <ajax@redhat.com>
- Actually apply the EDID retry patch
- drm-edid-header-fixup.patch: Fix up some broken EDID headers (#534120)

* Thu Nov 12 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31.6-130
- Use ApplyOptionalPatch for v4l and firewire updates.
- Drop unused v4l ABI fix.

* Thu Nov 12 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31.6-129
- Linux 2.6.31.6
- Drop merged patches:
  linux-2.6-iwlwifi-reduce-noise-when-skb-allocation-fails.patch
  linux-2.6-libertas-crash.patch
  pci-increase-alignment-to-make-more-space.patch
  acpi-revert-attach-device-to-handle-early.patch
  ahci-revert-restore-sb600-sata-controller-64-bit-dma.patch
  acpi-pci-fix-null-pointer-dereference-in-acpi-get-pci-dev.patch
  af_unix-fix-deadlock-connecting-to-shutdown-socket.patch
  keys-get_instantiation_keyring-should-inc-the-keyring-refcount.patch
  netlink-fix-typo-in-initialization.patch
  fs-pipe-null-ptr-deref-fix.patch

* Wed Nov 11 2009 Justin M. Forbes <jforbes@redhat.com> 2.6.31.5-128
- Fix KSM for i686 users. (#532215)
- Add KSM fixes from 2.6.32

* Sun Nov 08 2009 David Woodhouse <David.Woodhouse@intel.com> 2.6.31.5-127
- Apply fix for fallback when HP/Acer BIOS bug detected (#524808)
- Re-enable DMAR.
- Fix libertas crash due to skb pointer bug

* Sat Nov 07 2009 Kyle McMartin <kyle@redhat.com> 2.6.31.5-126
- Re-enable linux-2.6-die-closed-source-bios-muppets-die.patch, DMAR
  still defaulting to off.

* Sat Nov 07 2009 Kyle McMartin <kyle@redhat.com> 2.6.31.5-125
- Disable linux-2.6-die-closed-source-bios-muppets-die.patch and
  default DMAR to off (can be re-enabled with intel_iommu=on on the
  command line due to last minute issues and reversion upstream.)

* Thu Nov 05 2009 Jarod Wilson <jarod@redhat.com>
- Add --with dbgonly rpmbuild option to build only debug kernels

* Thu Nov 05 2009 Dave Airlie <airlied@redhat.com> 2.6.31.5-122
- comment out kmap atomic for now, it breaks ppc build

* Thu Nov 05 2009 Dave Airlie <airlied@redhat.com> 2.6.31.5-121
- drm-radeon-fix-agp-resume.patch (#531825)

* Thu Nov 05 2009 Kyle McMartin <kyle@redhat.com>
- Add two patches from Soren from mingo/linux-2.6-x86.git to fix
  debug_kmap_atomic prints.

* Thu Nov 05 2009 Ben Skeggs <bskeggs@redhat.com>
- nouveau: fix rh#532924

* Wed Nov 04 2009 Kyle McMartin <kyle@redhat.com>
- Make JBD2_DEBUG a toggleable debug setting. Leave it the way it was.
  (Double checked resulting configs, don't fret.)

* Wed Nov 04 2009 Adam Jackson <ajax@redhat.com> 2.6.31.5-117
- drm-edid-retry.patch: Try DDC up to four times, like X. (#532957)

* Wed Nov 04 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31.5-116
- tg3 bug fixes (#527209)

* Wed Nov 04 2009 Kyle McMartin <kyle@redhat.com> 2.6.31.5-115
- fs/pipe.c: fix null pointer dereference (CVE-2009-3547)

* Wed Nov 04 2009 Ben Skeggs <bskeggs@redhat.com> 2.6.31.5-114
- nouveau: provide info userspace needs to handle low memory situations
- nouveau: fix for rh#532711
- nouveau: add option to provide more debug info for rh#532579
- patch only so large because of included register rename

* Tue Nov 03 2009 Adam Jackson <ajax@redhat.com> 2.6.31.5-113
- drm-conservative-fallback-modes.patch: When an output is connected but
  fails EDID, only add modes with refresh rates <= 60 (#514600)

* Tue Nov 03 2009 Dave Airlie <airlied@redhat.com> 2.6.31.5-112
- drm-r600-lenovo-w500-fix.patch: add second patch from upstream fix

* Tue Nov 03 2009 Dave Airlie <airlied@redhat.com> 2.6.31.5-111
- drm-r600-lenovo-w500-fix.patch: fix lenovo w500 acpi video kill laptop dead
- drop aspm r600 patch as correct fix should be in 110

* Tue Nov 03 2009 Dave Airlie <airlied@redhat.com> 2.6.31.5-110
- r600: fix for ring setup RMW issue.

* Mon Nov 02 2009 John W. Linville <linville@redhat.com> 2.6.31.5-109
- prism54: remove pci modinfo device table (#447047)

* Mon Nov 02 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31.5-108
- Enable acerhdf driver for fan speed control on Acer Aspire One notebook (#532463)

* Mon Nov 02 2009 Dave Airlie <airlied@redhat.com> 2.6.31.5-107
- r600: back that out, thanks to yaneti for testing.

* Mon Nov 02 2009 Dave Airlie <airlied@redhat.com> 2.6.31.5-106
- r600: ring size guesswork fix.

* Fri Oct 30 2009 Dave Airlie <airlied@redhat.com> 2.6.31.5-105
- drm-radeon-agp-font-fix.patch: hopefully fix AGP coherency issue

* Wed Oct 28 2009 Dave Airlie <airlied@redhat.com> 2.6.31.5-104
- drm-next-ea1495a6.patch: fix rs400 resume on my test box

* Wed Oct 28 2009 Dave Airlie <airlied@redhat.com> 2.6.31.5-103
- drm-next-fc7f7119.patch: fix oops in SS code, fix multi-card, dvo.
- drm-radeon-kms-arbiter-return-ignore.patch: fix arbiter for non-VGA display

* Tue Oct 27 2009 Chuck Ebbert <cebbert@redhat.com>
- Fix oops in VIA padlock-aes code.

* Tue Oct 27 2009 Dave Airlie <airlied@redhat.com>
- kms: add offb handoff patch for ppc to work

* Tue Oct 27 2009 Ben Skeggs <bskeggs@redhat.com>
- drm-nouveau.patch: misc fixes, very initial NVA8 work

* Tue Oct 27 2009 Dave Airlie <airlied@redhat.com>
- fix dd command lines

* Mon Oct 26 2009 Dave Jones <davej@redhat.com>
- Make a 20MB initramfs file so rpm gets its diskspace calculations right. (#530778)

* Mon Oct 26 2009 Dave Airlie <airlied@redhat.com> 2.6.31.5-97
- drm: rebase to drm-next, drop palette fix, merged upstream
- drm-intel-big-hammer.patch: drop, proper fix in 2.6.31.5
- drm-disable-r600-aspm.patch: test patch to disable aspm on r600/r700 for now

* Fri Oct 23 2009 Kyle McMartin <kyle@redhat.com> 2.6.31.5-96
- Bump NR_CPUS to 256 on x86_64.
- Add two backports (ugh, just had to go renaming perf counters to events...)
  for fixing sysprof with perf.

* Fri Oct 23 2009 Dave Airlie <airlied@redhat.com> 2.6.31.5-95
- re enable MSI

* Fri Oct 23 2009 Dave Airlie <airlied@redhat.com> 2.6.31.5-94
- disable debug + stackprotector

* Fri Oct 23 2009 Chuck Ebbert <cebbert@redhat.com>
- Linux 2.6.31.5

* Thu Oct 22 2009 Chuck Ebbert <cebbert@redhat.com>
- Fix exploitable OOPS in keyring code. (CVE-2009-3624)
- Fix kernel memory leak to userspace. (CVE-2009-3612)

* Thu Oct 22 2009 Dave Airlie <airlied@redhat.com>  2.6.31.5-91.rc1
- kms: fix palette

* Wed Oct 21 2009 Chuck Ebbert <cebbert@redhat.com>
- Disable powersave by default for AC97 audio devices. (#524414)

* Wed Oct 21 2009 Chuck Ebbert <cebbert@redhat.com>
- Linux 2.6.31.5-rc1
- Remove the merged HP DC7900 workaround from iommu-updates patch.
- Drop merged patch:
  linux-2.6-raidlockdep.patch

* Mon Oct 19 2009 Kyle McMartin <kyle@redhat.com>
- af_unix-fix-deadlock-connecting-to-shutdown-socket.patch: fix for
  rhbz#529626.

* Sat Oct 17 2009 Chuck Ebbert <cebbert@redhat.com>
- Replace linux-2.6-bluetooth-autosuspend.diff with upstream version.

* Fri Oct 16 2009 Josef Bacik <josef@toxicpanda.com>
- Update btrfs to latest upstream

* Fri Oct 16 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31.4-85
- Fix another ACPI boot hang (#513680)

* Fri Oct 16 2009 Ben Skeggs <bskeggs@redhat.com> 2.6.31.4-84
- nouveau: more vbios opcodes, minor fixes, hopeful fix for rh#529292

* Wed Oct 14 2009 Roland McGrath <roland@redhat.com> 2.6.31.4-83
- Remove work-around for gcc bug #521991, now fixed.
- Build *docs non-parallel, working around kernel's makefile bugs.

* Wed Oct 14 2009 Peter Jones <pjones@redhat.com>
- Add scsi_register_device_handler to modules.block's symbol list so
  we'll have scsi device handlers in installer images.

* Tue Oct 13 2009 Steve Dickson <steved@redhat.com> 2.6.31.4-81
- Fixed hang during NFS installs (bz 528537)

* Tue Oct 13 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31.4-80
- Disable 64-bit DMA on SB600 SATA controllers.

* Tue Oct 13 2009 Kyle McMartin <kyle@redhat.com>
- Always build perf docs, regardless of whether we build kernel-doc.
  Seems rather unfair to not ship the manpages half the time.
  Also, drop BuildRequires %if when not with_doc, the rules about %if
  there are f*!&^ing complicated.

* Mon Oct 12 2009 Kyle McMartin <kyle@redhat.com>
- Build the perf manpages properly.

* Mon Oct 12 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31.4-77
- Fix boot hang with ACPI on some systems.

* Mon Oct 12 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31.4-76
- Linux 2.6.31.4

* Mon Oct 12 2009 Kyle McMartin <kyle@redhat.com> 2.6.31.4-75.rc2
- improve-resource-counter-scalability.patch: Fix scalability issues
  on big machines, requested by prarit.

* Mon Oct 12 2009 Jarod Wilson <jarod@redhat.com>
- Fix irq status check bugs in lirc_ene0100

* Mon Oct 12 2009 Chuck Ebbert <cebbert@redhat.com>
- Fix 2.6.31 regression that caused device failures with ACPI enabled.

* Sun Oct 11 2009 Chuck Ebbert <cebbert@redhat.com>
- Linux 2.6.31.4-rc2
- Drop merged patch: linux-2.6-frace-fixes.patch

* Sat Oct 10 2009 Chuck Ebbert <cebbert@redhat.com>
- Make performance counter API available to userspace programs (#527264)

* Sat Oct 10 2009 Dave Jones <davej@redhat.com>
- Drop the NX kernel data patch for now. Causes no-boot on some systems.

* Fri Oct 09 2009 Dave Jones <davej@redhat.com>
- Backport two critical ftrace fixes.
  ftrace: check for failure for all conversions
  tracing: correct module boundaries for ftrace_release

* Fri Oct 09 2009 Jarod Wilson <jarod@redhat.com>
- Build docs sub-package again

* Thu Oct 08 2009 Kyle McMartin <kyle@redhat.com> 2.6.31.3-67
- Linux 2.6.31.3
- rebase drm-next trivially.
- dropped merged upstream patches,
 - linux-2.6-fix-usb-serial-autosuspend.diff
 - linux-2.6-iwlagn-modify-digital-SVR-for-1000.patch
 - linux-2.6-iwlwifi-Handle-new-firmware-file-with-ucode-build-number-in-header.patch
 - linux-2.6-iwlwifi-fix-debugfs-buffer-handling.patch
 - linux-2.6-iwlwifi-fix-unloading-driver-while-scanning.patch
 - linux-2.6-iwlwifi-remove-deprecated-6000-series-adapters.patch
 - linux-2.6-iwlwifi-traverse-linklist-to-find-the-valid-OTP-block.patch
 - linux-2.6-iwlwifi-update-1000-series-API-version-to-match-firmware.patch
 - linux-2.6-xen-check-efer-fix.patch
 - linux-2.6-xen-spinlock-enable-interrupts-only-when-blocking.patch
 - linux-2.6-xen-spinlock-stronger-barrier.patch
 - linux-2.6-xen-stack-protector-fix.patch
 - linux-2.6.31-cpufreq-powernow-k8-oops.patch

* Thu Oct 08 2009 Ben Skeggs <bskeggs@redhat.com>
- ppc: compile nvidiafb as a module only, nvidiafb+nouveau = bang! (rh#491308)

* Thu Oct 08 2009 Ben Skeggs <bskeggs@redhat.com> 2.6.31.1-65
- nouveau: {drm-next,context,fbcon,misc} fixes, connector forcing

* Thu Oct 08 2009 Dave Airlie <airlied@redhat.com> 2.6.31.1-64
- rebase latest drm-next, fixes many s/r and r600 problems

* Wed Oct 07 2009 Dave Jones <davej@redhat.com>
- Don't mark the initramfs file as a ghost.

* Wed Oct 07 2009 Dave Jones <davej@redhat.com>
- Enable FUNCTION_GRAPH_TRACER on x86-64.

* Wed Oct 07 2009 Dave Jones <davej@redhat.com>
- Disable CONFIG_IRQSOFF_TRACER on srostedt's recommendation.
  (Adds unwanted overhead when not in use).

* Tue Oct  6 2009 Justin M. Forbes <jforbes@redhat.com>
- virtio_blk: add support for cache flush (#526869)

* Fri Oct  2 2009 John W. Linville <linville@redhat.com>
- Backport "iwlwifi: reduce noise when skb allocation fails"

* Wed Sep 30 2009 David Woodhouse <David.Woodhouse@intel.com>
- Update IOMMU code; mostly a bunch more workarounds for broken BIOSes.

* Wed Sep 30 2009 Dave Airlie <airlied@redhat.com> 2.6.31.1-56
- revert all the arjan patches until someone tests them.

* Tue Sep 29 2009 Steve Dickson <steved@redhat.com>  2.6.31.1-55
- Updated the NFS4 pseudo root code with a fix from upstream

* Tue Sep 29 2009 Dave Airlie <airlied@redhat.com> 2.6.31.1-54
- Fix broken capabilties that stopped dbus working due to copy from user
  fixups.

* Tue Sep 29 2009 Dave Airlie <airlied@redhat.com> 2.6.31.1-53
- drm-next-4c57edba4.patch: fix r600 dri1 memory leak and r600 bugs

* Mon Sep 28 2009 Dave Jones <davej@redhat.com> 2.6.31.1-52
- Use __builtin_object_size to validate the buffer size for copy_from_user
  + associated fixes to various copy_from_user invocations.

* Mon Sep 28 2009 Justin M. Forbes <jmforbes@redhat.com> 2.6.31.1-50
- Increase timeout for xen frontend devices to connect.

* Sat Sep 26 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31.1-49
- Add Xen spinlock patches to improve scalability.

* Sat Sep 26 2009 Dave Airlie <airlied@redhat.com> 2.6.31.1-48
- drm-next-8ef8678c8.patch: fix intel/nouveau kms

* Fri Sep 25 2009 Justin M. Forbes <bskeggs@redhat.com> 2.6.31.1-47
- Fix xen guest booting when NX is disabled (#525290)

* Fri Sep 25 2009 Ben Skeggs <bskeggs@redhat.com> 2.6.31.1-46
- drm-nouveau.patch: cleanups, fixes, pre-G80 s/r fixes, init rework

* Fri Sep 25 2009 Dave Airlie <airlied@redhat.com> 2.6.31.1-45
- drm-next-adea4796c.patch: fix r600 glxgears

* Fri Sep 25 2009 Dave Airlie <airlied@redhat.com> 2.6.31.1-44
- bump a extra one because I accidentially CVS.

* Thu Sep 24 2009 Dave Airlie <airlied@redhat.com> 2.6.31.1-42
- drm-next update - fix r600 s/r, and command line mode picking and r600 tv

* Thu Sep 24 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31.1-41
- Linux 2.6.31.1
- Drop patches merged upstream:
    linux-2.6-kvm-vmx-check-cpl-before-emulating-debug-register-access.patch
    linux-2.6-use-__pa_symbol-to-calculate-address-of-C-symbol.patch
    linux-2.6-kvm-pvmmu-do-not-batch-pte-updates-from-interrupt-context.patch
    linux-2.6-scsi-sd-fix-oops-during-scanning.patch
    linux-2.6-scsi-sg-fix-oops-in-error-path.patch

* Thu Sep 24 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31-40
- Drop the modules-ro-nx patch: it's causing ftrace to be unable
  to NOP out module function call tracking. (#524042)

* Wed Sep 23 2009 Kyle McMartin <kyle@redhat.com> 2.6.31-39
- touch initramfs-$foo not dracut-$foo.

* Wed Sep 23 2009 Adam Jackson <ajax@redhat.com> 2.6.31-37
- drm: Fix various buglets in EDID parsing.

* Mon Sep 21 2009 Ben Skeggs <bskeggs@redhat.com>
- nouveau: more on rh#522649, added some useful info to debugfs
- lots of coding style cleanups, which is the reason for the huge commit

* Fri Sep 18 2009 Dave Jones <davej@redhat.com>
- %ghost the dracut initramfs file.

* Thu Sep 17 2009 Hans de Goede <hdegoede@redhat.com>
- Now that we have %%post generation of dracut images we do not need to
  Require dracut-kernel anymore

* Thu Sep 17 2009 Kyle McMartin <kyle@redhat.com> 2.6.31-33
- Turn off CONFIG_CC_OPTIMIZE_FOR_SIZE on ppc64 until ld decides to play nice
  and generate the save/restore stubs.

* Thu Sep 17 2009 Kristian Høgsberg <krh@redhat.com>
- Drop drm page-flip patch for F12.

* Thu Sep 17 2009 Dave Jones <davej@redhat.com>
- cpuidle: Fix the menu governor to boost IO performance.

* Wed Sep 16 2009 John W. Linville <linville@redhat.com>
- Add a few more iwl1000 support patches.
- Remove support for deprecated iwl6000 parts.

* Wed Sep 16 2009 Eric Paris <eparis@redhat.com>
- Do not check CAP_SYS_MODULE when networking tres to autoload a module

* Wed Sep 16 2009 John W. Linville <linville@redhat.com>
- Add iwl1000 support patches.

* Wed Sep 16 2009 Adam Jackson <ajax@redhat.com>
- Disable hotplug interrupts on TV connectors on i915.

* Wed Sep 16 2009 Dave Jones <davej@redhat.com>
- Fix NULL deref in powernow-k8 driver. (korg #13780)

* Wed Sep 16 2009 Hans de Goede <hdegoede@redhat.com>
- Fix lockdep warning (and potential real deadlock) in mdraid10 code,
  requested for -stable, rh#515471

* Wed Sep 16 2009 Ben Skeggs <bskeggs@redhat.com> 2.6.31-17
- nouveau: potential fix for rh#522649 + misc other fixes

* Tue Sep 15 2009 Chuck Ebbert <cebbert@redhat.com>
- Add unused-kernel-patches Make target, change some patches to
  use ApplyOptionalPatch

* Tue Sep 15 2009 Ben Skeggs <bskeggs@redhat.com>
- nouveau: misc fixes to context-related issues, fixes some severe nv4x bugs

* Tue Sep 15 2009 Ben Skeggs <bskeggs@redhat.com>
- nouveau: temporarily disable fbcon accel, it's racing with ttm

* Mon Sep 14 2009 Steve Dickson <steved@redhat.com>
- Added support for -o v4 mount parsing

* Mon Sep 14 2009 Ben Skeggs <bskeggs@redhat.com>
- nouveau: avoid PFIFO IRQ hardlock, misc LVDS mode fixes, nv5x RAMFC cleanup

* Sun Sep 13 2009 Chuck Ebbert <cebbert@redhat.com>
- SCSI oops fixes requested for -stable

* Fri Sep 11 2009 Dave Jones <davej@redhat.com>
- Apply NX/RO to modules

* Fri Sep 11 2009 Dave Jones <davej@redhat.com>
- Mark kernel data section as NX

* Fri Sep 11 2009 Ben Skeggs <bskeggs@redhat.com>
- nouveau: bring in Matthew Garret's initial switchable graphics support

* Fri Sep 11 2009 Ben Skeggs <bskeggs@redhat.com>
- nouveau: fixed use of strap-based panel mode when required (rh#522649)
- nouveau: temporarily block accel on NVAC chipsets (rh#522361, rh#522575)

* Thu Sep 10 2009 Matthew Garrett <mjg@redhat.com>
- linux-2.6-ahci-export-capabilities.patch: Backport from upstream
- linux-2.6-rtc-show-hctosys.patch: Export the hctosys state of an rtc
- linux-2.6-rfkill-all.patch: Support for keys that toggle all rfkill state

* Thu Sep 10 2009 Ben Skeggs <bskeggs@redhat.com>
- drm-nouveau.patch: add some scaler-only modes for LVDS, GEM/TTM fixes

* Wed Sep 09 2009 Dennis Gilmore <dennis@ausil.us> 2.6.31-2
- touch the dracut initrd file when using %%{with_dracut}

* Wed Sep 09 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31-1
- Linux 2.6.31

* Wed Sep 09 2009 Chuck Ebbert <cebbert@redhat.com>
- Enable VXpocket and PDaudioCF PCMCIA sound drivers.

* Wed Sep 09 2009 Hans de Goede <hdegoede@redhat.com>
- Move to %%post generation of dracut initrd, because of GPL issues surrounding
  shipping a prebuild initrd
- Require grubby >= 7.0.4-1, for %%post generation

* Wed Sep  9 2009 Steve Dickson <steved@redhat.com>
- Updated the NFS4 pseudo root code to the latest release.

* Wed Sep 09 2009 Justin M. Forbes <jforbes@redhat.com>
- Revert virtio_blk to rotational mode. (#509383)

* Wed Sep 09 2009 Dave Airlie <airlied@redhat.com> 2.6.31-0.219.rc9.git
- uggh lost nouveau bits in page flip

* Wed Sep 09 2009 Dave Airlie <airlied@redhat.com> 2.6.31-0.218.rc9.git2
- fix r600 oops with page flip patch (#520766)

* Wed Sep 09 2009 Ben Skeggs <bskeggs@redhat.com>
- drm-nouveau.patch: fix display resume on pre-G8x chips

* Wed Sep 09 2009 Ben Skeggs <bskeggs@redhat.com>
- drm-nouveau.patch: add getparam to know using tile_flags is ok for scanout

* Wed Sep 09 2009 Chuck Ebbert <cebbert@redhat.com>
- 2.6.31-rc9-git2

* Wed Sep  9 2009 Roland McGrath <roland@redhat.com> 2.6.31-0.214.rc9.git1
- compile with -fno-var-tracking-assignments, work around gcc bug #521991

* Wed Sep 09 2009 Dave Airlie <airlied@redhat.com> 2.6.31-0.213.rc9.git1
- fix two bugs in r600 kms, fencing + mobile lvds

* Tue Sep 08 2009 Ben Skeggs <bskeggs@redhat.com> 2.6.31-0.212.rc9.git1
- drm-nouveau.patch: fix ppc build

* Tue Sep 08 2009 Ben Skeggs <bskeggs@redhat.com> 2.6.31-0.211.rc9.git1
- drm-nouveau.patch: more misc fixes

* Tue Sep 08 2009 Dave Airlie <airlied@redhat.com> 2.6.31-0.210.rc9.git1
- drm-page-flip.patch: rebase again

* Tue Sep 08 2009 Dave Airlie <airlied@redhat.com> 2.6.31-0.209.rc9.git1
- drm-next.patch: fix r600 signal interruption return value

* Tue Sep 08 2009 Ben Skeggs <bskeggs@redhat.com> 2.6.31-0.208.rc9.git1
- drm-nouveau.patch: latest upstream + rebase onto drm-next

* Tue Sep 08 2009 Dave Airlie <airlied@redhat.com> 2.6.31-0.207.rc9.git1
- drm-vga-arb.patch: update to avoid lockdep + add r600 support

* Tue Sep 08 2009 Dave Airlie <airlied@redhat.com> 2.6.31-0.206.rc9.git1
- drm: rebase to drm-next - r600 accel + kms should start working now

* Mon Sep 07 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31-0.205.rc9.git1
- 2.6.31-rc9-git1
- Temporarily hack the drm-next patch so it still applies; the result
  should still be safe to build.

* Sat Sep 05 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31-0.204.rc9
- 2.6.31-rc9

* Fri Sep 04 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31-0.203.rc8.git2
- Fix kernel build errors when building firmware by removing the
  .config file before that step and restoring it afterward.

* Thu Sep 03 2009 Adam Jackson <ajax@redhat.com>
- drm-ddc-caching-bug.patch: Empty the connector's mode list when it's
  disconnected.

* Thu Sep 03 2009 Jarod Wilson <jarod@redhat.com>
- Update hdpvr and lirc_zilog drivers for 2.6.31 i2c

* Thu Sep 03 2009 Justin M.Forbes <jforbes@redhat.com>
- Fix xen guest with stack protector. (#508120)
- Small kvm fixes.

* Wed Sep 02 2009 Adam Jackson <ajax@redhat.com> 2.6.31-0.199.rc8.git2
- drm-intel-pm.patch: Disable by default, too flickery on too many machines.
  Enable with i915.powersave=1.

* Wed Sep 02 2009 Dave Jones <davej@redhat.com>
- Add missing scriptlet dependancy. (#520788)

* Tue Sep 01 2009 Adam Jackson <ajax@redhat.com>
- Make DRM less chatty about EDID failures.  No one cares.

* Tue Sep 01 2009 Chuck Ebbert <cebbert@redhat.com>
- 2.6.31-rc8-git2
- Blank out drm-intel-next: entire contents are now upstream.

* Tue Sep 01 2009 Dave Jones <davej@redhat.com>
- Make firmware buildarch noarch. (Suggested by drago01 on irc)

* Tue Sep 01 2009 Jarod Wilson <jarod@redhat.com>
- Fix up lirc_zilog to enable functional IR transmit and receive
  on the Hauppauge HD PVR
- Fix audio on PVR-500 when used in same system as HVR-1800 (#480728)

* Sun Aug 30 2009 Chuck Ebbert <cebbert@redhat.com>
- 2.6.31-rc8-git1
- Drop linux-2.6-inotify-accounting.patch, merged upstream.

* Sun Aug 30 2009 Jarod Wilson <jarod@redhat.com>
- fix lirc_imon oops on older devices w/o tx ctrl ep (#520008)

* Fri Aug 28 2009 Eric Paris <eparis@redhat.com> 2.6.31-0.190.rc8
- fix inotify length accounting and send inotify events

* Fri Aug 28 2009 David Woodhouse <David.Woodhouse@intel.com>
- Enable Solos DSL driver

* Fri Aug 28 2009 Chuck Ebbert <cebbert@redhat.com>
- 2.6.31-rc8

* Thu Aug 27 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31-0.185.rc7.git6
- 2.6.31-rc7-git6
- Drop patch merged upstream:
  xen-fb-probe-fix.patch

* Thu Aug 27 2009 Adam Jackson <ajax@redhat.com>
- drm-rv710-ucode-fix.patch: Treat successful microcode load on RV710 as,
  you know, success. (#519718)

* Thu Aug 27 2009 Chuck Ebbert <cebbert@redhat.com>
- 2.6.31-rc7-git5
- Drop patch linux-2.6-ima-leak.patch, now merged upstream.

* Wed Aug 26 2009 Jarod Wilson <jarod@redhat.com>
- Fix up hdpvr ir enable patch for use w/modular i2c (David Engel)

* Wed Aug 26 2009 Eric Paris <eparis@redhat.com>
- fix iint_cache leak in IMA code
  drop the ima=0 patch

* Wed Aug 26 2009 Justin M. Forbes <jforbes@redhat.com>
- Fix munlock with KSM (#516909)
- Re-enable KSM

* Wed Aug 26 2009 Chuck Ebbert <cebbert@redhat.com>
- 2.6.31-rc7-git4
- Drop patches merged upstream:
  xen-x86-fix-stackprotect.patch
  xen-x86-no-stackprotect.patch

* Wed Aug 26 2009 Adam Jackson <ajax@redhat.com>
- drm-intel-next.patch: Update, various output setup fixes.

* Wed Aug 26 2009 David Woodhouse <David.Woodhouse@intel.com>
- Make WiMAX modular (#512070)

* Tue Aug 25 2009 Kyle McMartin <kyle@redhat.com>
- allow-disabling-ima.diff: debugging patch... adds ima=0 kernel
  param to disable initialization of IMA.

* Tue Aug 25 2009 Ben Skeggs <bskeggs@redhat.com> 2.6.31-0.174.rc7.git2
- drm-nouveau.patch: upstream update, pre-nv50 tv-out + misc fixes

* Tue Aug 25 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31-0.173.rc7.git2
- Fix Xen boot (#508120)

* Tue Aug 25 2009 Dave Airlie <airlied@redhat.com>
- pull in drm-next tree + rebase around it

* Mon Aug 24 2009 Chuck Ebbert <cebbert@redhat.com>
- 2.6.31-rc7-git2

* Mon Aug 24 2009 Chuck Ebbert <cebbert@redhat.com>
- 2.6.31-rc7-git1

* Sat Aug 22 2009 Chuck Ebbert <cebbert@redhat.com>
- 2.6.31-rc7

* Thu Aug 20 2009 Mark McLoughlin <markmc@redhat.com>
- Disable LZMA for xen (#515831)

* Thu Aug 20 2009 Chuck Ebbert <cebbert@redhat.com>
- 2.6.31-rc6-git5
- Fix up drm-r600-kms.patch
- Drop fix-perf-make-man-failure.patch

* Wed Aug 19 2009 Chuck Ebbert <cebbert@redhat.com>
- 2.6.31-rc6-git5
- Revert linux-2.6-debug-vm-would-have-oomkilled.patch to v1.2
  because upstream changes to oom-kill.c were all reverted.

* Tue Aug 18 2009 Kyle McMartin <kyle@redhat.com>
- Fix up perf so that it builds docs now that they are fixed.
- with_docs disables perf docs too. be warned. (logic is that the
  build deps are (mostly) the same, so if you don't want one, odds are...)

* Tue Aug 18 2009 Dave Jones <davej@redhat.com>
- 2.6.31-rc6-git3

* Mon Aug 17 2009 Dave Jones <davej@redhat.com> 2.6.31-0.161.rc6.git2
- 2.6.31-rc6-git2

* Mon Aug 17 2009 Chuck Ebbert <cebbert@redhat.com>
- Stop generating the (unused) ppc64-kdump.config file.

* Mon Aug 17 2009 Jarod Wilson <jarod@redhat.com>
- Add new lirc driver for built-in ENE0100 device on some laptops

* Sun Aug 16 2009 Kyle McMartin <kyle@redhat.com> 2.6.31-0.158.rc6
- Improve the perf script so it prints something helpful if the
  perf binary doesn't exist.

* Sat Aug 15 2009 Dave Jones <davej@redhat.com> 2.6.31-0.157.rc6
- Disable KSM patches on a hunch.  Chasing the "encrypted VGs don't work" bug.

* Fri Aug 14 2009 Dave Jones <davej@redhat.com> 2.6.31-0.155.rc6
- 2.6.31-rc6

* Wed Aug 12 2009 Kyle McMartin <kyle@redhat.com>
- fix perf.
- move perf to perf.$ver instead of perf-$ver...

* Wed Aug 12 2009 Dennis Gilmore <dennis@ausil.us>
- Obsolete kernel-smp on sparc64
- Require grubby >= 7.0.2-1 since thats what introduces the dracut options we use

* Wed Aug 12 2009 Kristian Høgsberg <krh@redhat.com>
- Fix drm-page-flip.patch to not break radeon kms and to not reset
  crtc offset into fb on flip.

* Wed Aug 12 2009 Adam Jackson <ajax@redhat.com>
- Update drm-intel-next patch

* Tue Aug 11 2009 Dennis Gilmore <dennis@ausil.us> - 2.6.31-0.149.rc5.git3
- disable building the -smp kernel on sparc64
- disable building kernel-perf on sparc64 syscalls not supported

* Tue Aug 11 2009 Eric Paris <eparis@redhat.com>
- Enable config IMA

* Tue Aug 11 2009 Ben Skeggs <bskeggs@redhat.com>
- nouveau: various cleanups and fixes + more sanity checking in dma paths

* Mon Aug 10 2009 Jarod Wilson <jarod@redhat.com>
- Add new device ID to lirc_mceusb (#512483)
- Fix some lockdep false positives
- Add support for setting and enabling iMON clock via sysfs
- Add tunable pad threshold support to lirc_imon
- Add new pseudo-IR protocl to lirc_imon for universals w/o a pad
- Fix mouse device support on older iMON devices

* Mon Aug 10 2009 David Woodhouse <David.Woodhouse@intel.com> 2.6.31-0.145.rc5.git3
- Merge latest Intel IOMMU fixes and BIOS workarounds, re-enable by default.

* Sun Aug 09 2009 Kyle McMartin <kyle@redhat.com>
- btusb autosuspend: fix build on !CONFIG_PM by stubbing out
  suspend/resume methods.

* Sat Aug 08 2009 Dennis Gilmore <dennis@ausil.us> 2.6.31-0.141.rc5.git3
- disable kgdb on sparc64 uni-processor kernel
- set max cpus to 256 on sparc64
- enable AT keyboard on sparc64

* Fri Aug 07 2009 Justin M. Forbes <jforbes@redhat.com>
- Apply KSM updates from upstream

* Fri Aug 07 2009 Hans de Goede <hdegoede@redhat.com>
- When building a dracut generic initrd tell new-kernel-pkg to use that
  instead of running mkinitrd

* Fri Aug 07 2009 Dave Airlie <airlied@redhat.com> 2.6.31-0.139.rc5.git3
- drm-r600-kms.patch - update r600 KMS
- drm-radeon-fixes.patch - patches for queue to Linus

* Thu Aug 06 2009 Justin M. Forbes <jforbes@redhat.com> 2.6.31-0.138.rc5.git3
- Fix kvm virtio_blk errors (#514901)

* Thu Aug 06 2009 Adam Jackson <ajax@redhat.com>
- Hush DRM vblank warnings, they're constant (and harmless) under DRI2.

* Thu Aug 06 2009 Dave Airlie <airlied@redhat.com> 2.6.31.0.134.rc5.git3
- fixup vga arb warning at startup and handover between gpus

* Thu Aug 06 2009 Kyle McMartin <kyle@redhat.com> 2.6.31.0.133.rc5.git3
- die-floppy-die.patch: it's the 21st century, let's not rely on
  steam powered technology.

* Wed Aug 05 2009 Dave Airlie <airlied@redhat.com> 2.6.31.0.132.rc5.git3
- revert-ftrace-powerpc-snafu.patch - fix ppc build

* Wed Aug 05 2009 Ben Skeggs <bskeggs@redhat.com>
- nouveau: respect nomodeset

* Wed Aug 05 2009 Chuck Ebbert <cebbert@redhat.com>
- Fix /usr/sbin/perf script. (#515494)

* Wed Aug 05 2009 Dave Jones <davej@redhat.com>
- Fix shift in pci cacheline size printk.

* Wed Aug 05 2009 Dave Airlie <airlied@redhat.com> 2.6.31.0.128.rc5.git3
- 2.6.31-rc5-git3
- drop cpufreq + set memory fixes

* Wed Aug 05 2009 Dave Airlie <airlied@redhat.com>
- Add Jeromes initial r600 kms work.
- rebase arb patch

* Tue Aug 04 2009 Kyle McMartin <kyle@redhat.com>
- alsa-tell-user-that-stream-to-be-rewound-is-suspended.patch: apply patch
  destined for 2.6.32, requested by Lennart.

* Tue Aug 04 2009 Ben Skeggs <bskeggs@redhat.com>
- nouveau: more code share between nv50/<nv50 kms, bug fixes

* Tue Aug 04 2009 Dave Airlie <airlied@redhat.com>
- update VGA arb patches again

* Mon Aug 03 2009 Adam Jackson <ajax@redhat.com>
- Update intel drm from anholt's tree
- Rebase drm-intel-pm.patch to match
- Drop gen3 fb hack, merged
- Drop previous watermark setup change

* Mon Aug 03 2009 Dave Jones <davej@redhat.com> 2.6.31-0.122.rc5.git2
- 2.6.31-rc5-git2

* Mon Aug 03 2009 Adam Jackson <ajax@redhat.com>
- (Attempt to) fix watermark setup on Intel 9xx parts.

* Mon Aug 03 2009 Jarod Wilson <jarod@redhat.com>
- make usbhid driver ignore all recent SoundGraph iMON devices, so the
  lirc_imon driver can grab them instead

* Mon Aug 03 2009 Dave Airlie <airlied@redhat.com>
- update VGA arb patches

* Sat Aug 01 2009 David Woodhouse <David.Woodhouse@intel.com> 2.6.31-0.118.rc5
- Fix boot failures on ppc32 (#514010, #505071)

* Fri Jul 31 2009 Kyle McMartin <kyle@redhat.com> 2.6.31-0.117.rc5
- Linux 2.6.31-rc5

* Fri Jul 31 2009 Matthew Garrett <mjg@redhat.com>
- linux-2.6-dell-laptop-rfkill-fix.patch: Fix up Dell rfkill

* Fri Jul 31 2009 Ben Skeggs <bskeggs@redhat.com>
- nouveau: build against 2.6.31-rc4-git6, fix script parsing on some G8x chips

* Thu Jul 30 2009 Chuck Ebbert <cebbert@redhat.com>
- Linux 2.6.31-rc4-git6
  New config item: CONFIG_BATTERY_DS2782 is not set
- Add last-minute set_memory_wc() fix from LKML.

* Thu Jul 30 2009 Matthew Garrett <mjg@redhat.com>
- drm-intel-pm.patch: Don't reclock external outputs. Increase the reduced
   clock slightly to avoid upsetting some hardware. Disable renderclock
   adjustment for the moment - it's breaking on some hardware.

* Thu Jul 30 2009 Ben Skeggs <bskeggs@redhat.com>
- nouveau: another DCB 1.5 entry, G80 corruption fixes, small <G80 KMS fix

* Thu Jul 30 2009 Dave Airlie <airlied@redhat.com>
- fix VGA ARB + kms

* Wed Jul 29 2009 Dave Jones <davej@redhat.com>
- Add support for dracut. (Harald Hoyer)

* Wed Jul 29 2009 Ben Skeggs <bskeggs@redhat.com>
- drm-nouveau.patch: nv50/nva0 tiled scanout fixes, nv40 kms fixes

* Wed Jul 29 2009 Chuck Ebbert <cebbert@redhat.com>
- Linux 2.6.31-rc4-git3
- Drop linux-2.6-ecryptfs-overflow-fixes.patch, merged upstream now.

* Wed Jul 29 2009 Dave Airlie <airlied@redhat.com>
- update VGA arb patches

* Tue Jul 28 2009 Adam Jackson <ajax@redhat.com>
- Remove the pcspkr modalias.  If you're still living in 1994, load it
  by hand.

* Tue Jul 28 2009 Eric Sandeen <sandeen@redhat.com> 2.6.31-0.102.rc4.git2
- Fix eCryptfs overflow issues (CVE-2009-2406, CVE-2009-2407)

* Tue Jul 28 2009 Kyle McMartin <kyle@redhat.com> 2.6.31-0.101.rc4.git2
- 2.6.31-rc4-git2
- rebase linux-2.6-fix-usb-serial-autosuspend.diff
- config changes:
 - USB_GSPCA_SN9C20X=m (_EVDEV=y)

* Tue Jul 28 2009 Ben Skeggs <bskeggs@redhat.com>
- drm-nouveau.patch: cleanup userspace API, various bugfixes.
  Looks worse than it is, register macros got cleaned up, which
  touches pretty much everywhere..

* Mon Jul 27 2009 Adam Jackson <ajax@redhat.com>
- Warn quieter about not finding PCI bus parents for ROM BARs, they're
  not usually needed and there's nothing you can do about it anyway.

* Mon Jul 27 2009 Matthew Garrett <mjg@redhat.com>
- linux-2.6-alsa-improve-hda-powerdown.patch - attempt to reduce audio glitches
   caused by HDA powerdown
- disable CONFIG_DEBUG_KOBJECT again for now, since it produces huge dmesg spew

* Mon Jul 27 2009 Dave Airlie <airlied@redhat.com>
- update vga arb code

* Mon Jul 27 2009 Matthew Garrett <mjg@redhat.com>
- drm-intel-pm.patch - Add runtime PM for Intel graphics

* Fri Jul 24 2009 Kristian Høgsberg <krh@redhat.com>
- Add drm-page-flip.patch to support vsynced page flipping on intel
  chipsets.
- Really add patch.
- Fix patch to not break nouveau.

* Fri Jul 24 2009 Chuck Ebbert <cebbert@redhat.com>
- Enable CONFIG_DEBUG_KOBJECT in debug kernels. (#513606)

* Thu Jul 23 2009 Kyle McMartin <kyle@redhat.com>
- perf BuildRequires binutils-devel now.

* Thu Jul 23 2009 Justin M. Forbes <jforbes@redhat.com>
- Add KSM support

* Thu Jul 23 2009 Kyle McMartin <kyle@redhat.com> 2.6.31-0.87.rc4
- Linux 2.6.31-rc4
- config changes:
 - USB_CDC_PHONET=m [all]
 - EVENT_PROFILE=y [i386, x86_64, powerpc, s390]

* Wed Jul 22 2009 Tom "spot" Callaway <tcallawa@redhat.com>
- We have to override the new %%install behavior because, well... the kernel is special.

* Wed Jul 22 2009 Dave Jones <davej@redhat.com>
- 2.6.31-rc3-git5

* Wed Jul 22 2009 Ben Skeggs <bskeggs@redhat.com> 2.6.31-0.82.rc3.git4
- Enable KMS for nouveau

* Wed Jul 22 2009 Ben Skeggs <bskeggs@redhat.com>
- Update nouveau from upstream (initial suspend/resume + misc bugfixes)

* Mon Jul 20 2009 Adam Jackson <ajax@redhat.com>
- Disable VGA arbiter patches for a moment

* Mon Jul 20 2009 Adam Jackson <ajax@redhat.com>
- Revive 4k framebuffers for intel gen3

* Mon Jul 20 2009 Dave Jones <davej@redhat.com> 2.6.31-0.78.rc3.git4
- Enable CONFIG_RTC_HCTOSYS (#489494)

* Mon Jul 20 2009 Dave Jones <davej@redhat.com> 2.6.31-0.77.rc3.git4
- Don't build 586 kernels any more.

* Sun Jul 19 2009 Dave Jones <davej@redhat.com> 2.6.31-0.75.rc3.git4
- build a 'full' package on i686 (Bill Nottingham)

* Sun Jul 19 2009 Dave Jones <davej@redhat.com> 2.6.31-0.74.rc3.git4
- 2.6.31-rc3-git4

* Sat Jul 18 2009 Matthew Garrett <mjg@redhat.com>
- linux-2.6-driver-level-usb-autosuspend.diff - allow drivers to enable autopm
- linux-2.6-fix-usb-serial-autosuspend.diff - fix generic usb-serial autopm
- linux-2.6-qcserial-autosuspend.diff - enable autopm by default on qcserial
- linux-2.6-bluetooth-autosuspend.diff - enable autopm by default on btusb
- linux-2.6-usb-uvc-autosuspend.diff - enable autopm by default on uvc

* Thu Jul 16 2009 Chuck Ebbert <cebbert@redhat.com>
- 2.6.31-rc3-git3

* Thu Jul 16 2009 Matthew Garrett <mjg@redhat.com>
- linux-2.6-defaults-aspm.patch - default ASPM to on for PCIe >= 1.1 hardware

* Thu Jul 16 2009 Dave Airlie <airlied@redhat.com> 2.6.31-0.69.rc3
- linux-2.6-vga-arb.patch - add VGA arbiter.
- drm-vga-arb.patch - add VGA arbiter support to drm

* Tue Jul 14 2009 Kyle McMartin <kyle@redhat.com> 2.6.31-0.68-rc3
- 2.6.31-rc3
- config changes:
 - RTL8192SU is not set, (staging)

* Mon Jul 13 2009 Kyle McMartin <kyle@redhat.com> 2.6.31-0.67.rc2.git9
- 2.6.31-rc2-git9
- config changes:
 - BLK_DEV_OSD=m

* Mon Jul 13 2009 Ben Skeggs <bskeggs@redhat.com>
- drm-nouveau.patch: update from upstream

* Fri Jul 10 2009 Chuck Ebbert <cebbert@redhat.com>
- 2.6.31-rc2-git6
- Drop dmadebug-spinlock patch -- merged upstream.

* Fri Jul 10 2009 Dave Jones <davej@redhat.com> 2.6.31-0.64.rc2.git5
- Don't jump through hoops that ppc powerbooks have to on sensible systems
  in cpufreq_suspend.

* Fri Jul 10 2009 Dave Jones <davej@redhat.com>
- 2.6.31-rc2-git5

* Thu Jul 09 2009 Dave Jones <davej@redhat.com> 2.6.31-0.62.rc2.git4
- Use correct spinlock initialization in dma-debug

* Thu Jul 09 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31-0.61.rc2.git4
- 2.6.31-rc2-git4

* Thu Jul 09 2009 Jarod Wilson <jarod@redhat.com>
- Enable IR receiver on the Hauppauge HD PVR
- Trim the changelog, axing everything before 2.6.29 (see cvs
  if you still really want to see that far back)

* Wed Jul 08 2009 Dave Jones <davej@redhat.com>
- Enable a bunch of debugging options that were missed somehow.

* Wed Jul 08 2009 Kyle McMartin <kyle@redhat.com>
- Bump NR_CPUS on x86_64 to 512.

* Wed Jul 08 2009 Adam Jackson <ajax@redhat.com>
- drm-no-gem-on-i8xx.patch: Drop, intel 2D driver requires GEM now. This
  should be entertaining.

* Wed Jul 08 2009 Kyle McMartin <kyle@redhat.com>
- First cut of /usr/sbin/perf wrapper script and 'perf'
  subpackage.

* Wed Jul 08 2009 Kyle McMartin <kyle@redhat.com> 2.6.31-0.54.rc2.git2
- Rebase and re-apply all the Fedora-specific linux-2.6-debug-*
  patches.
- Cull a bunch of upstreamed patches from the spec.

* Wed Jul 08 2009 Steve Dickson <steved@redhat.com>
- Added NFSD v4 dynamic pseudo root patch which allows
  NFS v3 exports to be mounted by v4 clients.

* Tue Jul 07 2009 Jarod Wilson <jarod@redhat.com>
- See if we can't make lirc_streamzap behave better... (#508952)

* Tue Jul 07 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31-0.47.rc2.git2
- 2.6.31-rc2-git2

* Tue Jul 07 2009 Jarod Wilson <jarod@redhat.com>
- Make lirc_i2c actually work with 2.6.31 i2c

* Mon Jul 06 2009 Chuck Ebbert <cebbert@redhat.com>
- Use LZMA for kernel compression on X86.

* Mon Jul 06 2009 Jarod Wilson <jarod@redhat.com>
- Hack up lirc_i2c and lirc_zilog to compile with 2.6.31 i2c
  changes. The drivers might not actually be functional now, but
  at least they compile again. Will fix later, if need be...

* Sat Jul 04 2009 Dave Jones <davej@redhat.com> 2.6.31-0.42.rc2
- 2.6.31-rc2

* Sat Jul 04 2009 Chuck Ebbert <cebbert@redhat.com>
- 2.6.31-rc1-git11

* Fri Jul 03 2009 Hans de Goede <hdegoede@redhat.com>
- Disable v4l1 ov511 and quickcam_messenger drivers (obsoleted by
  v4l2 gspca subdrivers)

* Thu Jul 02 2009 Kyle McMartin <kyle@redhat.com> 2.6.31-0.39.rc1.git9
- 2.6.31-rc1-git9
- linux-2.6-dm-fix-exstore-search.patch: similar patch merged upstream.

* Tue Jun 30 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31-0.38.rc1.git7
- 2.6.31-rc1-git7

* Tue Jun 30 2009 Dave Jones <davej@redhat.com> 2.6.31-0.37.rc1.git5
- Disable kmemleak. Way too noisy, and not finding any real bugs.

* Tue Jun 30 2009 Ben Skeggs <bskeggs@redhat.com>
- drm-nouveau.patch: match upstream

* Mon Jun 29 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31-0.35.rc1.git5
- 2.6.31-rc1-git5
- CONFIG_LEDS_LP3944=m

* Mon Jun 29 2009 Chuck Ebbert <cebbert@redhat.com>
- Try to fix the dm overlay bug for real (#505121)

* Sat Jun 27 2009 Ben Skeggs <bskeggs@redhat.com> 2.6.31-0.33.rc1.git2
- drm-nouveau.patch: fix conflicts from 2.6.31-rc1-git2

* Fri Jun 26 2009 Dave Jones <davej@redhat.com> 2.6.31-0.31.rc1.git2
- Further improvements to kmemleak

* Fri Jun 26 2009 Dave Jones <davej@redhat.com> 2.6.31-0.30.rc1.git2
- 2.6.31-rc1-git2

* Fri Jun 26 2009 Ben Skeggs <bskeggs@redhat.com>
- drm-nouveau.patch: latest upstream + reenable

* Thu Jun 25 2009 Dave Jones <davej@redhat.com> 2.6.31-0.29.rc1
- Make kmemleak scan process stacks by default.
  Should reduce false positives (which does also increase false negatives,
  but that's at least less noisy)

* Wed Jun 24 2009 Kyle McMartin <kyle@redhat.com> 2.6.31-0.28.rc1
- 2.6.31-rc1
- linux-2.6-utrace.patch: rebase on kernel/Makefile changes
- config changes:
 - generic:
  - CONFIG_DM_LOG_USERSPACE=m
  - CONFIG_DM_MULTIPATH_QL=m
  - CONFIG_DM_MULTIPATH_ST=m
  - CONFIG_BATTERY_MAX17040=m
  - CONFIG_I2C_DESIGNWARE is off (depends on clk.h)

* Wed Jun 24 2009 Kyle McMartin <kyle@redhat.com>
- Move perf to /usr/libexec/perf-$KernelVer.

* Wed Jun 24 2009 Kyle McMartin <kyle@redhat.com>
- config changes:
 - generic:
  - CONFIG_SCSI_DEBUG=m (was off, requested by davidz)

* Wed Jun 24 2009 Dave Jones <davej@redhat.com> 2.6.31-0.22.rc0.git22
- 2.6.30-git22

* Tue Jun 23 2009 Dave Jones <davej@redhat.com> 2.6.31-0.22.rc0.git20
- 2.6.30-git20

* Mon Jun 22 2009 Kyle McMartin <kyle@redhat.com> 2.6.31-0.24.rc0.git18
- Enable tools/perf, installed as /bin/perf-$KernelVer. Docs and a /bin/perf
  wrapper come next if this builds ok.

* Mon Jun 22 2009 Kyle McMartin <kyle@redhat.com>
- sched-introduce-SCHED_RESET_ON_FORK-scheduling-policy-flag.patch: pull in
  two fixes from Mike Galbraith from tip.git

* Sun Jun 21 2009 Dave Jones <davej@redhat.com> 2.6.31-0.21.rc0.git18
- Add patch to possibly fix the pktlen problem on via-velocity.

* Sun Jun 21 2009 Dave Jones <davej@redhat.com> 2.6.31-0.20.rc0.git18
- 2.6.30-git18
  VIA crypto & mmc patches now upstream.

* Sun Jun 21 2009 Dave Jones <davej@redhat.com>
- Determine cacheline sizes in a generic manner.

* Sun Jun 21 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.31-0.18.rc0.git17
- 2.6.30-git17
- Config changes:
  - powerpc32-generic
      CONFIG_PERF_COUNTERS=y
  - generic
      CONFIG_KEYBOARD_LM8323 is not set
      CONFIG_MOUSE_SYNAPTICS_I2C=m
      CONFIG_TOUCHSCREEN_EETI=m
      CONFIG_TOUCHSCREEN_W90X900=m
- Dropped agp-set_memory_ucwb.patch, all fixed upstream now.

* Sat Jun 20 2009 Kyle McMartin <kyle@redhat.com> 2.6.31.0.17.rc0.git15
- config changes:
 - ppc generic:
  - CONFIG_PPC_DISABLE_WERROR=y (switched... chrp fails otherwise, stack
    frame size.)

* Sat Jun 20 2009 Kyle McMartin <kyle@redhat.com> 2.6.31.0.16.rc0.git15
- 2.6.30-git15
- config changes:
 - generic:
  - CONFIG_LBDAF=y
 - staging:
  - CONFIG_USB_SERIAL_QUATECH2 is not set
  - CONFIG_VT6655 is not set
  - CONFIG_USB_CPC is not set
  - CONFIG_RDC_17F3101X is not set
  - CONFIG_FB_UDL is not set
 - ppc32:
  - CONFIG_KMETER1=y
 - ppc generic:
  - CONFIG_PPC_DISABLE_WERROR is not set
- lirc disabled due to i2c detach_client removal.

* Sat Jun 20 2009 Kyle McMartin <kyle@redhat.com>
- sched-introduce-SCHED_RESET_ON_FORK-scheduling-policy-flag.patch: add,
  queued in tip/sched/core (ca94c442535a44d508c99a77e54f21a59f4fc462)

* Fri Jun 19 2009 Kyle McMartin <kyle@redhat.com> 2.6.31.0.15.rc0.git14
- Fix up ptrace, hopefully. Builds on x86_64 at least.

* Fri Jun 19 2009 Chuck Ebbert <cebbert@redhat.com>
- linux-2.6-tip.git-203abd67b75f7714ce98ab0cdbd6cfd7ad79dec4.patch
  Fixes oops on boot with qemu (#507007)

* Fri Jun 19 2009 Kyle McMartin <kyle@redhat.com> 2.6.31-0.13.rc0.git14
- 2.6.30-git14

* Fri Jun 19 2009 Chuck Ebbert <cebbert@redhat.com>
- Fix up the via-sdmmc and via-hwmon-temp-sensor patches.
- Drop VIA Padlock patches merged upstream:
    via-rng-enable-64bit.patch
    via-padlock-10-enable-64bit.patch
    via-padlock-20-add-x86-dependency.patch

* Thu Jun 18 2009 Kyle McMartin <kyle@redhat.com> 2.6.31-0.11.rc0.git13
- 2.6.30-git13
- config changes:
 - arm:
  - CONFIG_UACCESS_WITH_MEMCPY is not set
 - i686-PAE:
  - CONFIG_XEN_DEV_EVTCHN=m
  - CONFIG_XEN_SYS_HYPERVISOR=y
 - ia64:
  - CONFIG_RCU_FANOUT=64
 - nodebug:
  - CONFIG_DEBUG_KMEMLEAK is not set
  - CONFIG_DEBUG_KMEMLEAK_TEST=m
 - powerpc:
  - CONFIG_CAN_SJA1000_OF_PLATFORM=m
  - CONFIG_PPC_EMULATED_STATS=y
  - CONFIG_SWIOTLB=y
  - CONFIG_RDS is not set (broken on ppc32)
 - powerpc32:
  - CONFIG_RCU_FANOUT=32
 - powerpc64:
  - CONFIG_RCU_FANOUT=64
  - CONFIG_PERF_COUNTERS=y
 - s390x:
  - CONFIG_RCU_FANOUT=64
  - CONFIG_SECCOMP=y
  - CONFIG_PM=y
  - CONFIG_HIBERNATION=y
  - CONFIG_PM_STD_PARTITION="/dev/jokes"
 - sparc64:
  - CONFIG_RCU_FANOUT=64
 - x86:
  - CONFIG_RCU_FANOUT=32
  - CONFIG_IOMMU_STRESS is not set
  - CONFIG_PERF_COUNTERS=y
  - CONFIG_X86_OLD_MCE is not set
  - CONFIG_X86_MCE_INTEL=y
  - CONFIG_X86_MCE_AMD=y
  - CONFIG_X86_ANCIENT_MCE is not set
  - CONFIG_X86_MCE_INJECT is not set
 - x86_64:
  - CONFIG_EDAC_AMD64=m
  - CONFIG_EDAC_AMD64_ERROR_INJECTION is not set
  - CONFIG_XEN_DEV_EVTCHN=m
  - CONFIG_XEN_SYS_HYPERVISOR=y
  - CONFIG_RCU_FANOUT=64
  - CONFIG_IOMMU_STRESS is not set
  - CONFIG_PERF_COUNTERS=y
  - CONFIG_X86_MCE_INJECT is not set
 - generic:
  - CONFIG_RCU_FANOUT=32
  - CONFIG_MMC_SDHCI_PLTFM=m
  - CONFIG_MMC_CB710=m
  - CONFIG_CB710_CORE=m
  - CONFIG_CB710_DEBUG is not set
  - CONFIG_SCSI_MVSAS_DEBUG is not set
  - CONFIG_SCSI_BNX2_ISCSI=m
  - CONFIG_NETFILTER_XT_MATCH_OSF=m
  - CONFIG_RFKILL_INPUT=y (used to be =m, which was invalid)
  - CONFIG_DE2104X_DSL=0
  - CONFIG_KS8842 is not set
  - CONFIG_CFG80211_DEBUGFS=y
  - CONFIG_MAC80211_DEFAULT_PS=y
  - CONFIG_IWM=m
  - CONFIG_IWM_DEBUG is not set
  - CONFIG_RT2800USB=m
  - CONFIG_CAN_DEV=m
  - CONFIG_CAN_CALC_BITTIMING=y
  - CONFIG_CAN_SJA1000=m
  - CONFIG_CAN_SJA1000_PLATFORM=m
  - CONFIG_CAN_EMS_PCI=m
  - CONFIG_CAN_KVASER_PCI=m
  - CONFIG_EEPROM_MAX6875=m
  - CONFIG_SENSORS_TMP401=m
  - CONFIG_MEDIA_SUPPORT=m
  - CONFIG_SND_CTXFI=m
  - CONFIG_SND_LX6464ES=m
  - CONFIG_SND_HDA_CODEC_CA0110=y
  - CONFIG_USB_XHCI_HCD=m
  - CONFIG_USB_XHCI_HCD_DEBUGGING is not set
  - CONFIG_DRAGONRISE_FF=y (used to be =m)
  - CONFIG_GREENASIA_FF=y (used to be =m)
  - CONFIG_SMARTJOYPLUS_FF=y (used to be =m)
  - CONFIG_USB_NET_INT51X1=m
  - CONFIG_CUSE=m
  - CONFIG_FUNCTION_PROFILER=y
  - CONFIG_RING_BUFFER_BENCHMARK=m
  - CONFIG_REGULATOR_USERSPACE_CONSUMER=m
  - CONFIG_REGULATOR_MAX1586=m
  - CONFIG_REGULATOR_LP3971=m
  - CONFIG_RCU_FANOUT_EXACT is not set
  - CONFIG_DEFAULT_MMAP_MIN_ADDR=4096
  - CONFIG_FSNOTIFY=y
  - CONFIG_IEEE802154=m
  - CONFIG_IEEE802154_DRIVERS=m
  - CONFIG_IEEE802154_FAKEHARD=m
  - CONFIG_CNIC=m

* Wed Jun 17 2009 Jarod Wilson <jarod@redhat.com>
- New lirc_imon hotness, update 2:
  * support dual-interface devices with a single lirc device
  * directional pad functions as an input device mouse
  * touchscreen devices finally properly supported
  * support for using MCE/RC-6 protocol remotes
  * fix oops in RF remote association code (F10 bug #475496)
  * fix re-enabling case/panel buttons and/or knobs
- Add some misc additional lirc_mceusb2 transceiver IDs
- Add missing unregister_chrdev_region() call to lirc_dev exit
- Add it8720 support to lirc_it87

* Tue Jun 16 2009 Chuck Ebbert <cebbert@redhat.com>
- Update via-sdmmc driver

* Mon Jun 15 2009 Jarod Wilson <jarod@redhat.com>
- Update lirc patches w/new imon hotness

* Fri Jun 12 2009 Chuck Ebbert <cebbert@redhat.com>
- Update VIA temp sensor and mmc drivers.

* Fri Jun 12 2009 John W. Linville <linville@redhat.com> 2.6.30-6
- neigh: fix state transition INCOMPLETE->FAILED via Netlink request
- enable CONFIG_ARPD (used by OpenNHRP)

* Wed Jun 10 2009 Chuck Ebbert <cebbert@redhat.com>
- VIA Nano updates:
  Enable Padlock AES encryption and random number generator on x86-64
  Add via-sdmmc and via-cputemp drivers

* Wed Jun 10 2009 Kyle McMartin <kyle@redhat.com> 2.6.30-1
- Linux 2.6.30 rebase.

* Tue Jun 09 2009 John W. Linville <linville@tuxdriver.com>
- Clean-up some wireless bits in config-generic

* Tue Jun 09 2009 Chuck Ebbert <cebbert@redhat.com>
- Add support for ACPI P-states on VIA processors.
- Disable the e_powersaver driver.

* Tue Jun 09 2009 Chuck Ebbert <cebbert@redhat.com>
- Linux 2.6.30-rc8-git6

* Fri Jun 05 2009 Chuck Ebbert <cebbert@redhat.com>
- Linux 2.6.30-rc8-git1

* Wed Jun 03 2009 Kyle McMartin <kyle@redhat.com>
- Linux 2.6.30-rc8

* Tue Jun  2 2009 Roland McGrath <roland@redhat.com>
- utrace update (fixes stap PR10185)

* Tue Jun 02 2009 Dave Jones <davej@redhat.com>
- For reasons unknown, RT2X00 driver was being built-in.
  Make it modular.

* Tue Jun 02 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc7-git5

* Sat May 30 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc7-git4

* Thu May 28 2009 Dave Jones <davej@redhat.com
- 2.6.30-rc7-git3

* Wed May 27 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc7-git2

* Tue May 26 2009 Dave Jones <davej@redhat.com>
- Various cpufreq patches from git.

* Tue May 26 2009 Dave Jones <davej@redhat.com
- 2.6.30-rc7-git1

* Tue May 26 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc7-git1

* Mon May 25 2009 Kyle McMartin <kyle@redhat.com>
- rds-only-on-64-bit-or-x86.patch: drop patch, issue is fixed upstream.

* Sat May 23 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc7

* Thu May 21 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc6-git6

* Wed May 20 2009  Chuck Ebbert <cebbert@redhat.com>
- Enable Divas (formerly Eicon) ISDN drivers on x86_64. (#480837)

* Wed May 20 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc6-git5

* Mon May 18 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc6-git3

* Sun May 17 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc6-git2

* Sat May 16 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc6

* Mon May 11 2009 Kyle McMartin <kyle@redhat.com>
- Linux 2.6.30-rc5-git1

* Fri May 08 2009 Kyle McMartin <kyle@redhat.com>
- Linux 2.6.30-rc5

* Fri May 08 2009 Kyle McMartin <kyle@redhat.com>
- Linux 2.6.30-rc4-git4

* Wed May 06 2009 Kyle McMartin <kyle@redhat.com>
- Linux 2.6.30-rc4-git3
- linux-2.6-cdrom-door-status.patch: merged upstream.
- linux-2.6-iwl3945-remove-useless-exports.patch: merged upstream.
- linux-2.6-utrace.patch: rebase against changes to fs/proc/array.c
- USB_NET_CDC_EEM=m

* Fri May 01 2009 Eric Sandeen <sandeen@redhat.com>
- Fix ext4 corruption on partial write into prealloc block

* Thu Apr 30 2009 Kyle McMartin <kyle@redhat.com>
- 2.6.30-rc4

* Wed Apr 29 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc3-git6

* Tue Apr 28 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc3-git4

* Tue Apr 28 2009 Chuck Ebbert <cebbert@redhat.com>
- Make the kernel-vanilla package buildable again.
- Allow building with older versions of RPM.

* Tue Apr 28 2009 Neil Horman <nhorman@redhat.com>
- Backport missing snmp stats (bz 492391)

* Tue Apr 28 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.30-0.72.rc3.git3
- Drop unused exports from the iwl3945 driver.

* Tue Apr 28 2009 Chuck Ebbert <cebbert@redhat.com>
- Linux 2.6.30-rc3-git3

* Mon Apr 27 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc3-git2

* Sun Apr 26 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.30-0.68.rc3.git1
- Linux 2.6.30-rc3-git1

* Wed Apr 22 2009 Dave Jones <davej@redhat.com> 2.6.30-0.67.rc3
- Disable SYSFS_DEPRECATED on ia64

* Wed Apr 22 2009 Kyle McMartin <kyle@redhat.com>
- Linux 2.6.30-rc3
- PROC_VMCORE=y: Exports the dump image of crashed
  kernel in ELF format

* Wed Apr 22 2009 Neil Horman <nhorman@redhat.com>
- Enable RELOCATABLE and CRASH_DUMP for powerpc64
- With this we can remove the -kdump build variant
- for the ppc64 arch

* Tue Apr 21 2009 Chuck Ebbert <cebbert@redhat.com>
- Don't include the modules.*.bin files in the RPM package.

* Tue Apr 21 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc2-git7

* Mon Apr 20 2009 Dave Jones <davej@redhat.com>
- Various s390x config tweaks. (#496596, #496601, #496605, #496607)

* Mon Apr 20 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc2-git6

* Sat Apr 18 2009 Chuck Ebbert <cebbert@redhat.com>
- Set CONFIG_UEVENT_HELPER_PATH to the empty string (#496296)

* Fri Apr 17 2009 Dave Jones <davej@redhat.com>
- 2.6.30-rc2-git3

* Thu Apr 16 2009 Kyle McMartin <kyle@redhat.com> 2.6.30-0.58.rc2.git1
- 2.6.30-rc2-git1

* Wed Apr 15 2009 Kyle McMartin <kyle@redhat.com> 2.6.30-0.57.rc2
- 2.6.30-rc2

* Tue Apr 14 2009 Kyle McMartin <kyle@redhat.com>
- 2.6.30-rc1-git7
- CONFIG_TOUCHSCREEN_AD7879_I2C=m
- CONFIG_STRIP_ASM_SYMS=y, off for -debug

* Mon Apr 13 2009 Kyle McMartin <kyle@redhat.com>
- ppc-fix-parport_pc.patch: add from linuxppc-dev@

* Mon Apr 13 2009 Kyle McMartin <kyle@redhat.com>
- execshield: fix build (load_user_cs_desc is 32-bit only in tlb.c)

* Sun Apr 12 2009 Kyle McMartin <kyle@redhat.com>
- 2.6.30-rc1-git5
- revert-fix-modules_install-via-nfs.patch: reverted upstream

* Thu Apr 09 2009 Kyle McMartin <kyle@redhat.com>
- actually drop utrace-ftrace from srpm.

* Thu Apr 09 2009 Kyle McMartin <kyle@redhat.com>
- 2.6.30-rc1-git2
- CONFIG_IGBVF=m
- CONFIG_NETFILTER_XT_TARGET_LED=m

* Thu Apr 09 2009 Dave Jones <davej@redhat.com>
- Bring back the /dev/crash driver. (#492803)

* Wed Apr 08 2009 Dave Jones <davej@redhat.com>
- disable MMIOTRACE in non-debug builds (#494584)

* Wed Apr 08 2009 Kyle McMartin <kyle@redhat.com> 2.6.30-0.44.rc1
- 2.6.30-rc1
- linux-2.6-hwmon-atk0110.patch: drop
- CONFIG_DETECT_HUNG_TASK=y
- # CONFIG_BOOTPARAM_HUNG_TASK_PANIC is not set

* Tue Apr  7 2009 Roland McGrath <roland@redhat.com>
- utrace update, drop unfinished utrace-ftrace

* Tue Apr 07 2009 Kyle McMartin <kyle@redhat.com>
- Linux 2.6.29-git15
- EXT3_DEFAULTS_TO_ORDERED on for now.
- X86_X2APIC enabled.
- LEDS_LP5521, LEDS_BD2802 off... look not generally relevant.
- LIBFCOE on.

* Tue Apr 07 2009 Dave Jones <davej@redhat.com>
- Enable CONFIG_CIFS_STATS (#494545)

* Mon Apr 06 2009 Kyle McMartin <kyle@redhat.com>
- linux-2.6-execshield.patch: rebase for 2.6.30

* Mon Apr 06 2009 Kyle McMartin <kyle@redhat.com>
- Linux 2.6.29-git13
- drop patches merged upstream:
  - fix-ppc-debug_kmap_atomic.patch
  - fix-staging-at76.patch
  - linux-2.6-acpi-video-didl-intel-outputs.patch
  - linux-2.6-acpi-strict-resources.patch
  - linux-2.6-sony-laptop-rfkill.patch
  - linux-2.6-btrfs-fix-umount-hang.patch
  - linux-2.6-fiemap-header-install.patch
  - linux-2.6-debug-dma-api.patch
  - dma-api-debug-fixes.patch
  - linux-2.6-ext4-flush-on-close.patch
  - linux-2.6-relatime-by-default.patch
  - linux-2.6-pci-sysfs-remove-id.patch
  - linux-2.6-scsi-cpqarray-set-master.patch
  - alsa-rewrite-hw_ptr-updaters.patch
  - alsa-pcm-always-reset-invalid-position.patch
  - alsa-pcm-fix-delta-calc-at-overlap.patch
  - alsa-pcm-safer-boundary-checks.patch
  - linux-2.6-input-hid-extra-gamepad.patch
  - linux-2.6-ipw2x00-age-scan-results-on-resume.patch
  - linux-2.6-dropwatch-protocol.patch
  - linux-2.6-net-fix-gro-bug.patch
  - linux-2.6-net-fix-another-gro-bug.patch
  - linux-2.6-net-xfrm-fix-spin-unlock.patch
  - linux-2.6.29-pat-change-is_linear_pfn_mapping-to-not-use-vm_pgoff.patch
  - linux-2.6.29-pat-pci-change-prot-for-inherit.patch

* Thu Apr 02 2009 Josef Bacik <josef@toxicpanda.com>
- linux-2.6-btrfs-fix-umount-hang.patch: fix umount hang on btrfs

* Thu Apr 02 2009 Kyle McMartin <kyle@redhat.com>
- fix-ppc-debug_kmap_atomic.patch: fix build failures on ppc.

* Thu Apr 02 2009 Kyle McMartin <kyle@redhat.com>
- Linux 2.6.29-git9

* Tue Mar 31 2009 Kyle McMartin <kyle@redhat.com>
- rds-only-on-64-bit-or-x86.patch: add
- at76-netdev_ops.patch: add

* Tue Mar 31 2009 Kyle McMartin <kyle@redhat.com>
- Linux 2.6.29-git8
- linux-2.6-net-fix-another-gro-bug.patch: upstream.

* Tue Mar 31 2009 Eric Sandeen <sandeen@redhat.com>
- add fiemap.h to kernel-headers
- build ext4 (and jbd2 and crc16) into the kernel

* Tue Mar 31 2009 Kyle McMartin <kyle@redhat.com>
- Linux 2.6.29-git7
- fix-staging-at76.patch: pull patch from linux-wireless to fix...

* Mon Mar 30 2009 Kyle McMartin <kyle@redhat.com> 2.6.30-0.28.rc0.git6
- Linux 2.6.29-git6
- Bunch of stuff disabled, most merged, some needs rebasing.

* Mon Mar 30 2009 Chuck Ebbert <cebbert@redhat.com>
- Make the .shared-srctree file a list so more than two checkouts
  can share source files.

* Mon Mar 30 2009 Chuck Ebbert <cebbert@redhat.com>
- Separate PAT fixes that are headed for -stable from our out-of-tree ones.

* Mon Mar 30 2009 Dave Jones <davej@redhat.com>
- Make io schedulers selectable at boot time again. (#492817)

* Mon Mar 30 2009 Dave Jones <davej@redhat.com>
- Add a strict-devmem=0 boot argument (#492803)

* Mon Mar 30 2009 Adam Jackson <ajax@redhat.com>
- linux-2.6.29-pat-fixes.patch: Fix PAT/GTT interaction

* Mon Mar 30 2009 Mauro Carvalho Chehab <mchehab@redhat.com>
- some fixes of troubles caused by v4l2 subdev conversion

* Mon Mar 30 2009 Mark McLoughlin <markmc@redhat.com> 2.6.29-21
- Fix guest->remote network stall with virtio/GSO (#490266)

* Mon Mar 30 2009 Ben Skeggs <bskeggs@redhat.com>
- drm-nouveau.patch
  - rewrite nouveau PCI(E) GART functions, should fix rh#492492
  - kms: kernel option to allow dual-link dvi
  - modinfo descriptions for module parameters

* Sun Mar 29 2009 Mauro Carvalho Chehab <mchehab@redhat.com>
- more v4l/dvb updates: v4l subdev conversion and some driver improvements

* Sun Mar 29 2009 Chuck Ebbert <cebbert@redhat.com>
- More fixes for ALSA hardware pointer updating.

* Sat Mar 28 2009 Mauro Carvalho Chehab <mchehab@redhat.com>
- linux-2.6-revert-dvb-net-kabi-change.patch: attempt to fix dvb net breakage
- update v4l fixes patch to reflect what's ready for 2.6.30
- update v4l devel patch to reflect what will be kept on linux-next for a while

* Fri Mar 27 2009 Chuck Ebbert <cebbert@redhat.com> 2.6.29-16
- Fix 2.6.29 networking lockups.
- Fix locking in net/xfrm/xfrm_state.c (#489764)

* Fri Mar 27 2009 Ben Skeggs <bskeggs@redhat.com>
- drm-nouveau.patch: do nothing for dac_{prepare,commit}, it's useless
  and breaks some things in strange ways.

* Fri Mar 27 2009 Ben Skeggs <bskeggs@redhat.com>
- nv50: clear 0x1900/8 on init, possible fix for rh#492240
- forcibly disable GEM also if KMS requested where not supported
- inform the user if we disable KMS because of it not being supported

* Thu Mar 26 2009 Matthew Garrett <mjg@redhat.com>
- linux-2.6-relatime-by-default.patch: Backport relatime code from 2.6.30

* Thu Mar 26 2009 Dave Jones <davej@redhat.com>
- Check for modesetting enabled before forcing mode on 915. (#490336)

* Thu Mar 26 2009 Dave Jones <davej@redhat.com>
- Set kernel-PAE as default in grub. (#487578)

* Thu Mar 26 2009 Dave Jones <davej@redhat.com>
- Enable CONFIG_MOUSE_PS2_ELANTECH (#492163)

* Thu Mar 26 2009 Kyle McMartin <kyle@redhat.com>
- linux-2.6-v4l-pvrusb2-fixes.patch: fix build for uncle steve.

* Thu Mar 26 2009 Mauro Carvalho Chehab <mchehab@redhat.com>
- Move all 2.6.30 stuff into linux-2.6-v4l-dvb-fixes.patch, in
  preparation for upstream pull;
- Added two new drivers: gspca sq905c and DVB Intel ce6230
- Updated to the latest v4l-dvb drivers.

* Wed Mar 25 2009 Mauro Carvalho Chehab <mchehab@redhat.com>
- remove duplicated Cinergy T2 entry at config-generic

* Wed Mar 25 2009 Neil Horman <nhorman@redhat.com>
- Add dropmonitor/dropwatch protocol from 2.6.30

* Wed Mar 25 2009 Kyle McMartin <kyle@redhat.com>
- alsa-rewrite-hw_ptr-updaters.patch: snd_pcm_update_hw_ptr() tries to
  detect the unexpected hwptr jumps more strictly to avoid the position
  mess-up, which often results in the bad quality I/O with pulseaudio.

* Wed Mar 25 2009 Ben Skeggs <bskeggs@redhat.com>
- drm-nouveau.patch: idle channels better before destroying them

* Tue Mar 24 2009 Kyle McMartin <kyle@redhat.com>
- Disable DMAR by default until suspend & resume is fixed.

* Tue Mar 24 2009 Josef Bacik <josef@toxicpanda.com>
- fsync replay fixes for btrfs

* Mon Mar 23 2009 Dave Jones <davej@redhat.com>
- 2.6.29

###
