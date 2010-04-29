%global __spec_install_pre %{___build_pre}

Summary: The Linux kernel

# For a stable, released kernel, released_kernel should be 1. For rawhide
# and/or a kernel built from an rc or git snapshot, released_kernel should
# be 0.
%define released_kernel 1

# Versions of various parts

# Polite request for people who spin their own kernel rpms:
# please modify the "buildid" define in a way that identifies
# that the kernel isn't the stock distribution kernel, for example,
# by setting the define to ".local" or ".bz123456"
#
# % define buildid .local

%define rhel 1
%if %{rhel}
%define distro_build 19
%define signmodules 1
%else
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
%define fedora_cvs_origin   1462
%define fedora_cvs_revision() %2
%global distro_build %(echo %{fedora_cvs_origin}.%{fedora_cvs_revision $Revision: 1.1532 $} | awk -F . '{ OFS = "."; ORS = ""; print $3 - $1 ; i = 4 ; OFS = ""; while (i <= NF) { print ".", $i ; i++} }')
%define distro_build %{fedora_build}
%define signmodules 0
%endif

# if patch fuzzy patch applying will be forbidden
%define with_fuzzy_patches 0

# base_sublevel is the kernel version we're starting with and patching
# on top of -- for example, 2.6.22-rc7-git1 starts with a 2.6.21 base,
# which yields a base_sublevel of 21.
%define base_sublevel 32

## If this is a released kernel ##
%if 0%{?released_kernel}

# Do we have a -stable update to apply?
%define stable_update 0
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
%define rcrev 0
# The git snapshot level
%define gitrev 0
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
%define with_debug     %{?_without_debug:     0} %{?!_without_debug:     1}
# kernel-doc
%define with_doc       %{?_without_doc:       0} %{?!_without_doc:       1}
# kernel-headers
%define with_headers   %{?_without_headers:   0} %{?!_without_headers:   1}
# kernel-firmware
%define with_firmware  %{?_with_firmware:     0} %{?!_with_firmware:     1}
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
# Temporary variant: framepointer
%define with_framepointer	%{?_with_framepointer:	1} %{?!_with_framepointer:	0}

# Build the kernel-doc package, but don't fail the build if it botches.
# Here "true" means "continue" and "false" means "fail the build".
%if 0%{?released_kernel}
%define doc_build_fail false
%else
%define doc_build_fail true
%endif

# Control whether we perform a compat. check against published ABI.
%define with_kabichk   %{?_without_kabichk:   1} %{?!_without_kabichk: 0}
# Control whether we perform a compat. check against published ABI.
%define with_fips      %{?_without_fips:      0} %{?!_without_fips:      1}

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
%define pkg_release %{distro_build}%{?stable_rctag}%{?dist}%{?buildid}

%else

# non-released_kernel
%if 0%{?rcrev}
%define rctag .rc%rcrev
%else
%define rctag .rc0
%endif
%if 0%{?gitrev}
%define gittag .git%gitrev
%else
%define gittag .git0
%endif
%define pkg_release 0.%{distro_build}%{?rctag}%{?gittag}%{?dist}%{?buildid}

%endif

# The kernel tarball/base version
%define kversion 2.6.%{base_sublevel}

%define make_target bzImage

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
%define pkg_release 0.%{distro_build}%{upstream_branch_tag}%{?dist}%{?buildid}
%endif

%if %{rhel}
%define pkg_release %{distro_build}%{?dist}%{?buildid}
%endif
%define KVERREL %{rpmversion}-%{pkg_release}.%{_target_cpu}

%if !%{debugbuildsenabled}
%define with_debug 0
%endif

%if !%{with_debuginfo}
%define _enable_debug_packages 0
%endif
%define debuginfodir /usr/lib/debug

%define with_pae 0

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
%define with_framepointer 0
%endif

%define all_x86 i386 i686

%ifnarch %{all_x86} x86_64
%define with_framepointer 0
%endif

%if %{with_vdso_install}
# These arches install vdso/ directories.
%define vdso_arches %{all_x86} x86_64 ppc ppc64
%endif

# Overrides for generic default options

# only ppc and alphav56 need separate smp kernels
%ifnarch ppc alphaev56
%define with_smp 0
%endif

%ifarch s390x
%define with_kdump 1
%else
%define with_kdump 0
%endif

# don't do debug builds on anything but i686 and x86_64
%ifnarch i686 x86_64 s390x ppc64
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

%ifarch s390
%define all_arch_configs kernel-%{kversion}-s390*.config
%define image_install_path boot
%define make_target image
%define kernel_image arch/s390/boot/image
%endif

%ifarch s390x
%define asmarch s390
%define hdrarch s390
%define all_arch_configs kernel-%{version}-s390x*.config
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
%define nobuildarches i386 i586 s390 sparc sparc64 ppc ia64 %{arm}

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
%define package_conflicts initscripts < 7.23, udev < 145-11, iptables < 1.3.2-1, ipw2200-firmware < 2.4, iwl4965-firmware < 228.57.2, selinux-policy-targeted < 1.25.3-14, squashfs-tools < 4.0, wireless-tools < 29-3

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
%define kernel_prereq  fileutils, module-init-tools, initscripts >= 8.11.1-1, kernel-firmware >= %{rpmversion}-%{pkg_release}, grubby >= 7.0.4-1
%if %{with_dracut}
%define initrd_prereq  dracut-kernel >= 002-18.git413bcf78
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
ExclusiveArch: noarch %{all_x86} x86_64 ppc ppc64 ia64 sparc sparc64 s390 s390x alpha alphaev56 %{arm}
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
BuildRequires: net-tools, patchutils
%if %{with_doc}
BuildRequires: xmlto
BuildRequires: asciidoc
%endif
%if %{with_sparse}
BuildRequires: sparse >= 0.4.1
%endif
%if %{with_perftool}
BuildRequires: elfutils-libelf-devel zlib-devel binutils-devel
%endif
%if %{signmodules}
BuildRequires: gnupg
%endif
BuildRequires: python
%if %{with_fips}
BuildRequires: hmaccalc
%endif
%ifarch s390x
# Ensure glibc{,-devel} is installed so zfcpdump can be built
BuildRequires: glibc-static
%endif

BuildConflicts: rhbuildsys(DiskFree) < 7Gb

%define fancy_debuginfo 1

%if %{fancy_debuginfo}
# Fancy new debuginfo generation introduced in Fedora 8.
BuildRequires: rpm-build >= 4.4.2.1-4
%define debuginfo_args --strict-build-id
%endif

Source0: ftp://ftp.kernel.org/pub/linux/kernel/v2.6/linux-%{kversion}.tar.bz2

Source1: Makefile.common

%if 0%{?rcrev}
Source2: ftp://ftp.kernel.org/pub/linux/kernel/v2.6/testing/incr/patch-2.6.%{upstream_sublevel}-rc%{rcrev}.bz2
%if 0%{?gitrev}
Source3: ftp://ftp.kernel.org/pub/linux/kernel/v2.6/testing/incr/patch-2.6.%{upstream_sublevel}-rc%{rcrev}-git%{gitrev}.bz2
%endif
%endif

Source11: genkey
Source14: find-provides
Source15: merge.pl
Source16: perf
Source17: kabitool
Source18: check-kabi
Source19: extrakeys.pub

Source20: Makefile.config

Source30: Module.kabi_i686
Source31: Module.kabi_ppc64
Source32: Module.kabi_s390x
Source33: Module.kabi_x86_64

Source50: config-i686
Source51: config-s390x-kdump-rhel
Source52: config-generic
Source53: config-x86-generic-rhel
Source54: config-s390x-generic-rhel
Source55: config-x86-generic
Source56: config-s390x-rhel
Source57: config-powerpc64-kdump
Source58: config-nodebug
Source59: config-s390x
Source60: config-powerpc-generic-rhel
Source61: config-ia64-generic-rhel
Source62: config-s390x-kdump
Source63: config-nodebug-rhel
Source64: config-powerpc-generic
Source65: config-framepointer
Source66: config-i686-rhel
Source67: config-powerpc64-rhel
Source68: config-debug
Source69: config-x86_64-generic
Source70: config-x86_64-generic-rhel
Source71: config-debug-rhel
Source72: config-generic-rhel
Source73: config-powerpc64

Patch: Fedora-redhat-introduce-nonint_oldconfig-target.patch
Patch1: Fedora-build-introduce-AFTER_LINK-variable.patch
Patch2: Fedora-utrace-introduce-utrace-implementation.patch
Patch3: Fedora-hwmon-add-VIA-hwmon-temperature-sensor-support.patch
Patch4: Fedora-powerpc-add-modalias_show-operation.patch
Patch5: Fedora-execshield-introduce-execshield.patch
Patch6: Fedora-nfsd4-proots.patch
Patch7: Fedora-nfs-make-nfs4-callback-hidden.patch
Patch8: Fedora-usb-Allow-drivers-to-enable-USB-autosuspend-on-a-per-device-basis.patch
Patch9: Fedora-usb-enable-autosuspend-by-default-on-qcserial.patch
Patch10: Fedora-usb-enable-autosuspend-on-UVC-by-default.patch
Patch11: Fedora-acpi-Disable-brightness-switch-by-default.patch
Patch12: Fedora-acpi-Disable-firmware-video-brightness-change-by-default.patch
Patch13: Fedora-debug-print-common-struct-sizes-at-boot-time.patch
Patch14: Fedora-x86-add-option-to-control-the-NMI-watchdog-timeout.patch
Patch15: Fedora-debug-display-tainted-information-on-other-places.patch
Patch16: Fedora-debug-add-calls-to-print_tainted-on-spinlock-functions.patch
Patch17: Fedora-debug-add-would_have_oomkilled-procfs-ctl.patch
Patch18: Fedora-debug-always-inline-kzalloc.patch
Patch19: Fedora-pci-add-config-option-to-control-the-default-state-of-PCI-MSI-interrupts.patch
Patch20: Fedora-pci-sets-PCIE-ASPM-default-policy-to-POWERSAVE.patch
Patch21: Fedora-sound-disables-hda-beep-by-default.patch
Patch22: Fedora-sound-hda-intel-prealloc-4mb-dmabuffer.patch
Patch23: Fedora-input-remove-unwanted-messages-on-spurious-events.patch
Patch24: Fedora-floppy-remove-the-floppy-pnp-modalias.patch
Patch25: Fedora-input-remove-pcspkr-modalias.patch
Patch26: Fedora-serial-Enable-higher-baud-rates-for-16C95x.patch
Patch27: Fedora-serio-disable-error-messages-when-i8042-isn-t-found.patch
Patch28: Fedora-pci-silence-some-PCI-resource-allocation-errors.patch
Patch29: Fedora-fb-disable-fbcon-logo-with-parameter.patch
Patch30: Fedora-crash-add-crash-driver.patch
Patch31: Fedora-pci-cacheline-sizing.patch
Patch32: Fedora-e1000-add-quirk-for-ich9.patch
Patch33: Fedora-drm-intel-big-hammer.patch
Patch34: Fedora-acpi-be-less-verbose-about-old-BIOSes.patch
Patch35: Fedora-rfkill-add-support-to-a-key-to-control-all-radios.patch
Patch36: redhat-adding-redhat-directory.patch
Patch37: redhat-Importing-config-options.patch
Patch38: redhat-s390x-adding-zfcpdump-application-used-by-s390x-kdump-kernel.patch
Patch39: redhat-Include-FIPS-required-checksum-of-the-kernel-image.patch
Patch40: redhat-Silence-tagging-messages-by-rh-release.patch
Patch41: redhat-Disabling-debug-options-for-beta.patch
Patch42: redhat-kernel-requires-udev-145-11-or-newer.patch
Patch43: redhat-tagging-2-6-31-50-el6.patch
Patch44: redhat-updating-lastcommit-for-2-6-31-50.patch
Patch45: block-get-rid-of-the-WRITE_ODIRECT-flag.patch
Patch46: block-aio-implement-request-batching.patch
Patch47: kdump-x86-add-CONFIG_KEXEC_AUTO_RESERVE.patch
Patch48: kdump-x86-implement-crashkernel-auto.patch
Patch49: kdump-ia64-add-CONFIG_KEXEC_AUTO_RESERVE.patch
Patch50: kdump-ia64-implement-crashkernel-auto.patch
Patch51: kdump-powerpc-add-CONFIG_KEXEC_AUTO_RESERVE.patch
Patch52: kdump-powerpc-implement-crashkernel-auto.patch
Patch53: kdump-doc-update-the-kdump-document.patch
Patch54: kdump-kexec-allow-to-shrink-reserved-memory.patch
Patch55: kernel-Set-panic_on_oops-to-1.patch
Patch56: redhat-fix-BZ-and-CVE-info-printing-on-changelog-when-HIDE_REDHAT-is-enabled.patch
Patch57: redhat-tagging-2-6-31-51-el6.patch
Patch58: redhat-updating-lastcommit-for-2-6-31-51.patch
Patch59: redhat-fixing-the-kernel-versions-on-the-SPEC-changelog.patch
Patch60: redhat-Fix-version-passed-to-update_changelog-sh.patch
Patch61: mm-Limit-32-bit-x86-systems-to-16GB-and-prevent-panic-on-boot-when-system-has-more-than-30GB.patch
Patch62: ppc64-Fix-kcrctab_-sections-to-undo-undesireable-relocations-that-break-kdump.patch
Patch63: net-export-device-speed-and-duplex-via-sysfs.patch
Patch64: scsi-devinfo-update-for-Hitachi-entries.patch
Patch65: x86-AMD-Northbridge-Verify-NB-s-node-is-online.patch
Patch66: redhat-tagging-2-6-32-0-52-el6.patch
Patch67: redhat-updating-lastcommit-for-2-6-32-0-52.patch
Patch68: redhat-fix-STAMP-version-on-rh-release-commit-phase.patch
Patch69: redhat-enable-debug-builds-also-on-s390x-and-ppc64.patch
Patch70: s390x-fix-build-failure-with-CONFIG_FTRACE_SYSCALLS.patch
Patch71: procfs-add-ability-to-modify-proc-file-limits-from-outside-a-processes-own-context.patch
Patch72: modsign-Multiprecision-maths-library.patch
Patch73: modsign-Add-indications-of-module-ELF-types.patch
Patch74: modsign-Module-ELF-verifier.patch
Patch75: modsign-Module-signature-checker-and-key-manager.patch
Patch76: modsign-Apply-signature-checking-to-modules-on-module-load.patch
Patch77: modsign-Don-t-include-note-gnu-build-id-in-the-digest.patch
Patch78: modsign-Enable-module-signing-in-the-RHEL-RPM.patch
Patch79: net-Add-acession-counts-to-all-datagram-protocols.patch
Patch80: redhat-tagging-2-6-32-0-53-el6.patch
Patch81: redhat-updating-lastcommit-for-2-6-32-0-53.patch
Patch82: redhat-fixing-wrong-bug-number-536759-536769.patch
Patch83: redhat-adding-top-makefile-to-enable-rh-targets.patch
Patch84: redhat-add-temporary-framepointer-variant.patch
Patch85: redhat-add-rh-key-target-to-Makefile.patch
Patch86: infiniband-Rewrite-SG-handling-for-RDMA-logic.patch
Patch87: x86-panic-if-AMD-cpu_khz-is-wrong.patch
Patch88: x86-Enable-CONFIG_SPARSE_IRQ.patch
Patch89: edac-amd64_edac-disabling-temporarily.patch
Patch90: redhat-tagging-2-6-32-0-54-el6.patch
Patch91: redhat-updating-lastcommit-for-2-6-32-0-54.patch
Patch92: redhat-create-patches-sh-use-first-parent-to-use-the-right-branch-history.patch
Patch93: redhat-Rebasing-to-kernel-2-6-32.patch
Patch94: redhat-updating-lastcommit-for-2-6-32-1.patch
Patch95: redhat-introduce-rh-kernel-debug-target.patch
Patch96: redhat-update-build-targets-in-Makefile.patch
Patch97: redhat-include-missing-System-map-file-for-debug-only-builds.patch
Patch98: Fedora-updating-linux-2-6-execshield-patch-2-6-32-8-fc13-reference.patch
Patch99: Fedora-updating-patch-linux-2-6-nfsd4-proots-patch-2-6-32-8-fc13-reference.patch
Patch100: Fedora-intel-iommu-backport.patch
Patch101: Fedora-ath9k-backports.patch
Patch102: Fedora-KVM-allow-userspace-to-adjust-kvmclock-offset.patch
Patch103: Fedora-drm-radeon-fixes.patch
Patch104: Fedora-drm-radeon-dp-support.patch
Patch105: Fedora-drm-nouveau-fixes.patch
Patch106: Fedora-drm-i915-Fix-sync-to-vblank-when-VGA-output-is-turned-off.patch
Patch107: Fedora-agp-clear-GTT-on-intel.patch
Patch108: Fedora-ext4-Fix-insuficient-checks-in-EXT4_IOC_MOVE_EXT.patch
Patch109: Fedora-perf-Don-t-free-perf_mmap_data-until-work-has-been-done.patch
Patch110: redhat-updating-config-files-based-on-current-requests-12-10.patch
Patch111: redhat-Config-updates-12-15.patch
Patch112: block-revert-cfq-iosched-limit-coop-preemption.patch
Patch113: block-CFQ-is-more-than-a-desktop-scheduler.patch
Patch114: block-cfq-calculate-the-seek_mean-per-cfq_queue-not-per-cfq_io_context.patch
Patch115: block-cfq-merge-cooperating-cfq_queues.patch
Patch116: block-cfq-change-the-meaning-of-the-cfqq_coop-flag.patch
Patch117: block-cfq-break-apart-merged-cfqqs-if-they-stop-cooperating.patch
Patch118: block-cfq-iosched-improve-hw_tag-detection.patch
Patch119: block-cfq-iosched-adapt-slice-to-number-of-processes-doing-I-O.patch
Patch120: block-cfq-iosched-preparation-to-handle-multiple-service-trees.patch
Patch121: block-cfq-iosched-reimplement-priorities-using-different-service-trees.patch
Patch122: block-cfq-iosched-enable-idling-for-last-queue-on-priority-class.patch
Patch123: block-cfq-iosched-fairness-for-sync-no-idle-queues.patch
Patch124: block-cfq-iosched-fix-style-issue-in-cfq_get_avg_queues.patch
Patch125: block-blkdev-flush-disk-cache-on-fsync.patch
Patch126: block-cfq-iosched-simplify-prio-unboost-code.patch
Patch127: block-cfq-iosched-fix-next_rq-computation.patch
Patch128: block-Expose-discard-granularity.patch
Patch129: block-partitions-use-sector-size-for-EFI-GPT.patch
Patch130: block-partitions-read-whole-sector-with-EFI-GPT-header.patch
Patch131: block-cfq-Make-use-of-service-count-to-estimate-the-rb_key-offset.patch
Patch132: block-cfq-iosched-cleanup-unreachable-code.patch
Patch133: block-cfq-iosched-fix-ncq-detection-code.patch
Patch134: block-cfq-iosched-fix-no-idle-preemption-logic.patch
Patch135: block-cfq-iosched-idling-on-deep-seeky-sync-queues.patch
Patch136: block-cfq-iosched-fix-corner-cases-in-idling-logic.patch
Patch137: block-Revert-cfq-Make-use-of-service-count-to-estimate-the-rb_key-offset.patch
Patch138: block-Allow-devices-to-indicate-whether-discarded-blocks-are-zeroed.patch
Patch139: block-cfq-iosched-no-dispatch-limit-for-single-queue.patch
Patch140: block-blkio-Set-must_dispatch-only-if-we-decided-to-not-dispatch-the-request.patch
Patch141: block-blkio-Introduce-the-notion-of-cfq-groups.patch
Patch142: block-blkio-Implement-macro-to-traverse-each-service-tree-in-group.patch
Patch143: block-blkio-Keep-queue-on-service-tree-until-we-expire-it.patch
Patch144: block-blkio-Introduce-the-root-service-tree-for-cfq-groups.patch
Patch145: block-blkio-Introduce-blkio-controller-cgroup-interface.patch
Patch146: block-blkio-Introduce-per-cfq-group-weights-and-vdisktime-calculations.patch
Patch147: block-blkio-Implement-per-cfq-group-latency-target-and-busy-queue-avg.patch
Patch148: block-blkio-Group-time-used-accounting-and-workload-context-save-restore.patch
Patch149: block-blkio-Dynamic-cfq-group-creation-based-on-cgroup-tasks-belongs-to.patch
Patch150: block-blkio-Take-care-of-cgroup-deletion-and-cfq-group-reference-counting.patch
Patch151: block-blkio-Some-debugging-aids-for-CFQ.patch
Patch152: block-blkio-Export-disk-time-and-sectors-used-by-a-group-to-user-space.patch
Patch153: block-blkio-Provide-some-isolation-between-groups.patch
Patch154: block-blkio-Drop-the-reference-to-queue-once-the-task-changes-cgroup.patch
Patch155: block-blkio-Propagate-cgroup-weight-updation-to-cfq-groups.patch
Patch156: block-blkio-Wait-for-cfq-queue-to-get-backlogged-if-group-is-empty.patch
Patch157: block-blkio-Determine-async-workload-length-based-on-total-number-of-queues.patch
Patch158: block-blkio-Implement-group_isolation-tunable.patch
Patch159: block-blkio-Wait-on-sync-noidle-queue-even-if-rq_noidle-1.patch
Patch160: block-blkio-Documentation.patch
Patch161: block-cfq-iosched-fix-compile-problem-with-CONFIG_CGROUP.patch
Patch162: block-cfq-iosched-move-IO-controller-declerations-to-a-header-file.patch
Patch163: block-io-controller-quick-fix-for-blk-cgroup-and-modular-CFQ.patch
Patch164: block-cfq-iosched-make-nonrot-check-logic-consistent.patch
Patch165: block-blkio-Export-some-symbols-from-blkio-as-its-user-CFQ-can-be-a-module.patch
Patch166: block-blkio-Implement-dynamic-io-controlling-policy-registration.patch
Patch167: block-blkio-Allow-CFQ-group-IO-scheduling-even-when-CFQ-is-a-module.patch
Patch168: block-cfq-iosched-use-call_rcu-instead-of-doing-grace-period-stall-on-queue-exit.patch
Patch169: block-include-linux-err-h-to-use-ERR_PTR.patch
Patch170: block-cfq-iosched-Do-not-access-cfqq-after-freeing-it.patch
Patch171: block-dio-fix-performance-regression.patch
Patch172: block-Add-support-for-the-ATA-TRIM-command-in-libata.patch
Patch173: scsi-Add-missing-command-definitions.patch
Patch174: scsi-scsi_debug-Thin-provisioning-support.patch
Patch175: scsi-sd-WRITE-SAME-16-UNMAP-support.patch
Patch176: scsi-Correctly-handle-thin-provisioning-write-error.patch
Patch177: libata-Report-zeroed-read-after-Trim-and-max-discard-size.patch
Patch178: libata-Clarify-ata_set_lba_range_entries-function.patch
Patch179: block-config-enable-CONFIG_BLK_CGROUP.patch
Patch180: block-config-enable-CONFIG_BLK_DEV_INTEGRITY.patch
Patch181: redhat-tagging-2-6-32-2-el6.patch
Patch182: redhat-updating-lastcommit-for-2-6-32-2.patch
Patch183: redhat-force-to-run-rh-key-target-when-compiling-the-kernel-locally-without-RPM.patch
Patch184: redhat-run-rngd-on-rh-key-to-speed-up-key-generation.patch
Patch185: redhat-make-the-documentation-build-j1.patch
Patch186: redhat-remove-unused-config-file-config-powerpc64-generic-rhel.patch
Patch187: redhat-fix-problem-when-using-other-rh-targets.patch
Patch188: redhat-reverting-makefile-magic.patch
Patch189: redhat-remove-gcc-bug-workaround.patch
Patch190: redhat-run-rh-key-when-the-GPG-keys-aren-t-present.patch
Patch191: nfs-convert-proto-option-to-use-netids-rather-than-a-protoname.patch
Patch192: scsi-fix-dma-handling-when-using-virtual-hosts.patch
Patch193: dm-core-and-mpath-changes-from-2-6-33.patch
Patch194: dm-raid1-changes-from-2-6-33.patch
Patch195: dm-crypt-changes-from-2-6-33.patch
Patch196: dm-snapshot-changes-from-2-6-33.patch
Patch197: dm-snapshot-merge-support-from-2-6-33.patch
Patch198: redhat-add-vhost-to-config-generic-rhel.patch
Patch199: virt-tun-export-underlying-socket.patch
Patch200: virt-mm-export-use_mm-unuse_mm-to-modules.patch
Patch201: virt-vhost_net-a-kernel-level-virtio-server.patch
Patch202: virt-vhost-add-missing-architectures.patch
Patch203: s390-kernel-clear-high-order-bits-after-switching-to-64-bit-mode.patch
Patch204: s390-zcrypt-Do-not-simultaneously-schedule-hrtimer.patch
Patch205: s390-dasd-support-DIAG-access-for-read-only-devices.patch
Patch206: s390-kernel-fix-dump-indicator.patch
Patch207: s390-kernel-performance-counter-fix-and-page-fault-optimization.patch
Patch208: s390-zcrypt-initialize-ap_messages-for-cex3-exploitation.patch
Patch209: s390-zcrypt-special-command-support-for-cex3-exploitation.patch
Patch210: s390-zcrypt-add-support-for-cex3-device-types.patch
Patch211: s390-zcrypt-use-definitions-for-cex3.patch
Patch212: s390-zcrypt-adjust-speed-rating-between-cex2-and-pcixcc.patch
Patch213: s390-zcrypt-adjust-speed-rating-of-cex3-adapters.patch
Patch214: s390-OSA-QDIO-data-connection-isolation.patch
Patch215: redhat-Build-in-standard-PCI-hotplug-support.patch
Patch216: pci-pciehp-Provide-an-option-to-disable-native-PCIe-hotplug.patch
Patch217: modsign-Don-t-check-e_entry-in-ELF-header.patch
Patch218: redhat-fixing-lastcommit-contents-for-2-6-32-2-el6.patch
Patch219: redhat-tagging-2-6-32-3-el6.patch
Patch220: redhat-updating-lastcommit-for-2-6-32-3.patch
Patch221: misc-Revert-utrace-introduce-utrace-implementation.patch
Patch222: ptrace-cleanup-ptrace_init_task-ptrace_link-path.patch
Patch223: ptrace-copy_process-should-disable-stepping.patch
Patch224: ptrace-introduce-user_single_step_siginfo-helper.patch
Patch225: ptrace-powerpc-implement-user_single_step_siginfo.patch
Patch226: ptrace-change-tracehook_report_syscall_exit-to-handle-stepping.patch
Patch227: ptrace-x86-implement-user_single_step_siginfo.patch
Patch228: ptrace-x86-change-syscall_trace_leave-to-rely-on-tracehook-when-stepping.patch
Patch229: signals-check-group_stop_count-after-tracehook_get_signal.patch
Patch230: tracehooks-kill-some-PT_PTRACED-checks.patch
Patch231: tracehooks-check-PT_PTRACED-before-reporting-the-single-step.patch
Patch232: ptrace_signal-check-PT_PTRACED-before-reporting-a-signal.patch
Patch233: ptrace-export-__ptrace_detach-and-do_notify_parent_cldstop.patch
Patch234: ptrace-reorder-the-code-in-kernel-ptrace-c.patch
Patch235: utrace-implement-utrace-ptrace.patch
Patch236: utrace-utrace-core.patch
Patch237: sound-ALSA-HDA-driver-update-2009-12-15.patch
Patch238: s390-dasd-enable-prefix-independent-of-pav-support.patch
Patch239: s390-dasd-remove-strings-from-s390dbf.patch
Patch240: s390-dasd-let-device-initialization-wait-for-LCU-setup.patch
Patch241: redhat-kernel-enable-hibernation-support-on-s390x.patch
Patch242: s390-iucv-add-work_queue-cleanup-for-suspend.patch
Patch243: s390-cmm-free-pages-on-hibernate.patch
Patch244: s390-ctcm-suspend-has-to-wait-for-outstanding-I-O.patch
Patch245: s390-zfcp-Don-t-fail-SCSI-commands-when-transitioning-to-blocked-fc_rport.patch
Patch246: s390-zfcp-Assign-scheduled-work-to-driver-queue.patch
Patch247: s390-zfcp-fix-ELS-ADISC-handling-to-prevent-QDIO-errors.patch
Patch248: s390-zfcp-improve-FSF-error-reporting.patch
Patch249: scsi-scsi_transport_fc-Introduce-helper-function-for-blocking-scsi_eh.patch
Patch250: s390-zfcp-Block-SCSI-EH-thread-for-rport-state-BLOCKED.patch
Patch251: uv-x86-SGI-UV-Fix-BAU-initialization.patch
Patch252: uv-x86-function-to-translate-from-gpa-socket_paddr.patch
Patch253: uv-x86-introduce-uv_gpa_is_mmr.patch
Patch254: uv-x86-RTC-Fix-early-expiry-handling.patch
Patch255: uv-x86-RTC-Add-clocksource-only-boot-option.patch
Patch256: uv-x86-RTC-Clean-up-error-handling.patch
Patch257: uv-gru-function-to-generate-chipset-IPI-values.patch
Patch258: uv-x86-SGI-Map-low-MMR-ranges.patch
Patch259: xen-wait-up-to-5-minutes-for-device-connetion-and-fix-fallout.patch
Patch260: xen-support-MAXSMP.patch
Patch261: mm-move-inc_zone_page_state-NR_ISOLATED-to-just-isolated-place.patch
Patch262: mm-swap_info-private-to-swapfile-c.patch
Patch263: mm-swap_info-change-to-array-of-pointers.patch
Patch264: mm-swap_info-include-first_swap_extent.patch
Patch265: mm-swap_info-miscellaneous-minor-cleanups.patch
Patch266: mm-swap_info-SWAP_HAS_CACHE-cleanups.patch
Patch267: mm-swap_info-swap_map-of-chars-not-shorts.patch
Patch268: mm-swap_info-swap-count-continuations.patch
Patch269: mm-swap_info-note-SWAP_MAP_SHMEM.patch
Patch270: mm-define-PAGE_MAPPING_FLAGS.patch
Patch271: mm-mlocking-in-try_to_unmap_one.patch
Patch272: mm-CONFIG_MMU-for-PG_mlocked.patch
Patch273: mm-pass-address-down-to-rmap-ones.patch
Patch274: mm-vmscan-have-kswapd-sleep-for-a-short-interval-and-double-check-it-should-be-asleep.patch
Patch275: mm-vmscan-stop-kswapd-waiting-on-congestion-when-the-min-watermark-is-not-being-met.patch
Patch276: mm-vmscan-separate-sc-swap_cluster_max-and-sc-nr_max_reclaim.patch
Patch277: mm-vmscan-kill-hibernation-specific-reclaim-logic-and-unify-it.patch
Patch278: mm-vmscan-zone_reclaim-dont-use-insane-swap_cluster_max.patch
Patch279: mm-vmscan-kill-sc-swap_cluster_max.patch
Patch280: mm-vmscan-make-consistent-of-reclaim-bale-out-between-do_try_to_free_page-and-shrink_zone.patch
Patch281: mm-vmscan-do-not-evict-inactive-pages-when-skipping-an-active-list-scan.patch
Patch282: mm-stop-ptlock-enlarging-struct-page.patch
Patch283: ksm-three-remove_rmap_item_from_tree-cleanups.patch
Patch284: ksm-remove-redundancies-when-merging-page.patch
Patch285: ksm-cleanup-some-function-arguments.patch
Patch286: ksm-singly-linked-rmap_list.patch
Patch287: ksm-separate-stable_node.patch
Patch288: ksm-stable_node-point-to-page-and-back.patch
Patch289: ksm-fix-mlockfreed-to-munlocked.patch
Patch290: ksm-let-shared-pages-be-swappable.patch
Patch291: ksm-hold-anon_vma-in-rmap_item.patch
Patch292: ksm-take-keyhole-reference-to-page.patch
Patch293: ksm-share-anon-page-without-allocating.patch
Patch294: ksm-mem-cgroup-charge-swapin-copy.patch
Patch295: ksm-rmap_walk-to-remove_migation_ptes.patch
Patch296: ksm-memory-hotremove-migration-only.patch
Patch297: ksm-remove-unswappable-max_kernel_pages.patch
Patch298: ksm-fix-ksm-h-breakage-of-nommu-build.patch
Patch299: mm-Add-mm-tracepoint-definitions-to-kmem-h.patch
Patch300: mm-Add-anonynmous-page-mm-tracepoints.patch
Patch301: mm-Add-file-page-mm-tracepoints.patch
Patch302: mm-Add-page-reclaim-mm-tracepoints.patch
Patch303: mm-Add-file-page-writeback-mm-tracepoints.patch
Patch304: scsi-hpsa-new-driver.patch
Patch305: scsi-cciss-remove-pci-ids.patch
Patch306: quota-Move-definition-of-QFMT_OCFS2-to-linux-quota-h.patch
Patch307: quota-Implement-quota-format-with-64-bit-space-and-inode-limits.patch
Patch308: quota-ext3-Support-for-vfsv1-quota-format.patch
Patch309: quota-ext4-Support-for-64-bit-quota-format.patch
Patch310: kvm-core-x86-Add-user-return-notifiers.patch
Patch311: kvm-VMX-Move-MSR_KERNEL_GS_BASE-out-of-the-vmx-autoload-msr-area.patch
Patch312: kvm-x86-shared-msr-infrastructure.patch
Patch313: kvm-VMX-Use-shared-msr-infrastructure.patch
Patch314: redhat-excluding-Reverts-from-changelog-too.patch
Patch315: redhat-tagging-2-6-32-4-el6.patch
Patch316: redhat-updating-lastcommit-for-2-6-32-4.patch
Patch317: redhat-do-not-export-redhat-directory-contents.patch
Patch318: x86-Remove-the-CPU-cache-size-printk-s.patch
Patch319: x86-Remove-CPU-cache-size-output-for-non-Intel-too.patch
Patch320: x86-cpu-mv-display_cacheinfo-cpu_detect_cache_sizes.patch
Patch321: x86-Limit-the-number-of-processor-bootup-messages.patch
Patch322: init-Limit-the-number-of-per-cpu-calibration-bootup-messages.patch
Patch323: sched-Limit-the-number-of-scheduler-debug-messages.patch
Patch324: x86-Limit-number-of-per-cpu-TSC-sync-messages.patch
Patch325: x86-Remove-enabling-x2apic-message-for-every-CPU.patch
Patch326: x86-ucode-amd-Load-ucode-patches-once-and-not-separately-of-each-CPU.patch
Patch327: block-cfq-iosched-reduce-write-depth-only-if-sync-was-delayed.patch
Patch328: block-cfq-Optimization-for-close-cooperating-queue-searching.patch
Patch329: block-cfq-iosched-Get-rid-of-cfqq-wait_busy_done-flag.patch
Patch330: block-cfq-iosched-Take-care-of-corner-cases-of-group-losing-share-due-to-deletion.patch
Patch331: block-cfq-iosched-commenting-non-obvious-initialization.patch
Patch332: block-cfq-Remove-wait_request-flag-when-idle-time-is-being-deleted.patch
Patch333: block-Fix-a-CFQ-crash-in-for-2-6-33-branch-of-block-tree.patch
Patch334: block-cfq-set-workload-as-expired-if-it-doesn-t-have-any-slice-left.patch
Patch335: block-cfq-iosched-Remove-the-check-for-same-cfq-group-from-allow_merge.patch
Patch336: block-cfq-iosched-Get-rid-of-nr_groups.patch
Patch337: block-cfq-iosched-Remove-prio_change-logic-for-workload-selection.patch
Patch338: netdrv-ixgbe-add-support-for-82599-KR-and-update-to-latest-upstream.patch
Patch339: netdrv-bnx2x-update-to-1-52-1-5.patch
Patch340: netdrv-update-tg3-to-version-3-105.patch
Patch341: scsi-scsi_dh-Change-the-scsidh_activate-interface-to-be-asynchronous.patch
Patch342: scsi-scsi_dh-Make-rdac-hardware-handler-s-activate-async.patch
Patch343: scsi-scsi_dh-Make-hp-hardware-handler-s-activate-async.patch
Patch344: scsi-scsi_dh-Make-alua-hardware-handler-s-activate-async.patch
Patch345: block-Fix-topology-stacking-for-data-and-discard-alignment.patch
Patch346: dlm-always-use-GFP_NOFS.patch
Patch347: redhat-Some-storage-related-kernel-config-parameter-changes.patch
Patch348: scsi-eliminate-double-free.patch
Patch349: scsi-make-driver-PCI-legacy-I-O-port-free.patch
Patch350: gfs2-Fix-potential-race-in-glock-code.patch
Patch351: netdrv-cnic-fixes-for-RHEL6.patch
Patch352: netdrv-bnx2i-update-to-2-1-0.patch
Patch353: mm-hwpoison-backport-the-latest-patches-from-linux-2-6-33.patch
Patch354: fs-ext4-wait-for-log-to-commit-when-unmounting.patch
Patch355: cifs-NULL-out-tcon-pSesInfo-and-srvTcp-pointers-when-chasing-DFS-referrals.patch
Patch356: fusion-remove-unnecessary-printk.patch
Patch357: fusion-fix-for-incorrect-data-underrun.patch
Patch358: fusion-bump-version-to-3-04-13.patch
Patch359: ext4-make-trim-discard-optional-and-off-by-default.patch
Patch360: fat-make-discard-a-mount-option.patch
Patch361: fs-fs-writeback-Add-helper-function-to-start-writeback-if-idle.patch
Patch362: fs-ext4-flush-delalloc-blocks-when-space-is-low.patch
Patch363: scsi-scsi_dh_rdac-add-two-IBM-devices-to-rdac_dev_list.patch
Patch364: vfs-force-reval-of-target-when-following-LAST_BIND-symlinks.patch
Patch365: input-Add-support-for-adding-i8042-filters.patch
Patch366: input-dell-laptop-Update-rfkill-state-on-switch-change.patch
Patch367: sunrpc-Deprecate-support-for-site-local-addresses.patch
Patch368: sunrpc-Don-t-display-zero-scope-IDs.patch
Patch369: s390-cio-double-free-under-memory-pressure.patch
Patch370: s390-cio-device-recovery-stalls-after-multiple-hardware-events.patch
Patch371: s390-cio-device-recovery-fails-after-concurrent-hardware-changes.patch
Patch372: s390-cio-setting-a-device-online-or-offline-fails-for-unknown-reasons.patch
Patch373: s390-cio-incorrect-device-state-after-device-recognition-and-recovery.patch
Patch374: s390-cio-kernel-panic-after-unexpected-interrupt.patch
Patch375: s390-cio-initialization-of-I-O-devices-fails.patch
Patch376: s390-cio-not-operational-devices-cannot-be-deactivated.patch
Patch377: s390-cio-erratic-DASD-I-O-behavior.patch
Patch378: s390-cio-DASD-cannot-be-set-online.patch
Patch379: s390-cio-DASD-steal-lock-task-hangs.patch
Patch380: s390-cio-memory-leaks-when-checking-unusable-devices.patch
Patch381: s390-cio-deactivated-devices-can-cause-use-after-free-panic.patch
Patch382: nfs-NFS-update-to-2-6-33-part-1.patch
Patch383: nfs-NFS-update-to-2-6-33-part-2.patch
Patch384: nfs-NFS-update-to-2-6-33-part-3.patch
Patch385: nfs-fix-insecure-export-option.patch
Patch386: redhat-enable-NFS_V4_1.patch
Patch387: x86-Compile-mce-inject-module.patch
Patch388: modsign-Don-t-attempt-to-sign-a-module-if-there-are-no-key-files.patch
Patch389: scsi-cciss-hpsa-reassign-controllers.patch
Patch390: scsi-cciss-fix-spinlock-use.patch
Patch391: redhat-disabling-temporaly-DEVTMPFS.patch
Patch392: redhat-don-t-use-PACKAGE_VERSION-and-PACKAGE_RELEASE.patch
Patch393: redhat-tagging-2-6-32-5-el6.patch
Patch394: redhat-updating-lastcommit-for-2-6-32-5.patch
Patch395: redhat-add-symbol-to-look-on-while-building-modules-block.patch
Patch396: stable-signal-Fix-alternate-signal-stack-check.patch
Patch397: stable-SCSI-osd_protocol-h-Add-missing-include.patch
Patch398: stable-SCSI-megaraid_sas-fix-64-bit-sense-pointer-truncation.patch
Patch399: stable-ext4-fix-potential-buffer-head-leak-when-add_dirent_to_buf-returns-ENOSPC.patch
Patch400: stable-ext4-avoid-divide-by-zero-when-trying-to-mount-a-corrupted-file-system.patch
Patch401: stable-ext4-fix-the-returned-block-count-if-EXT4_IOC_MOVE_EXT-fails.patch
Patch402: stable-ext4-fix-lock-order-problem-in-ext4_move_extents.patch
Patch403: stable-ext4-fix-possible-recursive-locking-warning-in-EXT4_IOC_MOVE_EXT.patch
Patch404: stable-ext4-plug-a-buffer_head-leak-in-an-error-path-of-ext4_iget.patch
Patch405: stable-ext4-make-sure-directory-and-symlink-blocks-are-revoked.patch
Patch406: stable-ext4-fix-i_flags-access-in-ext4_da_writepages_trans_blocks.patch
Patch407: stable-ext4-journal-all-modifications-in-ext4_xattr_set_handle.patch
Patch408: stable-ext4-don-t-update-the-superblock-in-ext4_statfs.patch
Patch409: stable-ext4-fix-uninit-block-bitmap-initialization-when-s_meta_first_bg-is-non-zero.patch
Patch410: stable-ext4-fix-block-validity-checks-so-they-work-correctly-with-meta_bg.patch
Patch411: stable-ext4-avoid-issuing-unnecessary-barriers.patch
Patch412: stable-ext4-fix-error-handling-in-ext4_ind_get_blocks.patch
Patch413: stable-ext4-make-norecovery-an-alias-for-noload.patch
Patch414: stable-ext4-Fix-double-free-of-blocks-with-EXT4_IOC_MOVE_EXT.patch
Patch415: stable-ext4-initialize-moved_len-before-calling-ext4_move_extents.patch
Patch416: stable-ext4-move_extent_per_page-cleanup.patch
Patch417: stable-jbd2-Add-ENOMEM-checking-in-and-for-jbd2_journal_write_metadata_buffer.patch
Patch418: stable-ext4-Return-the-PTR_ERR-of-the-correct-pointer-in-setup_new_group_blocks.patch
Patch419: stable-ext4-Avoid-data-filesystem-corruption-when-write-fails-to-copy-data.patch
Patch420: stable-ext4-remove-blocks-from-inode-prealloc-list-on-failure.patch
Patch421: stable-ext4-ext4_get_reserved_space-must-return-bytes-instead-of-blocks.patch
Patch422: stable-ext4-quota-macros-cleanup.patch
Patch423: stable-ext4-fix-incorrect-block-reservation-on-quota-transfer.patch
Patch424: stable-ext4-Wait-for-proper-transaction-commit-on-fsync.patch
Patch425: stable-ext4-Fix-potential-fiemap-deadlock-mmap_sem-vs-i_data_sem.patch
Patch426: stable-USB-usb-storage-fix-bug-in-fill_inquiry.patch
Patch427: stable-USB-option-add-pid-for-ZTE.patch
Patch428: stable-firewire-ohci-handle-receive-packets-with-a-data-length-of-zero.patch
Patch429: stable-rcu-Prepare-for-synchronization-fixes-clean-up-for-non-NO_HZ-handling-of-completed-counter.patch
Patch430: stable-rcu-Fix-synchronization-for-rcu_process_gp_end-uses-of-completed-counter.patch
Patch431: stable-rcu-Fix-note_new_gpnum-uses-of-gpnum.patch
Patch432: stable-rcu-Remove-inline-from-forward-referenced-functions.patch
Patch433: stable-perf_event-Fix-invalid-type-in-ioctl-definition.patch
Patch434: stable-perf_event-Initialize-data-period-in-perf_swevent_hrtimer.patch
Patch435: stable-PM-Runtime-Fix-lockdep-warning-in-__pm_runtime_set_status.patch
Patch436: stable-sched-Check-for-an-idle-shared-cache-in-select_task_rq_fair.patch
Patch437: stable-sched-Fix-affinity-logic-in-select_task_rq_fair.patch
Patch438: stable-sched-Rate-limit-newidle.patch
Patch439: stable-sched-Fix-and-clean-up-rate-limit-newidle-code.patch
Patch440: stable-x86-amd-iommu-attach-devices-to-pre-allocated-domains-early.patch
Patch441: stable-x86-amd-iommu-un__init-iommu_setup_msi.patch
Patch442: stable-x86-Calgary-IOMMU-quirk-Find-nearest-matching-Calgary-while-walking-up-the-PCI-tree.patch
Patch443: stable-x86-Fix-iommu-nodac-parameter-handling.patch
Patch444: stable-x86-GART-pci-gart_64-c-Use-correct-length-in-strncmp.patch
Patch445: stable-x86-ASUS-P4S800-reboot-bios-quirk.patch
Patch446: stable-x86-apic-Enable-lapic-nmi-watchdog-on-AMD-Family-11h.patch
Patch447: stable-ssb-Fix-range-check-in-sprom-write.patch
Patch448: stable-ath5k-allow-setting-txpower-to-0.patch
Patch449: stable-ath5k-enable-EEPROM-checksum-check.patch
Patch450: stable-hrtimer-Fix-proc-timer_list-regression.patch
Patch451: stable-ALSA-hrtimer-Fix-lock-up.patch
Patch452: stable-KVM-x86-emulator-limit-instructions-to-15-bytes.patch
Patch453: stable-KVM-s390-Fix-prefix-register-checking-in-arch-s390-kvm-sigp-c.patch
Patch454: stable-KVM-s390-Make-psw-available-on-all-exits-not-just-a-subset.patch
Patch455: stable-KVM-fix-irq_source_id-size-verification.patch
Patch456: stable-KVM-x86-include-pvclock-MSRs-in-msrs_to_save.patch
Patch457: stable-x86-Prevent-GCC-4-4-x-pentium-mmx-et-al-function-prologue-wreckage.patch
Patch458: stable-x86-Use-maccumulate-outgoing-args-for-sane-mcount-prologues.patch
Patch459: stable-x86-mce-don-t-restart-timer-if-disabled.patch
Patch460: stable-x86-mce-Set-up-timer-unconditionally.patch
Patch461: stable-x86-Fix-duplicated-UV-BAU-interrupt-vector.patch
Patch462: stable-x86-Add-new-Intel-CPU-cache-size-descriptors.patch
Patch463: stable-x86-Fix-typo-in-Intel-CPU-cache-size-descriptor.patch
Patch464: stable-pata_hpt-37x-3x2n-fix-timing-register-masks-take-2.patch
Patch465: stable-V4L-DVB-Fix-test-in-copy_reg_bits.patch
Patch466: stable-bsdacct-fix-uid-gid-misreporting.patch
Patch467: stable-UBI-flush-wl-before-clearing-update-marker.patch
Patch468: stable-jbd2-don-t-wipe-the-journal-on-a-failed-journal-checksum.patch
Patch469: stable-USB-xhci-Add-correct-email-and-files-to-MAINTAINERS-entry.patch
Patch470: stable-USB-musb_gadget_ep0-fix-unhandled-endpoint-0-IRQs-again.patch
Patch471: stable-USB-option-c-add-support-for-D-Link-DWM-162-U5.patch
Patch472: stable-USB-usbtmc-repeat-usb_bulk_msg-until-whole-message-is-transfered.patch
Patch473: stable-USB-usb-storage-add-BAD_SENSE-flag.patch
Patch474: stable-USB-Close-usb_find_interface-race-v3.patch
Patch475: stable-pxa-em-x270-fix-usb-hub-power-up-reset-sequence.patch
Patch476: stable-hfs-fix-a-potential-buffer-overflow.patch
Patch477: stable-md-bitmap-protect-against-bitmap-removal-while-being-updated.patch
Patch478: stable-futex-Take-mmap_sem-for-get_user_pages-in-fault_in_user_writeable.patch
Patch479: stable-devpts_get_tty-should-validate-inode.patch
Patch480: stable-debugfs-fix-create-mutex-racy-fops-and-private-data.patch
Patch481: stable-Driver-core-fix-race-in-dev_driver_string.patch
Patch482: stable-Serial-Do-not-read-IIR-in-serial8250_start_tx-when-UART_BUG_TXEN.patch
Patch483: stable-mac80211-Fix-bug-in-computing-crc-over-dynamic-IEs-in-beacon.patch
Patch484: stable-mac80211-Fixed-bug-in-mesh-portal-paths.patch
Patch485: stable-mac80211-Revert-Use-correct-sign-for-mesh-active-path-refresh.patch
Patch486: stable-mac80211-fix-scan-abort-sanity-checks.patch
Patch487: stable-wireless-correctly-report-signal-value-for-IEEE80211_HW_SIGNAL_UNSPEC.patch
Patch488: stable-rtl8187-Fix-wrong-rfkill-switch-mask-for-some-models.patch
Patch489: stable-x86-Fix-bogus-warning-in-apic_noop-apic_write.patch
Patch490: stable-mm-hugetlb-fix-hugepage-memory-leak-in-mincore.patch
Patch491: stable-mm-hugetlb-fix-hugepage-memory-leak-in-walk_page_range.patch
Patch492: stable-powerpc-windfarm-Add-detection-for-second-cpu-pump.patch
Patch493: stable-powerpc-therm_adt746x-Record-pwm-invert-bit-at-module-load-time.patch
Patch494: stable-powerpc-Fix-usage-of-64-bit-instruction-in-32-bit-altivec-code.patch
Patch495: stable-drm-radeon-kms-Add-quirk-for-HIS-X1300-board.patch
Patch496: stable-drm-radeon-kms-handle-vblanks-properly-with-dpms-on.patch
Patch497: stable-drm-radeon-kms-fix-legacy-crtc2-dpms.patch
Patch498: stable-drm-radeon-kms-fix-vram-setup-on-rs600.patch
Patch499: stable-drm-radeon-kms-rs6xx-rs740-clamp-vram-to-aperture-size.patch
Patch500: stable-drm-ttm-Fix-build-failure-due-to-missing-struct-page.patch
Patch501: stable-drm-i915-Set-the-error-code-after-failing-to-insert-new-offset-into-mm-ht.patch
Patch502: stable-drm-i915-Add-the-missing-clonemask-for-display-port-on-Ironlake.patch
Patch503: stable-xen-xenbus-make-DEVICE_ATTR-s-static.patch
Patch504: stable-xen-re-register-runstate-area-earlier-on-resume.patch
Patch505: stable-xen-restore-runstate_info-even-if-have_vcpu_info_placement.patch
Patch506: stable-xen-correctly-restore-pfn_to_mfn_list_list-after-resume.patch
Patch507: stable-xen-register-timer-interrupt-with-IRQF_TIMER.patch
Patch508: stable-xen-register-runstate-on-secondary-CPUs.patch
Patch509: stable-xen-don-t-call-dpm_resume_noirq-with-interrupts-disabled.patch
Patch510: stable-xen-register-runstate-info-for-boot-CPU-early.patch
Patch511: stable-xen-call-clock-resume-notifier-on-all-CPUs.patch
Patch512: stable-xen-improve-error-handling-in-do_suspend.patch
Patch513: stable-xen-don-t-leak-IRQs-over-suspend-resume.patch
Patch514: stable-xen-use-iret-for-return-from-64b-kernel-to-32b-usermode.patch
Patch515: stable-xen-explicitly-create-destroy-stop_machine-workqueues-outside-suspend-resume-region.patch
Patch516: stable-Xen-balloon-fix-totalram_pages-counting.patch
Patch517: stable-xen-try-harder-to-balloon-up-under-memory-pressure.patch
Patch518: stable-slc90e66-fix-UDMA-handling.patch
Patch519: stable-tcp-Stalling-connections-Fix-timeout-calculation-routine.patch
Patch520: stable-ip_fragment-also-adjust-skb-truesize-for-packets-not-owned-by-a-socket.patch
Patch521: stable-b44-WOL-setup-one-bit-off-stack-corruption-kernel-panic-fix.patch
Patch522: stable-sparc64-Don-t-specify-IRQF_SHARED-for-LDC-interrupts.patch
Patch523: stable-sparc64-Fix-overly-strict-range-type-matching-for-PCI-devices.patch
Patch524: stable-sparc64-Fix-stack-debugging-IRQ-stack-regression.patch
Patch525: stable-sparc-Set-UTS_MACHINE-correctly.patch
Patch526: stable-b43legacy-avoid-PPC-fault-during-resume.patch
Patch527: stable-tracing-Fix-event-format-export.patch
Patch528: stable-ath9k-fix-tx-status-reporting.patch
Patch529: stable-mac80211-Fix-dynamic-power-save-for-scanning.patch
Patch530: stable-memcg-fix-memory-memsw-usage_in_bytes-for-root-cgroup.patch
Patch531: stable-thinkpad-acpi-fix-default-brightness_mode-for-R50e-R51.patch
Patch532: stable-thinkpad-acpi-preserve-rfkill-state-across-suspend-resume.patch
Patch533: stable-ipw2100-fix-rebooting-hang-with-driver-loaded.patch
Patch534: stable-matroxfb-fix-problems-with-display-stability.patch
Patch535: stable-acerhdf-add-new-BIOS-versions.patch
Patch536: stable-asus-laptop-change-light-sens-default-values.patch
Patch537: stable-vmalloc-conditionalize-build-of-pcpu_get_vm_areas.patch
Patch538: stable-ACPI-Use-the-ARB_DISABLE-for-the-CPU-which-model-id-is-less-than-0x0f.patch
Patch539: stable-net-Fix-userspace-RTM_NEWLINK-notifications.patch
Patch540: stable-ext3-Fix-data-filesystem-corruption-when-write-fails-to-copy-data.patch
Patch541: stable-V4L-DVB-13116-gspca-ov519-Webcam-041e-4067-added.patch
Patch542: stable-bcm63xx_enet-fix-compilation-failure-after-get_stats_count-removal.patch
Patch543: stable-x86-Under-BIOS-control-restore-AP-s-APIC_LVTTHMR-to-the-BSP-value.patch
Patch544: stable-drm-i915-Avoid-NULL-dereference-with-component_only-tv_modes.patch
Patch545: stable-drm-i915-PineView-only-has-LVDS-and-CRT-ports.patch
Patch546: stable-drm-i915-Fix-LVDS-stability-issue-on-Ironlake.patch
Patch547: stable-mm-sigbus-instead-of-abusing-oom.patch
Patch548: stable-ipvs-zero-usvc-and-udest.patch
Patch549: stable-jffs2-Fix-long-standing-bug-with-symlink-garbage-collection.patch
Patch550: stable-intel-iommu-ignore-page-table-validation-in-pass-through-mode.patch
Patch551: stable-netfilter-xtables-document-minimal-required-version.patch
Patch552: stable-perf_event-Fix-incorrect-range-check-on-cpu-number.patch
Patch553: stable-implement-early_io-re-un-map-for-ia64.patch
Patch554: stable-SCSI-ipr-fix-EEH-recovery.patch
Patch555: stable-SCSI-qla2xxx-dpc-thread-can-execute-before-scsi-host-has-been-added.patch
Patch556: stable-SCSI-st-fix-mdata-page_order-handling.patch
Patch557: stable-SCSI-fc-class-fix-fc_transport_init-error-handling.patch
Patch558: stable-sched-Fix-task_hot-test-order.patch
Patch559: stable-x86-cpuid-Add-volatile-to-asm-in-native_cpuid.patch
Patch560: stable-sched-Select_task_rq_fair-must-honour-SD_LOAD_BALANCE.patch
Patch561: stable-clockevents-Prevent-clockevent_devices-list-corruption-on-cpu-hotplug.patch
Patch562: stable-pata_hpt3x2n-fix-clock-turnaround.patch
Patch563: stable-pata_cmd64x-fix-overclocking-of-UDMA0-2-modes.patch
Patch564: stable-ASoC-wm8974-fix-a-wrong-bit-definition.patch
Patch565: stable-sound-sgio2audio-pdaudiocf-usb-audio-initialize-PCM-buffer.patch
Patch566: stable-ALSA-hda-Fix-missing-capsrc_nids-for-ALC88x.patch
Patch567: stable-acerhdf-limit-modalias-matching-to-supported.patch
Patch568: stable-ACPI-EC-Fix-MSI-DMI-detection.patch
Patch569: stable-ACPI-Use-the-return-result-of-ACPI-lid-notifier-chain-correctly.patch
Patch570: stable-powerpc-Handle-VSX-alignment-faults-correctly-in-little-endian-mode.patch
Patch571: stable-ASoC-Do-not-write-to-invalid-registers-on-the-wm9712.patch
Patch572: stable-drm-radeon-fix-build-on-64-bit-with-some-compilers.patch
Patch573: stable-USB-emi62-fix-crash-when-trying-to-load-EMI-6-2-firmware.patch
Patch574: stable-USB-option-support-hi-speed-for-modem-Haier-CE100.patch
Patch575: stable-USB-Fix-a-bug-on-appledisplay-c-regarding-signedness.patch
Patch576: stable-USB-musb-gadget_ep0-avoid-SetupEnd-interrupt.patch
Patch577: stable-Bluetooth-Prevent-ill-timed-autosuspend-in-USB-driver.patch
Patch578: stable-USB-rename-usb_configure_device.patch
Patch579: stable-USB-fix-bugs-in-usb_-de-authorize_device.patch
Patch580: stable-drivers-net-usb-Correct-code-taking-the-size-of-a-pointer.patch
Patch581: stable-x86-SGI-UV-Fix-writes-to-led-registers-on-remote-uv-hubs.patch
Patch582: stable-md-Fix-unfortunate-interaction-with-evms.patch
Patch583: stable-dma-at_hdmac-correct-incompatible-type-for-argument-1-of-spin_lock_bh.patch
Patch584: stable-dma-debug-Do-not-add-notifier-when-dma-debugging-is-disabled.patch
Patch585: stable-dma-debug-Fix-bug-causing-build-warning.patch
Patch586: stable-x86-amd-iommu-Fix-initialization-failure-panic.patch
Patch587: stable-ioat3-fix-p-disabled-q-continuation.patch
Patch588: stable-ioat2-3-put-channel-hardware-in-known-state-at-init.patch
Patch589: stable-KVM-MMU-remove-prefault-from-invlpg-handler.patch
Patch590: stable-KVM-LAPIC-make-sure-IRR-bitmap-is-scanned-after-vm-load.patch
Patch591: stable-Libertas-fix-buffer-overflow-in-lbs_get_essid.patch
Patch592: stable-iwmc3200wifi-fix-array-out-of-boundary-access.patch
Patch593: stable-mac80211-fix-propagation-of-failed-hardware-reconfigurations.patch
Patch594: stable-mac80211-fix-WMM-AP-settings-application.patch
Patch595: stable-mac80211-Fix-IBSS-merge.patch
Patch596: stable-cfg80211-fix-race-between-deauth-and-assoc-response.patch
Patch597: stable-ath5k-fix-SWI-calibration-interrupt-storm.patch
Patch598: stable-ath9k-wake-hardware-for-interface-IBSS-AP-Mesh-removal.patch
Patch599: stable-ath9k-Fix-TX-queue-draining.patch
Patch600: stable-ath9k-fix-missed-error-codes-in-the-tx-status-check.patch
Patch601: stable-ath9k-wake-hardware-during-AMPDU-TX-actions.patch
Patch602: stable-ath9k-fix-suspend-by-waking-device-prior-to-stop.patch
Patch603: stable-ath9k_hw-Fix-possible-OOB-array-indexing-in-gen_timer_index-on-64-bit.patch
Patch604: stable-ath9k_hw-Fix-AR_GPIO_INPUT_EN_VAL_BT_PRIORITY_BB-and-its-shift-value-in-0x4054.patch
Patch605: stable-iwl3945-disable-power-save.patch
Patch606: stable-iwl3945-fix-panic-in-iwl3945-driver.patch
Patch607: stable-iwlwifi-fix-EEPROM-OTP-reading-endian-annotations-and-a-bug.patch
Patch608: stable-iwlwifi-fix-more-eeprom-endian-bugs.patch
Patch609: stable-iwlwifi-fix-40MHz-operation-setting-on-cards-that-do-not-allow-it.patch
Patch610: stable-mac80211-fix-race-with-suspend-and-dynamic_ps_disable_work.patch
Patch611: stable-NOMMU-Optimise-away-the-dac_-mmap_min_addr-tests.patch
Patch612: stable-sysctl_max_map_count-should-be-non-negative.patch
Patch613: stable-kernel-sysctl-c-fix-the-incomplete-part-of-sysctl_max_map_count-should-be-non-negative-patch.patch
Patch614: stable-V4L-DVB-13596-ov511-c-typo-lock-unlock.patch
Patch615: stable-x86-ptrace-make-genregs-32-_get-set-more-robust.patch
Patch616: stable-memcg-avoid-oom-killing-innocent-task-in-case-of-use_hierarchy.patch
Patch617: stable-e100-Fix-broken-cbs-accounting-due-to-missing-memset.patch
Patch618: stable-ipv6-reassembly-use-seperate-reassembly-queues-for-conntrack-and-local-delivery.patch
Patch619: stable-netfilter-fix-crashes-in-bridge-netfilter-caused-by-fragment-jumps.patch
Patch620: stable-hwmon-sht15-Off-by-one-error-in-array-index-incorrect-constants.patch
Patch621: stable-b43-avoid-PPC-fault-during-resume.patch
Patch622: stable-Keys-KEYCTL_SESSION_TO_PARENT-needs-TIF_NOTIFY_RESUME-architecture-support.patch
Patch623: stable-sched-Fix-balance-vs-hotplug-race.patch
Patch624: stable-drm-radeon-kms-fix-crtc-vblank-update-for-r600.patch
Patch625: stable-drm-disable-all-the-possible-outputs-crtcs-before-entering-KMS-mode.patch
Patch626: stable-orinoco-fix-GFP_KERNEL-in-orinoco_set_key-with-interrupts-disabled.patch
Patch627: stable-udf-Try-harder-when-looking-for-VAT-inode.patch
Patch628: stable-Add-unlocked-version-of-inode_add_bytes-function.patch
Patch629: stable-quota-decouple-fs-reserved-space-from-quota-reservation.patch
Patch630: stable-ext4-Convert-to-generic-reserved-quota-s-space-management.patch
Patch631: stable-ext4-fix-sleep-inside-spinlock-issue-with-quota-and-dealloc-14739.patch
Patch632: stable-x86-msr-Unify-rdmsr_on_cpus-wrmsr_on_cpus.patch
Patch633: stable-cpumask-use-modern-cpumask-style-in-drivers-edac-amd64_edac-c.patch
Patch634: stable-amd64_edac-unify-MCGCTL-ECC-switching.patch
Patch635: stable-x86-msr-Add-support-for-non-contiguous-cpumasks.patch
Patch636: stable-x86-msr-msrs_alloc-free-for-CONFIG_SMP-n.patch
Patch637: stable-amd64_edac-fix-driver-instance-freeing.patch
Patch638: stable-amd64_edac-make-driver-loading-more-robust.patch
Patch639: stable-amd64_edac-fix-forcing-module-load-unload.patch
Patch640: stable-sched-Sched_rt_periodic_timer-vs-cpu-hotplug.patch
Patch641: stable-ext4-Update-documentation-to-correct-the-inode_readahead_blks-option-name.patch
Patch642: stable-lguest-fix-bug-in-setting-guest-GDT-entry.patch
Patch643: stable-rt2x00-Disable-powersaving-for-rt61pci-and-rt2800pci.patch
Patch644: stable-generic_permission-MAY_OPEN-is-not-write-access.patch
Patch645: redhat-fix-typo-while-disabling-CONFIG_CPU_SUP_CENTAUR.patch
Patch646: redhat-check-if-patchutils-is-installed-before-creating-patches.patch
Patch647: redhat-do-a-basic-sanity-check-to-verify-the-modules-are-being-signed.patch
Patch648: redhat-Fix-kABI-dependency-generation.patch
Patch649: tpm-autoload-tpm_tis-driver.patch
Patch650: x86-mce-fix-confusion-between-bank-attributes-and-mce-attributes.patch
Patch651: netdrv-be2net-update-be2net-driver-to-latest-upstream.patch
Patch652: s390x-tape-incomplete-device-removal.patch
Patch653: s390-kernel-improve-code-generated-by-atomic-operations.patch
Patch654: x86-AMD-Fix-stale-cpuid4_info-shared_map-data-in-shared_cpu_map-cpumasks.patch
Patch655: nfs-fix-oops-in-nfs_rename.patch
Patch656: modsign-Remove-Makefile-modpost-qualifying-message-for-module-sign-failure.patch
Patch657: redhat-disable-framepointer-build-by-default.patch
Patch658: redhat-use-sysconf-_SC_PAGESIZE-instead-of-getpagesize.patch
Patch659: redhat-tagging-2-6-32-6-el6.patch
Patch660: redhat-updating-lastcommit-for-2-6-32-6.patch
Patch661: kvm-Dont-pass-kvm_run-arguments.patch
Patch662: kvm-Call-pic_clear_isr-on-pic-reset-to-reuse-logic-there.patch
Patch663: kvm-Move-irq-sharing-information-to-irqchip-level.patch
Patch664: kvm-Change-irq-routing-table-to-use-gsi-indexed-array.patch
Patch665: kvm-Maintain-back-mapping-from-irqchip-pin-to-gsi.patch
Patch666: kvm-Move-irq-routing-data-structure-to-rcu-locking.patch
Patch667: kvm-Move-irq-ack-notifier-list-to-arch-independent-code.patch
Patch668: kvm-Convert-irq-notifiers-lists-to-RCU-locking.patch
Patch669: kvm-Move-IO-APIC-to-its-own-lock.patch
Patch670: kvm-Drop-kvm-irq_lock-lock-from-irq-injection-path.patch
Patch671: kvm-Add-synchronize_srcu_expedited.patch
Patch672: kvm-rcu-Add-synchronize_srcu_expedited-to-the-rcutorture-test-suite.patch
Patch673: kvm-rcu-Add-synchronize_srcu_expedited-to-the-documentation.patch
Patch674: kvm-rcu-Enable-synchronize_sched_expedited-fastpath.patch
Patch675: kvm-modify-memslots-layout-in-struct-kvm.patch
Patch676: kvm-modify-alias-layout-in-x86s-struct-kvm_arch.patch
Patch677: kvm-split-kvm_arch_set_memory_region-into-prepare-and-commit.patch
Patch678: kvm-introduce-gfn_to_pfn_memslot.patch
Patch679: kvm-use-gfn_to_pfn_memslot-in-kvm_iommu_map_pages.patch
Patch680: kvm-introduce-kvm-srcu-and-convert-kvm_set_memory_region-to-SRCU-update.patch
Patch681: kvm-use-SRCU-for-dirty-log.patch
Patch682: kvm-x86-switch-kvm_set_memory_alias-to-SRCU-update.patch
Patch683: kvm-convert-io_bus-to-SRCU.patch
Patch684: kvm-switch-vcpu-context-to-use-SRCU.patch
Patch685: kvm-convert-slots_lock-to-a-mutex.patch
Patch686: kvm-Bump-maximum-vcpu-count-to-64.patch
Patch687: kvm-avoid-taking-ioapic-mutex-for-non-ioapic-EOIs.patch
Patch688: kvm-VMX-Use-macros-instead-of-hex-value-on-cr0-initialization.patch
Patch689: kvm-SVM-Reset-cr0-properly-on-vcpu-reset.patch
Patch690: kvm-SVM-init_vmcb-remove-redundant-save-cr0-initialization.patch
Patch691: kvm-fix-kvmclock-adjust-offset-ioctl-to-match-upstream.patch
Patch692: kvm-x86-Add-KVM_GET-SET_VCPU_EVENTS.patch
Patch693: kvm-x86-Extend-KVM_SET_VCPU_EVENTS-with-selective-updates.patch
Patch694: kvm-remove-pre_task_link-setting-in-save_state_to_tss16.patch
Patch695: kvm-SVM-Move-INTR-vmexit-out-of-atomic-code.patch
Patch696: kvm-SVM-Notify-nested-hypervisor-of-lost-event-injections.patch
Patch697: kvm-SVM-remove-needless-mmap_sem-acquision-from-nested_svm_map.patch
Patch698: kvm-VMX-Disable-unrestricted-guest-when-EPT-disabled.patch
Patch699: kvm-x86-disallow-multiple-KVM_CREATE_IRQCHIP.patch
Patch700: kvm-x86-disallow-KVM_-SET-GET-_LAPIC-without-allocated-in-kernel-lapic.patch
Patch701: kvm-x86-disable-paravirt-mmu-reporting.patch
Patch702: kvm-Allow-internal-errors-reported-to-userspace-to-carry-extra-data.patch
Patch703: kvm-VMX-Report-unexpected-simultaneous-exceptions-as-internal-errors.patch
Patch704: kvm-fix-lock-imbalance-in-kvm_-_irq_source_id.patch
Patch705: kvm-only-clear-irq_source_id-if-irqchip-is-present.patch
Patch706: kvm-Fix-possible-circular-locking-in-kvm_vm_ioctl_assign_device.patch
Patch707: block-Fix-incorrect-alignment-offset-reporting-and-update-documentation.patch
Patch708: block-Correct-handling-of-bottom-device-misaligment.patch
Patch709: block-Fix-discard-alignment-calculation-and-printing.patch
Patch710: block-bdev_stack_limits-wrapper.patch
Patch711: dm-Fix-device-mapper-topology-stacking.patch
Patch712: block-Stop-using-byte-offsets.patch
Patch713: dm-add-feature-flags-to-reduce-future-kABI-impact.patch
Patch714: netdrv-igb-Update-igb-driver-to-support-Barton-Hills.patch
Patch715: redhat-Don-t-compile-DECNET.patch
Patch716: redhat-fs-don-t-build-freevxfs.patch
Patch717: redhat-disable-KVM-on-non-x86_64-arches.patch
Patch718: gfs-GFS2-Fix-up-system-xattrs.patch
Patch719: gfs-VFS-Add-forget_all_cached_acls.patch
Patch720: gfs-GFS2-Use-forget_all_cached_acls.patch
Patch721: gfs-GFS2-Use-gfs2_set_mode-instead-of-munge_mode.patch
Patch722: gfs-GFS2-Clean-up-ACLs.patch
Patch723: gfs-GFS2-Add-cached-ACLs-support.patch
Patch724: gfs-VFS-Use-GFP_NOFS-in-posix_acl_from_xattr.patch
Patch725: gfs-GFS2-Fix-gfs2_xattr_acl_chmod.patch
Patch726: gfs2-Fix-o-meta-mounts-for-subsequent-mounts.patch
Patch727: gfs2-Alter-arguments-of-gfs2_quota-statfs_sync.patch
Patch728: gfs2-Hook-gfs2_quota_sync-into-VFS-via-gfs2_quotactl_ops.patch
Patch729: gfs2-Remove-obsolete-code-in-quota-c.patch
Patch730: gfs2-Add-get_xstate-quota-function.patch
Patch731: gfs2-Add-proper-error-reporting-to-quota-sync-via-sysfs.patch
Patch732: gfs2-Remove-constant-argument-from-qdsb_get.patch
Patch733: gfs2-Remove-constant-argument-from-qd_get.patch
Patch734: gfs2-Clean-up-gfs2_adjust_quota-and-do_glock.patch
Patch735: gfs2-Add-get_xquota-support.patch
Patch736: gfs2-Add-set_xquota-support.patch
Patch737: gfs2-Improve-statfs-and-quota-usability.patch
Patch738: gfs2-remove-division-from-new-statfs-code.patch
Patch739: gfs2-add-barrier-nobarrier-mount-options.patch
Patch740: gfs2-only-show-nobarrier-option-on-proc-mounts-when-the-option-is-active.patch
Patch741: gfs-GFS2-Fix-lock-ordering-in-gfs2_check_blk_state.patch
Patch742: gfs-GFS2-Fix-locking-bug-in-rename.patch
Patch743: gfs-GFS2-Ensure-uptodate-inode-size-when-using-O_APPEND.patch
Patch744: gfs-GFS2-Fix-glock-refcount-issues.patch
Patch745: powerpc-pseries-Add-extended_cede_processor-helper-function.patch
Patch746: powerpc-pseries-Add-hooks-to-put-the-CPU-into-an-appropriate-offline-state.patch
Patch747: powerpc-Kernel-handling-of-Dynamic-Logical-Partitioning.patch
Patch748: powerpc-sysfs-cpu-probe-release-files.patch
Patch749: powerpc-CPU-DLPAR-handling.patch
Patch750: powerpc-Add-code-to-online-offline-CPUs-of-a-DLPAR-node.patch
Patch751: powerpc-cpu-allocation-deallocation-process.patch
Patch752: powerpc-pseries-Correct-pseries-dlpar-c-build-break-without-CONFIG_SMP.patch
Patch753: net-dccp-fix-module-load-dependency-btw-dccp_probe-and-dccp.patch
Patch754: drm-drm-edid-update-to-2-6-33-EDID-parser-code.patch
Patch755: drm-mm-patch-drm-core-memory-range-manager-up-to-2-6-33.patch
Patch756: drm-ttm-rollup-upstream-TTM-fixes.patch
Patch757: drm-unlocked-ioctl-support-for-core-macro-fixes.patch
Patch758: drm-add-new-userspace-core-drm-interfaces-from-2-6-33.patch
Patch759: drm-remove-address-mask-param-for-drm_pci_alloc.patch
Patch760: drm-kms-rollup-KMS-core-and-helper-changes-to-2-6-33.patch
Patch761: drm-radeon-intel-realign-displayport-helper-code-with-upstream.patch
Patch762: drm-i915-bring-Intel-DRM-KMS-driver-up-to-2-6-33.patch
Patch763: drm-radeon-kms-update-to-2-6-33-without-TTM-API-changes.patch
Patch764: drm-ttm-validation-API-changes-ERESTART-fixes.patch
Patch765: drm-nouveau-update-to-2-6-33-level.patch
Patch766: x86-allow-fbdev-primary-video-code-on-64-bit.patch
Patch767: offb-add-support-for-framebuffer-handoff-to-offb.patch
Patch768: drm-minor-printk-fixes-from-upstream.patch
Patch769: redhat-tagging-2-6-32-7-el6.patch
Patch770: redhat-updating-lastcommit-for-2-6-32-7.patch
Patch771: build-Revert-redhat-disabling-temporaly-DEVTMPFS.patch
Patch772: redhat-tagging-2-6-32-8-el6.patch
Patch773: redhat-updating-lastcommit-for-2-6-32-8.patch
Patch774: serial-8250-add-support-for-DTR-DSR-hardware-flow-control.patch
Patch775: s390x-qeth-Support-for-HiperSockets-Network-Traffic-Analyzer.patch
Patch776: s390-qeth-fix-packet-loss-if-TSO-is-switched-on.patch
Patch777: s390x-tape-Add-pr_fmt-macro-to-all-tape-source-files.patch
Patch778: net-dccp-modify-how-dccp-creates-slab-caches-to-prevent-bug-halt-in-SLUB.patch
Patch779: scsi-sync-fcoe-with-upstream.patch
Patch780: block-Honor-the-gfp_mask-for-alloc_page-in-blkdev_issue_discard.patch
Patch781: sound-ALSA-HDA-driver-update-2009-12-15-2.patch
Patch782: scsi-mpt2sas-use-sas-address-instead-of-handle-as-a-lookup.patch
Patch783: scsi-mpt2sas-fix-expander-remove-fail.patch
Patch784: scsi-mpt2sas-check-for-valid-response-info.patch
Patch785: scsi-mpt2sas-new-device-SAS2208-support.patch
Patch786: scsi-mpt2sas-adding-MPI-Headers-revision-L.patch
Patch787: scsi-mpt2sas-stop-driver-when-firmware-encounters-faults.patch
Patch788: scsi-mpt2sas-fix-some-comments.patch
Patch789: scsi-mpt2sas-add-command-line-option-diag_buffer_enable.patch
Patch790: scsi-mpt2sas-add-extended-type-for-diagnostic-buffer-support.patch
Patch791: scsi-mpt2sas-add-TimeStamp-support-when-sending-ioc_init.patch
Patch792: scsi-mpt2sas-limit-the-max_depth-to-32-for-SATA-devices.patch
Patch793: scsi-mpt2sas-add-new-info-messages-for-IR-and-Expander-events.patch
Patch794: scsi-mpt2sas-retrieve-the-ioc-facts-prior-to-putting-the-controller-into-READY-state.patch
Patch795: scsi-mpt2sas-return-DID_TRANSPORT_DISRUPTED-in-nexus-loss-and-SCSI_MLQUEUE_DEVICE_BUSY-if-device-is-busy.patch
Patch796: scsi-mpt2sas-mpt2sas_base_get_sense_buffer_dma-returns-little-endian.patch
Patch797: scsi-mpt2sas-fix-PPC-endian-bug.patch
Patch798: scsi-mpt2sas-freeze-the-sdev-IO-queue-when-firmware-sends-internal-device-reset.patch
Patch799: scsi-mpt2sas-add-support-for-RAID-Action-System-Shutdown-Initiated-at-OS-Shutdown.patch
Patch800: scsi-mpt2sas-don-t-update-links-nor-unblock-device-at-no-link-rate-change.patch
Patch801: scsi-mpt2sas-Bump-version-03-100-03-00.patch
Patch802: scsi-megaraid-upgrade-to-4-17.patch
Patch803: uv-x86-SGI-Dont-track-GRU-space-in-PAT.patch
Patch804: uv-x86-mm-Call-is_untracked_pat_range-rather-than-is_ISA_range.patch
Patch805: uv-x86-mm-is_untracked_pat_range-takes-a-normal-semiclosed-range.patch
Patch806: uv-x86-platform-Change-is_untracked_pat_range-to-bool.patch
Patch807: uv-x86-Change-is_ISA_range-into-an-inline-function.patch
Patch808: uv-x86-mm-Correct-the-implementation-of-is_untracked_pat_range.patch
Patch809: uv-x86-RTC-Rename-generic_interrupt-to-x86_platform_ipi.patch
Patch810: uv-x86-RTC-Always-enable-RTC-clocksource.patch
Patch811: uv-x86-SGI-Fix-irq-affinity-for-hub-based-interrupts.patch
Patch812: uv-x86-apic-Move-SGI-UV-functionality-out-of-generic-IO-APIC-code.patch
Patch813: uv-x86-irq-Allow-0xff-for-proc-irq-n-smp_affinity-on-an-8-cpu-system.patch
Patch814: uv-x86-Remove-move_cleanup_count-from-irq_cfg.patch
Patch815: uv-x86-irq-Check-move_in_progress-before-freeing-the-vector-mapping.patch
Patch816: uv-xpc-needs-to-provide-an-abstraction-for-uv_gpa.patch
Patch817: uv-x86-update-XPC-to-handle-updated-BIOS-interface.patch
Patch818: uv-x86-xpc-NULL-deref-when-mesq-becomes-empty.patch
Patch819: uv-x86-xpc_make_first_contact-hang-due-to-not-accepting-ACTIVE-state.patch
Patch820: uv-x86-XPC-receive-message-reuse-triggers-invalid-BUG_ON.patch
Patch821: uv-XPC-pass-nasid-instead-of-nid-to-gru_create_message_queue.patch
Patch822: gru-GRU-Rollup-patch.patch
Patch823: uv-React-2-6-32-y-isolcpus-broken-in-2-6-32-y-kernel.patch
Patch824: nfs-nfsd-make-sure-data-is-on-disk-before-calling-fsync.patch
Patch825: nfs-sunrpc-fix-peername-failed-on-closed-listener.patch
Patch826: nfs-SUNRPC-Fix-up-an-error-return-value-in-gss_import_sec_context_kerberos.patch
Patch827: nfs-SUNRPC-Fix-the-return-value-in-gss_import_sec_context.patch
Patch828: nfs-sunrpc-on-successful-gss-error-pipe-write-don-t-return-error.patch
Patch829: nfs-sunrpc-fix-build-time-warning.patch
Patch830: scsi-bfa-update-from-2-1-2-0-to-2-1-2-1.patch
Patch831: scsi-qla2xxx-Update-support-for-FC-FCoE-HBA-CNA.patch
Patch832: irq-Expose-the-irq_desc-node-as-proc-irq-node.patch
Patch833: cgroups-fix-for-kernel-BUG-at-kernel-cgroup-c-790.patch
Patch834: tracing-tracepoint-Add-signal-tracepoints.patch
Patch835: block-direct-io-cleanup-blockdev_direct_IO-locking.patch
Patch836: pci-PCIe-AER-honor-ACPI-HEST-FIRMWARE-FIRST-mode.patch
Patch837: x86-Add-kernel-pagefault-tracepoint-for-x86-x86_64.patch
Patch838: fs-xfs-2-6-33-updates.patch
Patch839: x86-dell-wmi-Add-support-for-new-Dell-systems.patch
Patch840: x86-core-make-LIST_POISON-less-deadly.patch
Patch841: kvm-fix-cleanup_srcu_struct-on-vm-destruction.patch
Patch842: redhat-tagging-2-6-32-9-el6.patch
Patch843: redhat-updating-lastcommit-for-2-6-32-9.patch
Patch844: scsi-scsi_transport_fc-Allow-LLD-to-reset-FC-BSG-timeout.patch
Patch845: s390x-zfcp-introduce-BSG-timeout-callback.patch
Patch846: s390x-zfcp-set-HW-timeout-requested-by-BSG-request.patch
Patch847: redhat-config-increase-printk-buffer.patch
Patch848: netdrv-qlge-update-to-upstream-version-v1-00-00-23-00-00-01.patch
Patch849: gfs-Add-quota-netlink-support.patch
Patch850: gfs-Use-dquot_send_warning.patch
Patch851: netdrv-e1000e-update-to-the-latest-upstream.patch
Patch852: x86-Disable-Memory-hot-add-on-x86-32-bit.patch
Patch853: utrace-fix-utrace_maybe_reap-vs-find_matching_engine-race.patch
Patch854: perf-add-kernel-internal-interface.patch
Patch855: perf-improve-error-reporting.patch
Patch856: perf-Add-a-callback-to-perf-events.patch
Patch857: perf-Allow-for-custom-overflow-handlers.patch
Patch858: perf-Fix-PERF_FORMAT_GROUP-scale-info.patch
Patch859: perf-Fix-event-scaling-for-inherited-counters.patch
Patch860: perf-Fix-locking-for-PERF_FORMAT_GROUP.patch
Patch861: perf-Use-overflow-handler-instead-of-the-event-callback.patch
Patch862: perf-Remove-the-event-callback-from-perf-events.patch
Patch863: s390x-ptrace-dont-abuse-PT_PTRACED.patch
Patch864: s390x-fix-loading-of-PER-control-registers-for-utrace.patch
Patch865: scsi-aic79xx-check-for-non-NULL-scb-in-ahd_handle_nonpkt_busfree.patch
Patch866: sound-Fix-SPDIF-In-for-AD1988-codecs-add-Intel-Cougar-IDs.patch
Patch867: x86-Force-irq-complete-move-during-cpu-offline.patch
Patch868: netdrv-vxge-fix-issues-found-in-Neterion-testing.patch
Patch869: mm-mmap-don-t-return-ENOMEM-when-mapcount-is-temporarily-exceeded-in-munmap.patch
Patch870: redhat-tagging-2-6-32-10-el6.patch
Patch871: redhat-updating-lastcommit-for-2-6-32-10.patch
Patch872: stable-untangle-the-do_mremap-mess.patch
Patch873: stable-fasync-split-fasync_helper-into-separate-add-remove-functions.patch
Patch874: stable-ASoC-fix-params_rate-macro-use-in-several-codecs.patch
Patch875: stable-modules-Skip-empty-sections-when-exporting-section-notes.patch
Patch876: stable-exofs-simple_write_end-does-not-mark_inode_dirty.patch
Patch877: stable-Revert-x86-Side-step-lguest-problem-by-only-building-cmpxchg8b_emu-for-pre-Pentium.patch
Patch878: stable-rtc_cmos-convert-shutdown-to-new-pnp_driver-shutdown.patch
Patch879: stable-drivers-cpuidle-governors-menu-c-fix-undefined-reference-to-__udivdi3.patch
Patch880: stable-lib-rational-c-needs-module-h.patch
Patch881: stable-dma-debug-allow-DMA_BIDIRECTIONAL-mappings-to-be-synced-with-DMA_FROM_DEVICE-and.patch
Patch882: stable-kernel-signal-c-fix-kernel-information-leak-with-print-fatal-signals-1.patch
Patch883: stable-mmc_block-add-dev_t-initialization-check.patch
Patch884: stable-mmc_block-fix-probe-error-cleanup-bug.patch
Patch885: stable-mmc_block-fix-queue-cleanup.patch
Patch886: stable-ALSA-ac97-Add-Dell-Dimension-2400-to-Headphone-Line-Jack-Sense-blacklist.patch
Patch887: stable-ALSA-atiixp-Specify-codec-for-Foxconn-RC4107MA-RS2.patch
Patch888: stable-ASoC-Fix-WM8350-DSP-mode-B-configuration.patch
Patch889: stable-netfilter-ebtables-enforce-CAP_NET_ADMIN.patch
Patch890: stable-netfilter-nf_ct_ftp-fix-out-of-bounds-read-in-update_nl_seq.patch
Patch891: stable-hwmon-coretemp-Fix-TjMax-for-Atom-N450-D410-D510-CPUs.patch
Patch892: stable-hwmon-adt7462-Fix-pin-28-monitoring.patch
Patch893: stable-quota-Fix-dquot_transfer-for-filesystems-different-from-ext4.patch
Patch894: stable-xen-fix-hang-on-suspend.patch
Patch895: stable-iwlwifi-fix-iwl_queue_used-bug-when-read_ptr-write_ptr.patch
Patch896: stable-ath5k-Fix-eeprom-checksum-check-for-custom-sized-eeproms.patch
Patch897: stable-cfg80211-fix-syntax-error-on-user-regulatory-hints.patch
Patch898: stable-iwl-off-by-one-bug.patch
Patch899: stable-mac80211-add-missing-sanity-checks-for-action-frames.patch
Patch900: stable-libertas-Remove-carrier-signaling-from-the-scan-code.patch
Patch901: stable-kernel-sysctl-c-fix-stable-merge-error-in-NOMMU-mmap_min_addr.patch
Patch902: stable-mac80211-fix-skb-buffering-issue-and-fixes-to-that.patch
Patch903: stable-fix-braindamage-in-audit_tree-c-untag_chunk.patch
Patch904: stable-fix-more-leaks-in-audit_tree-c-tag_chunk.patch
Patch905: stable-module-handle-ppc64-relocating-kcrctabs-when-CONFIG_RELOCATABLE-y.patch
Patch906: stable-ipv6-skb_dst-can-be-NULL-in-ipv6_hop_jumbo.patch
Patch907: misc-Revert-stable-module-handle-ppc64-relocating-kcrctabs-when-CONFIG_RELOCATABLE-y.patch
Patch908: netdrv-e1000e-enhance-frame-fragment-detection.patch
Patch909: redhat-kABI-internal-only-files.patch
Patch910: drm-bring-RHEL6-radeon-drm-up-to-2-6-33-rc4-5-level.patch
Patch911: s390x-qeth-set-default-BLKT-settings-dependend-on-OSA-hw-level.patch
Patch912: block-dm-replicator-documentation-and-module-registry.patch
Patch913: block-dm-replicator-replication-log-and-site-link-handler-interfaces-and-main-replicator-module.patch
Patch914: block-dm-replicator-ringbuffer-replication-log-handler.patch
Patch915: block-dm-replicator-blockdev-site-link-handler.patch
Patch916: block-dm-raid45-add-raid45-target.patch
Patch917: dm-dm-raid45-export-missing-dm_rh_inc.patch
Patch918: kdump-Remove-the-32MB-limitation-for-crashkernel.patch
Patch919: redhat-tagging-2-6-32-11-el6.patch
Patch920: redhat-updating-lastcommit-for-2-6-32-11.patch
Patch921: redhat-Revert-edac-amd64_edac-disabling-temporarily.patch
Patch922: x86-Use-EOI-register-in-io-apic-on-intel-platforms.patch
Patch923: x86-Remove-local_irq_enable-local_irq_disable-in-fixup_irqs.patch
Patch924: x86-io-apic-Move-the-effort-of-clearing-remoteIRR-explicitly-before-migrating-the-irq.patch
Patch925: x86-ioapic-Fix-the-EOI-register-detection-mechanism.patch
Patch926: x86-ioapic-Document-another-case-when-level-irq-is-seen-as-an-edge.patch
Patch927: x86-Remove-unnecessary-mdelay-from-cpu_disable_common.patch
Patch928: redhat-rpadlpar_io-should-be-built-in-kernel.patch
Patch929: x86-msr-cpuid-Register-enough-minors-for-the-MSR-and-CPUID-drivers.patch
Patch930: scsi-Sync-be2iscsi-with-upstream.patch
Patch931: redhat-config-disable-CONFIG_X86_CPU_DEBUG.patch
Patch932: pci-Always-set-prefetchable-base-limit-upper32-registers.patch
Patch933: mm-Memory-tracking-for-Stratus.patch
Patch934: redhat-enable-Memory-tracking-for-Stratus.patch
Patch935: drm-radeon-possible-security-issue.patch
Patch936: redhat-tagging-2-6-32-12-el6.patch
Patch937: redhat-updating-lastcommit-for-2-6-32-12.patch
Patch938: mm-Memory-tracking-for-Stratus-2.patch
Patch939: kdump-backport-upstream-ppc64-kcrctab-fixes.patch
Patch940: x86-acpi-Export-acpi_pci_irq_-add-del-_prt.patch
Patch941: kvm-Fix-race-between-APIC-TMR-and-IRR.patch
Patch942: kvm-x86-Fix-host_mapping_level.patch
Patch943: kvm-MMU-bail-out-pagewalk-on-kvm_read_guest-error.patch
Patch944: kvm-x86-Fix-probable-memory-leak-of-vcpu-arch-mce_banks.patch
Patch945: kvm-x86-Fix-leak-of-free-lapic-date-in-kvm_arch_vcpu_init.patch
Patch946: kvm-only-allow-one-gsi-per-fd.patch
Patch947: kvm-properly-check-max-PIC-pin-in-irq-route-setup.patch
Patch948: kvm-eventfd-allow-atomic-read-and-waitqueue-remove.patch
Patch949: kvm-fix-spurious-interrupt-with-irqfd.patch
Patch950: x86-Add-AMD-Node-ID-MSR-support.patch
Patch951: x86-Fix-crash-when-profiling-more-than-28-events.patch
Patch952: virtio-console-comment-cleanup.patch
Patch953: virtio-console-statically-initialize-virtio_cons.patch
Patch954: virtio-hvc_console-make-the-ops-pointer-const.patch
Patch955: virtio-hvc_console-Remove-__devinit-annotation-from-hvc_alloc.patch
Patch956: virtio-console-We-support-only-one-device-at-a-time.patch
Patch957: virtio-console-port-encapsulation.patch
Patch958: virtio-console-encapsulate-buffer-information-in-a-struct.patch
Patch959: virtio-console-ensure-add_inbuf-can-work-for-multiple-ports-as-well.patch
Patch960: virtio-console-introduce-a-get_inbuf-helper-to-fetch-bufs-from-in_vq.patch
Patch961: virtio-console-use-vdev-priv-to-avoid-accessing-global-var.patch
Patch962: virtio-console-don-t-assume-a-single-console-port.patch
Patch963: virtio-console-remove-global-var.patch
Patch964: virtio-console-struct-ports-for-multiple-ports-per-device.patch
Patch965: virtio-console-ensure-console-size-is-updated-on-hvc-open.patch
Patch966: virtio-console-Separate-out-console-specific-data-into-a-separate-struct.patch
Patch967: virtio-console-Separate-out-console-init-into-a-new-function.patch
Patch968: virtio-console-Separate-out-find_vqs-operation-into-a-different-function.patch
Patch969: virtio-console-Introduce-function-to-hand-off-data-from-host-to-readers.patch
Patch970: virtio-console-Introduce-a-send_buf-function-for-a-common-path-for-sending-data-to-host.patch
Patch971: virtio-console-Add-a-new-MULTIPORT-feature-support-for-generic-ports.patch
Patch972: virtio-console-Prepare-for-writing-to-reading-from-userspace-buffers.patch
Patch973: virtio-console-Associate-each-port-with-a-char-device.patch
Patch974: virtio-console-Add-file-operations-to-ports-for-open-read-write-poll.patch
Patch975: virtio-console-Ensure-only-one-process-can-have-a-port-open-at-a-time.patch
Patch976: virtio-console-Register-with-sysfs-and-create-a-name-attribute-for-ports.patch
Patch977: virtio-console-Remove-cached-data-on-port-close.patch
Patch978: virtio-console-Handle-port-hot-plug.patch
Patch979: virtio-Add-ability-to-detach-unused-buffers-from-vrings.patch
Patch980: virtio-hvc_console-Export-GPL-ed-hvc_remove.patch
Patch981: virtio-console-Add-ability-to-hot-unplug-ports.patch
Patch982: virtio-console-Add-debugfs-files-for-each-port-to-expose-debug-info.patch
Patch983: virtio-console-show-error-message-if-hvc_alloc-fails-for-console-ports.patch
Patch984: redhat-tagging-2-6-32-13-el6.patch
Patch985: redhat-updating-lastcommit-for-2-6-32-13.patch
Patch986: uv-x86-Add-function-retrieving-node-controller-revision-number.patch
Patch987: uv-x86-Ensure-hub-revision-set-for-all-ACPI-modes.patch
Patch988: s390x-cio-channel-path-vary-operation-has-no-effect.patch
Patch989: s390x-zcrypt-Do-not-remove-coprocessor-in-case-of-error-8-72.patch
Patch990: s390x-dasd-Fix-null-pointer-in-s390dbf-and-discipline-checking.patch
Patch991: redhat-Enable-oprofile-multiplexing-on-x86_64-and-x86-architectures-only.patch
Patch992: netdrv-update-tg3-to-version-3-106-and-fix-panic.patch
Patch993: redhat-disable-CONFIG_OPTIMIZE_FOR_SIZE.patch
Patch994: s390x-dasd-fix-online-offline-race.patch
Patch995: redhat-tagging-2-6-32-14-el6.patch
Patch996: redhat-updating-lastcommit-for-2-6-32-14.patch
Patch997: gfs-GFS2-Fix-refcnt-leak-on-gfs2_follow_link-error-path.patch
Patch998: gfs-GFS2-Use-GFP_NOFS-for-alloc-structure.patch
Patch999: gfs-GFS2-Use-MAX_LFS_FILESIZE-for-meta-inode-size.patch
Patch1000: gfs-GFS2-Wait-for-unlock-completion-on-umount.patch
Patch1001: gfs-GFS2-Extend-umount-wait-coverage-to-full-glock-lifetime.patch
Patch1002: x86-intr-remap-generic-support-for-remapping-HPET-MSIs.patch
Patch1003: x86-arch-specific-support-for-remapping-HPET-MSIs.patch
Patch1004: x86-Disable-HPET-MSI-on-ATI-SB700-SB800.patch
Patch1005: fs-ext4-fix-type-of-offset-in-ext4_io_end.patch
Patch1006: redhat-disable-CONFIG_DEBUG_PERF_USE_VMALLOC-on-production-kernels.patch
Patch1007: x86-fix-Add-AMD-Node-ID-MSR-support.patch
Patch1008: redhat-config-disable-CONFIG_VMI.patch
Patch1009: quota-64-bit-quota-format-fixes.patch
Patch1010: block-cfq-Do-not-idle-on-async-queues-and-drive-deeper-WRITE-depths.patch
Patch1011: block-blk-cgroup-Fix-lockdep-warning-of-potential-deadlock-in-blk-cgroup.patch
Patch1012: redhat-tagging-2-6-32-15-el6.patch
Patch1013: redhat-updating-lastcommit-for-2-6-32-15.patch
Patch1014: redhat-x86_64-enable-function-tracers.patch
Patch1015: nfs-nfsd41-nfsd4_decode_compound-does-not-recognize-all-ops.patch
Patch1016: nfs-nfsd4-Use-FIRST_NFS4_OP-in-nfsd4_decode_compound.patch
Patch1017: nfs-nfsd-use-vfs_fsync-for-non-directories.patch
Patch1018: nfs-nfsd41-Create-the-recovery-entry-for-the-NFSv4-1-client.patch
Patch1019: nfs-nfsd-4-1-has-an-rfc-number.patch
Patch1020: nfs-SUNRPC-Use-rpc_pton-in-ip_map_parse.patch
Patch1021: nfs-NFSD-Support-AF_INET6-in-svc_addsock-function.patch
Patch1022: nfs-SUNRPC-Bury-ifdef-IPV6-in-svc_create_xprt.patch
Patch1023: nfs-SUNRPC-NFS-kernel-APIs-shouldn-t-return-ENOENT-for-transport-not-found.patch
Patch1024: nfs-NFSD-Create-PF_INET6-listener-in-write_ports.patch
Patch1025: nfs-nfs41-Adjust-max-cache-response-size-value.patch
Patch1026: nfs-nfs41-Check-slot-table-for-referring-calls.patch
Patch1027: nfs-nfs41-Process-callback-s-referring-call-list.patch
Patch1028: nfs-nfs41-fix-wrong-error-on-callback-header-xdr-overflow.patch
Patch1029: nfs-nfs41-directly-encode-back-channel-error.patch
Patch1030: nfs-nfs41-remove-uneeded-checks-in-callback-processing.patch
Patch1031: nfs-nfs41-prepare-for-back-channel-drc.patch
Patch1032: nfs-nfs41-back-channel-drc-minimal-implementation.patch
Patch1033: nfs-nfs41-implement-cb_recall_slot.patch
Patch1034: nfs-nfs41-resize-slot-table-in-reset.patch
Patch1035: nfs-nfs41-fix-nfs4_callback_recallslot.patch
Patch1036: nfs-nfs41-clear-NFS4CLNT_RECALL_SLOT-bit-on-session-reset.patch
Patch1037: nfs-nfs41-cleanup-callback-code-to-use-__be32-type.patch
Patch1038: nfs-Fix-a-reference-leak-in-nfs_wb_cancel_page.patch
Patch1039: nfs-Try-to-commit-unstable-writes-in-nfs_release_page.patch
Patch1040: nfs-Make-nfs_commitdata_release-static.patch
Patch1041: nfs-Avoid-warnings-when-CONFIG_NFS_V4-n.patch
Patch1042: nfs-Ensure-that-the-NFSv4-locking-can-recover-from-stateid-errors.patch
Patch1043: nfs-NFSv4-Don-t-allow-posix-locking-against-servers-that-don-t-support-it.patch
Patch1044: nfs-NFSv4-1-Don-t-call-nfs4_schedule_state_recovery-unnecessarily.patch
Patch1045: nfs-Ensure-that-we-handle-NFS4ERR_STALE_STATEID-correctly.patch
Patch1046: nfs-sunrpc-cache-fix-module-refcnt-leak-in-a-failure-path.patch
Patch1047: scsi-lpfc-Update-from-8-3-4-to-8-3-5-4-FC-FCoE.patch
Patch1048: dm-fix-kernel-panic-at-releasing-bio-on-recovery-failed-region.patch
Patch1049: dm-dm-raid1-fix-deadlock-at-suspending-failed-device.patch
Patch1050: vhost-fix-high-32-bit-in-FEATURES-ioctls.patch
Patch1051: vhost-prevent-modification-of-an-active-ring.patch
Patch1052: vhost-add-access_ok-checks.patch
Patch1053: vhost-make-default-mapping-empty-by-default.patch
Patch1054: vhost-access-check-thinko-fixes.patch
Patch1055: vhost-vhost-net-comment-use-of-invalid-fd-when-setting-vhost-backend.patch
Patch1056: vhost-vhost-net-defer-f-private_data-until-setup-succeeds.patch
Patch1057: vhost-fix-TUN-m-VHOST_NET-y.patch
Patch1058: kvm-emulate-accessed-bit-for-EPT.patch
Patch1059: net-nf_conntrack-fix-memory-corruption.patch
Patch1060: nfs-nfs4-handle-EKEYEXPIRED-errors-from-RPC-layer.patch
Patch1061: nfs-sunrpc-parse-and-return-errors-reported-by-gssd.patch
Patch1062: nfs-handle-NFSv2-EKEYEXPIRED-returns-from-RPC-layer-appropriately.patch
Patch1063: nfs-nfs-handle-NFSv3-EKEYEXPIRED-errors-as-we-would-EJUKEBOX.patch
Patch1064: oprofile-Support-Nehalem-EX-CPU-in-Oprofile.patch
Patch1065: mm-define-MADV_HUGEPAGE.patch
Patch1066: mm-add-a-compound_lock.patch
Patch1067: mm-alter-compound-get_page-put_page.patch
Patch1068: mm-update-futex-compound-knowledge.patch
Patch1069: mm-clear-compound-mapping.patch
Patch1070: mm-add-native_set_pmd_at.patch
Patch1071: mm-add-pmd-paravirt-ops.patch
Patch1072: mm-no-paravirt-version-of-pmd-ops.patch
Patch1073: mm-export-maybe_mkwrite.patch
Patch1074: mm-comment-reminder-in-destroy_compound_page.patch
Patch1075: mm-config_transparent_hugepage.patch
Patch1076: mm-special-pmd_trans_-functions.patch
Patch1077: mm-add-pmd-mangling-generic-functions.patch
Patch1078: mm-add-pmd-mangling-functions-to-x86.patch
Patch1079: mm-bail-out-gup_fast-on-splitting-pmd.patch
Patch1080: mm-pte-alloc-trans-splitting.patch
Patch1081: mm-add-pmd-mmu_notifier-helpers.patch
Patch1082: mm-clear-page-compound.patch
Patch1083: mm-add-pmd_huge_pte-to-mm_struct.patch
Patch1084: mm-split_huge_page_mm-vma.patch
Patch1085: mm-split_huge_page-paging.patch
Patch1086: mm-clear_huge_page-fix.patch
Patch1087: mm-clear_copy_huge_page.patch
Patch1088: mm-kvm-mmu-transparent-hugepage-support.patch
Patch1089: mm-backport-page_referenced-microoptimization.patch
Patch1090: mm-introduce-_GFP_NO_KSWAPD.patch
Patch1091: mm-dont-alloc-harder-for-gfp-nomemalloc-even-if-nowait.patch
Patch1092: mm-transparent-hugepage-core.patch
Patch1093: mm-verify-pmd_trans_huge-isnt-leaking.patch
Patch1094: mm-madvise-MADV_HUGEPAGE.patch
Patch1095: mm-pmd_trans_huge-migrate-bugcheck.patch
Patch1096: mm-memcg-compound.patch
Patch1097: mm-memcg-huge-memory.patch
Patch1098: mm-transparent-hugepage-vmstat.patch
Patch1099: mm-introduce-khugepaged.patch
Patch1100: mm-hugepage-redhat-customization.patch
Patch1101: mm-remove-madvise-MADV_HUGEPAGE.patch
Patch1102: uv-PCI-update-pci_set_vga_state-to-call-arch-functions.patch
Patch1103: pci-update-pci_set_vga_state-to-call-arch-functions.patch
Patch1104: uv-x86_64-update-uv-arch-to-target-legacy-VGA-I-O-correctly.patch
Patch1105: gpu-vgaarb-fix-vga-arbiter-to-accept-PCI-domains-other-than-0.patch
Patch1106: uv-vgaarb-add-user-selectability-of-the-number-of-gpus-in-a-system.patch
Patch1107: s390x-ctcm-lcs-claw-remove-cu3088-layer.patch
Patch1108: uv-x86-Fix-RTC-latency-bug-by-reading-replicated-cachelines.patch
Patch1109: pci-PCI-ACS-support-functions.patch
Patch1110: pci-Enablement-of-PCI-ACS-control-when-IOMMU-enabled-on-system.patch
Patch1111: net-do-not-check-CAP_NET_RAW-for-kernel-created-sockets.patch
Patch1112: fs-inotify-fix-inotify-WARN-and-compatibility-issues.patch
Patch1113: redhat-kABI-updates-02-11.patch
Patch1114: mm-fix-BUG-s-caused-by-the-transparent-hugepage-patch.patch
Patch1115: redhat-Allow-only-particular-kernel-rpms-to-be-built.patch
Patch1116: nfs-Fix-a-bug-in-nfs_fscache_release_page.patch
Patch1117: nfs-Remove-a-redundant-check-for-PageFsCache-in-nfs_migrate_page.patch
Patch1118: redhat-tagging-2-6-32-16-el6.patch
Patch1119: redhat-updating-lastcommit-for-2-6-32-16.patch
Patch1120: redhat-find-provides-also-require-python.patch
Patch1121: ppc-Add-kdump-support-to-Collaborative-Memory-Manager.patch
Patch1122: gfs-GFS2-problems-on-single-node-cluster.patch
Patch1123: selinux-print-the-module-name-when-SELinux-denies-a-userspace-upcall.patch
Patch1124: kernel-Prevent-futex-user-corruption-to-crash-the-kernel.patch
Patch1125: kvm-PIT-control-word-is-write-only.patch
Patch1126: virt-virtio_blk-add-block-topology-support.patch
Patch1127: redhat-make-CONFIG_CRASH-built-in.patch
Patch1128: mm-anon_vma-linking-changes-to-improve-multi-process-scalability.patch
Patch1129: mm-anon_vma-locking-updates-for-transparent-hugepage-code.patch
Patch1130: dm-stripe-avoid-divide-by-zero-with-invalid-stripe-count.patch
Patch1131: dm-log-userspace-fix-overhead_size-calcuations.patch
Patch1132: dm-mpath-fix-stall-when-requeueing-io.patch
Patch1133: dm-raid1-fail-writes-if-errors-are-not-handled-and-log-fails.patch
Patch1134: kernel-time-Implement-logarithmic-time-accumalation.patch
Patch1135: kernel-time-Remove-xtime_cache.patch
Patch1136: watchdog-Add-support-for-iTCO-watchdog-on-Ibex-Peak-chipset.patch
Patch1137: block-fix-bio_add_page-for-non-trivial-merge_bvec_fn-case.patch
Patch1138: block-freeze_bdev-don-t-deactivate-successfully-frozen-MS_RDONLY-sb.patch
Patch1139: x86-nmi_watchdog-enable-by-default-on-RHEL-6.patch
Patch1140: x86-x86-32-clean-up-rwsem-inline-asm-statements.patch
Patch1141: x86-clean-up-rwsem-type-system.patch
Patch1142: x86-x86-64-support-native-xadd-rwsem-implementation.patch
Patch1143: x86-x86-64-rwsem-64-bit-xadd-rwsem-implementation.patch
Patch1144: x86-x86-64-rwsem-Avoid-store-forwarding-hazard-in-__downgrade_write.patch
Patch1145: nfs-mount-nfs-Unknown-error-526.patch
Patch1146: s390-zfcp-cancel-all-pending-work-for-a-to-be-removed-zfcp_port.patch
Patch1147: s390-zfcp-report-BSG-errors-in-correct-field.patch
Patch1148: s390-qdio-prevent-kernel-bug-message-in-interrupt-handler.patch
Patch1149: s390-qdio-continue-polling-for-buffer-state-ERROR.patch
Patch1150: s390-hvc_iucv-allocate-IUCV-send-receive-buffers-in-DMA-zone.patch
Patch1151: redhat-whitelists-intentional-kABI-Module-kabi-rebase-pre-beta1-for-VM-bits.patch
Patch1152: redhat-tagging-2-6-32-17-el6.patch
Patch1153: redhat-updating-lastcommit-for-2-6-32-17.patch
Patch1154: redhat-disable-GFS2-on-s390x.patch
Patch1155: redhat-disable-XFS-on-all-arches-except-x86_64.patch
Patch1156: redhat-disable-all-the-filesystems-we-aren-t-going-to-support-and-fix-ext3-4.patch
Patch1157: redhat-run-new-kernel-pkg-on-posttrans.patch
Patch1158: redhat-fix-typo-on-redhat-Makefile.patch
Patch1159: kvm-virtio-console-Allow-sending-variable-sized-buffers-to-host-efault-on-copy_from_user-err.patch
Patch1160: kvm-virtio-console-return-efault-for-fill_readbuf-if-copy_to_user-fails.patch
Patch1161: kvm-virtio-console-outbufs-are-no-longer-needed.patch
Patch1162: kvm-virtio-Initialize-vq-data-entries-to-NULL.patch
Patch1163: kvm-virtio-console-update-Red-Hat-copyright-for-2010.patch
Patch1164: kvm-virtio-console-Ensure-no-memleaks-in-case-of-unused-buffers.patch
Patch1165: kvm-virtio-console-Add-ability-to-remove-module.patch
Patch1166: kvm-virtio-console-Error-out-if-we-can-t-allocate-buffers-for-control-queue.patch
Patch1167: kvm-virtio-console-Fill-ports-entire-in_vq-with-buffers.patch
Patch1168: kvm-Add-MAINTAINERS-entry-for-virtio_console.patch
Patch1169: kvm-fix-large-packet-drops-on-kvm-hosts-with-ipv6.patch
Patch1170: scsi-megaraid_sas-fix-for-32bit-apps.patch
Patch1171: x86-AES-PCLMUL-Instruction-support-Add-PCLMULQDQ-accelerated-implementation.patch
Patch1172: x86-AES-PCLMUL-Instruction-support-Various-small-fixes-for-AES-PCMLMUL-and-generate-byte-code-for-some-new-instructions-via-gas-macro.patch
Patch1173: x86-AES-PCLMUL-Instruction-support-Use-gas-macro-for-AES-NI-instructions.patch
Patch1174: x86-AES-PCLMUL-Instruction-support-Various-fixes-for-AES-NI-and-PCLMMUL.patch
Patch1175: kvm-x86-emulator-Add-push-pop-sreg-instructions.patch
Patch1176: kvm-x86-emulator-Introduce-No64-decode-option.patch
Patch1177: kvm-x86-emulator-Add-group8-instruction-decoding.patch
Patch1178: kvm-x86-emulator-Add-group9-instruction-decoding.patch
Patch1179: kvm-x86-emulator-Add-Virtual-8086-mode-of-emulation.patch
Patch1180: kvm-x86-emulator-fix-memory-access-during-x86-emulation.patch
Patch1181: kvm-x86-emulator-Check-IOPL-level-during-io-instruction-emulation.patch
Patch1182: kvm-x86-emulator-Fix-popf-emulation.patch
Patch1183: kvm-x86-emulator-Check-CPL-level-during-privilege-instruction-emulation.patch
Patch1184: kvm-x86-emulator-Add-LOCK-prefix-validity-checking.patch
Patch1185: kvm-x86-emulator-code-style-cleanup.patch
Patch1186: kvm-x86-emulator-Fix-properties-of-instructions-in-group-1_82.patch
Patch1187: kvm-inject-UD-in-64bit-mode-from-instruction-that-are-not-valid-there.patch
Patch1188: kvm-x86-emulator-X86EMUL-macro-replacements-from-do_fetch_insn_byte-to-x86_decode_insn.patch
Patch1189: kvm-x86-emulator-X86EMUL-macro-replacements-x86_emulate_insn-and-its-helpers.patch
Patch1190: kvm-x86-emulator-Fix-x86_emulate_insn-not-to-use-the-variable-rc-for-non-X86EMUL-values.patch
Patch1191: kvm-x86-emulator-Forbid-modifying-CS-segment-register-by-mov-instruction.patch
Patch1192: kvm-Fix-load_guest_segment_descriptor-to-inject-page-fault.patch
Patch1193: kvm-Fix-segment-descriptor-loading.patch
Patch1194: kvm-Fix-emulate_sys-call-enter-exit-s-fault-handling.patch
Patch1195: netdrv-ixgbe-prevent-speculative-processing-of-descriptors.patch
Patch1196: x86-nmi_watchdog-use-__cpuinit-for-nmi_watchdog_default.patch
Patch1197: net-netfilter-nf_conntrack-per-netns-nf_conntrack_cachep.patch
Patch1198: scsi-mpt2sas-fix-missing-initialization.patch
Patch1199: dm-raid45-target-constructor-error-path-oops-fix.patch
Patch1200: netdrv-cxgb3-add-memory-barriers.patch
Patch1201: mm-fix-anon_vma-locking-updates-for-transparent-hugepage-code.patch
Patch1202: mm-Fix-hugetlb-c-clear_huge_page-parameter.patch
Patch1203: gfs2-print-glock-numbers-in-hex.patch
Patch1204: selinux-netlabel-fix-corruption-of-SELinux-MLS-categories-127.patch
Patch1205: ata-ahci-disable-FPDMA-auto-activate-optimization-on-NVIDIA-AHCI.patch
Patch1206: scsi-lpfc-Update-from-8-3-5-4-to-8-3-5-5-FC-FCoE.patch
Patch1207: scsi-pmcraid-bug-fixes-from-upstream.patch
Patch1208: mm-Fix-potential-crash-with-sys_move_pages.patch
Patch1209: s390x-qeth-avoid-recovery-during-device-online-setting.patch
Patch1210: kernel-Fix-SMT-scheduler-regression-in-find_busiest_queue.patch
Patch1211: drm-bring-drm-core-ttm-fb-layer-fixes-in-from-upstream.patch
Patch1212: s390x-vdso-glibc-does-not-use-vdso-functions.patch
Patch1213: kernel-sched-Fix-SCHED_MC-regression-caused-by-change-in-sched-cpu_power.patch
Patch1214: dvb-Fix-endless-loop-when-decoding-ULE-at-dvb-core.patch
Patch1215: drm-radeon-kms-bring-all-v2-6-33-fixes-into-EL6-kernel.patch
Patch1216: redhat-Change-RPM-name-format-for-custom-builds.patch
Patch1217: uv-Have-mmu_notifiers-use-SRCU-so-they-may-safely-schedule.patch
Patch1218: uv-Fix-unmap_vma-bug-related-to-mmu_notifiers.patch
Patch1219: net-bug-fix-for-vlan-gro-issue.patch
Patch1220: redhat-add-suport-for-rhts-testing.patch
Patch1221: redhat-add-filesystem-test.patch
Patch1222: vhost-vhost-net-switch-to-smp-barriers.patch
Patch1223: vhost-logging-thinko-fix.patch
Patch1224: vhost-initialize-log-eventfd-context-pointer.patch
Patch1225: vhost-fix-get_user_pages_fast-error-handling.patch
Patch1226: vhost-vhost-net-restart-tx-poll-on-sk_sndbuf-full.patch
Patch1227: x86-Intel-Cougar-Point-chipset-support.patch
Patch1228: drm-Remove-loop-in-IronLake-graphics-interrupt-handler.patch
Patch1229: scsi-scsi_dh_emc-fix-mode-select-setup.patch
Patch1230: scsi-Add-netapp-to-scsi-dh-alua-dev-list.patch
Patch1231: virt-hvc_console-Fix-race-between-hvc_close-and-hvc_remove.patch
Patch1232: kernel-time-revert-cc2f92ad1d0e03fe527e8ccfc1f918c368964dc8.patch
Patch1233: redhat-infiniband-remove-all-infiniband-symbols-from-RHEL6-kABI.patch
Patch1234: redhat-kABI-Update-the-RHEL6-kABI-for-kernel-2-6-32-18-el6.patch
Patch1235: redhat-tagging-2-6-32-18-el6.patch
Patch1236: redhat-updating-lastcommit-for-2-6-32-18.patch
Patch1237: mm-Switch-to-SLAB.patch
Patch1238: redhat-kABI-Update-the-kABI-for-2-6-18-el6.patch

Patch88888: patch-2.6.32-19.el6-vs2.3.0.36.29.4.diff
Patch90250: linux-2.6-250-ipsets.patch
Patch90510: linux-2.6-510-ipod.patch
Patch90521: linux-2.6-521-packet-tagging.patch
#
#Patch90522: linux-2.6-522-iptables-connection-tagging.patch
#
Patch90523: linux-2.6-523-raw-sockets.patch
Patch90524: linux-2.6-524-peercred.patch
Patch90525: linux-2.6-525-sknid-elevator.patch
Patch90530: linux-2.6-530-built-by-support.patch
Patch90540: linux-2.6-540-oom-kill.patch

# empty final patch file to facilitate testing of kernel patches
Patch99999: linux-kernel-test.patch

BuildRoot: %{_tmppath}/kernel-%{KVERREL}-root

# Override find_provides to use a script that provides "kernel(symbol) = hash".
# Pass path of the RPM temp dir containing kabideps to find-provides script.
%global _use_internal_dependency_generator 0
%define __find_provides %_sourcedir/find-provides %{_tmppath}
%define __find_requires /usr/lib/rpm/redhat/find-requires kernel

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


%define variant_summary The Linux kernel compiled with CONFIG_FRAME_POINTER
%kernel_variant_package framepointer
%description framepointer
This is a TEMPORARY package with the CONFIG_FRAME_POINTER.


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

%if %{with_fuzzy_patches}
  patch_command='patch -p1 -s'
%else
  patch_command='patch -p1 -F1 -s'
%endif

ApplyPatch()
{
  local patch=$1
  shift
  if [ ! -f $RPM_SOURCE_DIR/$patch ]; then
    exit 1
  fi
  # check if the patch is empty
  if [ -z "$(lsdiff $RPM_SOURCE_DIR/$patch)" ]; then
    echo "WARNING: empty patch";
    return;
  fi
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
%else
%define vanillaversion 2.6.%{base_sublevel}
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

# Drop some necessary files from the source dir into the buildroot
cp $RPM_SOURCE_DIR/config-* .
cp %{SOURCE15} %{SOURCE1} %{SOURCE16} %{SOURCE17} %{SOURCE18} .

# Dynamically generate kernel .config files from config-* files
make -f %{SOURCE20} VERSION=%{version} configs

ApplyPatch Fedora-redhat-introduce-nonint_oldconfig-target.patch
ApplyPatch Fedora-build-introduce-AFTER_LINK-variable.patch
ApplyPatch Fedora-utrace-introduce-utrace-implementation.patch
ApplyPatch Fedora-hwmon-add-VIA-hwmon-temperature-sensor-support.patch
ApplyPatch Fedora-powerpc-add-modalias_show-operation.patch
ApplyPatch Fedora-execshield-introduce-execshield.patch
ApplyPatch Fedora-nfsd4-proots.patch
ApplyPatch Fedora-nfs-make-nfs4-callback-hidden.patch
ApplyPatch Fedora-usb-Allow-drivers-to-enable-USB-autosuspend-on-a-per-device-basis.patch
ApplyPatch Fedora-usb-enable-autosuspend-by-default-on-qcserial.patch
ApplyPatch Fedora-usb-enable-autosuspend-on-UVC-by-default.patch
ApplyPatch Fedora-acpi-Disable-brightness-switch-by-default.patch
ApplyPatch Fedora-acpi-Disable-firmware-video-brightness-change-by-default.patch
ApplyPatch Fedora-debug-print-common-struct-sizes-at-boot-time.patch
ApplyPatch Fedora-x86-add-option-to-control-the-NMI-watchdog-timeout.patch
ApplyPatch Fedora-debug-display-tainted-information-on-other-places.patch
ApplyPatch Fedora-debug-add-calls-to-print_tainted-on-spinlock-functions.patch
ApplyPatch Fedora-debug-add-would_have_oomkilled-procfs-ctl.patch
ApplyPatch Fedora-debug-always-inline-kzalloc.patch
ApplyPatch Fedora-pci-add-config-option-to-control-the-default-state-of-PCI-MSI-interrupts.patch
ApplyPatch Fedora-pci-sets-PCIE-ASPM-default-policy-to-POWERSAVE.patch
ApplyPatch Fedora-sound-disables-hda-beep-by-default.patch
ApplyPatch Fedora-sound-hda-intel-prealloc-4mb-dmabuffer.patch
ApplyPatch Fedora-input-remove-unwanted-messages-on-spurious-events.patch
ApplyPatch Fedora-floppy-remove-the-floppy-pnp-modalias.patch
ApplyPatch Fedora-input-remove-pcspkr-modalias.patch
ApplyPatch Fedora-serial-Enable-higher-baud-rates-for-16C95x.patch
ApplyPatch Fedora-serio-disable-error-messages-when-i8042-isn-t-found.patch
ApplyPatch Fedora-pci-silence-some-PCI-resource-allocation-errors.patch
ApplyPatch Fedora-fb-disable-fbcon-logo-with-parameter.patch
ApplyPatch Fedora-crash-add-crash-driver.patch
ApplyPatch Fedora-pci-cacheline-sizing.patch
ApplyPatch Fedora-e1000-add-quirk-for-ich9.patch
ApplyPatch Fedora-drm-intel-big-hammer.patch
ApplyPatch Fedora-acpi-be-less-verbose-about-old-BIOSes.patch
ApplyPatch Fedora-rfkill-add-support-to-a-key-to-control-all-radios.patch
ApplyPatch redhat-adding-redhat-directory.patch
ApplyPatch redhat-Importing-config-options.patch
ApplyPatch redhat-s390x-adding-zfcpdump-application-used-by-s390x-kdump-kernel.patch
ApplyPatch redhat-Include-FIPS-required-checksum-of-the-kernel-image.patch
ApplyPatch redhat-Silence-tagging-messages-by-rh-release.patch
ApplyPatch redhat-Disabling-debug-options-for-beta.patch
ApplyPatch redhat-kernel-requires-udev-145-11-or-newer.patch
ApplyPatch redhat-tagging-2-6-31-50-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-31-50.patch
ApplyPatch block-get-rid-of-the-WRITE_ODIRECT-flag.patch
ApplyPatch block-aio-implement-request-batching.patch
ApplyPatch kdump-x86-add-CONFIG_KEXEC_AUTO_RESERVE.patch
ApplyPatch kdump-x86-implement-crashkernel-auto.patch
ApplyPatch kdump-ia64-add-CONFIG_KEXEC_AUTO_RESERVE.patch
ApplyPatch kdump-ia64-implement-crashkernel-auto.patch
ApplyPatch kdump-powerpc-add-CONFIG_KEXEC_AUTO_RESERVE.patch
ApplyPatch kdump-powerpc-implement-crashkernel-auto.patch
ApplyPatch kdump-doc-update-the-kdump-document.patch
ApplyPatch kdump-kexec-allow-to-shrink-reserved-memory.patch
ApplyPatch kernel-Set-panic_on_oops-to-1.patch
ApplyPatch redhat-fix-BZ-and-CVE-info-printing-on-changelog-when-HIDE_REDHAT-is-enabled.patch
ApplyPatch redhat-tagging-2-6-31-51-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-31-51.patch
ApplyPatch redhat-fixing-the-kernel-versions-on-the-SPEC-changelog.patch
ApplyPatch redhat-Fix-version-passed-to-update_changelog-sh.patch
ApplyPatch mm-Limit-32-bit-x86-systems-to-16GB-and-prevent-panic-on-boot-when-system-has-more-than-30GB.patch
ApplyPatch ppc64-Fix-kcrctab_-sections-to-undo-undesireable-relocations-that-break-kdump.patch
ApplyPatch net-export-device-speed-and-duplex-via-sysfs.patch
ApplyPatch scsi-devinfo-update-for-Hitachi-entries.patch
ApplyPatch x86-AMD-Northbridge-Verify-NB-s-node-is-online.patch
ApplyPatch redhat-tagging-2-6-32-0-52-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-0-52.patch
ApplyPatch redhat-fix-STAMP-version-on-rh-release-commit-phase.patch
ApplyPatch redhat-enable-debug-builds-also-on-s390x-and-ppc64.patch
ApplyPatch s390x-fix-build-failure-with-CONFIG_FTRACE_SYSCALLS.patch
ApplyPatch procfs-add-ability-to-modify-proc-file-limits-from-outside-a-processes-own-context.patch
ApplyPatch modsign-Multiprecision-maths-library.patch
ApplyPatch modsign-Add-indications-of-module-ELF-types.patch
ApplyPatch modsign-Module-ELF-verifier.patch
ApplyPatch modsign-Module-signature-checker-and-key-manager.patch
ApplyPatch modsign-Apply-signature-checking-to-modules-on-module-load.patch
ApplyPatch modsign-Don-t-include-note-gnu-build-id-in-the-digest.patch
ApplyPatch modsign-Enable-module-signing-in-the-RHEL-RPM.patch
ApplyPatch net-Add-acession-counts-to-all-datagram-protocols.patch
ApplyPatch redhat-tagging-2-6-32-0-53-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-0-53.patch
ApplyPatch redhat-fixing-wrong-bug-number-536759-536769.patch
ApplyPatch redhat-adding-top-makefile-to-enable-rh-targets.patch
ApplyPatch redhat-add-temporary-framepointer-variant.patch
ApplyPatch redhat-add-rh-key-target-to-Makefile.patch
ApplyPatch infiniband-Rewrite-SG-handling-for-RDMA-logic.patch
ApplyPatch x86-panic-if-AMD-cpu_khz-is-wrong.patch
ApplyPatch x86-Enable-CONFIG_SPARSE_IRQ.patch
ApplyPatch edac-amd64_edac-disabling-temporarily.patch
ApplyPatch redhat-tagging-2-6-32-0-54-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-0-54.patch
ApplyPatch redhat-create-patches-sh-use-first-parent-to-use-the-right-branch-history.patch
ApplyPatch redhat-Rebasing-to-kernel-2-6-32.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-1.patch
ApplyPatch redhat-introduce-rh-kernel-debug-target.patch
ApplyPatch redhat-update-build-targets-in-Makefile.patch
ApplyPatch redhat-include-missing-System-map-file-for-debug-only-builds.patch
ApplyPatch Fedora-updating-linux-2-6-execshield-patch-2-6-32-8-fc13-reference.patch
ApplyPatch Fedora-updating-patch-linux-2-6-nfsd4-proots-patch-2-6-32-8-fc13-reference.patch
ApplyPatch Fedora-intel-iommu-backport.patch
ApplyPatch Fedora-ath9k-backports.patch
ApplyPatch Fedora-KVM-allow-userspace-to-adjust-kvmclock-offset.patch
ApplyPatch Fedora-drm-radeon-fixes.patch
ApplyPatch Fedora-drm-radeon-dp-support.patch
ApplyPatch Fedora-drm-nouveau-fixes.patch
ApplyPatch Fedora-drm-i915-Fix-sync-to-vblank-when-VGA-output-is-turned-off.patch
ApplyPatch Fedora-agp-clear-GTT-on-intel.patch
ApplyPatch Fedora-ext4-Fix-insuficient-checks-in-EXT4_IOC_MOVE_EXT.patch
ApplyPatch Fedora-perf-Don-t-free-perf_mmap_data-until-work-has-been-done.patch
ApplyPatch redhat-updating-config-files-based-on-current-requests-12-10.patch
ApplyPatch redhat-Config-updates-12-15.patch
ApplyPatch block-revert-cfq-iosched-limit-coop-preemption.patch
ApplyPatch block-CFQ-is-more-than-a-desktop-scheduler.patch
ApplyPatch block-cfq-calculate-the-seek_mean-per-cfq_queue-not-per-cfq_io_context.patch
ApplyPatch block-cfq-merge-cooperating-cfq_queues.patch
ApplyPatch block-cfq-change-the-meaning-of-the-cfqq_coop-flag.patch
ApplyPatch block-cfq-break-apart-merged-cfqqs-if-they-stop-cooperating.patch
ApplyPatch block-cfq-iosched-improve-hw_tag-detection.patch
ApplyPatch block-cfq-iosched-adapt-slice-to-number-of-processes-doing-I-O.patch
ApplyPatch block-cfq-iosched-preparation-to-handle-multiple-service-trees.patch
ApplyPatch block-cfq-iosched-reimplement-priorities-using-different-service-trees.patch
ApplyPatch block-cfq-iosched-enable-idling-for-last-queue-on-priority-class.patch
ApplyPatch block-cfq-iosched-fairness-for-sync-no-idle-queues.patch
ApplyPatch block-cfq-iosched-fix-style-issue-in-cfq_get_avg_queues.patch
ApplyPatch block-blkdev-flush-disk-cache-on-fsync.patch
ApplyPatch block-cfq-iosched-simplify-prio-unboost-code.patch
ApplyPatch block-cfq-iosched-fix-next_rq-computation.patch
ApplyPatch block-Expose-discard-granularity.patch
ApplyPatch block-partitions-use-sector-size-for-EFI-GPT.patch
ApplyPatch block-partitions-read-whole-sector-with-EFI-GPT-header.patch
ApplyPatch block-cfq-Make-use-of-service-count-to-estimate-the-rb_key-offset.patch
ApplyPatch block-cfq-iosched-cleanup-unreachable-code.patch
ApplyPatch block-cfq-iosched-fix-ncq-detection-code.patch
ApplyPatch block-cfq-iosched-fix-no-idle-preemption-logic.patch
ApplyPatch block-cfq-iosched-idling-on-deep-seeky-sync-queues.patch
ApplyPatch block-cfq-iosched-fix-corner-cases-in-idling-logic.patch
ApplyPatch block-Revert-cfq-Make-use-of-service-count-to-estimate-the-rb_key-offset.patch
ApplyPatch block-Allow-devices-to-indicate-whether-discarded-blocks-are-zeroed.patch
ApplyPatch block-cfq-iosched-no-dispatch-limit-for-single-queue.patch
ApplyPatch block-blkio-Set-must_dispatch-only-if-we-decided-to-not-dispatch-the-request.patch
ApplyPatch block-blkio-Introduce-the-notion-of-cfq-groups.patch
ApplyPatch block-blkio-Implement-macro-to-traverse-each-service-tree-in-group.patch
ApplyPatch block-blkio-Keep-queue-on-service-tree-until-we-expire-it.patch
ApplyPatch block-blkio-Introduce-the-root-service-tree-for-cfq-groups.patch
ApplyPatch block-blkio-Introduce-blkio-controller-cgroup-interface.patch
ApplyPatch block-blkio-Introduce-per-cfq-group-weights-and-vdisktime-calculations.patch
ApplyPatch block-blkio-Implement-per-cfq-group-latency-target-and-busy-queue-avg.patch
ApplyPatch block-blkio-Group-time-used-accounting-and-workload-context-save-restore.patch
ApplyPatch block-blkio-Dynamic-cfq-group-creation-based-on-cgroup-tasks-belongs-to.patch
ApplyPatch block-blkio-Take-care-of-cgroup-deletion-and-cfq-group-reference-counting.patch
ApplyPatch block-blkio-Some-debugging-aids-for-CFQ.patch
ApplyPatch block-blkio-Export-disk-time-and-sectors-used-by-a-group-to-user-space.patch
ApplyPatch block-blkio-Provide-some-isolation-between-groups.patch
ApplyPatch block-blkio-Drop-the-reference-to-queue-once-the-task-changes-cgroup.patch
ApplyPatch block-blkio-Propagate-cgroup-weight-updation-to-cfq-groups.patch
ApplyPatch block-blkio-Wait-for-cfq-queue-to-get-backlogged-if-group-is-empty.patch
ApplyPatch block-blkio-Determine-async-workload-length-based-on-total-number-of-queues.patch
ApplyPatch block-blkio-Implement-group_isolation-tunable.patch
ApplyPatch block-blkio-Wait-on-sync-noidle-queue-even-if-rq_noidle-1.patch
ApplyPatch block-blkio-Documentation.patch
ApplyPatch block-cfq-iosched-fix-compile-problem-with-CONFIG_CGROUP.patch
ApplyPatch block-cfq-iosched-move-IO-controller-declerations-to-a-header-file.patch
ApplyPatch block-io-controller-quick-fix-for-blk-cgroup-and-modular-CFQ.patch
ApplyPatch block-cfq-iosched-make-nonrot-check-logic-consistent.patch
ApplyPatch block-blkio-Export-some-symbols-from-blkio-as-its-user-CFQ-can-be-a-module.patch
ApplyPatch block-blkio-Implement-dynamic-io-controlling-policy-registration.patch
ApplyPatch block-blkio-Allow-CFQ-group-IO-scheduling-even-when-CFQ-is-a-module.patch
ApplyPatch block-cfq-iosched-use-call_rcu-instead-of-doing-grace-period-stall-on-queue-exit.patch
ApplyPatch block-include-linux-err-h-to-use-ERR_PTR.patch
ApplyPatch block-cfq-iosched-Do-not-access-cfqq-after-freeing-it.patch
ApplyPatch block-dio-fix-performance-regression.patch
ApplyPatch block-Add-support-for-the-ATA-TRIM-command-in-libata.patch
ApplyPatch scsi-Add-missing-command-definitions.patch
ApplyPatch scsi-scsi_debug-Thin-provisioning-support.patch
ApplyPatch scsi-sd-WRITE-SAME-16-UNMAP-support.patch
ApplyPatch scsi-Correctly-handle-thin-provisioning-write-error.patch
ApplyPatch libata-Report-zeroed-read-after-Trim-and-max-discard-size.patch
ApplyPatch libata-Clarify-ata_set_lba_range_entries-function.patch
ApplyPatch block-config-enable-CONFIG_BLK_CGROUP.patch
ApplyPatch block-config-enable-CONFIG_BLK_DEV_INTEGRITY.patch
ApplyPatch redhat-tagging-2-6-32-2-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-2.patch
ApplyPatch redhat-force-to-run-rh-key-target-when-compiling-the-kernel-locally-without-RPM.patch
ApplyPatch redhat-run-rngd-on-rh-key-to-speed-up-key-generation.patch
ApplyPatch redhat-make-the-documentation-build-j1.patch
ApplyPatch redhat-remove-unused-config-file-config-powerpc64-generic-rhel.patch
ApplyPatch redhat-fix-problem-when-using-other-rh-targets.patch
ApplyPatch redhat-reverting-makefile-magic.patch
ApplyPatch redhat-remove-gcc-bug-workaround.patch
ApplyPatch redhat-run-rh-key-when-the-GPG-keys-aren-t-present.patch
ApplyPatch nfs-convert-proto-option-to-use-netids-rather-than-a-protoname.patch
ApplyPatch scsi-fix-dma-handling-when-using-virtual-hosts.patch
ApplyPatch dm-core-and-mpath-changes-from-2-6-33.patch
ApplyPatch dm-raid1-changes-from-2-6-33.patch
ApplyPatch dm-crypt-changes-from-2-6-33.patch
ApplyPatch dm-snapshot-changes-from-2-6-33.patch
ApplyPatch dm-snapshot-merge-support-from-2-6-33.patch
ApplyPatch redhat-add-vhost-to-config-generic-rhel.patch
ApplyPatch virt-tun-export-underlying-socket.patch
ApplyPatch virt-mm-export-use_mm-unuse_mm-to-modules.patch
ApplyPatch virt-vhost_net-a-kernel-level-virtio-server.patch
ApplyPatch virt-vhost-add-missing-architectures.patch
ApplyPatch s390-kernel-clear-high-order-bits-after-switching-to-64-bit-mode.patch
ApplyPatch s390-zcrypt-Do-not-simultaneously-schedule-hrtimer.patch
ApplyPatch s390-dasd-support-DIAG-access-for-read-only-devices.patch
ApplyPatch s390-kernel-fix-dump-indicator.patch
ApplyPatch s390-kernel-performance-counter-fix-and-page-fault-optimization.patch
ApplyPatch s390-zcrypt-initialize-ap_messages-for-cex3-exploitation.patch
ApplyPatch s390-zcrypt-special-command-support-for-cex3-exploitation.patch
ApplyPatch s390-zcrypt-add-support-for-cex3-device-types.patch
ApplyPatch s390-zcrypt-use-definitions-for-cex3.patch
ApplyPatch s390-zcrypt-adjust-speed-rating-between-cex2-and-pcixcc.patch
ApplyPatch s390-zcrypt-adjust-speed-rating-of-cex3-adapters.patch
ApplyPatch s390-OSA-QDIO-data-connection-isolation.patch
ApplyPatch redhat-Build-in-standard-PCI-hotplug-support.patch
ApplyPatch pci-pciehp-Provide-an-option-to-disable-native-PCIe-hotplug.patch
ApplyPatch modsign-Don-t-check-e_entry-in-ELF-header.patch
ApplyPatch redhat-fixing-lastcommit-contents-for-2-6-32-2-el6.patch
ApplyPatch redhat-tagging-2-6-32-3-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-3.patch
ApplyPatch misc-Revert-utrace-introduce-utrace-implementation.patch
ApplyPatch ptrace-cleanup-ptrace_init_task-ptrace_link-path.patch
ApplyPatch ptrace-copy_process-should-disable-stepping.patch
ApplyPatch ptrace-introduce-user_single_step_siginfo-helper.patch
ApplyPatch ptrace-powerpc-implement-user_single_step_siginfo.patch
ApplyPatch ptrace-change-tracehook_report_syscall_exit-to-handle-stepping.patch
ApplyPatch ptrace-x86-implement-user_single_step_siginfo.patch
ApplyPatch ptrace-x86-change-syscall_trace_leave-to-rely-on-tracehook-when-stepping.patch
ApplyPatch signals-check-group_stop_count-after-tracehook_get_signal.patch
ApplyPatch tracehooks-kill-some-PT_PTRACED-checks.patch
ApplyPatch tracehooks-check-PT_PTRACED-before-reporting-the-single-step.patch
ApplyPatch ptrace_signal-check-PT_PTRACED-before-reporting-a-signal.patch
ApplyPatch ptrace-export-__ptrace_detach-and-do_notify_parent_cldstop.patch
ApplyPatch ptrace-reorder-the-code-in-kernel-ptrace-c.patch
ApplyPatch utrace-implement-utrace-ptrace.patch
ApplyPatch utrace-utrace-core.patch
ApplyPatch sound-ALSA-HDA-driver-update-2009-12-15.patch
ApplyPatch s390-dasd-enable-prefix-independent-of-pav-support.patch
ApplyPatch s390-dasd-remove-strings-from-s390dbf.patch
ApplyPatch s390-dasd-let-device-initialization-wait-for-LCU-setup.patch
ApplyPatch redhat-kernel-enable-hibernation-support-on-s390x.patch
ApplyPatch s390-iucv-add-work_queue-cleanup-for-suspend.patch
ApplyPatch s390-cmm-free-pages-on-hibernate.patch
ApplyPatch s390-ctcm-suspend-has-to-wait-for-outstanding-I-O.patch
ApplyPatch s390-zfcp-Don-t-fail-SCSI-commands-when-transitioning-to-blocked-fc_rport.patch
ApplyPatch s390-zfcp-Assign-scheduled-work-to-driver-queue.patch
ApplyPatch s390-zfcp-fix-ELS-ADISC-handling-to-prevent-QDIO-errors.patch
ApplyPatch s390-zfcp-improve-FSF-error-reporting.patch
ApplyPatch scsi-scsi_transport_fc-Introduce-helper-function-for-blocking-scsi_eh.patch
ApplyPatch s390-zfcp-Block-SCSI-EH-thread-for-rport-state-BLOCKED.patch
ApplyPatch uv-x86-SGI-UV-Fix-BAU-initialization.patch
ApplyPatch uv-x86-function-to-translate-from-gpa-socket_paddr.patch
ApplyPatch uv-x86-introduce-uv_gpa_is_mmr.patch
ApplyPatch uv-x86-RTC-Fix-early-expiry-handling.patch
ApplyPatch uv-x86-RTC-Add-clocksource-only-boot-option.patch
ApplyPatch uv-x86-RTC-Clean-up-error-handling.patch
ApplyPatch uv-gru-function-to-generate-chipset-IPI-values.patch
ApplyPatch uv-x86-SGI-Map-low-MMR-ranges.patch
ApplyPatch xen-wait-up-to-5-minutes-for-device-connetion-and-fix-fallout.patch
ApplyPatch xen-support-MAXSMP.patch
ApplyPatch mm-move-inc_zone_page_state-NR_ISOLATED-to-just-isolated-place.patch
ApplyPatch mm-swap_info-private-to-swapfile-c.patch
ApplyPatch mm-swap_info-change-to-array-of-pointers.patch
ApplyPatch mm-swap_info-include-first_swap_extent.patch
ApplyPatch mm-swap_info-miscellaneous-minor-cleanups.patch
ApplyPatch mm-swap_info-SWAP_HAS_CACHE-cleanups.patch
ApplyPatch mm-swap_info-swap_map-of-chars-not-shorts.patch
ApplyPatch mm-swap_info-swap-count-continuations.patch
ApplyPatch mm-swap_info-note-SWAP_MAP_SHMEM.patch
ApplyPatch mm-define-PAGE_MAPPING_FLAGS.patch
ApplyPatch mm-mlocking-in-try_to_unmap_one.patch
ApplyPatch mm-CONFIG_MMU-for-PG_mlocked.patch
ApplyPatch mm-pass-address-down-to-rmap-ones.patch
ApplyPatch mm-vmscan-have-kswapd-sleep-for-a-short-interval-and-double-check-it-should-be-asleep.patch
ApplyPatch mm-vmscan-stop-kswapd-waiting-on-congestion-when-the-min-watermark-is-not-being-met.patch
ApplyPatch mm-vmscan-separate-sc-swap_cluster_max-and-sc-nr_max_reclaim.patch
ApplyPatch mm-vmscan-kill-hibernation-specific-reclaim-logic-and-unify-it.patch
ApplyPatch mm-vmscan-zone_reclaim-dont-use-insane-swap_cluster_max.patch
ApplyPatch mm-vmscan-kill-sc-swap_cluster_max.patch
ApplyPatch mm-vmscan-make-consistent-of-reclaim-bale-out-between-do_try_to_free_page-and-shrink_zone.patch
ApplyPatch mm-vmscan-do-not-evict-inactive-pages-when-skipping-an-active-list-scan.patch
ApplyPatch mm-stop-ptlock-enlarging-struct-page.patch
ApplyPatch ksm-three-remove_rmap_item_from_tree-cleanups.patch
ApplyPatch ksm-remove-redundancies-when-merging-page.patch
ApplyPatch ksm-cleanup-some-function-arguments.patch
ApplyPatch ksm-singly-linked-rmap_list.patch
ApplyPatch ksm-separate-stable_node.patch
ApplyPatch ksm-stable_node-point-to-page-and-back.patch
ApplyPatch ksm-fix-mlockfreed-to-munlocked.patch
ApplyPatch ksm-let-shared-pages-be-swappable.patch
ApplyPatch ksm-hold-anon_vma-in-rmap_item.patch
ApplyPatch ksm-take-keyhole-reference-to-page.patch
ApplyPatch ksm-share-anon-page-without-allocating.patch
ApplyPatch ksm-mem-cgroup-charge-swapin-copy.patch
ApplyPatch ksm-rmap_walk-to-remove_migation_ptes.patch
ApplyPatch ksm-memory-hotremove-migration-only.patch
ApplyPatch ksm-remove-unswappable-max_kernel_pages.patch
ApplyPatch ksm-fix-ksm-h-breakage-of-nommu-build.patch
ApplyPatch mm-Add-mm-tracepoint-definitions-to-kmem-h.patch
ApplyPatch mm-Add-anonynmous-page-mm-tracepoints.patch
ApplyPatch mm-Add-file-page-mm-tracepoints.patch
ApplyPatch mm-Add-page-reclaim-mm-tracepoints.patch
ApplyPatch mm-Add-file-page-writeback-mm-tracepoints.patch
ApplyPatch scsi-hpsa-new-driver.patch
ApplyPatch scsi-cciss-remove-pci-ids.patch
ApplyPatch quota-Move-definition-of-QFMT_OCFS2-to-linux-quota-h.patch
ApplyPatch quota-Implement-quota-format-with-64-bit-space-and-inode-limits.patch
ApplyPatch quota-ext3-Support-for-vfsv1-quota-format.patch
ApplyPatch quota-ext4-Support-for-64-bit-quota-format.patch
ApplyPatch kvm-core-x86-Add-user-return-notifiers.patch
ApplyPatch kvm-VMX-Move-MSR_KERNEL_GS_BASE-out-of-the-vmx-autoload-msr-area.patch
ApplyPatch kvm-x86-shared-msr-infrastructure.patch
ApplyPatch kvm-VMX-Use-shared-msr-infrastructure.patch
ApplyPatch redhat-excluding-Reverts-from-changelog-too.patch
ApplyPatch redhat-tagging-2-6-32-4-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-4.patch
ApplyPatch redhat-do-not-export-redhat-directory-contents.patch
ApplyPatch x86-Remove-the-CPU-cache-size-printk-s.patch
ApplyPatch x86-Remove-CPU-cache-size-output-for-non-Intel-too.patch
ApplyPatch x86-cpu-mv-display_cacheinfo-cpu_detect_cache_sizes.patch
ApplyPatch x86-Limit-the-number-of-processor-bootup-messages.patch
ApplyPatch init-Limit-the-number-of-per-cpu-calibration-bootup-messages.patch
ApplyPatch sched-Limit-the-number-of-scheduler-debug-messages.patch
ApplyPatch x86-Limit-number-of-per-cpu-TSC-sync-messages.patch
ApplyPatch x86-Remove-enabling-x2apic-message-for-every-CPU.patch
ApplyPatch x86-ucode-amd-Load-ucode-patches-once-and-not-separately-of-each-CPU.patch
ApplyPatch block-cfq-iosched-reduce-write-depth-only-if-sync-was-delayed.patch
ApplyPatch block-cfq-Optimization-for-close-cooperating-queue-searching.patch
ApplyPatch block-cfq-iosched-Get-rid-of-cfqq-wait_busy_done-flag.patch
ApplyPatch block-cfq-iosched-Take-care-of-corner-cases-of-group-losing-share-due-to-deletion.patch
ApplyPatch block-cfq-iosched-commenting-non-obvious-initialization.patch
ApplyPatch block-cfq-Remove-wait_request-flag-when-idle-time-is-being-deleted.patch
ApplyPatch block-Fix-a-CFQ-crash-in-for-2-6-33-branch-of-block-tree.patch
ApplyPatch block-cfq-set-workload-as-expired-if-it-doesn-t-have-any-slice-left.patch
ApplyPatch block-cfq-iosched-Remove-the-check-for-same-cfq-group-from-allow_merge.patch
ApplyPatch block-cfq-iosched-Get-rid-of-nr_groups.patch
ApplyPatch block-cfq-iosched-Remove-prio_change-logic-for-workload-selection.patch
ApplyPatch netdrv-ixgbe-add-support-for-82599-KR-and-update-to-latest-upstream.patch
ApplyPatch netdrv-bnx2x-update-to-1-52-1-5.patch
ApplyPatch netdrv-update-tg3-to-version-3-105.patch
ApplyPatch scsi-scsi_dh-Change-the-scsidh_activate-interface-to-be-asynchronous.patch
ApplyPatch scsi-scsi_dh-Make-rdac-hardware-handler-s-activate-async.patch
ApplyPatch scsi-scsi_dh-Make-hp-hardware-handler-s-activate-async.patch
ApplyPatch scsi-scsi_dh-Make-alua-hardware-handler-s-activate-async.patch
ApplyPatch block-Fix-topology-stacking-for-data-and-discard-alignment.patch
ApplyPatch dlm-always-use-GFP_NOFS.patch
ApplyPatch redhat-Some-storage-related-kernel-config-parameter-changes.patch
ApplyPatch scsi-eliminate-double-free.patch
ApplyPatch scsi-make-driver-PCI-legacy-I-O-port-free.patch
ApplyPatch gfs2-Fix-potential-race-in-glock-code.patch
ApplyPatch netdrv-cnic-fixes-for-RHEL6.patch
ApplyPatch netdrv-bnx2i-update-to-2-1-0.patch
ApplyPatch mm-hwpoison-backport-the-latest-patches-from-linux-2-6-33.patch
ApplyPatch fs-ext4-wait-for-log-to-commit-when-unmounting.patch
ApplyPatch cifs-NULL-out-tcon-pSesInfo-and-srvTcp-pointers-when-chasing-DFS-referrals.patch
ApplyPatch fusion-remove-unnecessary-printk.patch
ApplyPatch fusion-fix-for-incorrect-data-underrun.patch
ApplyPatch fusion-bump-version-to-3-04-13.patch
ApplyPatch ext4-make-trim-discard-optional-and-off-by-default.patch
ApplyPatch fat-make-discard-a-mount-option.patch
ApplyPatch fs-fs-writeback-Add-helper-function-to-start-writeback-if-idle.patch
ApplyPatch fs-ext4-flush-delalloc-blocks-when-space-is-low.patch
ApplyPatch scsi-scsi_dh_rdac-add-two-IBM-devices-to-rdac_dev_list.patch
ApplyPatch vfs-force-reval-of-target-when-following-LAST_BIND-symlinks.patch
ApplyPatch input-Add-support-for-adding-i8042-filters.patch
ApplyPatch input-dell-laptop-Update-rfkill-state-on-switch-change.patch
ApplyPatch sunrpc-Deprecate-support-for-site-local-addresses.patch
ApplyPatch sunrpc-Don-t-display-zero-scope-IDs.patch
ApplyPatch s390-cio-double-free-under-memory-pressure.patch
ApplyPatch s390-cio-device-recovery-stalls-after-multiple-hardware-events.patch
ApplyPatch s390-cio-device-recovery-fails-after-concurrent-hardware-changes.patch
ApplyPatch s390-cio-setting-a-device-online-or-offline-fails-for-unknown-reasons.patch
ApplyPatch s390-cio-incorrect-device-state-after-device-recognition-and-recovery.patch
ApplyPatch s390-cio-kernel-panic-after-unexpected-interrupt.patch
ApplyPatch s390-cio-initialization-of-I-O-devices-fails.patch
ApplyPatch s390-cio-not-operational-devices-cannot-be-deactivated.patch
ApplyPatch s390-cio-erratic-DASD-I-O-behavior.patch
ApplyPatch s390-cio-DASD-cannot-be-set-online.patch
ApplyPatch s390-cio-DASD-steal-lock-task-hangs.patch
ApplyPatch s390-cio-memory-leaks-when-checking-unusable-devices.patch
ApplyPatch s390-cio-deactivated-devices-can-cause-use-after-free-panic.patch
ApplyPatch nfs-NFS-update-to-2-6-33-part-1.patch
ApplyPatch nfs-NFS-update-to-2-6-33-part-2.patch
ApplyPatch nfs-NFS-update-to-2-6-33-part-3.patch
ApplyPatch nfs-fix-insecure-export-option.patch
ApplyPatch redhat-enable-NFS_V4_1.patch
ApplyPatch x86-Compile-mce-inject-module.patch
ApplyPatch modsign-Don-t-attempt-to-sign-a-module-if-there-are-no-key-files.patch
ApplyPatch scsi-cciss-hpsa-reassign-controllers.patch
ApplyPatch scsi-cciss-fix-spinlock-use.patch
ApplyPatch redhat-disabling-temporaly-DEVTMPFS.patch
ApplyPatch redhat-don-t-use-PACKAGE_VERSION-and-PACKAGE_RELEASE.patch
ApplyPatch redhat-tagging-2-6-32-5-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-5.patch
ApplyPatch redhat-add-symbol-to-look-on-while-building-modules-block.patch
ApplyPatch stable-signal-Fix-alternate-signal-stack-check.patch
ApplyPatch stable-SCSI-osd_protocol-h-Add-missing-include.patch
ApplyPatch stable-SCSI-megaraid_sas-fix-64-bit-sense-pointer-truncation.patch
ApplyPatch stable-ext4-fix-potential-buffer-head-leak-when-add_dirent_to_buf-returns-ENOSPC.patch
ApplyPatch stable-ext4-avoid-divide-by-zero-when-trying-to-mount-a-corrupted-file-system.patch
ApplyPatch stable-ext4-fix-the-returned-block-count-if-EXT4_IOC_MOVE_EXT-fails.patch
ApplyPatch stable-ext4-fix-lock-order-problem-in-ext4_move_extents.patch
ApplyPatch stable-ext4-fix-possible-recursive-locking-warning-in-EXT4_IOC_MOVE_EXT.patch
ApplyPatch stable-ext4-plug-a-buffer_head-leak-in-an-error-path-of-ext4_iget.patch
ApplyPatch stable-ext4-make-sure-directory-and-symlink-blocks-are-revoked.patch
ApplyPatch stable-ext4-fix-i_flags-access-in-ext4_da_writepages_trans_blocks.patch
ApplyPatch stable-ext4-journal-all-modifications-in-ext4_xattr_set_handle.patch
ApplyPatch stable-ext4-don-t-update-the-superblock-in-ext4_statfs.patch
ApplyPatch stable-ext4-fix-uninit-block-bitmap-initialization-when-s_meta_first_bg-is-non-zero.patch
ApplyPatch stable-ext4-fix-block-validity-checks-so-they-work-correctly-with-meta_bg.patch
ApplyPatch stable-ext4-avoid-issuing-unnecessary-barriers.patch
ApplyPatch stable-ext4-fix-error-handling-in-ext4_ind_get_blocks.patch
ApplyPatch stable-ext4-make-norecovery-an-alias-for-noload.patch
ApplyPatch stable-ext4-Fix-double-free-of-blocks-with-EXT4_IOC_MOVE_EXT.patch
ApplyPatch stable-ext4-initialize-moved_len-before-calling-ext4_move_extents.patch
ApplyPatch stable-ext4-move_extent_per_page-cleanup.patch
ApplyPatch stable-jbd2-Add-ENOMEM-checking-in-and-for-jbd2_journal_write_metadata_buffer.patch
ApplyPatch stable-ext4-Return-the-PTR_ERR-of-the-correct-pointer-in-setup_new_group_blocks.patch
ApplyPatch stable-ext4-Avoid-data-filesystem-corruption-when-write-fails-to-copy-data.patch
ApplyPatch stable-ext4-remove-blocks-from-inode-prealloc-list-on-failure.patch
ApplyPatch stable-ext4-ext4_get_reserved_space-must-return-bytes-instead-of-blocks.patch
ApplyPatch stable-ext4-quota-macros-cleanup.patch
ApplyPatch stable-ext4-fix-incorrect-block-reservation-on-quota-transfer.patch
ApplyPatch stable-ext4-Wait-for-proper-transaction-commit-on-fsync.patch
ApplyPatch stable-ext4-Fix-potential-fiemap-deadlock-mmap_sem-vs-i_data_sem.patch
ApplyPatch stable-USB-usb-storage-fix-bug-in-fill_inquiry.patch
ApplyPatch stable-USB-option-add-pid-for-ZTE.patch
ApplyPatch stable-firewire-ohci-handle-receive-packets-with-a-data-length-of-zero.patch
ApplyPatch stable-rcu-Prepare-for-synchronization-fixes-clean-up-for-non-NO_HZ-handling-of-completed-counter.patch
ApplyPatch stable-rcu-Fix-synchronization-for-rcu_process_gp_end-uses-of-completed-counter.patch
ApplyPatch stable-rcu-Fix-note_new_gpnum-uses-of-gpnum.patch
ApplyPatch stable-rcu-Remove-inline-from-forward-referenced-functions.patch
ApplyPatch stable-perf_event-Fix-invalid-type-in-ioctl-definition.patch
ApplyPatch stable-perf_event-Initialize-data-period-in-perf_swevent_hrtimer.patch
ApplyPatch stable-PM-Runtime-Fix-lockdep-warning-in-__pm_runtime_set_status.patch
ApplyPatch stable-sched-Check-for-an-idle-shared-cache-in-select_task_rq_fair.patch
ApplyPatch stable-sched-Fix-affinity-logic-in-select_task_rq_fair.patch
ApplyPatch stable-sched-Rate-limit-newidle.patch
ApplyPatch stable-sched-Fix-and-clean-up-rate-limit-newidle-code.patch
ApplyPatch stable-x86-amd-iommu-attach-devices-to-pre-allocated-domains-early.patch
ApplyPatch stable-x86-amd-iommu-un__init-iommu_setup_msi.patch
ApplyPatch stable-x86-Calgary-IOMMU-quirk-Find-nearest-matching-Calgary-while-walking-up-the-PCI-tree.patch
ApplyPatch stable-x86-Fix-iommu-nodac-parameter-handling.patch
ApplyPatch stable-x86-GART-pci-gart_64-c-Use-correct-length-in-strncmp.patch
ApplyPatch stable-x86-ASUS-P4S800-reboot-bios-quirk.patch
ApplyPatch stable-x86-apic-Enable-lapic-nmi-watchdog-on-AMD-Family-11h.patch
ApplyPatch stable-ssb-Fix-range-check-in-sprom-write.patch
ApplyPatch stable-ath5k-allow-setting-txpower-to-0.patch
ApplyPatch stable-ath5k-enable-EEPROM-checksum-check.patch
ApplyPatch stable-hrtimer-Fix-proc-timer_list-regression.patch
ApplyPatch stable-ALSA-hrtimer-Fix-lock-up.patch
ApplyPatch stable-KVM-x86-emulator-limit-instructions-to-15-bytes.patch
ApplyPatch stable-KVM-s390-Fix-prefix-register-checking-in-arch-s390-kvm-sigp-c.patch
ApplyPatch stable-KVM-s390-Make-psw-available-on-all-exits-not-just-a-subset.patch
ApplyPatch stable-KVM-fix-irq_source_id-size-verification.patch
ApplyPatch stable-KVM-x86-include-pvclock-MSRs-in-msrs_to_save.patch
ApplyPatch stable-x86-Prevent-GCC-4-4-x-pentium-mmx-et-al-function-prologue-wreckage.patch
ApplyPatch stable-x86-Use-maccumulate-outgoing-args-for-sane-mcount-prologues.patch
ApplyPatch stable-x86-mce-don-t-restart-timer-if-disabled.patch
ApplyPatch stable-x86-mce-Set-up-timer-unconditionally.patch
ApplyPatch stable-x86-Fix-duplicated-UV-BAU-interrupt-vector.patch
ApplyPatch stable-x86-Add-new-Intel-CPU-cache-size-descriptors.patch
ApplyPatch stable-x86-Fix-typo-in-Intel-CPU-cache-size-descriptor.patch
ApplyPatch stable-pata_hpt-37x-3x2n-fix-timing-register-masks-take-2.patch
ApplyPatch stable-V4L-DVB-Fix-test-in-copy_reg_bits.patch
ApplyPatch stable-bsdacct-fix-uid-gid-misreporting.patch
ApplyPatch stable-UBI-flush-wl-before-clearing-update-marker.patch
ApplyPatch stable-jbd2-don-t-wipe-the-journal-on-a-failed-journal-checksum.patch
ApplyPatch stable-USB-xhci-Add-correct-email-and-files-to-MAINTAINERS-entry.patch
ApplyPatch stable-USB-musb_gadget_ep0-fix-unhandled-endpoint-0-IRQs-again.patch
ApplyPatch stable-USB-option-c-add-support-for-D-Link-DWM-162-U5.patch
ApplyPatch stable-USB-usbtmc-repeat-usb_bulk_msg-until-whole-message-is-transfered.patch
ApplyPatch stable-USB-usb-storage-add-BAD_SENSE-flag.patch
ApplyPatch stable-USB-Close-usb_find_interface-race-v3.patch
ApplyPatch stable-pxa-em-x270-fix-usb-hub-power-up-reset-sequence.patch
ApplyPatch stable-hfs-fix-a-potential-buffer-overflow.patch
ApplyPatch stable-md-bitmap-protect-against-bitmap-removal-while-being-updated.patch
ApplyPatch stable-futex-Take-mmap_sem-for-get_user_pages-in-fault_in_user_writeable.patch
ApplyPatch stable-devpts_get_tty-should-validate-inode.patch
ApplyPatch stable-debugfs-fix-create-mutex-racy-fops-and-private-data.patch
ApplyPatch stable-Driver-core-fix-race-in-dev_driver_string.patch
ApplyPatch stable-Serial-Do-not-read-IIR-in-serial8250_start_tx-when-UART_BUG_TXEN.patch
ApplyPatch stable-mac80211-Fix-bug-in-computing-crc-over-dynamic-IEs-in-beacon.patch
ApplyPatch stable-mac80211-Fixed-bug-in-mesh-portal-paths.patch
ApplyPatch stable-mac80211-Revert-Use-correct-sign-for-mesh-active-path-refresh.patch
ApplyPatch stable-mac80211-fix-scan-abort-sanity-checks.patch
ApplyPatch stable-wireless-correctly-report-signal-value-for-IEEE80211_HW_SIGNAL_UNSPEC.patch
ApplyPatch stable-rtl8187-Fix-wrong-rfkill-switch-mask-for-some-models.patch
ApplyPatch stable-x86-Fix-bogus-warning-in-apic_noop-apic_write.patch
ApplyPatch stable-mm-hugetlb-fix-hugepage-memory-leak-in-mincore.patch
ApplyPatch stable-mm-hugetlb-fix-hugepage-memory-leak-in-walk_page_range.patch
ApplyPatch stable-powerpc-windfarm-Add-detection-for-second-cpu-pump.patch
ApplyPatch stable-powerpc-therm_adt746x-Record-pwm-invert-bit-at-module-load-time.patch
ApplyPatch stable-powerpc-Fix-usage-of-64-bit-instruction-in-32-bit-altivec-code.patch
ApplyPatch stable-drm-radeon-kms-Add-quirk-for-HIS-X1300-board.patch
ApplyPatch stable-drm-radeon-kms-handle-vblanks-properly-with-dpms-on.patch
ApplyPatch stable-drm-radeon-kms-fix-legacy-crtc2-dpms.patch
ApplyPatch stable-drm-radeon-kms-fix-vram-setup-on-rs600.patch
ApplyPatch stable-drm-radeon-kms-rs6xx-rs740-clamp-vram-to-aperture-size.patch
ApplyPatch stable-drm-ttm-Fix-build-failure-due-to-missing-struct-page.patch
ApplyPatch stable-drm-i915-Set-the-error-code-after-failing-to-insert-new-offset-into-mm-ht.patch
ApplyPatch stable-drm-i915-Add-the-missing-clonemask-for-display-port-on-Ironlake.patch
ApplyPatch stable-xen-xenbus-make-DEVICE_ATTR-s-static.patch
ApplyPatch stable-xen-re-register-runstate-area-earlier-on-resume.patch
ApplyPatch stable-xen-restore-runstate_info-even-if-have_vcpu_info_placement.patch
ApplyPatch stable-xen-correctly-restore-pfn_to_mfn_list_list-after-resume.patch
ApplyPatch stable-xen-register-timer-interrupt-with-IRQF_TIMER.patch
ApplyPatch stable-xen-register-runstate-on-secondary-CPUs.patch
ApplyPatch stable-xen-don-t-call-dpm_resume_noirq-with-interrupts-disabled.patch
ApplyPatch stable-xen-register-runstate-info-for-boot-CPU-early.patch
ApplyPatch stable-xen-call-clock-resume-notifier-on-all-CPUs.patch
ApplyPatch stable-xen-improve-error-handling-in-do_suspend.patch
ApplyPatch stable-xen-don-t-leak-IRQs-over-suspend-resume.patch
ApplyPatch stable-xen-use-iret-for-return-from-64b-kernel-to-32b-usermode.patch
ApplyPatch stable-xen-explicitly-create-destroy-stop_machine-workqueues-outside-suspend-resume-region.patch
ApplyPatch stable-Xen-balloon-fix-totalram_pages-counting.patch
ApplyPatch stable-xen-try-harder-to-balloon-up-under-memory-pressure.patch
ApplyPatch stable-slc90e66-fix-UDMA-handling.patch
ApplyPatch stable-tcp-Stalling-connections-Fix-timeout-calculation-routine.patch
ApplyPatch stable-ip_fragment-also-adjust-skb-truesize-for-packets-not-owned-by-a-socket.patch
ApplyPatch stable-b44-WOL-setup-one-bit-off-stack-corruption-kernel-panic-fix.patch
ApplyPatch stable-sparc64-Don-t-specify-IRQF_SHARED-for-LDC-interrupts.patch
ApplyPatch stable-sparc64-Fix-overly-strict-range-type-matching-for-PCI-devices.patch
ApplyPatch stable-sparc64-Fix-stack-debugging-IRQ-stack-regression.patch
ApplyPatch stable-sparc-Set-UTS_MACHINE-correctly.patch
ApplyPatch stable-b43legacy-avoid-PPC-fault-during-resume.patch
ApplyPatch stable-tracing-Fix-event-format-export.patch
ApplyPatch stable-ath9k-fix-tx-status-reporting.patch
ApplyPatch stable-mac80211-Fix-dynamic-power-save-for-scanning.patch
ApplyPatch stable-memcg-fix-memory-memsw-usage_in_bytes-for-root-cgroup.patch
ApplyPatch stable-thinkpad-acpi-fix-default-brightness_mode-for-R50e-R51.patch
ApplyPatch stable-thinkpad-acpi-preserve-rfkill-state-across-suspend-resume.patch
ApplyPatch stable-ipw2100-fix-rebooting-hang-with-driver-loaded.patch
ApplyPatch stable-matroxfb-fix-problems-with-display-stability.patch
ApplyPatch stable-acerhdf-add-new-BIOS-versions.patch
ApplyPatch stable-asus-laptop-change-light-sens-default-values.patch
ApplyPatch stable-vmalloc-conditionalize-build-of-pcpu_get_vm_areas.patch
ApplyPatch stable-ACPI-Use-the-ARB_DISABLE-for-the-CPU-which-model-id-is-less-than-0x0f.patch
ApplyPatch stable-net-Fix-userspace-RTM_NEWLINK-notifications.patch
ApplyPatch stable-ext3-Fix-data-filesystem-corruption-when-write-fails-to-copy-data.patch
ApplyPatch stable-V4L-DVB-13116-gspca-ov519-Webcam-041e-4067-added.patch
ApplyPatch stable-bcm63xx_enet-fix-compilation-failure-after-get_stats_count-removal.patch
ApplyPatch stable-x86-Under-BIOS-control-restore-AP-s-APIC_LVTTHMR-to-the-BSP-value.patch
ApplyPatch stable-drm-i915-Avoid-NULL-dereference-with-component_only-tv_modes.patch
ApplyPatch stable-drm-i915-PineView-only-has-LVDS-and-CRT-ports.patch
ApplyPatch stable-drm-i915-Fix-LVDS-stability-issue-on-Ironlake.patch
ApplyPatch stable-mm-sigbus-instead-of-abusing-oom.patch
ApplyPatch stable-ipvs-zero-usvc-and-udest.patch
ApplyPatch stable-jffs2-Fix-long-standing-bug-with-symlink-garbage-collection.patch
ApplyPatch stable-intel-iommu-ignore-page-table-validation-in-pass-through-mode.patch
ApplyPatch stable-netfilter-xtables-document-minimal-required-version.patch
ApplyPatch stable-perf_event-Fix-incorrect-range-check-on-cpu-number.patch
ApplyPatch stable-implement-early_io-re-un-map-for-ia64.patch
ApplyPatch stable-SCSI-ipr-fix-EEH-recovery.patch
ApplyPatch stable-SCSI-qla2xxx-dpc-thread-can-execute-before-scsi-host-has-been-added.patch
ApplyPatch stable-SCSI-st-fix-mdata-page_order-handling.patch
ApplyPatch stable-SCSI-fc-class-fix-fc_transport_init-error-handling.patch
ApplyPatch stable-sched-Fix-task_hot-test-order.patch
ApplyPatch stable-x86-cpuid-Add-volatile-to-asm-in-native_cpuid.patch
ApplyPatch stable-sched-Select_task_rq_fair-must-honour-SD_LOAD_BALANCE.patch
ApplyPatch stable-clockevents-Prevent-clockevent_devices-list-corruption-on-cpu-hotplug.patch
ApplyPatch stable-pata_hpt3x2n-fix-clock-turnaround.patch
ApplyPatch stable-pata_cmd64x-fix-overclocking-of-UDMA0-2-modes.patch
ApplyPatch stable-ASoC-wm8974-fix-a-wrong-bit-definition.patch
ApplyPatch stable-sound-sgio2audio-pdaudiocf-usb-audio-initialize-PCM-buffer.patch
ApplyPatch stable-ALSA-hda-Fix-missing-capsrc_nids-for-ALC88x.patch
ApplyPatch stable-acerhdf-limit-modalias-matching-to-supported.patch
ApplyPatch stable-ACPI-EC-Fix-MSI-DMI-detection.patch
ApplyPatch stable-ACPI-Use-the-return-result-of-ACPI-lid-notifier-chain-correctly.patch
ApplyPatch stable-powerpc-Handle-VSX-alignment-faults-correctly-in-little-endian-mode.patch
ApplyPatch stable-ASoC-Do-not-write-to-invalid-registers-on-the-wm9712.patch
ApplyPatch stable-drm-radeon-fix-build-on-64-bit-with-some-compilers.patch
ApplyPatch stable-USB-emi62-fix-crash-when-trying-to-load-EMI-6-2-firmware.patch
ApplyPatch stable-USB-option-support-hi-speed-for-modem-Haier-CE100.patch
ApplyPatch stable-USB-Fix-a-bug-on-appledisplay-c-regarding-signedness.patch
ApplyPatch stable-USB-musb-gadget_ep0-avoid-SetupEnd-interrupt.patch
ApplyPatch stable-Bluetooth-Prevent-ill-timed-autosuspend-in-USB-driver.patch
ApplyPatch stable-USB-rename-usb_configure_device.patch
ApplyPatch stable-USB-fix-bugs-in-usb_-de-authorize_device.patch
ApplyPatch stable-drivers-net-usb-Correct-code-taking-the-size-of-a-pointer.patch
ApplyPatch stable-x86-SGI-UV-Fix-writes-to-led-registers-on-remote-uv-hubs.patch
ApplyPatch stable-md-Fix-unfortunate-interaction-with-evms.patch
ApplyPatch stable-dma-at_hdmac-correct-incompatible-type-for-argument-1-of-spin_lock_bh.patch
ApplyPatch stable-dma-debug-Do-not-add-notifier-when-dma-debugging-is-disabled.patch
ApplyPatch stable-dma-debug-Fix-bug-causing-build-warning.patch
ApplyPatch stable-x86-amd-iommu-Fix-initialization-failure-panic.patch
ApplyPatch stable-ioat3-fix-p-disabled-q-continuation.patch
ApplyPatch stable-ioat2-3-put-channel-hardware-in-known-state-at-init.patch
ApplyPatch stable-KVM-MMU-remove-prefault-from-invlpg-handler.patch
ApplyPatch stable-KVM-LAPIC-make-sure-IRR-bitmap-is-scanned-after-vm-load.patch
ApplyPatch stable-Libertas-fix-buffer-overflow-in-lbs_get_essid.patch
ApplyPatch stable-iwmc3200wifi-fix-array-out-of-boundary-access.patch
ApplyPatch stable-mac80211-fix-propagation-of-failed-hardware-reconfigurations.patch
ApplyPatch stable-mac80211-fix-WMM-AP-settings-application.patch
ApplyPatch stable-mac80211-Fix-IBSS-merge.patch
ApplyPatch stable-cfg80211-fix-race-between-deauth-and-assoc-response.patch
ApplyPatch stable-ath5k-fix-SWI-calibration-interrupt-storm.patch
ApplyPatch stable-ath9k-wake-hardware-for-interface-IBSS-AP-Mesh-removal.patch
ApplyPatch stable-ath9k-Fix-TX-queue-draining.patch
ApplyPatch stable-ath9k-fix-missed-error-codes-in-the-tx-status-check.patch
ApplyPatch stable-ath9k-wake-hardware-during-AMPDU-TX-actions.patch
ApplyPatch stable-ath9k-fix-suspend-by-waking-device-prior-to-stop.patch
ApplyPatch stable-ath9k_hw-Fix-possible-OOB-array-indexing-in-gen_timer_index-on-64-bit.patch
ApplyPatch stable-ath9k_hw-Fix-AR_GPIO_INPUT_EN_VAL_BT_PRIORITY_BB-and-its-shift-value-in-0x4054.patch
ApplyPatch stable-iwl3945-disable-power-save.patch
ApplyPatch stable-iwl3945-fix-panic-in-iwl3945-driver.patch
ApplyPatch stable-iwlwifi-fix-EEPROM-OTP-reading-endian-annotations-and-a-bug.patch
ApplyPatch stable-iwlwifi-fix-more-eeprom-endian-bugs.patch
ApplyPatch stable-iwlwifi-fix-40MHz-operation-setting-on-cards-that-do-not-allow-it.patch
ApplyPatch stable-mac80211-fix-race-with-suspend-and-dynamic_ps_disable_work.patch
ApplyPatch stable-NOMMU-Optimise-away-the-dac_-mmap_min_addr-tests.patch
ApplyPatch stable-sysctl_max_map_count-should-be-non-negative.patch
ApplyPatch stable-kernel-sysctl-c-fix-the-incomplete-part-of-sysctl_max_map_count-should-be-non-negative-patch.patch
ApplyPatch stable-V4L-DVB-13596-ov511-c-typo-lock-unlock.patch
ApplyPatch stable-x86-ptrace-make-genregs-32-_get-set-more-robust.patch
ApplyPatch stable-memcg-avoid-oom-killing-innocent-task-in-case-of-use_hierarchy.patch
ApplyPatch stable-e100-Fix-broken-cbs-accounting-due-to-missing-memset.patch
ApplyPatch stable-ipv6-reassembly-use-seperate-reassembly-queues-for-conntrack-and-local-delivery.patch
ApplyPatch stable-netfilter-fix-crashes-in-bridge-netfilter-caused-by-fragment-jumps.patch
ApplyPatch stable-hwmon-sht15-Off-by-one-error-in-array-index-incorrect-constants.patch
ApplyPatch stable-b43-avoid-PPC-fault-during-resume.patch
ApplyPatch stable-Keys-KEYCTL_SESSION_TO_PARENT-needs-TIF_NOTIFY_RESUME-architecture-support.patch
ApplyPatch stable-sched-Fix-balance-vs-hotplug-race.patch
ApplyPatch stable-drm-radeon-kms-fix-crtc-vblank-update-for-r600.patch
ApplyPatch stable-drm-disable-all-the-possible-outputs-crtcs-before-entering-KMS-mode.patch
ApplyPatch stable-orinoco-fix-GFP_KERNEL-in-orinoco_set_key-with-interrupts-disabled.patch
ApplyPatch stable-udf-Try-harder-when-looking-for-VAT-inode.patch
ApplyPatch stable-Add-unlocked-version-of-inode_add_bytes-function.patch
ApplyPatch stable-quota-decouple-fs-reserved-space-from-quota-reservation.patch
ApplyPatch stable-ext4-Convert-to-generic-reserved-quota-s-space-management.patch
ApplyPatch stable-ext4-fix-sleep-inside-spinlock-issue-with-quota-and-dealloc-14739.patch
ApplyPatch stable-x86-msr-Unify-rdmsr_on_cpus-wrmsr_on_cpus.patch
ApplyPatch stable-cpumask-use-modern-cpumask-style-in-drivers-edac-amd64_edac-c.patch
ApplyPatch stable-amd64_edac-unify-MCGCTL-ECC-switching.patch
ApplyPatch stable-x86-msr-Add-support-for-non-contiguous-cpumasks.patch
ApplyPatch stable-x86-msr-msrs_alloc-free-for-CONFIG_SMP-n.patch
ApplyPatch stable-amd64_edac-fix-driver-instance-freeing.patch
ApplyPatch stable-amd64_edac-make-driver-loading-more-robust.patch
ApplyPatch stable-amd64_edac-fix-forcing-module-load-unload.patch
ApplyPatch stable-sched-Sched_rt_periodic_timer-vs-cpu-hotplug.patch
ApplyPatch stable-ext4-Update-documentation-to-correct-the-inode_readahead_blks-option-name.patch
ApplyPatch stable-lguest-fix-bug-in-setting-guest-GDT-entry.patch
ApplyPatch stable-rt2x00-Disable-powersaving-for-rt61pci-and-rt2800pci.patch
ApplyPatch stable-generic_permission-MAY_OPEN-is-not-write-access.patch
ApplyPatch redhat-fix-typo-while-disabling-CONFIG_CPU_SUP_CENTAUR.patch
ApplyPatch redhat-check-if-patchutils-is-installed-before-creating-patches.patch
ApplyPatch redhat-do-a-basic-sanity-check-to-verify-the-modules-are-being-signed.patch
ApplyPatch redhat-Fix-kABI-dependency-generation.patch
ApplyPatch tpm-autoload-tpm_tis-driver.patch
ApplyPatch x86-mce-fix-confusion-between-bank-attributes-and-mce-attributes.patch
ApplyPatch netdrv-be2net-update-be2net-driver-to-latest-upstream.patch
ApplyPatch s390x-tape-incomplete-device-removal.patch
ApplyPatch s390-kernel-improve-code-generated-by-atomic-operations.patch
ApplyPatch x86-AMD-Fix-stale-cpuid4_info-shared_map-data-in-shared_cpu_map-cpumasks.patch
ApplyPatch nfs-fix-oops-in-nfs_rename.patch
ApplyPatch modsign-Remove-Makefile-modpost-qualifying-message-for-module-sign-failure.patch
ApplyPatch redhat-disable-framepointer-build-by-default.patch
ApplyPatch redhat-use-sysconf-_SC_PAGESIZE-instead-of-getpagesize.patch
ApplyPatch redhat-tagging-2-6-32-6-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-6.patch
ApplyPatch kvm-Dont-pass-kvm_run-arguments.patch
ApplyPatch kvm-Call-pic_clear_isr-on-pic-reset-to-reuse-logic-there.patch
ApplyPatch kvm-Move-irq-sharing-information-to-irqchip-level.patch
ApplyPatch kvm-Change-irq-routing-table-to-use-gsi-indexed-array.patch
ApplyPatch kvm-Maintain-back-mapping-from-irqchip-pin-to-gsi.patch
ApplyPatch kvm-Move-irq-routing-data-structure-to-rcu-locking.patch
ApplyPatch kvm-Move-irq-ack-notifier-list-to-arch-independent-code.patch
ApplyPatch kvm-Convert-irq-notifiers-lists-to-RCU-locking.patch
ApplyPatch kvm-Move-IO-APIC-to-its-own-lock.patch
ApplyPatch kvm-Drop-kvm-irq_lock-lock-from-irq-injection-path.patch
ApplyPatch kvm-Add-synchronize_srcu_expedited.patch
ApplyPatch kvm-rcu-Add-synchronize_srcu_expedited-to-the-rcutorture-test-suite.patch
ApplyPatch kvm-rcu-Add-synchronize_srcu_expedited-to-the-documentation.patch
ApplyPatch kvm-rcu-Enable-synchronize_sched_expedited-fastpath.patch
ApplyPatch kvm-modify-memslots-layout-in-struct-kvm.patch
ApplyPatch kvm-modify-alias-layout-in-x86s-struct-kvm_arch.patch
ApplyPatch kvm-split-kvm_arch_set_memory_region-into-prepare-and-commit.patch
ApplyPatch kvm-introduce-gfn_to_pfn_memslot.patch
ApplyPatch kvm-use-gfn_to_pfn_memslot-in-kvm_iommu_map_pages.patch
ApplyPatch kvm-introduce-kvm-srcu-and-convert-kvm_set_memory_region-to-SRCU-update.patch
ApplyPatch kvm-use-SRCU-for-dirty-log.patch
ApplyPatch kvm-x86-switch-kvm_set_memory_alias-to-SRCU-update.patch
ApplyPatch kvm-convert-io_bus-to-SRCU.patch
ApplyPatch kvm-switch-vcpu-context-to-use-SRCU.patch
ApplyPatch kvm-convert-slots_lock-to-a-mutex.patch
ApplyPatch kvm-Bump-maximum-vcpu-count-to-64.patch
ApplyPatch kvm-avoid-taking-ioapic-mutex-for-non-ioapic-EOIs.patch
ApplyPatch kvm-VMX-Use-macros-instead-of-hex-value-on-cr0-initialization.patch
ApplyPatch kvm-SVM-Reset-cr0-properly-on-vcpu-reset.patch
ApplyPatch kvm-SVM-init_vmcb-remove-redundant-save-cr0-initialization.patch
ApplyPatch kvm-fix-kvmclock-adjust-offset-ioctl-to-match-upstream.patch
ApplyPatch kvm-x86-Add-KVM_GET-SET_VCPU_EVENTS.patch
ApplyPatch kvm-x86-Extend-KVM_SET_VCPU_EVENTS-with-selective-updates.patch
ApplyPatch kvm-remove-pre_task_link-setting-in-save_state_to_tss16.patch
ApplyPatch kvm-SVM-Move-INTR-vmexit-out-of-atomic-code.patch
ApplyPatch kvm-SVM-Notify-nested-hypervisor-of-lost-event-injections.patch
ApplyPatch kvm-SVM-remove-needless-mmap_sem-acquision-from-nested_svm_map.patch
ApplyPatch kvm-VMX-Disable-unrestricted-guest-when-EPT-disabled.patch
ApplyPatch kvm-x86-disallow-multiple-KVM_CREATE_IRQCHIP.patch
ApplyPatch kvm-x86-disallow-KVM_-SET-GET-_LAPIC-without-allocated-in-kernel-lapic.patch
ApplyPatch kvm-x86-disable-paravirt-mmu-reporting.patch
ApplyPatch kvm-Allow-internal-errors-reported-to-userspace-to-carry-extra-data.patch
ApplyPatch kvm-VMX-Report-unexpected-simultaneous-exceptions-as-internal-errors.patch
ApplyPatch kvm-fix-lock-imbalance-in-kvm_-_irq_source_id.patch
ApplyPatch kvm-only-clear-irq_source_id-if-irqchip-is-present.patch
ApplyPatch kvm-Fix-possible-circular-locking-in-kvm_vm_ioctl_assign_device.patch
ApplyPatch block-Fix-incorrect-alignment-offset-reporting-and-update-documentation.patch
ApplyPatch block-Correct-handling-of-bottom-device-misaligment.patch
ApplyPatch block-Fix-discard-alignment-calculation-and-printing.patch
ApplyPatch block-bdev_stack_limits-wrapper.patch
ApplyPatch dm-Fix-device-mapper-topology-stacking.patch
ApplyPatch block-Stop-using-byte-offsets.patch
ApplyPatch dm-add-feature-flags-to-reduce-future-kABI-impact.patch
ApplyPatch netdrv-igb-Update-igb-driver-to-support-Barton-Hills.patch
ApplyPatch redhat-Don-t-compile-DECNET.patch
ApplyPatch redhat-fs-don-t-build-freevxfs.patch
ApplyPatch redhat-disable-KVM-on-non-x86_64-arches.patch
ApplyPatch gfs-GFS2-Fix-up-system-xattrs.patch
ApplyPatch gfs-VFS-Add-forget_all_cached_acls.patch
ApplyPatch gfs-GFS2-Use-forget_all_cached_acls.patch
ApplyPatch gfs-GFS2-Use-gfs2_set_mode-instead-of-munge_mode.patch
ApplyPatch gfs-GFS2-Clean-up-ACLs.patch
ApplyPatch gfs-GFS2-Add-cached-ACLs-support.patch
ApplyPatch gfs-VFS-Use-GFP_NOFS-in-posix_acl_from_xattr.patch
ApplyPatch gfs-GFS2-Fix-gfs2_xattr_acl_chmod.patch
ApplyPatch gfs2-Fix-o-meta-mounts-for-subsequent-mounts.patch
ApplyPatch gfs2-Alter-arguments-of-gfs2_quota-statfs_sync.patch
ApplyPatch gfs2-Hook-gfs2_quota_sync-into-VFS-via-gfs2_quotactl_ops.patch
ApplyPatch gfs2-Remove-obsolete-code-in-quota-c.patch
ApplyPatch gfs2-Add-get_xstate-quota-function.patch
ApplyPatch gfs2-Add-proper-error-reporting-to-quota-sync-via-sysfs.patch
ApplyPatch gfs2-Remove-constant-argument-from-qdsb_get.patch
ApplyPatch gfs2-Remove-constant-argument-from-qd_get.patch
ApplyPatch gfs2-Clean-up-gfs2_adjust_quota-and-do_glock.patch
ApplyPatch gfs2-Add-get_xquota-support.patch
ApplyPatch gfs2-Add-set_xquota-support.patch
ApplyPatch gfs2-Improve-statfs-and-quota-usability.patch
ApplyPatch gfs2-remove-division-from-new-statfs-code.patch
ApplyPatch gfs2-add-barrier-nobarrier-mount-options.patch
ApplyPatch gfs2-only-show-nobarrier-option-on-proc-mounts-when-the-option-is-active.patch
ApplyPatch gfs-GFS2-Fix-lock-ordering-in-gfs2_check_blk_state.patch
ApplyPatch gfs-GFS2-Fix-locking-bug-in-rename.patch
ApplyPatch gfs-GFS2-Ensure-uptodate-inode-size-when-using-O_APPEND.patch
ApplyPatch gfs-GFS2-Fix-glock-refcount-issues.patch
ApplyPatch powerpc-pseries-Add-extended_cede_processor-helper-function.patch
ApplyPatch powerpc-pseries-Add-hooks-to-put-the-CPU-into-an-appropriate-offline-state.patch
ApplyPatch powerpc-Kernel-handling-of-Dynamic-Logical-Partitioning.patch
ApplyPatch powerpc-sysfs-cpu-probe-release-files.patch
ApplyPatch powerpc-CPU-DLPAR-handling.patch
ApplyPatch powerpc-Add-code-to-online-offline-CPUs-of-a-DLPAR-node.patch
ApplyPatch powerpc-cpu-allocation-deallocation-process.patch
ApplyPatch powerpc-pseries-Correct-pseries-dlpar-c-build-break-without-CONFIG_SMP.patch
ApplyPatch net-dccp-fix-module-load-dependency-btw-dccp_probe-and-dccp.patch
ApplyPatch drm-drm-edid-update-to-2-6-33-EDID-parser-code.patch
ApplyPatch drm-mm-patch-drm-core-memory-range-manager-up-to-2-6-33.patch
ApplyPatch drm-ttm-rollup-upstream-TTM-fixes.patch
ApplyPatch drm-unlocked-ioctl-support-for-core-macro-fixes.patch
ApplyPatch drm-add-new-userspace-core-drm-interfaces-from-2-6-33.patch
ApplyPatch drm-remove-address-mask-param-for-drm_pci_alloc.patch
ApplyPatch drm-kms-rollup-KMS-core-and-helper-changes-to-2-6-33.patch
ApplyPatch drm-radeon-intel-realign-displayport-helper-code-with-upstream.patch
ApplyPatch drm-i915-bring-Intel-DRM-KMS-driver-up-to-2-6-33.patch
ApplyPatch drm-radeon-kms-update-to-2-6-33-without-TTM-API-changes.patch
ApplyPatch drm-ttm-validation-API-changes-ERESTART-fixes.patch
ApplyPatch drm-nouveau-update-to-2-6-33-level.patch
ApplyPatch x86-allow-fbdev-primary-video-code-on-64-bit.patch
ApplyPatch offb-add-support-for-framebuffer-handoff-to-offb.patch
ApplyPatch drm-minor-printk-fixes-from-upstream.patch
ApplyPatch redhat-tagging-2-6-32-7-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-7.patch
ApplyPatch build-Revert-redhat-disabling-temporaly-DEVTMPFS.patch
ApplyPatch redhat-tagging-2-6-32-8-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-8.patch
ApplyPatch serial-8250-add-support-for-DTR-DSR-hardware-flow-control.patch
ApplyPatch s390x-qeth-Support-for-HiperSockets-Network-Traffic-Analyzer.patch
ApplyPatch s390-qeth-fix-packet-loss-if-TSO-is-switched-on.patch
ApplyPatch s390x-tape-Add-pr_fmt-macro-to-all-tape-source-files.patch
ApplyPatch net-dccp-modify-how-dccp-creates-slab-caches-to-prevent-bug-halt-in-SLUB.patch
ApplyPatch scsi-sync-fcoe-with-upstream.patch
ApplyPatch block-Honor-the-gfp_mask-for-alloc_page-in-blkdev_issue_discard.patch
ApplyPatch sound-ALSA-HDA-driver-update-2009-12-15-2.patch
ApplyPatch scsi-mpt2sas-use-sas-address-instead-of-handle-as-a-lookup.patch
ApplyPatch scsi-mpt2sas-fix-expander-remove-fail.patch
ApplyPatch scsi-mpt2sas-check-for-valid-response-info.patch
ApplyPatch scsi-mpt2sas-new-device-SAS2208-support.patch
ApplyPatch scsi-mpt2sas-adding-MPI-Headers-revision-L.patch
ApplyPatch scsi-mpt2sas-stop-driver-when-firmware-encounters-faults.patch
ApplyPatch scsi-mpt2sas-fix-some-comments.patch
ApplyPatch scsi-mpt2sas-add-command-line-option-diag_buffer_enable.patch
ApplyPatch scsi-mpt2sas-add-extended-type-for-diagnostic-buffer-support.patch
ApplyPatch scsi-mpt2sas-add-TimeStamp-support-when-sending-ioc_init.patch
ApplyPatch scsi-mpt2sas-limit-the-max_depth-to-32-for-SATA-devices.patch
ApplyPatch scsi-mpt2sas-add-new-info-messages-for-IR-and-Expander-events.patch
ApplyPatch scsi-mpt2sas-retrieve-the-ioc-facts-prior-to-putting-the-controller-into-READY-state.patch
ApplyPatch scsi-mpt2sas-return-DID_TRANSPORT_DISRUPTED-in-nexus-loss-and-SCSI_MLQUEUE_DEVICE_BUSY-if-device-is-busy.patch
ApplyPatch scsi-mpt2sas-mpt2sas_base_get_sense_buffer_dma-returns-little-endian.patch
ApplyPatch scsi-mpt2sas-fix-PPC-endian-bug.patch
ApplyPatch scsi-mpt2sas-freeze-the-sdev-IO-queue-when-firmware-sends-internal-device-reset.patch
ApplyPatch scsi-mpt2sas-add-support-for-RAID-Action-System-Shutdown-Initiated-at-OS-Shutdown.patch
ApplyPatch scsi-mpt2sas-don-t-update-links-nor-unblock-device-at-no-link-rate-change.patch
ApplyPatch scsi-mpt2sas-Bump-version-03-100-03-00.patch
ApplyPatch scsi-megaraid-upgrade-to-4-17.patch
ApplyPatch uv-x86-SGI-Dont-track-GRU-space-in-PAT.patch
ApplyPatch uv-x86-mm-Call-is_untracked_pat_range-rather-than-is_ISA_range.patch
ApplyPatch uv-x86-mm-is_untracked_pat_range-takes-a-normal-semiclosed-range.patch
ApplyPatch uv-x86-platform-Change-is_untracked_pat_range-to-bool.patch
ApplyPatch uv-x86-Change-is_ISA_range-into-an-inline-function.patch
ApplyPatch uv-x86-mm-Correct-the-implementation-of-is_untracked_pat_range.patch
ApplyPatch uv-x86-RTC-Rename-generic_interrupt-to-x86_platform_ipi.patch
ApplyPatch uv-x86-RTC-Always-enable-RTC-clocksource.patch
ApplyPatch uv-x86-SGI-Fix-irq-affinity-for-hub-based-interrupts.patch
ApplyPatch uv-x86-apic-Move-SGI-UV-functionality-out-of-generic-IO-APIC-code.patch
ApplyPatch uv-x86-irq-Allow-0xff-for-proc-irq-n-smp_affinity-on-an-8-cpu-system.patch
ApplyPatch uv-x86-Remove-move_cleanup_count-from-irq_cfg.patch
ApplyPatch uv-x86-irq-Check-move_in_progress-before-freeing-the-vector-mapping.patch
ApplyPatch uv-xpc-needs-to-provide-an-abstraction-for-uv_gpa.patch
ApplyPatch uv-x86-update-XPC-to-handle-updated-BIOS-interface.patch
ApplyPatch uv-x86-xpc-NULL-deref-when-mesq-becomes-empty.patch
ApplyPatch uv-x86-xpc_make_first_contact-hang-due-to-not-accepting-ACTIVE-state.patch
ApplyPatch uv-x86-XPC-receive-message-reuse-triggers-invalid-BUG_ON.patch
ApplyPatch uv-XPC-pass-nasid-instead-of-nid-to-gru_create_message_queue.patch
ApplyPatch gru-GRU-Rollup-patch.patch
ApplyPatch uv-React-2-6-32-y-isolcpus-broken-in-2-6-32-y-kernel.patch
ApplyPatch nfs-nfsd-make-sure-data-is-on-disk-before-calling-fsync.patch
ApplyPatch nfs-sunrpc-fix-peername-failed-on-closed-listener.patch
ApplyPatch nfs-SUNRPC-Fix-up-an-error-return-value-in-gss_import_sec_context_kerberos.patch
ApplyPatch nfs-SUNRPC-Fix-the-return-value-in-gss_import_sec_context.patch
ApplyPatch nfs-sunrpc-on-successful-gss-error-pipe-write-don-t-return-error.patch
ApplyPatch nfs-sunrpc-fix-build-time-warning.patch
ApplyPatch scsi-bfa-update-from-2-1-2-0-to-2-1-2-1.patch
ApplyPatch scsi-qla2xxx-Update-support-for-FC-FCoE-HBA-CNA.patch
ApplyPatch irq-Expose-the-irq_desc-node-as-proc-irq-node.patch
ApplyPatch cgroups-fix-for-kernel-BUG-at-kernel-cgroup-c-790.patch
ApplyPatch tracing-tracepoint-Add-signal-tracepoints.patch
ApplyPatch block-direct-io-cleanup-blockdev_direct_IO-locking.patch
ApplyPatch pci-PCIe-AER-honor-ACPI-HEST-FIRMWARE-FIRST-mode.patch
ApplyPatch x86-Add-kernel-pagefault-tracepoint-for-x86-x86_64.patch
ApplyPatch fs-xfs-2-6-33-updates.patch
ApplyPatch x86-dell-wmi-Add-support-for-new-Dell-systems.patch
ApplyPatch x86-core-make-LIST_POISON-less-deadly.patch
ApplyPatch kvm-fix-cleanup_srcu_struct-on-vm-destruction.patch
ApplyPatch redhat-tagging-2-6-32-9-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-9.patch
ApplyPatch scsi-scsi_transport_fc-Allow-LLD-to-reset-FC-BSG-timeout.patch
ApplyPatch s390x-zfcp-introduce-BSG-timeout-callback.patch
ApplyPatch s390x-zfcp-set-HW-timeout-requested-by-BSG-request.patch
ApplyPatch redhat-config-increase-printk-buffer.patch
ApplyPatch netdrv-qlge-update-to-upstream-version-v1-00-00-23-00-00-01.patch
ApplyPatch gfs-Add-quota-netlink-support.patch
ApplyPatch gfs-Use-dquot_send_warning.patch
ApplyPatch netdrv-e1000e-update-to-the-latest-upstream.patch
ApplyPatch x86-Disable-Memory-hot-add-on-x86-32-bit.patch
ApplyPatch utrace-fix-utrace_maybe_reap-vs-find_matching_engine-race.patch
ApplyPatch perf-add-kernel-internal-interface.patch
ApplyPatch perf-improve-error-reporting.patch
ApplyPatch perf-Add-a-callback-to-perf-events.patch
ApplyPatch perf-Allow-for-custom-overflow-handlers.patch
ApplyPatch perf-Fix-PERF_FORMAT_GROUP-scale-info.patch
ApplyPatch perf-Fix-event-scaling-for-inherited-counters.patch
ApplyPatch perf-Fix-locking-for-PERF_FORMAT_GROUP.patch
ApplyPatch perf-Use-overflow-handler-instead-of-the-event-callback.patch
ApplyPatch perf-Remove-the-event-callback-from-perf-events.patch
ApplyPatch s390x-ptrace-dont-abuse-PT_PTRACED.patch
ApplyPatch s390x-fix-loading-of-PER-control-registers-for-utrace.patch
ApplyPatch scsi-aic79xx-check-for-non-NULL-scb-in-ahd_handle_nonpkt_busfree.patch
ApplyPatch sound-Fix-SPDIF-In-for-AD1988-codecs-add-Intel-Cougar-IDs.patch
ApplyPatch x86-Force-irq-complete-move-during-cpu-offline.patch
ApplyPatch netdrv-vxge-fix-issues-found-in-Neterion-testing.patch
ApplyPatch mm-mmap-don-t-return-ENOMEM-when-mapcount-is-temporarily-exceeded-in-munmap.patch
ApplyPatch redhat-tagging-2-6-32-10-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-10.patch
ApplyPatch stable-untangle-the-do_mremap-mess.patch
ApplyPatch stable-fasync-split-fasync_helper-into-separate-add-remove-functions.patch
ApplyPatch stable-ASoC-fix-params_rate-macro-use-in-several-codecs.patch
ApplyPatch stable-modules-Skip-empty-sections-when-exporting-section-notes.patch
ApplyPatch stable-exofs-simple_write_end-does-not-mark_inode_dirty.patch
ApplyPatch stable-Revert-x86-Side-step-lguest-problem-by-only-building-cmpxchg8b_emu-for-pre-Pentium.patch
ApplyPatch stable-rtc_cmos-convert-shutdown-to-new-pnp_driver-shutdown.patch
ApplyPatch stable-drivers-cpuidle-governors-menu-c-fix-undefined-reference-to-__udivdi3.patch
ApplyPatch stable-lib-rational-c-needs-module-h.patch
ApplyPatch stable-dma-debug-allow-DMA_BIDIRECTIONAL-mappings-to-be-synced-with-DMA_FROM_DEVICE-and.patch
ApplyPatch stable-kernel-signal-c-fix-kernel-information-leak-with-print-fatal-signals-1.patch
ApplyPatch stable-mmc_block-add-dev_t-initialization-check.patch
ApplyPatch stable-mmc_block-fix-probe-error-cleanup-bug.patch
ApplyPatch stable-mmc_block-fix-queue-cleanup.patch
ApplyPatch stable-ALSA-ac97-Add-Dell-Dimension-2400-to-Headphone-Line-Jack-Sense-blacklist.patch
ApplyPatch stable-ALSA-atiixp-Specify-codec-for-Foxconn-RC4107MA-RS2.patch
ApplyPatch stable-ASoC-Fix-WM8350-DSP-mode-B-configuration.patch
ApplyPatch stable-netfilter-ebtables-enforce-CAP_NET_ADMIN.patch
ApplyPatch stable-netfilter-nf_ct_ftp-fix-out-of-bounds-read-in-update_nl_seq.patch
ApplyPatch stable-hwmon-coretemp-Fix-TjMax-for-Atom-N450-D410-D510-CPUs.patch
ApplyPatch stable-hwmon-adt7462-Fix-pin-28-monitoring.patch
ApplyPatch stable-quota-Fix-dquot_transfer-for-filesystems-different-from-ext4.patch
ApplyPatch stable-xen-fix-hang-on-suspend.patch
ApplyPatch stable-iwlwifi-fix-iwl_queue_used-bug-when-read_ptr-write_ptr.patch
ApplyPatch stable-ath5k-Fix-eeprom-checksum-check-for-custom-sized-eeproms.patch
ApplyPatch stable-cfg80211-fix-syntax-error-on-user-regulatory-hints.patch
ApplyPatch stable-iwl-off-by-one-bug.patch
ApplyPatch stable-mac80211-add-missing-sanity-checks-for-action-frames.patch
ApplyPatch stable-libertas-Remove-carrier-signaling-from-the-scan-code.patch
ApplyPatch stable-kernel-sysctl-c-fix-stable-merge-error-in-NOMMU-mmap_min_addr.patch
ApplyPatch stable-mac80211-fix-skb-buffering-issue-and-fixes-to-that.patch
ApplyPatch stable-fix-braindamage-in-audit_tree-c-untag_chunk.patch
ApplyPatch stable-fix-more-leaks-in-audit_tree-c-tag_chunk.patch
ApplyPatch stable-module-handle-ppc64-relocating-kcrctabs-when-CONFIG_RELOCATABLE-y.patch
ApplyPatch stable-ipv6-skb_dst-can-be-NULL-in-ipv6_hop_jumbo.patch
ApplyPatch misc-Revert-stable-module-handle-ppc64-relocating-kcrctabs-when-CONFIG_RELOCATABLE-y.patch
ApplyPatch netdrv-e1000e-enhance-frame-fragment-detection.patch
ApplyPatch redhat-kABI-internal-only-files.patch
ApplyPatch drm-bring-RHEL6-radeon-drm-up-to-2-6-33-rc4-5-level.patch
ApplyPatch s390x-qeth-set-default-BLKT-settings-dependend-on-OSA-hw-level.patch
ApplyPatch block-dm-replicator-documentation-and-module-registry.patch
ApplyPatch block-dm-replicator-replication-log-and-site-link-handler-interfaces-and-main-replicator-module.patch
ApplyPatch block-dm-replicator-ringbuffer-replication-log-handler.patch
ApplyPatch block-dm-replicator-blockdev-site-link-handler.patch
ApplyPatch block-dm-raid45-add-raid45-target.patch
ApplyPatch dm-dm-raid45-export-missing-dm_rh_inc.patch
ApplyPatch kdump-Remove-the-32MB-limitation-for-crashkernel.patch
ApplyPatch redhat-tagging-2-6-32-11-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-11.patch
ApplyPatch redhat-Revert-edac-amd64_edac-disabling-temporarily.patch
ApplyPatch x86-Use-EOI-register-in-io-apic-on-intel-platforms.patch
ApplyPatch x86-Remove-local_irq_enable-local_irq_disable-in-fixup_irqs.patch
ApplyPatch x86-io-apic-Move-the-effort-of-clearing-remoteIRR-explicitly-before-migrating-the-irq.patch
ApplyPatch x86-ioapic-Fix-the-EOI-register-detection-mechanism.patch
ApplyPatch x86-ioapic-Document-another-case-when-level-irq-is-seen-as-an-edge.patch
ApplyPatch x86-Remove-unnecessary-mdelay-from-cpu_disable_common.patch
ApplyPatch redhat-rpadlpar_io-should-be-built-in-kernel.patch
ApplyPatch x86-msr-cpuid-Register-enough-minors-for-the-MSR-and-CPUID-drivers.patch
ApplyPatch scsi-Sync-be2iscsi-with-upstream.patch
ApplyPatch redhat-config-disable-CONFIG_X86_CPU_DEBUG.patch
ApplyPatch pci-Always-set-prefetchable-base-limit-upper32-registers.patch
ApplyPatch mm-Memory-tracking-for-Stratus.patch
ApplyPatch redhat-enable-Memory-tracking-for-Stratus.patch
ApplyPatch drm-radeon-possible-security-issue.patch
ApplyPatch redhat-tagging-2-6-32-12-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-12.patch
ApplyPatch mm-Memory-tracking-for-Stratus-2.patch
ApplyPatch kdump-backport-upstream-ppc64-kcrctab-fixes.patch
ApplyPatch x86-acpi-Export-acpi_pci_irq_-add-del-_prt.patch
ApplyPatch kvm-Fix-race-between-APIC-TMR-and-IRR.patch
ApplyPatch kvm-x86-Fix-host_mapping_level.patch
ApplyPatch kvm-MMU-bail-out-pagewalk-on-kvm_read_guest-error.patch
ApplyPatch kvm-x86-Fix-probable-memory-leak-of-vcpu-arch-mce_banks.patch
ApplyPatch kvm-x86-Fix-leak-of-free-lapic-date-in-kvm_arch_vcpu_init.patch
ApplyPatch kvm-only-allow-one-gsi-per-fd.patch
ApplyPatch kvm-properly-check-max-PIC-pin-in-irq-route-setup.patch
ApplyPatch kvm-eventfd-allow-atomic-read-and-waitqueue-remove.patch
ApplyPatch kvm-fix-spurious-interrupt-with-irqfd.patch
ApplyPatch x86-Add-AMD-Node-ID-MSR-support.patch
ApplyPatch x86-Fix-crash-when-profiling-more-than-28-events.patch
ApplyPatch virtio-console-comment-cleanup.patch
ApplyPatch virtio-console-statically-initialize-virtio_cons.patch
ApplyPatch virtio-hvc_console-make-the-ops-pointer-const.patch
ApplyPatch virtio-hvc_console-Remove-__devinit-annotation-from-hvc_alloc.patch
ApplyPatch virtio-console-We-support-only-one-device-at-a-time.patch
ApplyPatch virtio-console-port-encapsulation.patch
ApplyPatch virtio-console-encapsulate-buffer-information-in-a-struct.patch
ApplyPatch virtio-console-ensure-add_inbuf-can-work-for-multiple-ports-as-well.patch
ApplyPatch virtio-console-introduce-a-get_inbuf-helper-to-fetch-bufs-from-in_vq.patch
ApplyPatch virtio-console-use-vdev-priv-to-avoid-accessing-global-var.patch
ApplyPatch virtio-console-don-t-assume-a-single-console-port.patch
ApplyPatch virtio-console-remove-global-var.patch
ApplyPatch virtio-console-struct-ports-for-multiple-ports-per-device.patch
ApplyPatch virtio-console-ensure-console-size-is-updated-on-hvc-open.patch
ApplyPatch virtio-console-Separate-out-console-specific-data-into-a-separate-struct.patch
ApplyPatch virtio-console-Separate-out-console-init-into-a-new-function.patch
ApplyPatch virtio-console-Separate-out-find_vqs-operation-into-a-different-function.patch
ApplyPatch virtio-console-Introduce-function-to-hand-off-data-from-host-to-readers.patch
ApplyPatch virtio-console-Introduce-a-send_buf-function-for-a-common-path-for-sending-data-to-host.patch
ApplyPatch virtio-console-Add-a-new-MULTIPORT-feature-support-for-generic-ports.patch
ApplyPatch virtio-console-Prepare-for-writing-to-reading-from-userspace-buffers.patch
ApplyPatch virtio-console-Associate-each-port-with-a-char-device.patch
ApplyPatch virtio-console-Add-file-operations-to-ports-for-open-read-write-poll.patch
ApplyPatch virtio-console-Ensure-only-one-process-can-have-a-port-open-at-a-time.patch
ApplyPatch virtio-console-Register-with-sysfs-and-create-a-name-attribute-for-ports.patch
ApplyPatch virtio-console-Remove-cached-data-on-port-close.patch
ApplyPatch virtio-console-Handle-port-hot-plug.patch
ApplyPatch virtio-Add-ability-to-detach-unused-buffers-from-vrings.patch
ApplyPatch virtio-hvc_console-Export-GPL-ed-hvc_remove.patch
ApplyPatch virtio-console-Add-ability-to-hot-unplug-ports.patch
ApplyPatch virtio-console-Add-debugfs-files-for-each-port-to-expose-debug-info.patch
ApplyPatch virtio-console-show-error-message-if-hvc_alloc-fails-for-console-ports.patch
ApplyPatch redhat-tagging-2-6-32-13-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-13.patch
ApplyPatch uv-x86-Add-function-retrieving-node-controller-revision-number.patch
ApplyPatch uv-x86-Ensure-hub-revision-set-for-all-ACPI-modes.patch
ApplyPatch s390x-cio-channel-path-vary-operation-has-no-effect.patch
ApplyPatch s390x-zcrypt-Do-not-remove-coprocessor-in-case-of-error-8-72.patch
ApplyPatch s390x-dasd-Fix-null-pointer-in-s390dbf-and-discipline-checking.patch
ApplyPatch redhat-Enable-oprofile-multiplexing-on-x86_64-and-x86-architectures-only.patch
ApplyPatch netdrv-update-tg3-to-version-3-106-and-fix-panic.patch
ApplyPatch redhat-disable-CONFIG_OPTIMIZE_FOR_SIZE.patch
ApplyPatch s390x-dasd-fix-online-offline-race.patch
ApplyPatch redhat-tagging-2-6-32-14-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-14.patch
ApplyPatch gfs-GFS2-Fix-refcnt-leak-on-gfs2_follow_link-error-path.patch
ApplyPatch gfs-GFS2-Use-GFP_NOFS-for-alloc-structure.patch
ApplyPatch gfs-GFS2-Use-MAX_LFS_FILESIZE-for-meta-inode-size.patch
ApplyPatch gfs-GFS2-Wait-for-unlock-completion-on-umount.patch
ApplyPatch gfs-GFS2-Extend-umount-wait-coverage-to-full-glock-lifetime.patch
ApplyPatch x86-intr-remap-generic-support-for-remapping-HPET-MSIs.patch
ApplyPatch x86-arch-specific-support-for-remapping-HPET-MSIs.patch
ApplyPatch x86-Disable-HPET-MSI-on-ATI-SB700-SB800.patch
ApplyPatch fs-ext4-fix-type-of-offset-in-ext4_io_end.patch
ApplyPatch redhat-disable-CONFIG_DEBUG_PERF_USE_VMALLOC-on-production-kernels.patch
ApplyPatch x86-fix-Add-AMD-Node-ID-MSR-support.patch
ApplyPatch redhat-config-disable-CONFIG_VMI.patch
ApplyPatch quota-64-bit-quota-format-fixes.patch
ApplyPatch block-cfq-Do-not-idle-on-async-queues-and-drive-deeper-WRITE-depths.patch
ApplyPatch block-blk-cgroup-Fix-lockdep-warning-of-potential-deadlock-in-blk-cgroup.patch
ApplyPatch redhat-tagging-2-6-32-15-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-15.patch
ApplyPatch redhat-x86_64-enable-function-tracers.patch
ApplyPatch nfs-nfsd41-nfsd4_decode_compound-does-not-recognize-all-ops.patch
ApplyPatch nfs-nfsd4-Use-FIRST_NFS4_OP-in-nfsd4_decode_compound.patch
ApplyPatch nfs-nfsd-use-vfs_fsync-for-non-directories.patch
ApplyPatch nfs-nfsd41-Create-the-recovery-entry-for-the-NFSv4-1-client.patch
ApplyPatch nfs-nfsd-4-1-has-an-rfc-number.patch
ApplyPatch nfs-SUNRPC-Use-rpc_pton-in-ip_map_parse.patch
ApplyPatch nfs-NFSD-Support-AF_INET6-in-svc_addsock-function.patch
ApplyPatch nfs-SUNRPC-Bury-ifdef-IPV6-in-svc_create_xprt.patch
ApplyPatch nfs-SUNRPC-NFS-kernel-APIs-shouldn-t-return-ENOENT-for-transport-not-found.patch
ApplyPatch nfs-NFSD-Create-PF_INET6-listener-in-write_ports.patch
ApplyPatch nfs-nfs41-Adjust-max-cache-response-size-value.patch
ApplyPatch nfs-nfs41-Check-slot-table-for-referring-calls.patch
ApplyPatch nfs-nfs41-Process-callback-s-referring-call-list.patch
ApplyPatch nfs-nfs41-fix-wrong-error-on-callback-header-xdr-overflow.patch
ApplyPatch nfs-nfs41-directly-encode-back-channel-error.patch
ApplyPatch nfs-nfs41-remove-uneeded-checks-in-callback-processing.patch
ApplyPatch nfs-nfs41-prepare-for-back-channel-drc.patch
ApplyPatch nfs-nfs41-back-channel-drc-minimal-implementation.patch
ApplyPatch nfs-nfs41-implement-cb_recall_slot.patch
ApplyPatch nfs-nfs41-resize-slot-table-in-reset.patch
ApplyPatch nfs-nfs41-fix-nfs4_callback_recallslot.patch
ApplyPatch nfs-nfs41-clear-NFS4CLNT_RECALL_SLOT-bit-on-session-reset.patch
ApplyPatch nfs-nfs41-cleanup-callback-code-to-use-__be32-type.patch
ApplyPatch nfs-Fix-a-reference-leak-in-nfs_wb_cancel_page.patch
ApplyPatch nfs-Try-to-commit-unstable-writes-in-nfs_release_page.patch
ApplyPatch nfs-Make-nfs_commitdata_release-static.patch
ApplyPatch nfs-Avoid-warnings-when-CONFIG_NFS_V4-n.patch
ApplyPatch nfs-Ensure-that-the-NFSv4-locking-can-recover-from-stateid-errors.patch
ApplyPatch nfs-NFSv4-Don-t-allow-posix-locking-against-servers-that-don-t-support-it.patch
ApplyPatch nfs-NFSv4-1-Don-t-call-nfs4_schedule_state_recovery-unnecessarily.patch
ApplyPatch nfs-Ensure-that-we-handle-NFS4ERR_STALE_STATEID-correctly.patch
ApplyPatch nfs-sunrpc-cache-fix-module-refcnt-leak-in-a-failure-path.patch
ApplyPatch scsi-lpfc-Update-from-8-3-4-to-8-3-5-4-FC-FCoE.patch
ApplyPatch dm-fix-kernel-panic-at-releasing-bio-on-recovery-failed-region.patch
ApplyPatch dm-dm-raid1-fix-deadlock-at-suspending-failed-device.patch
ApplyPatch vhost-fix-high-32-bit-in-FEATURES-ioctls.patch
ApplyPatch vhost-prevent-modification-of-an-active-ring.patch
ApplyPatch vhost-add-access_ok-checks.patch
ApplyPatch vhost-make-default-mapping-empty-by-default.patch
ApplyPatch vhost-access-check-thinko-fixes.patch
ApplyPatch vhost-vhost-net-comment-use-of-invalid-fd-when-setting-vhost-backend.patch
ApplyPatch vhost-vhost-net-defer-f-private_data-until-setup-succeeds.patch
ApplyPatch vhost-fix-TUN-m-VHOST_NET-y.patch
ApplyPatch kvm-emulate-accessed-bit-for-EPT.patch
ApplyPatch net-nf_conntrack-fix-memory-corruption.patch
ApplyPatch nfs-nfs4-handle-EKEYEXPIRED-errors-from-RPC-layer.patch
ApplyPatch nfs-sunrpc-parse-and-return-errors-reported-by-gssd.patch
ApplyPatch nfs-handle-NFSv2-EKEYEXPIRED-returns-from-RPC-layer-appropriately.patch
ApplyPatch nfs-nfs-handle-NFSv3-EKEYEXPIRED-errors-as-we-would-EJUKEBOX.patch
ApplyPatch oprofile-Support-Nehalem-EX-CPU-in-Oprofile.patch
ApplyPatch mm-define-MADV_HUGEPAGE.patch
ApplyPatch mm-add-a-compound_lock.patch
ApplyPatch mm-alter-compound-get_page-put_page.patch
ApplyPatch mm-update-futex-compound-knowledge.patch
ApplyPatch mm-clear-compound-mapping.patch
ApplyPatch mm-add-native_set_pmd_at.patch
ApplyPatch mm-add-pmd-paravirt-ops.patch
ApplyPatch mm-no-paravirt-version-of-pmd-ops.patch
ApplyPatch mm-export-maybe_mkwrite.patch
ApplyPatch mm-comment-reminder-in-destroy_compound_page.patch
ApplyPatch mm-config_transparent_hugepage.patch
ApplyPatch mm-special-pmd_trans_-functions.patch
ApplyPatch mm-add-pmd-mangling-generic-functions.patch
ApplyPatch mm-add-pmd-mangling-functions-to-x86.patch
ApplyPatch mm-bail-out-gup_fast-on-splitting-pmd.patch
ApplyPatch mm-pte-alloc-trans-splitting.patch
ApplyPatch mm-add-pmd-mmu_notifier-helpers.patch
ApplyPatch mm-clear-page-compound.patch
ApplyPatch mm-add-pmd_huge_pte-to-mm_struct.patch
ApplyPatch mm-split_huge_page_mm-vma.patch
ApplyPatch mm-split_huge_page-paging.patch
ApplyPatch mm-clear_huge_page-fix.patch
ApplyPatch mm-clear_copy_huge_page.patch
ApplyPatch mm-kvm-mmu-transparent-hugepage-support.patch
ApplyPatch mm-backport-page_referenced-microoptimization.patch
ApplyPatch mm-introduce-_GFP_NO_KSWAPD.patch
ApplyPatch mm-dont-alloc-harder-for-gfp-nomemalloc-even-if-nowait.patch
ApplyPatch mm-transparent-hugepage-core.patch
ApplyPatch mm-verify-pmd_trans_huge-isnt-leaking.patch
ApplyPatch mm-madvise-MADV_HUGEPAGE.patch
ApplyPatch mm-pmd_trans_huge-migrate-bugcheck.patch
ApplyPatch mm-memcg-compound.patch
ApplyPatch mm-memcg-huge-memory.patch
ApplyPatch mm-transparent-hugepage-vmstat.patch
ApplyPatch mm-introduce-khugepaged.patch
ApplyPatch mm-hugepage-redhat-customization.patch
ApplyPatch mm-remove-madvise-MADV_HUGEPAGE.patch
ApplyPatch uv-PCI-update-pci_set_vga_state-to-call-arch-functions.patch
ApplyPatch pci-update-pci_set_vga_state-to-call-arch-functions.patch
ApplyPatch uv-x86_64-update-uv-arch-to-target-legacy-VGA-I-O-correctly.patch
ApplyPatch gpu-vgaarb-fix-vga-arbiter-to-accept-PCI-domains-other-than-0.patch
ApplyPatch uv-vgaarb-add-user-selectability-of-the-number-of-gpus-in-a-system.patch
ApplyPatch s390x-ctcm-lcs-claw-remove-cu3088-layer.patch
ApplyPatch uv-x86-Fix-RTC-latency-bug-by-reading-replicated-cachelines.patch
ApplyPatch pci-PCI-ACS-support-functions.patch
ApplyPatch pci-Enablement-of-PCI-ACS-control-when-IOMMU-enabled-on-system.patch
ApplyPatch net-do-not-check-CAP_NET_RAW-for-kernel-created-sockets.patch
ApplyPatch fs-inotify-fix-inotify-WARN-and-compatibility-issues.patch
ApplyPatch redhat-kABI-updates-02-11.patch
ApplyPatch mm-fix-BUG-s-caused-by-the-transparent-hugepage-patch.patch
ApplyPatch redhat-Allow-only-particular-kernel-rpms-to-be-built.patch
ApplyPatch nfs-Fix-a-bug-in-nfs_fscache_release_page.patch
ApplyPatch nfs-Remove-a-redundant-check-for-PageFsCache-in-nfs_migrate_page.patch
ApplyPatch redhat-tagging-2-6-32-16-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-16.patch
ApplyPatch redhat-find-provides-also-require-python.patch
ApplyPatch ppc-Add-kdump-support-to-Collaborative-Memory-Manager.patch
ApplyPatch gfs-GFS2-problems-on-single-node-cluster.patch
ApplyPatch selinux-print-the-module-name-when-SELinux-denies-a-userspace-upcall.patch
ApplyPatch kernel-Prevent-futex-user-corruption-to-crash-the-kernel.patch
ApplyPatch kvm-PIT-control-word-is-write-only.patch
ApplyPatch virt-virtio_blk-add-block-topology-support.patch
ApplyPatch redhat-make-CONFIG_CRASH-built-in.patch
ApplyPatch mm-anon_vma-linking-changes-to-improve-multi-process-scalability.patch
ApplyPatch mm-anon_vma-locking-updates-for-transparent-hugepage-code.patch
ApplyPatch dm-stripe-avoid-divide-by-zero-with-invalid-stripe-count.patch
ApplyPatch dm-log-userspace-fix-overhead_size-calcuations.patch
ApplyPatch dm-mpath-fix-stall-when-requeueing-io.patch
ApplyPatch dm-raid1-fail-writes-if-errors-are-not-handled-and-log-fails.patch
ApplyPatch kernel-time-Implement-logarithmic-time-accumalation.patch
ApplyPatch kernel-time-Remove-xtime_cache.patch
ApplyPatch watchdog-Add-support-for-iTCO-watchdog-on-Ibex-Peak-chipset.patch
ApplyPatch block-fix-bio_add_page-for-non-trivial-merge_bvec_fn-case.patch
ApplyPatch block-freeze_bdev-don-t-deactivate-successfully-frozen-MS_RDONLY-sb.patch
ApplyPatch x86-nmi_watchdog-enable-by-default-on-RHEL-6.patch
ApplyPatch x86-x86-32-clean-up-rwsem-inline-asm-statements.patch
ApplyPatch x86-clean-up-rwsem-type-system.patch
ApplyPatch x86-x86-64-support-native-xadd-rwsem-implementation.patch
ApplyPatch x86-x86-64-rwsem-64-bit-xadd-rwsem-implementation.patch
ApplyPatch x86-x86-64-rwsem-Avoid-store-forwarding-hazard-in-__downgrade_write.patch
ApplyPatch nfs-mount-nfs-Unknown-error-526.patch
ApplyPatch s390-zfcp-cancel-all-pending-work-for-a-to-be-removed-zfcp_port.patch
ApplyPatch s390-zfcp-report-BSG-errors-in-correct-field.patch
ApplyPatch s390-qdio-prevent-kernel-bug-message-in-interrupt-handler.patch
ApplyPatch s390-qdio-continue-polling-for-buffer-state-ERROR.patch
ApplyPatch s390-hvc_iucv-allocate-IUCV-send-receive-buffers-in-DMA-zone.patch
ApplyPatch redhat-whitelists-intentional-kABI-Module-kabi-rebase-pre-beta1-for-VM-bits.patch
ApplyPatch redhat-tagging-2-6-32-17-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-17.patch
ApplyPatch redhat-disable-GFS2-on-s390x.patch
ApplyPatch redhat-disable-XFS-on-all-arches-except-x86_64.patch
ApplyPatch redhat-disable-all-the-filesystems-we-aren-t-going-to-support-and-fix-ext3-4.patch
ApplyPatch redhat-run-new-kernel-pkg-on-posttrans.patch
ApplyPatch redhat-fix-typo-on-redhat-Makefile.patch
ApplyPatch kvm-virtio-console-Allow-sending-variable-sized-buffers-to-host-efault-on-copy_from_user-err.patch
ApplyPatch kvm-virtio-console-return-efault-for-fill_readbuf-if-copy_to_user-fails.patch
ApplyPatch kvm-virtio-console-outbufs-are-no-longer-needed.patch
ApplyPatch kvm-virtio-Initialize-vq-data-entries-to-NULL.patch
ApplyPatch kvm-virtio-console-update-Red-Hat-copyright-for-2010.patch
ApplyPatch kvm-virtio-console-Ensure-no-memleaks-in-case-of-unused-buffers.patch
ApplyPatch kvm-virtio-console-Add-ability-to-remove-module.patch
ApplyPatch kvm-virtio-console-Error-out-if-we-can-t-allocate-buffers-for-control-queue.patch
ApplyPatch kvm-virtio-console-Fill-ports-entire-in_vq-with-buffers.patch
ApplyPatch kvm-Add-MAINTAINERS-entry-for-virtio_console.patch
ApplyPatch kvm-fix-large-packet-drops-on-kvm-hosts-with-ipv6.patch
ApplyPatch scsi-megaraid_sas-fix-for-32bit-apps.patch
ApplyPatch x86-AES-PCLMUL-Instruction-support-Add-PCLMULQDQ-accelerated-implementation.patch
ApplyPatch x86-AES-PCLMUL-Instruction-support-Various-small-fixes-for-AES-PCMLMUL-and-generate-byte-code-for-some-new-instructions-via-gas-macro.patch
ApplyPatch x86-AES-PCLMUL-Instruction-support-Use-gas-macro-for-AES-NI-instructions.patch
ApplyPatch x86-AES-PCLMUL-Instruction-support-Various-fixes-for-AES-NI-and-PCLMMUL.patch
ApplyPatch kvm-x86-emulator-Add-push-pop-sreg-instructions.patch
ApplyPatch kvm-x86-emulator-Introduce-No64-decode-option.patch
ApplyPatch kvm-x86-emulator-Add-group8-instruction-decoding.patch
ApplyPatch kvm-x86-emulator-Add-group9-instruction-decoding.patch
ApplyPatch kvm-x86-emulator-Add-Virtual-8086-mode-of-emulation.patch
ApplyPatch kvm-x86-emulator-fix-memory-access-during-x86-emulation.patch
ApplyPatch kvm-x86-emulator-Check-IOPL-level-during-io-instruction-emulation.patch
ApplyPatch kvm-x86-emulator-Fix-popf-emulation.patch
ApplyPatch kvm-x86-emulator-Check-CPL-level-during-privilege-instruction-emulation.patch
ApplyPatch kvm-x86-emulator-Add-LOCK-prefix-validity-checking.patch
ApplyPatch kvm-x86-emulator-code-style-cleanup.patch
ApplyPatch kvm-x86-emulator-Fix-properties-of-instructions-in-group-1_82.patch
ApplyPatch kvm-inject-UD-in-64bit-mode-from-instruction-that-are-not-valid-there.patch
ApplyPatch kvm-x86-emulator-X86EMUL-macro-replacements-from-do_fetch_insn_byte-to-x86_decode_insn.patch
ApplyPatch kvm-x86-emulator-X86EMUL-macro-replacements-x86_emulate_insn-and-its-helpers.patch
ApplyPatch kvm-x86-emulator-Fix-x86_emulate_insn-not-to-use-the-variable-rc-for-non-X86EMUL-values.patch
ApplyPatch kvm-x86-emulator-Forbid-modifying-CS-segment-register-by-mov-instruction.patch
ApplyPatch kvm-Fix-load_guest_segment_descriptor-to-inject-page-fault.patch
ApplyPatch kvm-Fix-segment-descriptor-loading.patch
ApplyPatch kvm-Fix-emulate_sys-call-enter-exit-s-fault-handling.patch
ApplyPatch netdrv-ixgbe-prevent-speculative-processing-of-descriptors.patch
ApplyPatch x86-nmi_watchdog-use-__cpuinit-for-nmi_watchdog_default.patch
ApplyPatch net-netfilter-nf_conntrack-per-netns-nf_conntrack_cachep.patch
ApplyPatch scsi-mpt2sas-fix-missing-initialization.patch
ApplyPatch dm-raid45-target-constructor-error-path-oops-fix.patch
ApplyPatch netdrv-cxgb3-add-memory-barriers.patch
ApplyPatch mm-fix-anon_vma-locking-updates-for-transparent-hugepage-code.patch
ApplyPatch mm-Fix-hugetlb-c-clear_huge_page-parameter.patch
ApplyPatch gfs2-print-glock-numbers-in-hex.patch
ApplyPatch selinux-netlabel-fix-corruption-of-SELinux-MLS-categories-127.patch
ApplyPatch ata-ahci-disable-FPDMA-auto-activate-optimization-on-NVIDIA-AHCI.patch
ApplyPatch scsi-lpfc-Update-from-8-3-5-4-to-8-3-5-5-FC-FCoE.patch
ApplyPatch scsi-pmcraid-bug-fixes-from-upstream.patch
ApplyPatch mm-Fix-potential-crash-with-sys_move_pages.patch
ApplyPatch s390x-qeth-avoid-recovery-during-device-online-setting.patch
ApplyPatch kernel-Fix-SMT-scheduler-regression-in-find_busiest_queue.patch
ApplyPatch drm-bring-drm-core-ttm-fb-layer-fixes-in-from-upstream.patch
ApplyPatch s390x-vdso-glibc-does-not-use-vdso-functions.patch
ApplyPatch kernel-sched-Fix-SCHED_MC-regression-caused-by-change-in-sched-cpu_power.patch
ApplyPatch dvb-Fix-endless-loop-when-decoding-ULE-at-dvb-core.patch
ApplyPatch drm-radeon-kms-bring-all-v2-6-33-fixes-into-EL6-kernel.patch
ApplyPatch redhat-Change-RPM-name-format-for-custom-builds.patch
ApplyPatch uv-Have-mmu_notifiers-use-SRCU-so-they-may-safely-schedule.patch
ApplyPatch uv-Fix-unmap_vma-bug-related-to-mmu_notifiers.patch
ApplyPatch net-bug-fix-for-vlan-gro-issue.patch
ApplyPatch redhat-add-suport-for-rhts-testing.patch
ApplyPatch redhat-add-filesystem-test.patch
ApplyPatch vhost-vhost-net-switch-to-smp-barriers.patch
ApplyPatch vhost-logging-thinko-fix.patch
ApplyPatch vhost-initialize-log-eventfd-context-pointer.patch
ApplyPatch vhost-fix-get_user_pages_fast-error-handling.patch
ApplyPatch vhost-vhost-net-restart-tx-poll-on-sk_sndbuf-full.patch
ApplyPatch x86-Intel-Cougar-Point-chipset-support.patch
ApplyPatch drm-Remove-loop-in-IronLake-graphics-interrupt-handler.patch
ApplyPatch scsi-scsi_dh_emc-fix-mode-select-setup.patch
ApplyPatch scsi-Add-netapp-to-scsi-dh-alua-dev-list.patch
ApplyPatch virt-hvc_console-Fix-race-between-hvc_close-and-hvc_remove.patch
ApplyPatch kernel-time-revert-cc2f92ad1d0e03fe527e8ccfc1f918c368964dc8.patch
ApplyPatch redhat-infiniband-remove-all-infiniband-symbols-from-RHEL6-kABI.patch
ApplyPatch redhat-kABI-Update-the-RHEL6-kABI-for-kernel-2-6-32-18-el6.patch
ApplyPatch redhat-tagging-2-6-32-18-el6.patch
ApplyPatch redhat-updating-lastcommit-for-2-6-32-18.patch
ApplyPatch mm-Switch-to-SLAB.patch
ApplyPatch redhat-kABI-Update-the-kABI-for-2-6-18-el6.patch

ApplyPatch patch-2.6.32-19.el6-vs2.3.0.36.29.4.diff
ApplyPatch linux-2.6-250-ipsets.patch
ApplyPatch linux-2.6-510-ipod.patch
ApplyPatch linux-2.6-521-packet-tagging.patch
#
#ApplyPatch linux-2.6-522-iptables-connection-tagging.patch
#
ApplyPatch linux-2.6-523-raw-sockets.patch
ApplyPatch linux-2.6-524-peercred.patch
ApplyPatch linux-2.6-525-sknid-elevator.patch
ApplyPatch linux-2.6-540-oom-kill.patch
ApplyPatch linux-2.6-530-built-by-support.patch

ApplyOptionalPatch linux-kernel-test.patch

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
  make ARCH=$Arch %{oldconfig_target} > /dev/null
  echo "# $Arch" > configs/$i
  cat .config >> configs/$i
  rm -f include/generated/kernel.arch
  rm -f include/generated/kernel.cross
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

    if [ "$Arch" == "s390" -a "$Flavour" == "kdump" ]; then
        pushd arch/s390/boot
        gcc -static -o zfcpdump zfcpdump.c
        popd
    fi
    make -s ARCH=$Arch %{oldconfig_target} > /dev/null
    make -s ARCH=$Arch V=1 %{?_smp_mflags} $MakeTarget %{?sparse_mflags}
    if [ "$Arch" != "s390" -o "$Flavour" != "kdump" ]; then
        make -s ARCH=$Arch V=1 %{?_smp_mflags} modules %{?sparse_mflags} || exit 1
    fi

%if %{with_perftool}
    pushd tools/perf
# make sure the scripts are executable... won't be in tarball until 2.6.31 :/
    chmod +x util/generate-cmdlist.sh util/PERF-VERSION-GEN
    make -s V=1 %{?_smp_mflags} NO_DEMANGLE=1 perf
    mkdir -p $RPM_BUILD_ROOT/usr/libexec/
    install -m 755 perf $RPM_BUILD_ROOT/usr/libexec/perf.$KernelVer
    popd
%endif

    # Start installing the results
%if %{with_debuginfo}
    mkdir -p $RPM_BUILD_ROOT%{debuginfodir}/boot
    mkdir -p $RPM_BUILD_ROOT%{debuginfodir}/%{image_install_path}
    install -m 644 System.map $RPM_BUILD_ROOT/%{debuginfodir}/boot/System.map-$KernelVer
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

%if %{with_fips}
    # hmac sign the kernel for FIPS
    echo "Creating hmac file: $RPM_BUILD_ROOT/%{image_install_path}/.vmlinuz-$KernelVer.hmac"
    ls -l $RPM_BUILD_ROOT/%{image_install_path}/$InstallName-$KernelVer
    sha512hmac $RPM_BUILD_ROOT/%{image_install_path}/$InstallName-$KernelVer | sed -e "s,$RPM_BUILD_ROOT,," > $RPM_BUILD_ROOT/%{image_install_path}/.vmlinuz-$KernelVer.hmac;
%endif

    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer
    # Override $(mod-fw) because we don't want it to install any firmware
    # We'll do that ourselves with 'make firmware_install'
    if [ "$Arch" != "s390" -o "$Flavour" != "kdump" ]; then
      make -s ARCH=$Arch INSTALL_MOD_PATH=$RPM_BUILD_ROOT modules_install KERNELRELEASE=$KernelVer mod-fw=

      # check if the modules are being signed
%if %{signmodules}
      if [ -z "$(readelf -n $(find fs/ -name \*.ko | head -n 1) | grep module.sig)" ]; then
          echo "ERROR: modules are NOT signed" >&2;
          exit 1;
      fi
%endif
    else
      mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/{kernel,extra}
    fi

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

    # create the kABI metadata for use in packaging
    echo "**** GENERATING kernel ABI metadata ****"
    gzip -c9 < Module.symvers > $RPM_BUILD_ROOT/boot/symvers-$KernelVer.gz
    chmod 0755 %_sourcedir/kabitool
    rm -f %{_tmppath}/kernel-$KernelVer-kabideps
    %_sourcedir/kabitool -s Module.symvers -o %{_tmppath}/kernel-$KernelVer-kabideps

%if %{with_kabichk}
    echo "**** kABI checking is enabled in kernel SPEC file. ****"
    chmod 0755 $RPM_SOURCE_DIR/check-kabi
    if [ -e $RPM_SOURCE_DIR/Module.kabi_%{_target_cpu}$Flavour ]; then
        cp $RPM_SOURCE_DIR/Module.kabi_%{_target_cpu}$Flavour $RPM_BUILD_ROOT/Module.kabi
        $RPM_SOURCE_DIR/check-kabi -k $RPM_BUILD_ROOT/Module.kabi -s Module.symvers || exit 1
        rm $RPM_BUILD_ROOT/Module.kabi # for now, don't keep it around.
    else
        echo "**** NOTE: Cannot find reference Module.kabi file. ****"
    fi
%endif
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
    			 'ata_scsi_ioctl|scsi_add_host|scsi_add_host_with_dma|blk_init_queue|register_mtd_blktrans|scsi_esp_register|scsi_register_device_handler'
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

%if %{signmodules}
cp %{SOURCE19} .
gpg --homedir . --batch --gen-key %{SOURCE11}
# if there're external keys to be included
if [ -s %{SOURCE19} ]; then
	gpg --homedir . --no-default-keyring --keyring kernel.pub --import %{SOURCE19}
fi
gpg --homedir . --export --keyring ./kernel.pub Red > extract.pub
gcc -o scripts/bin2c scripts/bin2c.c
scripts/bin2c ksign_def_public_key __initdata <extract.pub >crypto/signature/key.h
%endif

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
%ifarch s390x
BuildKernel %make_target %kernel_image kdump
%else
BuildKernel vmlinux vmlinux kdump vmlinux
%endif
%endif

%if %{with_framepointer}
BuildKernel %make_target %kernel_image framepointer
%endif

%if %{with_doc}
# Make the HTML and man pages.
make -j1 htmldocs mandocs || %{doc_build_fail}

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
# for some reason, on RHEL-5 RPM_BUILD_ROOT doesn't get set
if [ -z "$RPM_BUILD_ROOT" ]; then
	export RPM_BUILD_ROOT="%{buildroot}";
fi

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
%{expand:\
%if %{with_dracut}\
/sbin/new-kernel-pkg --package kernel%{?1:-%{1}} --mkinitrd --dracut --depmod --install %{KVERREL}%{?1:.%{1}} --kernel-args="crashkernel=auto" || exit $?\
%else\
/sbin/new-kernel-pkg --package kernel%{?1:-%{1}} --mkinitrd --depmod --install %{KVERREL}%{?1:.%{1}} --kernel-args="crashkernel=auto" || exit $?\
%endif}\
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
if [ -x /sbin/weak-modules ]\
then\
    /sbin/weak-modules --add-kernel %{KVERREL}%{?-v:.%{-v*}} || exit $?\
fi\
%{nil}

#
# This macro defines a %%preun script for a kernel package.
#	%%kernel_variant_preun <subpackage>
#
%define kernel_variant_preun() \
%{expand:%%preun %{?1}}\
/sbin/new-kernel-pkg --rminitrd --rmmoddep --remove %{KVERREL}%{?1:.%{1}} || exit $?\
if [ -x /sbin/weak-modules ]\
then\
    /sbin/weak-modules --remove-kernel %{KVERREL}%{?-v:.%{-v*}} || exit $?\
fi\
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

%if %{with_framepointer}
%kernel_variant_preun framepointer
%kernel_variant_post -v framepointer
%endif

%ifarch s390x
%postun kdump
    # Create softlink to latest remaining kdump kernel.
    # If no more kdump kernel is available, remove softlink.
    if [ "$(readlink /boot/zfcpdump)" == "/boot/vmlinuz-%{KVERREL}.kdump" ]
    then
        vmlinuz_next=$(ls /boot/vmlinuz-*.kdump 2> /dev/null | sort | tail -n1)
        if [ $vmlinuz_next ]
        then
            ln -sf $vmlinuz_next /boot/zfcpdump
        else
            rm -f /boot/zfcpdump
        fi
    fi

%post kdump
    ln -sf /boot/vmlinuz-%{KVERREL}.kdump /boot/zfcpdump
%endif

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
%if %{with_fips} \
/%{image_install_path}/.vmlinuz-%{KVERREL}%{?2:.%{2}}.hmac \
%endif \
/boot/System.map-%{KVERREL}%{?2:.%{2}}\
/boot/symvers-%{KVERREL}%{?2:.%{2}}.gz\
%if %{with_perftool}\
/usr/libexec/perf.%{KVERREL}%{?2:.%{2}}\
%endif\
#/boot/symvers-%{KVERREL}%{?2:.%{2}}.gz\
/boot/config-%{KVERREL}%{?2:.%{2}}\
%dir /lib/modules/%{KVERREL}%{?2:.%{2}}\
/lib/modules/%{KVERREL}%{?2:.%{2}}/kernel\
/lib/modules/%{KVERREL}%{?2:.%{2}}/extra\
/lib/modules/%{KVERREL}%{?2:.%{2}}/build\
/lib/modules/%{KVERREL}%{?2:.%{2}}/source\
/lib/modules/%{KVERREL}%{?2:.%{2}}/updates\
/lib/modules/%{KVERREL}%{?2:.%{2}}/weak-updates\
%ifarch %{vdso_arches}\
/lib/modules/%{KVERREL}%{?2:.%{2}}/vdso\
/etc/ld.so.conf.d/kernel-%{KVERREL}%{?2:.%{2}}.conf\
%endif\
/lib/modules/%{KVERREL}%{?2:.%{2}}/modules.*\
%if %{with_dracut}\
%ghost /boot/initramfs-%{KVERREL}%{?2:.%{2}}.img\
%else\
%ghost /boot/initrd-%{KVERREL}%{?2:.%{2}}.img\
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
%{debuginfodir}/boot/System.map-%{KVERREL}%{?2:.%{2}}\
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
%{debuginfodir}/boot/System.map-%{KVERREL}%{?2:.%{2}}\
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
%ifarch s390x
%kernel_variant_files %{with_kdump} kdump
%else
%kernel_variant_files -k vmlinux %{with_kdump} kdump
%endif
%kernel_variant_files %{with_framepointer} framepointer

%changelog
* Tue Mar 09 2010 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-19.el6]
- [mm] Switch to SLAB (Aristeu Rozanski) [570614]

* Mon Mar 08 2010 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-18.el6]
- [kernel/time] revert cc2f92ad1d0e03fe527e8ccfc1f918c368964dc8 (Aristeu Rozanski) [567551]
- [virt] hvc_console: Fix race between hvc_close and hvc_remove (Amit Shah) [568624]
- [scsi] Add netapp to scsi dh alua dev list (Mike Christie) [559586]
- [scsi] scsi_dh_emc: fix mode select setup (Mike Christie) [570685]
- [drm] Remove loop in IronLake graphics interrupt handler (John Villalovos) [557838]
- [x86] Intel Cougar Point chipset support (John Villalovos) [560077]
- [vhost] vhost-net: restart tx poll on sk_sndbuf full (Michael S. Tsirkin) [562837]
- [vhost] fix get_user_pages_fast error handling (Michael S. Tsirkin) [562837]
- [vhost] initialize log eventfd context pointer (Michael S. Tsirkin) [562837]
- [vhost] logging thinko fix (Michael S. Tsirkin) [562837]
- [vhost] vhost-net: switch to smp barriers (Michael S. Tsirkin) [562837]
- [net] bug fix for vlan + gro issue (Andy Gospodarek) [569922]
- [uv] Fix unmap_vma() bug related to mmu_notifiers (George Beshers) [253033]
- [uv] Have mmu_notifiers use SRCU so they may safely schedule (George Beshers) [253033]
- [drm] radeon/kms: bring all v2.6.33 fixes into EL6 kernel (Dave Airlie) [547422 554323 566618 569704]
- [dvb] Fix endless loop when decoding ULE at dvb-core (Mauro Carvalho Chehab) [569243]
- [kernel] sched: Fix SCHED_MC regression caused by change in sched cpu_power (Danny Feng) [568123]
- [s390x] vdso: glibc does not use vdso functions (Hendrik Brueckner) [567755]
- [drm] bring drm core/ttm/fb layer fixes in from upstream (Dave Airlie) [569701]
- [kernel] Fix SMT scheduler regression in find_busiest_queue() (Danny Feng) [568120]
- [s390x] qeth: avoid recovery during device online setting (Hendrik Brueckner) [568781]
- [mm] Fix potential crash with sys_move_pages (Danny Feng) [562591] {CVE-2010-0415}
- [scsi] pmcraid: bug fixes from upstream (Rob Evers) [567376]
- [scsi] lpfc Update from 8.3.5.4 to 8.3.5.5 FC/FCoE (Rob Evers) [564508]
- [ata] ahci: disable FPDMA auto-activate optimization on NVIDIA AHCI (David Milburn) [568815]
- [selinux] netlabel: fix corruption of SELinux MLS categories > 127 (Eric Paris) [568370]
- [gfs2] print glock numbers in hex (Robert S Peterson) [566755]
- [mm] Fix hugetlb.c clear_huge_page parameter (Andrea Arcangeli) [566604]
- [mm] fix anon_vma locking updates for transparent hugepage code (Andrea Arcangeli) [564515]
- [netdrv] cxgb3: add memory barriers (Steve Best) [568390]
- [dm] raid45 target: constructor error path oops fix (Heinz Mauelshagen) [567605]
- [scsi] mpt2sas: fix missing initialization (Tomas Henzl) [567965]
- [net] netfilter: nf_conntrack: per netns nf_conntrack_cachep (Jiri Pirko) [567181]
- [x86] nmi_watchdog: use __cpuinit for nmi_watchdog_default (Don Zickus) [567601]
- [netdrv] ixgbe: prevent speculative processing of descriptors (Steve Best) [568391]
- [kvm] Fix emulate_sys[call, enter, exit]()'s fault handling (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] Fix segment descriptor loading (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] Fix load_guest_segment_descriptor() to inject page fault (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: Forbid modifying CS segment register by mov instruction (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: Fix x86_emulate_insn() not to use the variable rc for non-X86EMUL values (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: X86EMUL macro replacements: x86_emulate_insn() and its helpers (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: X86EMUL macro replacements: from do_fetch_insn_byte() to x86_decode_insn() (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] inject #UD in 64bit mode from instruction that are not valid there (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: Fix properties of instructions in group 1_82 (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: code style cleanup (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: Add LOCK prefix validity checking (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: Check CPL level during privilege instruction emulation (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: Fix popf emulation (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: Check IOPL level during io instruction emulation (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: fix memory access during x86 emulation (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: Add Virtual-8086 mode of emulation (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: Add group9 instruction decoding (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: Add group8 instruction decoding (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: Introduce No64 decode option (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [kvm] x86 emulator: Add 'push/pop sreg' instructions (Gleb Natapov) [560903 560904 563466] {CVE-2010-0298 CVE-2010-0306 CVE-2010-0419}
- [x86] AES/PCLMUL Instruction support: Various fixes for AES-NI and PCLMMUL (John Villalovos) [463496]
- [x86] AES/PCLMUL Instruction support: Use gas macro for AES-NI instructions (John Villalovos) [463496]
- [x86] AES/PCLMUL Instruction support: Various small fixes for AES/PCMLMUL and generate .byte code for some new instructions via gas macro (John Villalovos) [463496]
- [x86] AES/PCLMUL Instruction support: Add PCLMULQDQ accelerated implementation (John Villalovos) [463496]
- [scsi] megaraid_sas: fix for 32bit apps (Tomas Henzl) [559941]
- [kvm] fix large packet drops on kvm hosts with ipv6 (Neil Horman) [565525]
- [kvm] Add MAINTAINERS entry for virtio_console (Amit Shah) [566391]
- [kvm] virtio: console: Fill ports' entire in_vq with buffers (Amit Shah) [566391]
- [kvm] virtio: console: Error out if we can't allocate buffers for control queue (Amit Shah) [566391]
- [kvm] virtio: console: Add ability to remove module (Amit Shah) [566391]
- [kvm] virtio: console: Ensure no memleaks in case of unused buffers (Amit Shah) [566391]
- [kvm] virtio: console: update Red Hat copyright for 2010 (Amit Shah) [566391]
- [kvm] virtio: Initialize vq->data entries to NULL (Amit Shah) [566391]
- [kvm] virtio: console: outbufs are no longer needed (Amit Shah) [566391]
- [kvm] virtio: console: return -efault for fill_readbuf if copy_to_user fails (Amit Shah) [566391]
- [kvm] virtio: console: Allow sending variable-sized buffers to host, efault on copy_from_user err (Amit Shah) [566391]

* Thu Feb 18 2010 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-17.el6]
- [s390] hvc_iucv: allocate IUCV send/receive buffers in DMA zone (Hendrik Brueckner) [566188]
- [s390] qdio: continue polling for buffer state ERROR (Hendrik Brueckner) [565528]
- [s390] qdio: prevent kernel bug message in interrupt handler (Hendrik Brueckner) [565542]
- [s390] zfcp: report BSG errors in correct field (Hendrik Brueckner) [564378]
- [s390] zfcp: cancel all pending work for a to be removed zfcp_port (Hendrik Brueckner) [564382]
- [nfs] mount.nfs: Unknown error 526 (Steve Dickson) [561975]
- [x86] x86-64, rwsem: Avoid store forwarding hazard in __downgrade_write (Avi Kivity) [563801]
- [x86] x86-64, rwsem: 64-bit xadd rwsem implementation (Avi Kivity) [563801]
- [x86] x86-64: support native xadd rwsem implementation (Avi Kivity) [563801]
- [x86] clean up rwsem type system (Avi Kivity) [563801]
- [x86] x86-32: clean up rwsem inline asm statements (Avi Kivity) [563801]
- [x86] nmi_watchdog: enable by default on RHEL-6 (Don Zickus) [523857]
- [block] freeze_bdev: don't deactivate successfully frozen MS_RDONLY sb (Mike Snitzer) [565890]
- [block] fix bio_add_page for non trivial merge_bvec_fn case (Mike Snitzer) [565890]
- [watchdog] Add support for iTCO watchdog on Ibex Peak chipset (John Villalovos) [536698]
- [kernel] time: Remove xtime_cache (Prarit Bhargava) [563135]
- [kernel] time: Implement logarithmic time accumalation (Prarit Bhargava) [563135]
- [dm] raid1: fail writes if errors are not handled and log fails (Mike Snitzer) [565890]
- [dm] mpath: fix stall when requeueing io (Mike Snitzer) [565890]
- [dm] log: userspace fix overhead_size calcuations (Mike Snitzer) [565890]
- [dm] stripe: avoid divide by zero with invalid stripe count (Mike Snitzer) [565890]
- [mm] anon_vma locking updates for transparent hugepage code (Rik van Riel) [564515]
- [mm] anon_vma linking changes to improve multi-process scalability (Rik van Riel) [564515]
- [virt] virtio_blk: add block topology support (Christoph Hellwig) [556477]
- [kvm] PIT: control word is write-only (Eduardo Habkost) [560905] {CVE-2010-0309}
- [kernel] Prevent futex user corruption to crash the kernel (Jerome Marchand) [563957]
- [selinux] print the module name when SELinux denies a userspace upcall (Eric Paris) [563731]
- [gfs] GFS2 problems on single node cluster (Steven Whitehouse) [564329]
- [ppc] Add kdump support to Collaborative Memory Manager (Steve Best) [563316]

* Fri Feb 12 2010 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-16.el6]
- [nfs] Remove a redundant check for PageFsCache in nfs_migrate_page() (Steve Dickson) [563938]
- [nfs] Fix a bug in nfs_fscache_release_page() (Steve Dickson) [563938]
- [mm] fix BUG()s caused by the transparent hugepage patch (Larry Woodman) [556572]
- [fs] inotify: fix inotify WARN and compatibility issues (Eric Paris) [563363]
- [net] do not check CAP_NET_RAW for kernel created sockets (Eric Paris) [540560]
- [pci] Enablement of PCI ACS control when IOMMU enabled on system (Don Dutile) [523278]
- [pci] PCI ACS support functions (Don Dutile) [523278]
- [uv] x86: Fix RTC latency bug by reading replicated cachelines (George Beshers) [562189]
- [s390x] ctcm / lcs / claw: remove cu3088 layer (Hendrik Brueckner) [557522]
- [uv] vgaarb: add user selectability of the number of gpus in a system (George Beshers) [555879]
- [gpu] vgaarb: fix vga arbiter to accept PCI domains other than 0 (George Beshers) [555879]
- [uv] x86_64: update uv arch to target legacy VGA I/O correctly (George Beshers) [555879]
- [pci] update pci_set_vga_state to call arch functions (George Beshers) [555879]
- [uv] PCI: update pci_set_vga_state to call arch functions (George Beshers) [555879]
- [mm] remove madvise(MADV_HUGEPAGE) (Andrea Arcangeli) [556572]
- [mm] hugepage redhat customization (Andrea Arcangeli) [556572]
- [mm] introduce khugepaged (Andrea Arcangeli) [556572]
- [mm] transparent hugepage vmstat (Andrea Arcangeli) [556572]
- [mm] memcg huge memory (Andrea Arcangeli) [556572]
- [mm] memcg compound (Andrea Arcangeli) [556572]
- [mm] pmd_trans_huge migrate bugcheck (Andrea Arcangeli) [556572]
- [mm] madvise(MADV_HUGEPAGE) (Andrea Arcangeli) [556572]
- [mm] verify pmd_trans_huge isnt leaking (Andrea Arcangeli) [556572]
- [mm] transparent hugepage core (Andrea Arcangeli) [556572]
- [mm] dont alloc harder for gfp nomemalloc even if nowait (Andrea Arcangeli) [556572]
- [mm] introduce _GFP_NO_KSWAPD (Andrea Arcangeli) [556572]
- [mm] backport page_referenced microoptimization (Andrea Arcangeli) [556572]
- [mm] kvm mmu transparent hugepage support (Andrea Arcangeli) [556572]
- [mm] clear_copy_huge_page (Andrea Arcangeli) [556572]
- [mm] clear_huge_page fix (Andrea Arcangeli) [556572]
- [mm] split_huge_page paging (Andrea Arcangeli) [556572]
- [mm] split_huge_page_mm/vma (Andrea Arcangeli) [556572]
- [mm] add pmd_huge_pte to mm_struct (Andrea Arcangeli) [556572]
- [mm] clear page compound (Andrea Arcangeli) [556572]
- [mm] add pmd mmu_notifier helpers (Andrea Arcangeli) [556572]
- [mm] pte alloc trans splitting (Andrea Arcangeli) [556572]
- [mm] bail out gup_fast on splitting pmd (Andrea Arcangeli) [556572]
- [mm] add pmd mangling functions to x86 (Andrea Arcangeli) [556572]
- [mm] add pmd mangling generic functions (Andrea Arcangeli) [556572]
- [mm] special pmd_trans_* functions (Andrea Arcangeli) [556572]
- [mm] config_transparent_hugepage (Andrea Arcangeli) [556572]
- [mm] comment reminder in destroy_compound_page (Andrea Arcangeli) [556572]
- [mm] export maybe_mkwrite (Andrea Arcangeli) [556572]
- [mm] no paravirt version of pmd ops (Andrea Arcangeli) [556572]
- [mm] add pmd paravirt ops (Andrea Arcangeli) [556572]
- [mm] add native_set_pmd_at (Andrea Arcangeli) [556572]
- [mm] clear compound mapping (Andrea Arcangeli) [556572]
- [mm] update futex compound knowledge (Andrea Arcangeli) [556572]
- [mm] alter compound get_page/put_page (Andrea Arcangeli) [556572]
- [mm] add a compound_lock (Andrea Arcangeli) [556572]
- [mm] define MADV_HUGEPAGE (Andrea Arcangeli) [556572]
- [oprofile] Support Nehalem-EX CPU in Oprofile (John Villalovos) [528998]
- [nfs] nfs: handle NFSv3 -EKEYEXPIRED errors as we would -EJUKEBOX (Jeff Layton) [479359]
- [nfs] handle NFSv2 -EKEYEXPIRED returns from RPC layer appropriately (Jeff Layton) [479359]
- [nfs] sunrpc: parse and return errors reported by gssd (Jeff Layton) [479359]
- [nfs] nfs4: handle -EKEYEXPIRED errors from RPC layer (Jeff Layton) [479359]
- [net] nf_conntrack: fix memory corruption (Jon Masters) [559471]
- [kvm] emulate accessed bit for EPT (Rik van Riel) [555106]
- [vhost] fix TUN=m VHOST_NET=y (Michael S. Tsirkin) [562837]
- [vhost] vhost-net: defer f->private_data until setup succeeds (Chris Wright) [562837]
- [vhost] vhost-net: comment use of invalid fd when setting vhost backend (Chris Wright) [562837]
- [vhost] access check thinko fixes (Michael S. Tsirkin) [562837]
- [vhost] make default mapping empty by default (Michael S. Tsirkin) [562837]
- [vhost] add access_ok checks (Michael S. Tsirkin) [562837]
- [vhost] prevent modification of an active ring (Michael S. Tsirkin) [562837]
- [vhost] fix high 32 bit in FEATURES ioctls (Michael S. Tsirkin) [562837]
- [dm] dm-raid1: fix deadlock at suspending failed device (Takahiro Yasui) [557932]
- [dm] fix kernel panic at releasing bio on recovery failed region (Takahiro Yasui) [557934]
- [scsi] lpfc Update from 8.3.4 to 8.3.5.4 FC/FCoE (Rob Evers) [531028]
- [nfs] sunrpc/cache: fix module refcnt leak in a failure path (Steve Dickson) [562285]
- [nfs] Ensure that we handle NFS4ERR_STALE_STATEID correctly (Steve Dickson) [560784]
- [nfs] NFSv4.1: Don't call nfs4_schedule_state_recovery() unnecessarily (Steve Dickson) [560784]
- [nfs] NFSv4: Don't allow posix locking against servers that don't support it (Steve Dickson) [560784]
- [nfs] Ensure that the NFSv4 locking can recover from stateid errors (Steve Dickson) [560784]
- [nfs] Avoid warnings when CONFIG_NFS_V4=n (Steve Dickson) [560784]
- [nfs] Make nfs_commitdata_release static (Steve Dickson) [560784]
- [nfs] Try to commit unstable writes in nfs_release_page() (Steve Dickson) [560784]
- [nfs] Fix a reference leak in nfs_wb_cancel_page() (Steve Dickson) [560784]
- [nfs] nfs41: cleanup callback code to use __be32 type (Steve Dickson) [560785]
- [nfs] nfs41: clear NFS4CLNT_RECALL_SLOT bit on session reset (Steve Dickson) [560785]
- [nfs] nfs41: fix nfs4_callback_recallslot (Steve Dickson) [560785]
- [nfs] nfs41: resize slot table in reset (Steve Dickson) [560785]
- [nfs] nfs41: implement cb_recall_slot (Steve Dickson) [560785]
- [nfs] nfs41: back channel drc minimal implementation (Steve Dickson) [560785]
- [nfs] nfs41: prepare for back channel drc (Steve Dickson) [560785]
- [nfs] nfs41: remove uneeded checks in callback processing (Steve Dickson) [560785]
- [nfs] nfs41: directly encode back channel error (Steve Dickson) [560785]
- [nfs] nfs41: fix wrong error on callback header xdr overflow (Steve Dickson) [560785]
- [nfs] nfs41: Process callback's referring call list (Steve Dickson) [560785]
- [nfs] nfs41: Check slot table for referring calls (Steve Dickson) [560785]
- [nfs] nfs41: Adjust max cache response size value (Steve Dickson) [560785]
- [nfs] NFSD: Create PF_INET6 listener in write_ports (Steve Dickson) [560785]
- [nfs] SUNRPC: NFS kernel APIs shouldn't return ENOENT for "transport not found" (Steve Dickson) [560785]
- [nfs] SUNRPC: Bury "#ifdef IPV6" in svc_create_xprt() (Steve Dickson) [560785]
- [nfs] NFSD: Support AF_INET6 in svc_addsock() function (Steve Dickson) [560785]
- [nfs] SUNRPC: Use rpc_pton() in ip_map_parse() (Steve Dickson) [560785]
- [nfs] nfsd: 4.1 has an rfc number (Steve Dickson) [560785]
- [nfs] nfsd41: Create the recovery entry for the NFSv4.1 client (Steve Dickson) [560785]
- [nfs] nfsd: use vfs_fsync for non-directories (Steve Dickson) [560785]
- [nfs] nfsd4: Use FIRST_NFS4_OP in nfsd4_decode_compound() (Steve Dickson) [560785]
- [nfs] nfsd41: nfsd4_decode_compound() does not recognize all ops (Steve Dickson) [560785]

* Fri Feb 05 2010 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-15.el6]
- [block] blk-cgroup: Fix lockdep warning of potential deadlock in blk-cgroup (Vivek Goyal) [561903]
- [block] cfq: Do not idle on async queues and drive deeper WRITE depths (Vivek Goyal) [561902]
- [quota] 64-bit quota format fixes (Jerome Marchand) [546311]
- [x86] fix Add AMD Node ID MSR support (Bhavna Sarathy) [557540]
- [fs] ext4: fix type of "offset" in ext4_io_end (Eric Sandeen) [560097]
- [x86] Disable HPET MSI on ATI SB700/SB800 (Prarit Bhargava) [557332]
- [x86] arch specific support for remapping HPET MSIs (Prarit Bhargava) [557332]
- [x86] intr-remap: generic support for remapping HPET MSIs (Prarit Bhargava) [557332]
- [gfs] GFS2: Extend umount wait coverage to full glock lifetime (Steven Whitehouse) [561287]
- [gfs] GFS2: Wait for unlock completion on umount (Steven Whitehouse) [561287]
- [gfs] GFS2: Use MAX_LFS_FILESIZE for meta inode size (Steven Whitehouse) [561307]
- [gfs] GFS2: Use GFP_NOFS for alloc structure (Steven Whitehouse) [561307]
- [gfs] GFS2: Fix refcnt leak on gfs2_follow_link() error path (Steven Whitehouse) [561307]

* Wed Feb 03 2010 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-14.el6]
- [s390x] dasd: fix online/offline race (Hendrik Brueckner) [552840]
- [netdrv] update tg3 to version 3.106 and fix panic (John Feeney) [555101]
- [s390x] dasd: Fix null pointer in s390dbf and discipline checking (Hendrik Brueckner) [559615]
- [s390x] zcrypt: Do not remove coprocessor in case of error 8/72 (Hendrik Brueckner) [559613]
- [s390x] cio: channel path vary operation has no effect (Hendrik Brueckner) [559612]
- [uv] x86: Ensure hub revision set for all ACPI modes (George Beshers) [559752]
- [uv] x86: Add function retrieving node controller revision number (George Beshers) [559752]

* Fri Jan 29 2010 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-13.el6]
- [virtio] console: show error message if hvc_alloc fails for console ports (Amit Shah) [543824]
- [virtio] console: Add debugfs files for each port to expose debug info (Amit Shah) [543824]
- [virtio] console: Add ability to hot-unplug ports (Amit Shah) [543824]
- [virtio] hvc_console: Export (GPL'ed) hvc_remove (Amit Shah) [543824]
- [virtio] Add ability to detach unused buffers from vrings (Amit Shah) [543824]
- [virtio] console: Handle port hot-plug (Amit Shah) [543824]
- [virtio] console: Remove cached data on port close (Amit Shah) [543824]
- [virtio] console: Register with sysfs and create a 'name' attribute for ports (Amit Shah) [543824]
- [virtio] console: Ensure only one process can have a port open at a time (Amit Shah) [543824]
- [virtio] console: Add file operations to ports for open/read/write/poll (Amit Shah) [543824]
- [virtio] console: Associate each port with a char device (Amit Shah) [543824]
- [virtio] console: Prepare for writing to / reading from userspace buffers (Amit Shah) [543824]
- [virtio] console: Add a new MULTIPORT feature, support for generic ports (Amit Shah) [543824]
- [virtio] console: Introduce a send_buf function for a common path for sending data to host (Amit Shah) [543824]
- [virtio] console: Introduce function to hand off data from host to readers (Amit Shah) [543824]
- [virtio] console: Separate out find_vqs operation into a different function (Amit Shah) [543824]
- [virtio] console: Separate out console init into a new function (Amit Shah) [543824]
- [virtio] console: Separate out console-specific data into a separate struct (Amit Shah) [543824]
- [virtio] console: ensure console size is updated on hvc open (Amit Shah) [543824]
- [virtio] console: struct ports for multiple ports per device. (Amit Shah) [543824]
- [virtio] console: remove global var (Amit Shah) [543824]
- [virtio] console: don't assume a single console port. (Amit Shah) [543824]
- [virtio] console: use vdev->priv to avoid accessing global var. (Amit Shah) [543824]
- [virtio] console: introduce a get_inbuf helper to fetch bufs from in_vq (Amit Shah) [543824]
- [virtio] console: ensure add_inbuf can work for multiple ports as well (Amit Shah) [543824]
- [virtio] console: encapsulate buffer information in a struct (Amit Shah) [543824]
- [virtio] console: port encapsulation (Amit Shah) [543824]
- [virtio] console: We support only one device at a time (Amit Shah) [543824]
- [virtio] hvc_console: Remove __devinit annotation from hvc_alloc (Amit Shah) [543824]
- [virtio] hvc_console: make the ops pointer const. (Amit Shah) [543824]
- [virtio] console: statically initialize virtio_cons (Amit Shah) [543824]
- [virtio] console: comment cleanup (Amit Shah) [543824]
- [x86] Fix crash when profiling more than 28 events (Bhavna Sarathy) [557570]
- [x86] Add AMD Node ID MSR support (Bhavna Sarathy) [557540]
- [kvm] fix spurious interrupt with irqfd (Marcelo Tosatti) [559343]
- [kvm] eventfd: allow atomic read and waitqueue remove (Marcelo Tosatti) [559343]
- [kvm] properly check max PIC pin in irq route setup (Marcelo Tosatti) [559343]
- [kvm] only allow one gsi per fd (Marcelo Tosatti) [559343]
- [kvm] x86: Fix leak of free lapic date in kvm_arch_vcpu_init() (Marcelo Tosatti) [559343]
- [kvm] x86: Fix probable memory leak of vcpu->arch.mce_banks (Marcelo Tosatti) [559343]
- [kvm] MMU: bail out pagewalk on kvm_read_guest error (Marcelo Tosatti) [559343]
- [kvm] x86: Fix host_mapping_level() (Marcelo Tosatti) [559343]
- [kvm] Fix race between APIC TMR and IRR (Marcelo Tosatti) [559343]
- [x86] acpi: Export acpi_pci_irq_{add|del}_prt() (James Paradis) [553781]
- [kdump] backport upstream ppc64 kcrctab fixes (Neil Horman) [558803]
- [mm] Memory tracking for Stratus (James Paradis) [512400]

* Thu Jan 28 2010 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-12.el6]
- [drm] radeon possible security issue (Jerome Glisse) [556692]
- [mm] Memory tracking for Stratus (James Paradis) [512400]
- [pci] Always set prefetchable base/limit upper32 registers (Prarit Bhargava) [553471]
- [scsi] Sync be2iscsi with upstream (Mike Christie) [515256]
- [x86] msr/cpuid: Register enough minors for the MSR and CPUID drivers (George Beshers) [557554]
- [x86] Remove unnecessary mdelay() from cpu_disable_common() (Peter Bogdanovic) [463633]
- [x86] ioapic: Document another case when level irq is seen as an edge (Peter Bogdanovic) [463633]
- [x86] ioapic: Fix the EOI register detection mechanism (Peter Bogdanovic) [463633]
- [x86] io-apic: Move the effort of clearing remoteIRR explicitly before migrating the irq (Peter Bogdanovic) [463633]
- [x86] Remove local_irq_enable()/local_irq_disable() in fixup_irqs() (Peter Bogdanovic) [463633]
- [x86] Use EOI register in io-apic on intel platforms (Peter Bogdanovic) [463633]

* Tue Jan 26 2010 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-11.el6]
- [kdump] Remove the 32MB limitation for crashkernel (Steve Best) [529270]
- [dm] dm-raid45: export missing dm_rh_inc (Heinz Mauelshagen) [552329]
- [block] dm-raid45: add raid45 target (Heinz Mauelshagen) [552329]
- [block] dm-replicator: blockdev site link handler (Heinz Mauelshagen) [552364]
- [block] dm-replicator: ringbuffer replication log handler (Heinz Mauelshagen) [552364]
- [block] dm-replicator: replication log and site link handler interfaces and main replicator module (Heinz Mauelshagen) [552364]
- [block] dm-replicator: documentation and module registry (Heinz Mauelshagen) [552364]
- [s390x] qeth: set default BLKT settings dependend on OSA hw level (Hendrik Brueckner) [557474]
- [drm] bring RHEL6 radeon drm up to 2.6.33-rc4/5 level (Jerome Glisse) [557539]
- [netdrv] e1000e: enhance frame fragment detection (Andy Gospodarek) [462780]
- [stable] ipv6: skb_dst() can be NULL in ipv6_hop_jumbo(). (David S. Miller) [555084]
- [stable] module: handle ppc64 relocating kcrctabs when CONFIG_RELOCATABLE=y (Rusty Russell) [555084]
- [stable] fix more leaks in audit_tree.c tag_chunk() (Al Viro) [555084]
- [stable] fix braindamage in audit_tree.c untag_chunk() (Al Viro) [555084]
- [stable] mac80211: fix skb buffering issue (and fixes to that) (Johannes Berg) [555084]
- [stable] kernel/sysctl.c: fix stable merge error in NOMMU mmap_min_addr (Mike Frysinger) [555084]
- [stable] libertas: Remove carrier signaling from the scan code (Samuel Ortiz) [555084]
- [stable] mac80211: add missing sanity checks for action frames (Felix Fietkau) [555084]
- [stable] iwl: off by one bug (Dan Carpenter) [555084]
- [stable] cfg80211: fix syntax error on user regulatory hints (Luis R. Rodriguez) [555084]
- [stable] ath5k: Fix eeprom checksum check for custom sized eeproms (Luis R. Rodriguez) [555084]
- [stable] iwlwifi: fix iwl_queue_used bug when read_ptr == write_ptr (Zhu Yi) [555084]
- [stable] xen: fix hang on suspend. (Ian Campbell) [555084]
- [stable] quota: Fix dquot_transfer for filesystems different from ext4 (Jan Kara) [555084]
- [stable] hwmon: (adt7462) Fix pin 28 monitoring (Roger Blofeld) [555084]
- [stable] hwmon: (coretemp) Fix TjMax for Atom N450/D410/D510 CPUs (Yong Wang) [555084]
- [stable] netfilter: nf_ct_ftp: fix out of bounds read in update_nl_seq() (Patrick McHardy) [555084]
- [stable] netfilter: ebtables: enforce CAP_NET_ADMIN (Florian Westphal) [555084]
- [stable] ASoC: Fix WM8350 DSP mode B configuration (Mark Brown) [555084]
- [stable] ALSA: atiixp: Specify codec for Foxconn RC4107MA-RS2 (Daniel T Chen) [555084]
- [stable] ALSA: ac97: Add Dell Dimension 2400 to Headphone/Line Jack Sense blacklist (Daniel T Chen) [555084]
- [stable] mmc_block: fix queue cleanup (Adrian Hunter) [555084]
- [stable] mmc_block: fix probe error cleanup bug (Jarkko Lavinen) [555084]
- [stable] mmc_block: add dev_t initialization check (Anna Lemehova) [555084]
- [stable] kernel/signal.c: fix kernel information leak with print-fatal-signals=1 (Andi Kleen) [555084]
- [stable] dma-debug: allow DMA_BIDIRECTIONAL mappings to be synced with DMA_FROM_DEVICE and (Krzysztof Halasa) [555084]
- [stable] lib/rational.c needs module.h (Sascha Hauer) [555084]
- [stable] drivers/cpuidle/governors/menu.c: fix undefined reference to `__udivdi3' (Stephen Hemminger) [555084]
- [stable] rtc_cmos: convert shutdown to new pnp_driver->shutdown (OGAWA Hirofumi) [555084]
- [stable] Revert "x86: Side-step lguest problem by only building cmpxchg8b_emu for pre-Pentium" (Rusty Russell) [555084]
- [stable] exofs: simple_write_end does not mark_inode_dirty (Boaz Harrosh) [555084]
- [stable] modules: Skip empty sections when exporting section notes (Ben Hutchings) [555084]
- [stable] ASoC: fix params_rate() macro use in several codecs (Guennadi Liakhovetski) [555084]
- [stable] fasync: split 'fasync_helper()' into separate add/remove functions (Linus Torvalds) [555084]
- [stable] untangle the do_mremap() mess (Al Viro)

* Fri Jan 22 2010 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-10.el6]
- [mm] mmap: don't return ENOMEM when mapcount is temporarily exceeded in munmap() (Danny Feng) [557000]
- [netdrv] vxge: fix issues found in Neterion testing (Michal Schmidt) [493985]
- [x86] Force irq complete move during cpu offline (Prarit Bhargava) [541815]
- [sound] Fix SPDIF-In for AD1988 codecs + add Intel Cougar IDs (Jaroslav Kysela) [557473]
- [scsi] aic79xx: check for non-NULL scb in ahd_handle_nonpkt_busfree (Tomas Henzl) [557753]
- [s390x] fix loading of PER control registers for utrace. (CAI Qian) [556410]
- [s390x] ptrace: dont abuse PT_PTRACED (CAI Qian) [552102]
- [perf] Remove the "event" callback from perf events (Jason Baron) [525517]
- [perf] Use overflow handler instead of the event callback (Jason Baron) [525517]
- [perf] Fix locking for PERF_FORMAT_GROUP (Jason Baron) [525517]
- [perf] Fix event scaling for inherited counters (Jason Baron) [525517]
- [perf] Fix PERF_FORMAT_GROUP scale info (Jason Baron) [525517]
- [perf] Allow for custom overflow handlers (Jason Baron) [525517]
- [perf] Add a callback to perf events (Jason Baron) [525517]
- [perf] improve error reporting (Jason Baron) [525517]
- [perf] add kernel internal interface (Jason Baron) [525517]
- [utrace] fix utrace_maybe_reap() vs find_matching_engine() race (Oleg Nesterov) [557338]
- [x86] Disable Memory hot add on x86 32-bit (Prarit Bhargava) [557131]
- [netdrv] e1000e: update to the latest upstream (Andy Gospodarek) [462780]
- [gfs] Use dquot_send_warning() (Steven Whitehouse) [557057]
- [gfs] Add quota netlink support (Steven Whitehouse) [557057]
- [netdrv] qlge: update to upstream version v1.00.00.23.00.00-01 (Andy Gospodarek) [553357]
- [s390x] zfcp: set HW timeout requested by BSG request (Hendrik Brueckner) [556918]
- [s390x] zfcp: introduce BSG timeout callback (Hendrik Brueckner) [556918]
- [scsi] scsi_transport_fc: Allow LLD to reset FC BSG timeout (Hendrik Brueckner) [556918]

* Wed Jan 20 2010 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-9.el6]
- [kvm] fix cleanup_srcu_struct on vm destruction (Marcelo Tosatti) [554762]
- [x86] core: make LIST_POISON less deadly (Avi Kivity) [554640]
- [x86] dell-wmi: Add support for new Dell systems (Matthew Garrett) [525548]
- [fs] xfs: 2.6.33 updates (Eric Sandeen) [554891]
- [x86] Add kernel pagefault tracepoint for x86 & x86_64. (Larry Woodman) [526032]
- [pci] PCIe AER: honor ACPI HEST FIRMWARE FIRST mode (Matthew Garrett) [537205]
- [block] direct-io: cleanup blockdev_direct_IO locking (Eric Sandeen) [556547]
- [tracing] tracepoint: Add signal tracepoints (Masami Hiramatsu) [526030]
- [cgroups] fix for "kernel BUG at kernel/cgroup.c:790" (Dave Anderson) [547815]
- [irq] Expose the irq_desc node as /proc/irq/*/node (George Beshers) [555866]
- [scsi] qla2xxx - Update support for FC/FCoE HBA/CNA (Rob Evers) [553854]
- [scsi] bfa update from 2.1.2.0 to 2.1.2.1 (Rob Evers) [475704]
- [nfs] sunrpc: fix build-time warning (Steve Dickson) [437715]
- [nfs] sunrpc: on successful gss error pipe write, don't return error (Steve Dickson) [437715]
- [nfs] SUNRPC: Fix the return value in gss_import_sec_context() (Steve Dickson) [437715]
- [nfs] SUNRPC: Fix up an error return value in gss_import_sec_context_kerberos() (Steve Dickson) [437715]
- [nfs] sunrpc: fix peername failed on closed listener (Steve Dickson) [437715]
- [nfs] nfsd: make sure data is on disk before calling ->fsync (Steve Dickson) [437715]
- [uv] React 2.6.32.y: isolcpus broken in 2.6.32.y kernel (George Beshers) [548842]
- [gru] GRU Rollup patch (George Beshers) [546680]
- [uv] XPC: pass nasid instead of nid to gru_create_message_queue (George Beshers) [546695]
- [uv] x86: XPC receive message reuse triggers invalid BUG_ON (George Beshers) [546695]
- [uv] x86: xpc_make_first_contact hang due to not accepting ACTIVE state (George Beshers) [546695]
- [uv] x86: xpc NULL deref when mesq becomes empty (George Beshers) [546695]
- [uv] x86: update XPC to handle updated BIOS interface (George Beshers) [546695]
- [uv] xpc needs to provide an abstraction for uv_gpa (George Beshers) [546695]
- [uv] x86, irq: Check move_in_progress before freeing the vector mapping (George Beshers) [546668]
- [uv] x86: Remove move_cleanup_count from irq_cfg (George Beshers) [546668]
- [uv] x86, irq: Allow 0xff for /proc/irq/[n]/smp_affinity on an 8-cpu system (George Beshers) [546668]
- [uv] x86, apic: Move SGI UV functionality out of generic IO-APIC code (George Beshers) [546668]
- [uv] x86 SGI: Fix irq affinity for hub based interrupts (George Beshers) [546668]
- [uv] x86 RTC: Always enable RTC clocksource (George Beshers) [546668]
- [uv] x86 RTC: Rename generic_interrupt to x86_platform_ipi (George Beshers) [546668]
- [uv] x86, mm: Correct the implementation of is_untracked_pat_range() (George Beshers) [548524]
- [uv] x86: Change is_ISA_range() into an inline function (George Beshers) [548524]
- [uv] x86, platform: Change is_untracked_pat_range() to bool (George Beshers) [548524]
- [uv] x86, mm: is_untracked_pat_range() takes a normal semiclosed range (George Beshers) [548524]
- [uv] x86, mm: Call is_untracked_pat_range() rather than is_ISA_range() (George Beshers) [548524]
- [uv] x86 SGI: Dont track GRU space in PAT (George Beshers) [548524]
- [scsi] megaraid: upgrade to 4.17 (Tomas Henzl) [520729]
- [scsi] mpt2sas: Bump version 03.100.03.00 (Tomas Henzl) [470848]
- [scsi] mpt2sas: don't update links nor unblock device at no link rate change (Tomas Henzl) [470848]
- [scsi] mpt2sas: add support for RAID Action System Shutdown Initiated at OS Shutdown (Tomas Henzl) [470848]
- [scsi] mpt2sas: freeze the sdev IO queue when firmware sends internal device reset (Tomas Henzl) [470848]
- [scsi] mpt2sas: fix PPC endian bug (Tomas Henzl) [470848]
- [scsi] mpt2sas: mpt2sas_base_get_sense_buffer_dma returns little endian (Tomas Henzl) [470848]
- [scsi] mpt2sas: return DID_TRANSPORT_DISRUPTED in nexus loss and SCSI_MLQUEUE_DEVICE_BUSY if device is busy (Tomas Henzl) [470848]
- [scsi] mpt2sas: retrieve the ioc facts prior to putting the controller into READY state (Tomas Henzl) [470848]
- [scsi] mpt2sas: add new info messages for IR and Expander events (Tomas Henzl) [470848]
- [scsi] mpt2sas: limit the max_depth to 32 for SATA devices (Tomas Henzl) [470848]
- [scsi] mpt2sas: add TimeStamp support when sending ioc_init (Tomas Henzl) [470848]
- [scsi] mpt2sas: add extended type for diagnostic buffer support (Tomas Henzl) [470848]
- [scsi] mpt2sas: add command line option diag_buffer_enable (Tomas Henzl) [470848]
- [scsi] mpt2sas: fix some comments (Tomas Henzl) [470848]
- [scsi] mpt2sas: stop driver when firmware encounters faults (Tomas Henzl) [470848]
- [scsi] mpt2sas: adding MPI Headers - revision L (Tomas Henzl) [470848]
- [scsi] mpt2sas: new device SAS2208 support (Tomas Henzl) [470848]
- [scsi] mpt2sas: check for valid response info (Tomas Henzl) [470848]
- [scsi] mpt2sas: fix expander remove fail (Tomas Henzl) [470848]
- [scsi] mpt2sas: use sas address instead of handle as a lookup (Tomas Henzl) [470848]
- [sound] ALSA HDA driver update 2009-12-15 (Jaroslav Kysela) [555812]
- [block] Honor the gfp_mask for alloc_page() in blkdev_issue_discard() (Mike Snitzer) [554719]
- [scsi] sync fcoe with upstream (Mike Christie) [549945]
- [net] dccp: modify how dccp creates slab caches to prevent bug halt in SLUB (Neil Horman) [553698]
- [s390x] tape: Add pr_fmt() macro to all tape source files (Hendrik Brueckner) [554380]
- [s390] qeth: fix packet loss if TSO is switched on (Hendrik Brueckner) [546632]
- [s390x] qeth: Support for HiperSockets Network Traffic Analyzer (Hendrik Brueckner) [463706]
- [serial] 8250: add support for DTR/DSR hardware flow control (Mauro Carvalho Chehab) [523848]

* Tue Jan 19 2010 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-8.el6]
- [build] Revert "[redhat] disabling temporaly DEVTMPFS" (Aristeu Rozanski)

* Tue Jan 19 2010 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-7.el6]
- [drm] minor printk fixes from upstream (Dave Airlie) [554601]
- [offb] add support for framebuffer handoff to offb. (Dave Airlie) [554948]
- [x86] allow fbdev primary video code on 64-bit. (Dave Airlie) [554930]
- [drm] nouveau: update to 2.6.33 level (Dave Airlie) [549930]
- [drm] ttm: validation API changes + ERESTART fixes. (Dave Airlie) [554918]
- [drm] radeon/kms: update to 2.6.33 (without TTM API changes) (Dave Airlie) [554918]
- [drm] i915: bring Intel DRM/KMS driver up to 2.6.33 (Dave Airlie) [554616]
- [drm] radeon/intel: realign displayport helper code with upstream. (Dave Airlie) [554601]
- [drm] kms: rollup KMS core and helper changes to 2.6.33 (Dave Airlie) [554601]
- [drm] remove address mask param for drm_pci_alloc() (Dave Airlie) [554601]
- [drm] add new userspace core drm interfaces from 2.6.33 (Dave Airlie) [554601]
- [drm] unlocked ioctl support for core + macro fixes (Dave Airlie) [554601]
- [drm] ttm: rollup upstream TTM fixes (Dave Airlie) [554601]
- [drm] mm: patch drm core memory range manager up to 2.6.33 (Dave Airlie) [554601]
- [drm] drm/edid: update to 2.6.33 EDID parser code (Dave Airlie) [554601]
- [net] dccp: fix module load dependency btw dccp_probe and dccp (Neil Horman) [554840]
- [powerpc] pseries: Correct pseries/dlpar.c build break without CONFIG_SMP (Steve Best) [539318]
- [powerpc] cpu-allocation/deallocation process (Steve Best) [539318]
- [powerpc] Add code to online/offline CPUs of a DLPAR node (Steve Best) [539318]
- [powerpc] CPU DLPAR handling (Steve Best) [539318]
- [powerpc] sysfs cpu probe/release files (Steve Best) [539318]
- [powerpc] Kernel handling of Dynamic Logical Partitioning (Steve Best) [539318]
- [powerpc] pseries: Add hooks to put the CPU into an appropriate offline state (Steve Best) [539318]
- [powerpc] pseries: Add extended_cede_processor() helper function. (Steve Best) [539318]
- [gfs] GFS2: Fix glock refcount issues (Steven Whitehouse) [546634]
- [gfs] GFS2: Ensure uptodate inode size when using O_APPEND (Steven Whitehouse) [547639]
- [gfs] GFS2: Fix locking bug in rename (Steven Whitehouse) [547640]
- [gfs] GFS2: Fix lock ordering in gfs2_check_blk_state() (Steven Whitehouse) [554673]
- [gfs2] only show nobarrier option on /proc/mounts when the option is active (Steven Whitehouse) [546665]
- [gfs2] add barrier/nobarrier mount options (Steven Whitehouse) [546665]
- [gfs2] remove division from new statfs code (Steven Whitehouse) [298561]
- [gfs2] Improve statfs and quota usability (Steven Whitehouse) [298561]
- [gfs2] Add set_xquota support (Steven Whitehouse) [298561]
- [gfs2] Add get_xquota support (Steven Whitehouse) [298561]
- [gfs2] Clean up gfs2_adjust_quota() and do_glock() (Steven Whitehouse) [298561]
- [gfs2] Remove constant argument from qd_get() (Steven Whitehouse) [298561]
- [gfs2] Remove constant argument from qdsb_get() (Steven Whitehouse) [298561]
- [gfs2] Add proper error reporting to quota sync via sysfs (Steven Whitehouse) [298561]
- [gfs2] Add get_xstate quota function (Steven Whitehouse) [298561]
- [gfs2] Remove obsolete code in quota.c (Steven Whitehouse) [298561]
- [gfs2] Hook gfs2_quota_sync into VFS via gfs2_quotactl_ops (Steven Whitehouse) [298561]
- [gfs2] Alter arguments of gfs2_quota/statfs_sync (Steven Whitehouse) [298561]
- [gfs2] Fix -o meta mounts for subsequent mounts (Steven Whitehouse) [546664]
- [gfs] GFS2: Fix gfs2_xattr_acl_chmod() (Steven Whitehouse) [546294]
- [gfs] VFS: Use GFP_NOFS in posix_acl_from_xattr() (Steven Whitehouse) [546294]
- [gfs] GFS2: Add cached ACLs support (Steven Whitehouse) [546294]
- [gfs] GFS2: Clean up ACLs (Steven Whitehouse) [546294]
- [gfs] GFS2: Use gfs2_set_mode() instead of munge_mode() (Steven Whitehouse) [546294]
- [gfs] GFS2: Use forget_all_cached_acls() (Steven Whitehouse) [546294]
- [gfs] VFS: Add forget_all_cached_acls() (Steven Whitehouse) [546294]
- [gfs] GFS2: Fix up system xattrs (Steven Whitehouse) [546294]
- [netdrv] igb: Update igb driver to support Barton Hills (Stefan Assmann) [462783]
- [dm] add feature flags to reduce future kABI impact (Mike Snitzer) [547756]
- [block] Stop using byte offsets (Mike Snitzer) [554718]
- [dm] Fix device mapper topology stacking (Mike Snitzer) [554718]
- [block] bdev_stack_limits wrapper (Mike Snitzer) [554718]
- [block] Fix discard alignment calculation and printing (Mike Snitzer) [554718]
- [block] Correct handling of bottom device misaligment (Mike Snitzer) [554718]
- [block] Fix incorrect alignment offset reporting and update documentation (Mike Snitzer) [554718]
- [kvm] Fix possible circular locking in kvm_vm_ioctl_assign_device() (Marcelo Tosatti) [554762]
- [kvm] only clear irq_source_id if irqchip is present (Marcelo Tosatti) [554762]
- [kvm] fix lock imbalance in kvm_*_irq_source_id() (Marcelo Tosatti) [554762]
- [kvm] VMX: Report unexpected simultaneous exceptions as internal errors (Marcelo Tosatti) [554762]
- [kvm] Allow internal errors reported to userspace to carry extra data (Marcelo Tosatti) [554762]
- [kvm] x86: disable paravirt mmu reporting (Marcelo Tosatti) [554762]
- [kvm] x86: disallow KVM_{SET, GET}_LAPIC without allocated in-kernel lapic (Marcelo Tosatti) [554762]
- [kvm] x86: disallow multiple KVM_CREATE_IRQCHIP (Marcelo Tosatti) [554762]
- [kvm] VMX: Disable unrestricted guest when EPT disabled (Marcelo Tosatti) [554762]
- [kvm] SVM: remove needless mmap_sem acquision from nested_svm_map (Marcelo Tosatti) [554762]
- [kvm] SVM: Notify nested hypervisor of lost event injections (Marcelo Tosatti) [554762]
- [kvm] SVM: Move INTR vmexit out of atomic code (Marcelo Tosatti) [554762]
- [kvm] remove pre_task_link setting in save_state_to_tss16 (Marcelo Tosatti) [554762]
- [kvm] x86: Extend KVM_SET_VCPU_EVENTS with selective updates (Marcelo Tosatti) [554500]
- [kvm] x86: Add KVM_GET/SET_VCPU_EVENTS (Marcelo Tosatti) [554500]
- [kvm] fix kvmclock-adjust-offset ioctl to match upstream (Marcelo Tosatti) [554524]
- [kvm] SVM: init_vmcb(): remove redundant save->cr0 initialization (Marcelo Tosatti) [554506]
- [kvm] SVM: Reset cr0 properly on vcpu reset (Marcelo Tosatti) [554506]
- [kvm] VMX: Use macros instead of hex value on cr0 initialization (Marcelo Tosatti) [554506]
- [kvm] avoid taking ioapic mutex for non-ioapic EOIs (Marcelo Tosatti) [550809]
- [kvm] Bump maximum vcpu count to 64 (Marcelo Tosatti) [550809]
- [kvm] convert slots_lock to a mutex (Marcelo Tosatti) [550809]
- [kvm] switch vcpu context to use SRCU (Marcelo Tosatti) [550809]
- [kvm] convert io_bus to SRCU (Marcelo Tosatti) [550809]
- [kvm] x86: switch kvm_set_memory_alias to SRCU update (Marcelo Tosatti) [550809]
- [kvm] use SRCU for dirty log (Marcelo Tosatti) [550809]
- [kvm] introduce kvm->srcu and convert kvm_set_memory_region to SRCU update (Marcelo Tosatti) [550809]
- [kvm] use gfn_to_pfn_memslot in kvm_iommu_map_pages (Marcelo Tosatti) [550809]
- [kvm] introduce gfn_to_pfn_memslot (Marcelo Tosatti) [550809]
- [kvm] split kvm_arch_set_memory_region into prepare and commit (Marcelo Tosatti) [550809]
- [kvm] modify alias layout in x86s struct kvm_arch (Marcelo Tosatti) [550809]
- [kvm] modify memslots layout in struct kvm (Marcelo Tosatti) [550809]
- [kvm] rcu: Enable synchronize_sched_expedited() fastpath (Marcelo Tosatti) [550809]
- [kvm] rcu: Add synchronize_srcu_expedited() to the documentation (Marcelo Tosatti) [550809]
- [kvm] rcu: Add synchronize_srcu_expedited() to the rcutorture test suite (Marcelo Tosatti) [550809]
- [kvm] Add synchronize_srcu_expedited() (Marcelo Tosatti) [550809]
- [kvm] Drop kvm->irq_lock lock from irq injection path (Marcelo Tosatti) [550809]
- [kvm] Move IO APIC to its own lock (Marcelo Tosatti) [550809]
- [kvm] Convert irq notifiers lists to RCU locking (Marcelo Tosatti) [550809]
- [kvm] Move irq ack notifier list to arch independent code (Marcelo Tosatti) [550809]
- [kvm] Move irq routing data structure to rcu locking (Marcelo Tosatti) [550809]
- [kvm] Maintain back mapping from irqchip/pin to gsi (Marcelo Tosatti) [550809]
- [kvm] Change irq routing table to use gsi indexed array (Marcelo Tosatti) [550809]
- [kvm] Move irq sharing information to irqchip level (Marcelo Tosatti) [550809]
- [kvm] Call pic_clear_isr() on pic reset to reuse logic there (Marcelo Tosatti) [550809]
- [kvm] Dont pass kvm_run arguments (Marcelo Tosatti) [550809]

* Thu Jan 14 2010 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-6.el6]
- [modsign] Remove Makefile.modpost qualifying message for module sign failure (David Howells) [543529]
- [nfs] fix oops in nfs_rename() (Jeff Layton) [554337]
- [x86] AMD: Fix stale cpuid4_info shared_map data in shared_cpu_map cpumasks (Prarit Bhargava) [546610]
- [s390] kernel: improve code generated by atomic operations (Hendrik Brueckner) [547411]
- [s390x] tape: incomplete device removal (Hendrik Brueckner) [547415]
- [netdrv] be2net: update be2net driver to latest upstream (Ivan Vecera) [515262]
- [x86] mce: fix confusion between bank attributes and mce attributes (hiro muneda) [476606]
- [tpm] autoload tpm_tis driver (John Feeney) [531891]
- [stable] generic_permission: MAY_OPEN is not write access (Serge E. Hallyn) [555084]
- [stable] rt2x00: Disable powersaving for rt61pci and rt2800pci. (Gertjan van Wingerde) [555084]
- [stable] lguest: fix bug in setting guest GDT entry (Rusty Russell) [555084]
- [stable] ext4: Update documentation to correct the inode_readahead_blks option name (Fang Wenqi) [555084]
- [stable] sched: Sched_rt_periodic_timer vs cpu hotplug (Peter Zijlstra) [555084]
- [stable] amd64_edac: fix forcing module load/unload (Borislav Petkov) [555084]
- [stable] amd64_edac: make driver loading more robust (Borislav Petkov) [555084]
- [stable] amd64_edac: fix driver instance freeing (Borislav Petkov) [555084]
- [stable] x86, msr: msrs_alloc/free for CONFIG_SMP=n (Borislav Petkov) [555084]
- [stable] x86, msr: Add support for non-contiguous cpumasks (Borislav Petkov) [555084]
- [stable] amd64_edac: unify MCGCTL ECC switching (Borislav Petkov) [555084]
- [stable] cpumask: use modern cpumask style in drivers/edac/amd64_edac.c (Rusty Russell) [555084]
- [stable] x86, msr: Unify rdmsr_on_cpus/wrmsr_on_cpus (Borislav Petkov) [555084]
- [stable] ext4: fix sleep inside spinlock issue with quota and dealloc (#14739) (Dmitry Monakhov) [555084]
- [stable] ext4: Convert to generic reserved quota's space management. (Dmitry Monakhov) [555084]
- [stable] quota: decouple fs reserved space from quota reservation (Dmitry Monakhov) [555084]
- [stable] Add unlocked version of inode_add_bytes() function (Dmitry Monakhov) [555084]
- [stable] udf: Try harder when looking for VAT inode (Jan Kara) [555084]
- [stable] orinoco: fix GFP_KERNEL in orinoco_set_key with interrupts disabled (Andrey Borzenkov) [555084]
- [stable] drm: disable all the possible outputs/crtcs before entering KMS mode (Zhao Yakui) [555084]
- [stable] drm/radeon/kms: fix crtc vblank update for r600 (Dave Airlie) [555084]
- [stable] sched: Fix balance vs hotplug race (Peter Zijlstra) [555084]
- [stable] Keys: KEYCTL_SESSION_TO_PARENT needs TIF_NOTIFY_RESUME architecture support (Geert Uytterhoeven) [555084]
- [stable] b43: avoid PPC fault during resume (Larry Finger) [555084]
- [stable] hwmon: (sht15) Off-by-one error in array index + incorrect constants (Jonathan Cameron) [555084]
- [stable] netfilter: fix crashes in bridge netfilter caused by fragment jumps (Patrick McHardy) [555084]
- [stable] ipv6: reassembly: use seperate reassembly queues for conntrack and local delivery (Patrick McHardy) [555084]
- [stable] e100: Fix broken cbs accounting due to missing memset. (Roger Oksanen) [555084]
- [stable] memcg: avoid oom-killing innocent task in case of use_hierarchy (Daisuke Nishimura) [555084]
- [stable] x86/ptrace: make genregs[32]_get/set more robust (Linus Torvalds) [555084]
- [stable] V4L/DVB (13596): ov511.c typo: lock => unlock (Dan Carpenter) [555084]
- [stable] kernel/sysctl.c: fix the incomplete part of sysctl_max_map_count-should-be-non-negative.patch (WANG Cong) [555084]
- [stable] 'sysctl_max_map_count' should be non-negative (Amerigo Wang) [555084]
- [stable] NOMMU: Optimise away the {dac_,}mmap_min_addr tests (David Howells) [555084]
- [stable] mac80211: fix race with suspend and dynamic_ps_disable_work (Luis R. Rodriguez) [555084]
- [stable] iwlwifi: fix 40MHz operation setting on cards that do not allow it (Reinette Chatre) [555084]
- [stable] iwlwifi: fix more eeprom endian bugs (Johannes Berg) [555084]
- [stable] iwlwifi: fix EEPROM/OTP reading endian annotations and a bug (Johannes Berg) [555084]
- [stable] iwl3945: fix panic in iwl3945 driver (Zhu Yi) [555084]
- [stable] iwl3945: disable power save (Reinette Chatre) [555084]
- [stable] ath9k_hw: Fix AR_GPIO_INPUT_EN_VAL_BT_PRIORITY_BB and its shift value in 0x4054 (Vasanthakumar Thiagarajan) [555084]
- [stable] ath9k_hw: Fix possible OOB array indexing in gen_timer_index[] on 64-bit (Vasanthakumar Thiagarajan) [555084]
- [stable] ath9k: fix suspend by waking device prior to stop (Sujith) [555084]
- [stable] ath9k: wake hardware during AMPDU TX actions (Luis R. Rodriguez) [555084]
- [stable] ath9k: fix missed error codes in the tx status check (Felix Fietkau) [555084]
- [stable] ath9k: Fix TX queue draining (Sujith) [555084]
- [stable] ath9k: wake hardware for interface IBSS/AP/Mesh removal (Luis R. Rodriguez) [555084]
- [stable] ath5k: fix SWI calibration interrupt storm (Bob Copeland) [555084]
- [stable] cfg80211: fix race between deauth and assoc response (Johannes Berg) [555084]
- [stable] mac80211: Fix IBSS merge (Sujith) [555084]
- [stable] mac80211: fix WMM AP settings application (Johannes Berg) [555084]
- [stable] mac80211: fix propagation of failed hardware reconfigurations (Luis R. Rodriguez) [555084]
- [stable] iwmc3200wifi: fix array out-of-boundary access (Zhu Yi) [555084]
- [stable] Libertas: fix buffer overflow in lbs_get_essid() (Daniel Mack) [555084]
- [stable] KVM: LAPIC: make sure IRR bitmap is scanned after vm load (Marcelo Tosatti) [555084]
- [stable] KVM: MMU: remove prefault from invlpg handler (Marcelo Tosatti) [555084]
- [stable] ioat2,3: put channel hardware in known state at init (Dan Williams) [555084]
- [stable] ioat3: fix p-disabled q-continuation (Dan Williams) [555084]
- [stable] x86/amd-iommu: Fix initialization failure panic (Joerg Roedel) [555084]
- [stable] dma-debug: Fix bug causing build warning (Ingo Molnar) [555084]
- [stable] dma-debug: Do not add notifier when dma debugging is disabled. (Shaun Ruffell) [555084]
- [stable] dma: at_hdmac: correct incompatible type for argument 1 of 'spin_lock_bh' (Nicolas Ferre) [555084]
- [stable] md: Fix unfortunate interaction with evms (NeilBrown) [555084]
- [stable] x86: SGI UV: Fix writes to led registers on remote uv hubs (Mike Travis) [555084]
- [stable] drivers/net/usb: Correct code taking the size of a pointer (Julia Lawall) [555084]
- [stable] USB: fix bugs in usb_(de)authorize_device (Alan Stern) [555084]
- [stable] USB: rename usb_configure_device (Alan Stern) [555084]
- [stable] Bluetooth: Prevent ill-timed autosuspend in USB driver (Oliver Neukum) [555084]
- [stable] USB: musb: gadget_ep0: avoid SetupEnd interrupt (Sergei Shtylyov) [555084]
- [stable] USB: Fix a bug on appledisplay.c regarding signedness (pancho horrillo) [555084]
- [stable] USB: option: support hi speed for modem Haier CE100 (Donny Kurnia) [555084]
- [stable] USB: emi62: fix crash when trying to load EMI 6|2 firmware (Clemens Ladisch) [555084]
- [stable] drm/radeon: fix build on 64-bit with some compilers. (Dave Airlie) [555084]
- [stable] ASoC: Do not write to invalid registers on the wm9712. (Eric Millbrandt) [555084]
- [stable] powerpc: Handle VSX alignment faults correctly in little-endian mode (Neil Campbell) [555084]
- [stable] ACPI: Use the return result of ACPI lid notifier chain correctly (Zhao Yakui) [555084]
- [stable] ACPI: EC: Fix MSI DMI detection (Alexey Starikovskiy) [555084]
- [stable] acerhdf: limit modalias matching to supported (Stefan Bader) [555084]
- [stable] ALSA: hda - Fix missing capsrc_nids for ALC88x (Takashi Iwai) [555084]
- [stable] sound: sgio2audio/pdaudiocf/usb-audio: initialize PCM buffer (Clemens Ladisch) [555084]
- [stable] ASoC: wm8974: fix a wrong bit definition (Guennadi Liakhovetski) [555084]
- [stable] pata_cmd64x: fix overclocking of UDMA0-2 modes (Bartlomiej Zolnierkiewicz) [555084]
- [stable] pata_hpt3x2n: fix clock turnaround (Sergei Shtylyov) [555084]
- [stable] clockevents: Prevent clockevent_devices list corruption on cpu hotplug (Thomas Gleixner) [555084]
- [stable] sched: Select_task_rq_fair() must honour SD_LOAD_BALANCE (Peter Zijlstra) [555084]
- [stable] x86, cpuid: Add "volatile" to asm in native_cpuid() (Suresh Siddha) [555084]
- [stable] sched: Fix task_hot() test order (Peter Zijlstra) [555084]
- [stable] SCSI: fc class: fix fc_transport_init error handling (Mike Christie) [555084]
- [stable] SCSI: st: fix mdata->page_order handling (FUJITA Tomonori) [555084]
- [stable] SCSI: qla2xxx: dpc thread can execute before scsi host has been added (Michael Reed) [555084]
- [stable] SCSI: ipr: fix EEH recovery (Kleber Sacilotto de Souza) [555084]
- [stable] implement early_io{re,un}map for ia64 (Luck, Tony) [555084]
- [stable] perf_event: Fix incorrect range check on cpu number (Paul Mackerras) [555084]
- [stable] netfilter: xtables: document minimal required version (Jan Engelhardt) [555084]
- [stable] intel-iommu: ignore page table validation in pass through mode (Chris Wright) [555084]
- [stable] jffs2: Fix long-standing bug with symlink garbage collection. (David Woodhouse) [555084]
- [stable] ipvs: zero usvc and udest (Simon Horman) [555084]
- [stable] mm: sigbus instead of abusing oom (Hugh Dickins) [555084]
- [stable] drm/i915: Fix LVDS stability issue on Ironlake (Zhenyu Wang) [555084]
- [stable] drm/i915: PineView only has LVDS and CRT ports (Zhenyu Wang) [555084]
- [stable] drm/i915: Avoid NULL dereference with component_only tv_modes (Chris Wilson) [555084]
- [stable] x86: Under BIOS control, restore AP's APIC_LVTTHMR to the BSP value (Yong Wang) [555084]
- [stable] bcm63xx_enet: fix compilation failure after get_stats_count removal (Florian Fainelli) [555084]
- [stable] V4L/DVB (13116): gspca - ov519: Webcam 041e:4067 added. (Rafal Milecki) [555084]
- [stable] ext3: Fix data / filesystem corruption when write fails to copy data (Jan Kara) [555084]
- [stable] net: Fix userspace RTM_NEWLINK notifications. (Eric W. Biederman) [555084]
- [stable] ACPI: Use the ARB_DISABLE for the CPU which model id is less than 0x0f. (Zhao Yakui) [555084]
- [stable] vmalloc: conditionalize build of pcpu_get_vm_areas() (Tejun Heo) [555084]
- [stable] asus-laptop: change light sens default values. (Corentin Chary) [555084]
- [stable] acerhdf: add new BIOS versions (Peter Feuerer) [555084]
- [stable] matroxfb: fix problems with display stability (Alan Cox) [555084]
- [stable] ipw2100: fix rebooting hang with driver loaded (Zhu Yi) [555084]
- [stable] thinkpad-acpi: preserve rfkill state across suspend/resume (Henrique de Moraes Holschuh) [555084]
- [stable] thinkpad-acpi: fix default brightness_mode for R50e/R51 (Henrique de Moraes Holschuh) [555084]
- [stable] memcg: fix memory.memsw.usage_in_bytes for root cgroup (Kirill A. Shutemov) [555084]
- [stable] mac80211: Fix dynamic power save for scanning. (Vivek Natarajan) [555084]
- [stable] ath9k: fix tx status reporting (Felix Fietkau) [555084]
- [stable] tracing: Fix event format export (Johannes Berg) [555084]
- [stable] b43legacy: avoid PPC fault during resume (Larry Finger) [555084]
- [stable] sparc: Set UTS_MACHINE correctly. (David S. Miller) [555084]
- [stable] sparc64: Fix stack debugging IRQ stack regression. (David S. Miller) [555084]
- [stable] sparc64: Fix overly strict range type matching for PCI devices. (David S. Miller) [555084]
- [stable] sparc64: Don't specify IRQF_SHARED for LDC interrupts. (David S. Miller) [555084]
- [stable] b44 WOL setup: one-bit-off stack corruption kernel panic fix (Stanislav Brabec) [555084]
- [stable] ip_fragment: also adjust skb->truesize for packets not owned by a socket (Patrick McHardy) [555084]
- [stable] tcp: Stalling connections: Fix timeout calculation routine (Damian Lukowski) [555084]
- [stable] slc90e66: fix UDMA handling (Bartlomiej Zolnierkiewicz) [555084]
- [stable] xen: try harder to balloon up under memory pressure. (Ian Campbell) [555084]
- [stable] Xen balloon: fix totalram_pages counting. (Gianluca Guida) [555084]
- [stable] xen: explicitly create/destroy stop_machine workqueues outside suspend/resume region. (Ian Campbell) [555084]
- [stable] xen: use iret for return from 64b kernel to 32b usermode (Jeremy Fitzhardinge) [555084]
- [stable] xen: don't leak IRQs over suspend/resume. (Ian Campbell) [555084]
- [stable] xen: improve error handling in do_suspend. (Ian Campbell) [555084]
- [stable] xen: call clock resume notifier on all CPUs (Ian Campbell) [555084]
- [stable] xen: register runstate info for boot CPU early (Jeremy Fitzhardinge) [555084]
- [stable] xen: don't call dpm_resume_noirq() with interrupts disabled. (Jeremy Fitzhardinge) [555084]
- [stable] xen: register runstate on secondary CPUs (Ian Campbell) [555084]
- [stable] xen: register timer interrupt with IRQF_TIMER (Ian Campbell) [555084]
- [stable] xen: correctly restore pfn_to_mfn_list_list after resume (Ian Campbell) [555084]
- [stable] xen: restore runstate_info even if !have_vcpu_info_placement (Jeremy Fitzhardinge) [555084]
- [stable] xen: re-register runstate area earlier on resume. (Ian Campbell) [555084]
- [stable] xen/xenbus: make DEVICE_ATTR()s static (Jeremy Fitzhardinge) [555084]
- [stable] drm/i915: Add the missing clonemask for display port on Ironlake (Zhao Yakui) [555084]
- [stable] drm/i915: Set the error code after failing to insert new offset into mm ht. (Chris Wilson) [555084]
- [stable] drm/ttm: Fix build failure due to missing struct page (Martin Michlmayr) [555084]
- [stable] drm/radeon/kms: rs6xx/rs740: clamp vram to aperture size (Alex Deucher) [555084]
- [stable] drm/radeon/kms: fix vram setup on rs600 (Alex Deucher) [555084]
- [stable] drm/radeon/kms: fix legacy crtc2 dpms (Alex Deucher) [555084]
- [stable] drm/radeon/kms: handle vblanks properly with dpms on (Alex Deucher) [555084]
- [stable] drm/radeon/kms: Add quirk for HIS X1300 board (Alex Deucher) [555084]
- [stable] powerpc: Fix usage of 64-bit instruction in 32-bit altivec code (Benjamin Herrenschmidt) [555084]
- [stable] powerpc/therm_adt746x: Record pwm invert bit at module load time] (Darrick J. Wong) [555084]
- [stable] powerpc/windfarm: Add detection for second cpu pump (Bolko Maass) [555084]
- [stable] mm: hugetlb: fix hugepage memory leak in walk_page_range() (Naoya Horiguchi) [555084]
- [stable] mm: hugetlb: fix hugepage memory leak in mincore() (Naoya Horiguchi) [555084]
- [stable] x86: Fix bogus warning in apic_noop.apic_write() (Thomas Gleixner) [555084]
- [stable] rtl8187: Fix wrong rfkill switch mask for some models (Larry Finger) [555084]
- [stable] wireless: correctly report signal value for IEEE80211_HW_SIGNAL_UNSPEC (John W. Linville) [555084]
- [stable] mac80211: fix scan abort sanity checks (Johannes Berg) [555084]
- [stable] mac80211: Revert 'Use correct sign for mesh active path refresh' (Javier Cardona) [555084]
- [stable] mac80211: Fixed bug in mesh portal paths (Javier Cardona) [555084]
- [stable] mac80211: Fix bug in computing crc over dynamic IEs in beacon (Vasanthakumar Thiagarajan) [555084]
- [stable] Serial: Do not read IIR in serial8250_start_tx when UART_BUG_TXEN (Ian Jackson) [555084]
- [stable] Driver core: fix race in dev_driver_string (Alan Stern) [555084]
- [stable] debugfs: fix create mutex racy fops and private data (Mathieu Desnoyers) [555084]
- [stable] devpts_get_tty() should validate inode (Sukadev Bhattiprolu) [555084]
- [stable] futex: Take mmap_sem for get_user_pages in fault_in_user_writeable (Andi Kleen) [555084]
- [stable] md/bitmap: protect against bitmap removal while being updated. (NeilBrown) [555084]
- [stable] hfs: fix a potential buffer overflow (Amerigo Wang) [555084]
- [stable] pxa/em-x270: fix usb hub power up/reset sequence (Igor Grinberg) [555084]
- [stable] USB: Close usb_find_interface race v3 (Russ Dill) [555084]
- [stable] USB: usb-storage: add BAD_SENSE flag (Alan Stern) [555084]
- [stable] USB: usbtmc: repeat usb_bulk_msg until whole message is transfered (Andre Herms) [555084]
- [stable] USB: option.c: add support for D-Link DWM-162-U5 (Zhang Le) [555084]
- [stable] USB: musb_gadget_ep0: fix unhandled endpoint 0 IRQs, again (Sergei Shtylyov) [555084]
- [stable] USB: xhci: Add correct email and files to MAINTAINERS entry. (Sarah Sharp) [555084]
- [stable] jbd2: don't wipe the journal on a failed journal checksum (Theodore Ts'o) [555084]
- [stable] UBI: flush wl before clearing update marker (Sebastian Andrzej Siewior) [555084]
- [stable] bsdacct: fix uid/gid misreporting (Alexey Dobriyan) [555084]
- [stable] V4L/DVB: Fix test in copy_reg_bits() (Roel Kluin) [555084]
- [stable] pata_hpt{37x|3x2n}: fix timing register masks (take 2) (Sergei Shtylyov) [555084]
- [stable] x86: Fix typo in Intel CPU cache size descriptor (Dave Jones) [555084]
- [stable] x86: Add new Intel CPU cache size descriptors (Dave Jones) [555084]
- [stable] x86: Fix duplicated UV BAU interrupt vector (Cliff Wickman) [555084]
- [stable] x86/mce: Set up timer unconditionally (Jan Beulich) [555084]
- [stable] x86, mce: don't restart timer if disabled (Hidetoshi Seto) [555084]
- [stable] x86: Use -maccumulate-outgoing-args for sane mcount prologues (Thomas Gleixner) [555084]
- [stable] x86: Prevent GCC 4.4.x (pentium-mmx et al) function prologue wreckage (Thomas Gleixner) [555084]
- [stable] KVM: x86: include pvclock MSRs in msrs_to_save (Glauber Costa) [555084]
- [stable] KVM: fix irq_source_id size verification (Marcelo Tosatti) [555084]
- [stable] KVM: s390: Make psw available on all exits, not just a subset (Carsten Otte) [555084]
- [stable] KVM: s390: Fix prefix register checking in arch/s390/kvm/sigp.c (Carsten Otte) [555084]
- [stable] KVM: x86 emulator: limit instructions to 15 bytes (Avi Kivity) [555084]
- [stable] ALSA: hrtimer - Fix lock-up (Takashi Iwai) [555084]
- [stable] hrtimer: Fix /proc/timer_list regression (Feng Tang) [555084]
- [stable] ath5k: enable EEPROM checksum check (Luis R. Rodriguez) [555084]
- [stable] ath5k: allow setting txpower to 0 (Bob Copeland) [555084]
- [stable] ssb: Fix range check in sprom write (Michael Buesch) [555084]
- [stable] x86, apic: Enable lapic nmi watchdog on AMD Family 11h (Mikael Pettersson) [555084]
- [stable] x86: ASUS P4S800 reboot=bios quirk (Leann Ogasawara) [555084]
- [stable] x86: GART: pci-gart_64.c: Use correct length in strncmp (Joe Perches) [555084]
- [stable] x86: Fix iommu=nodac parameter handling (Tejun Heo) [555084]
- [stable] x86, Calgary IOMMU quirk: Find nearest matching Calgary while walking up the PCI tree (Darrick J. Wong) [555084]
- [stable] x86/amd-iommu: un__init iommu_setup_msi (Joerg Roedel) [555084]
- [stable] x86/amd-iommu: attach devices to pre-allocated domains early (Joerg Roedel) [555084]
- [stable] sched: Fix and clean up rate-limit newidle code (Mike Galbraith) [555084]
- [stable] sched: Rate-limit newidle (Mike Galbraith) [555084]
- [stable] sched: Fix affinity logic in select_task_rq_fair() (Mike Galbraith) [555084]
- [stable] sched: Check for an idle shared cache in select_task_rq_fair() (Mike Galbraith) [555084]
- [stable] PM / Runtime: Fix lockdep warning in __pm_runtime_set_status() (Rafael J. Wysocki) [555084]
- [stable] perf_event: Initialize data.period in perf_swevent_hrtimer() (Xiao Guangrong) [555084]
- [stable] perf_event: Fix invalid type in ioctl definition (Arjan van de Ven) [555084]
- [stable] rcu: Remove inline from forward-referenced functions (Paul E. McKenney) [555084]
- [stable] rcu: Fix note_new_gpnum() uses of ->gpnum (Paul E. McKenney) [555084]
- [stable] rcu: Fix synchronization for rcu_process_gp_end() uses of ->completed counter (Paul E. McKenney) [555084]
- [stable] rcu: Prepare for synchronization fixes: clean up for non-NO_HZ handling of ->completed counter (Paul E. McKenney) [555084]
- [stable] firewire: ohci: handle receive packets with a data length of zero (Jay Fenlason) [555084]
- [stable] USB: option: add pid for ZTE (zhao.ming9@zte.com.cn) [555084]
- [stable] USB: usb-storage: fix bug in fill_inquiry (Alan Stern) [555084]
- [stable] ext4: Fix potential fiemap deadlock (mmap_sem vs. i_data_sem) (Theodore Ts'o) [555084]
- [stable] ext4: Wait for proper transaction commit on fsync (Jan Kara) [555084]
- [stable] ext4: fix incorrect block reservation on quota transfer. (Dmitry Monakhov) [555084]
- [stable] ext4: quota macros cleanup (Dmitry Monakhov) [555084]
- [stable] ext4: ext4_get_reserved_space() must return bytes instead of blocks (Dmitry Monakhov) [555084]
- [stable] ext4: remove blocks from inode prealloc list on failure (Curt Wohlgemuth) [555084]
- [stable] ext4: Avoid data / filesystem corruption when write fails to copy data (Jan Kara) [555084]
- [stable] ext4: Return the PTR_ERR of the correct pointer in setup_new_group_blocks() (Roel Kluin) [555084]
- [stable] jbd2: Add ENOMEM checking in and for jbd2_journal_write_metadata_buffer() (Theodore Ts'o) [555084]
- [stable] ext4: move_extent_per_page() cleanup (Akira Fujita) [555084]
- [stable] ext4: initialize moved_len before calling ext4_move_extents() (Kazuya Mio) [555084]
- [stable] ext4: Fix double-free of blocks with EXT4_IOC_MOVE_EXT (Akira Fujita) [555084]
- [stable] ext4: make "norecovery" an alias for "noload" (Eric Sandeen) [555084]
- [stable] ext4: fix error handling in ext4_ind_get_blocks() (Jan Kara) [555084]
- [stable] ext4: avoid issuing unnecessary barriers (Theodore Ts'o) [555084]
- [stable] ext4: fix block validity checks so they work correctly with meta_bg (Theodore Ts'o) [555084]
- [stable] ext4: fix uninit block bitmap initialization when s_meta_first_bg is non-zero (Theodore Ts'o) [555084]
- [stable] ext4: don't update the superblock in ext4_statfs() (Theodore Ts'o) [555084]
- [stable] ext4: journal all modifications in ext4_xattr_set_handle (Eric Sandeen) [555084]
- [stable] ext4: fix i_flags access in ext4_da_writepages_trans_blocks() (Julia Lawall) [555084]
- [stable] ext4: make sure directory and symlink blocks are revoked (Theodore Ts'o) [555084]
- [stable] ext4: plug a buffer_head leak in an error path of ext4_iget() (Theodore Ts'o) [555084]
- [stable] ext4: fix possible recursive locking warning in EXT4_IOC_MOVE_EXT (Akira Fujita) [555084]
- [stable] ext4: fix lock order problem in ext4_move_extents() (Akira Fujita) [555084]
- [stable] ext4: fix the returned block count if EXT4_IOC_MOVE_EXT fails (Akira Fujita) [555084]
- [stable] ext4: avoid divide by zero when trying to mount a corrupted file system (Theodore Ts'o) [555084]
- [stable] ext4: fix potential buffer head leak when add_dirent_to_buf() returns ENOSPC (Theodore Ts'o) [555084]
- [stable] SCSI: megaraid_sas: fix 64 bit sense pointer truncation (Yang, Bo) [555084]
- [stable] SCSI: osd_protocol.h: Add missing #include (Martin Michlmayr) [555084]
- [stable] signal: Fix alternate signal stack check (Sebastian Andrzej Siewior) [555084]

* Sat Jan 09 2010 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-5.el6]
- [scsi] cciss: fix spinlock use (Tomas Henzl) [552910]
- [scsi] cciss,hpsa: reassign controllers (Tomas Henzl) [552192]
- [modsign] Don't attempt to sign a module if there are no key files (David Howells) [543529]
- [x86] Compile mce-inject module (Prarit Bhargava) [553323]
- [nfs] fix insecure export option (Steve Dickson) [437715]
- [nfs] NFS update to 2.6.33 part 3 (Steve Dickson) [437715]
- [nfs] NFS update to 2.6.33 part 2 (Steve Dickson) [437715]
- [nfs] NFS update to 2.6.33 part 1 (Steve Dickson) [437715]
- [s390] cio: deactivated devices can cause use after free panic (Hendrik Brueckner) [548490]
- [s390] cio: memory leaks when checking unusable devices (Hendrik Brueckner) [548490]
- [s390] cio: DASD steal lock task hangs (Hendrik Brueckner) [548490]
- [s390] cio: DASD cannot be set online (Hendrik Brueckner) [548490]
- [s390] cio: erratic DASD I/O behavior (Hendrik Brueckner) [548490]
- [s390] cio: not operational devices cannot be deactivated (Hendrik Brueckner) [548490]
- [s390] cio: initialization of I/O devices fails (Hendrik Brueckner) [548490]
- [s390] cio: kernel panic after unexpected interrupt (Hendrik Brueckner) [548490]
- [s390] cio: incorrect device state after device recognition and recovery (Hendrik Brueckner) [548490]
- [s390] cio: setting a device online or offline fails for unknown reasons (Hendrik Brueckner) [548490]
- [s390] cio: device recovery fails after concurrent hardware changes (Hendrik Brueckner) [548490]
- [s390] cio: device recovery stalls after multiple hardware events (Hendrik Brueckner) [548490]
- [s390] cio: double free under memory pressure (Hendrik Brueckner) [548490]
- [sunrpc] Don't display zero scope IDs (Jeff Layton) [463530]
- [sunrpc] Deprecate support for site-local addresses (Jeff Layton) [463530]
- [input] dell-laptop: Update rfkill state on switch change (Matthew Garrett) [547892]
- [input] Add support for adding i8042 filters (Matthew Garrett) [547892]
- [vfs] force reval of target when following LAST_BIND symlinks (Jeff Layton) [548153]
- [scsi] scsi_dh_rdac: add two IBM devices to rdac_dev_list (Rob Evers) [528576]
- [fs] ext4: flush delalloc blocks when space is low (Eric Sandeen) [526758]
- [fs] fs-writeback: Add helper function to start writeback if idle (Eric Sandeen) [526758]
- [fat] make discard a mount option (Jeff Moyer) [552355]
- [ext4] make trim/discard optional (and off by default) (Jeff Moyer) [552355]
- [fusion] bump version to 3.04.13 (Tomas Henzl) [548408]
- [fusion] fix for incorrect data underrun (Tomas Henzl) [548408]
- [fusion] remove unnecessary printk (Tomas Henzl) [548408]
- [cifs] NULL out tcon, pSesInfo, and srvTcp pointers when chasing DFS referrals (Jeff Layton) [545984]
- [fs] ext4: wait for log to commit when unmounting (Josef Bacik) [524267]
- [mm] hwpoison: backport the latest patches from linux-2.6.33 (Dean Nelson) [547705]
- [netdrv] bnx2i: update to 2.1.0 (Stanislaw Gruszka) [463268]
- [netdrv] cnic: fixes for RHEL6 (Stanislaw Gruszka) [463268]
- [gfs2] Fix potential race in glock code (Steven Whitehouse) [546279]
- [scsi] make driver PCI legacy I/O port free (Tomas Henzl) [549118]
- [scsi] eliminate double free (Tomas Henzl) [549351]
- [dlm] always use GFP_NOFS (David Teigland) [545904]
- [block] Fix topology stacking for data and discard alignment (Mike Snitzer) [549766]
- [scsi] scsi_dh: Make alua hardware handler s activate async (Rob Evers) [537257]
- [scsi] scsi_dh: Make hp hardware handler s activate async (Rob Evers) [537257]
- [scsi] scsi_dh: Make rdac hardware handler s activate async (Rob Evers) [537257]
- [scsi] scsi_dh: Change the scsidh_activate interface to be asynchronous (Rob Evers) [537257]
- [netdrv] update tg3 to version 3.105 (John Feeney) [465194]
- [netdrv] bnx2x: update to 1.52.1-5 (Stanislaw Gruszka) [464427]
- [netdrv] ixgbe: add support for 82599-KR and update to latest upstream (Andy Gospodarek) [462781]
- [block] cfq-iosched: Remove prio_change logic for workload selection (Jeff Moyer) [548796]
- [block] cfq-iosched: Get rid of nr_groups (Jeff Moyer) [548796]
- [block] cfq-iosched: Remove the check for same cfq group from allow_merge (Jeff Moyer) [548796]
- [block] cfq: set workload as expired if it doesn't have any slice left (Jeff Moyer) [548796]
- [block] Fix a CFQ crash in "for-2.6.33" branch of block tree (Jeff Moyer) [548796]
- [block] cfq: Remove wait_request flag when idle time is being deleted (Jeff Moyer) [548796]
- [block] cfq-iosched: commenting non-obvious initialization (Jeff Moyer) [548796]
- [block] cfq-iosched: Take care of corner cases of group losing share due to deletion (Jeff Moyer) [548796]
- [block] cfq-iosched: Get rid of cfqq wait_busy_done flag (Jeff Moyer) [548796]
- [block] cfq: Optimization for close cooperating queue searching (Jeff Moyer) [548796]
- [block] cfq-iosched: reduce write depth only if sync was delayed (Jeff Moyer) [548796]
- [x86] ucode-amd: Load ucode-patches once and not separately of each CPU (George Beshers) [548840]
- [x86] Remove enabling x2apic message for every CPU (George Beshers) [548840]
- [x86] Limit number of per cpu TSC sync messages (George Beshers) [548840]
- [sched] Limit the number of scheduler debug messages (George Beshers) [548840]
- [init] Limit the number of per cpu calibration bootup messages (George Beshers) [548840]
- [x86] Limit the number of processor bootup messages (George Beshers) [548840]
- [x86] cpu: mv display_cacheinfo -> cpu_detect_cache_sizes (George Beshers) [548840]
- [x86] Remove CPU cache size output for non-Intel too (George Beshers) [548840]
- [x86] Remove the CPU cache size printk's (George Beshers) [548840]

* Wed Dec 23 2009 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-4.el6]
- [kvm] VMX: Use shared msr infrastructure (Avi Kivity) [547777]
- [kvm] x86 shared msr infrastructure (Avi Kivity) [547777]
- [kvm] VMX: Move MSR_KERNEL_GS_BASE out of the vmx autoload msr area (Avi Kivity) [547777]
- [kvm] core, x86: Add user return notifiers (Avi Kivity) [547777]
- [quota] ext4: Support for 64-bit quota format (Jerome Marchand) [546311]
- [quota] ext3: Support for vfsv1 quota format (Jerome Marchand) [546311]
- [quota] Implement quota format with 64-bit space and inode limits (Jerome Marchand) [546311]
- [quota] Move definition of QFMT_OCFS2 to linux/quota.h (Jerome Marchand) [546311]
- [scsi] cciss: remove pci-ids (Tomas Henzl) [464649]
- [scsi] hpsa: new driver (Tomas Henzl) [464649]
- [mm] Add file page writeback mm tracepoints. (Larry Woodman) [523093]
- [mm] Add page reclaim mm tracepoints. (Larry Woodman) [523093]
- [mm] Add file page mm tracepoints. (Larry Woodman) [523093]
- [mm] Add anonynmous page mm tracepoints. (Larry Woodman) [523093]
- [mm] Add mm tracepoint definitions to kmem.h (Larry Woodman) [523093]
- [ksm] fix ksm.h breakage of nommu build (Izik Eidus) [548586]
- [ksm] remove unswappable max_kernel_pages (Izik Eidus) [548586]
- [ksm] memory hotremove migration only (Izik Eidus) [548586]
- [ksm] rmap_walk to remove_migation_ptes (Izik Eidus) [548586]
- [ksm] mem cgroup charge swapin copy (Izik Eidus) [548586]
- [ksm] share anon page without allocating (Izik Eidus) [548586]
- [ksm] take keyhole reference to page (Izik Eidus) [548586]
- [ksm] hold anon_vma in rmap_item (Izik Eidus) [548586]
- [ksm] let shared pages be swappable (Izik Eidus) [548586]
- [ksm] fix mlockfreed to munlocked (Izik Eidus) [548586]
- [ksm] stable_node point to page and back (Izik Eidus) [548586]
- [ksm] separate stable_node (Izik Eidus) [548586]
- [ksm] singly-linked rmap_list (Izik Eidus) [548586]
- [ksm] cleanup some function arguments (Izik Eidus) [548586]
- [ksm] remove redundancies when merging page (Izik Eidus) [548586]
- [ksm] three remove_rmap_item_from_tree cleanups (Izik Eidus) [548586]
- [mm] stop ptlock enlarging struct page (Izik Eidus) [548586]
- [mm] vmscan: do not evict inactive pages when skipping an active list scan (Rik van Riel) [548457]
- [mm] vmscan: make consistent of reclaim bale out between do_try_to_free_page and shrink_zone (Rik van Riel) [548457]
- [mm] vmscan: kill sc.swap_cluster_max (Rik van Riel) [548457]
- [mm] vmscan: zone_reclaim() dont use insane swap_cluster_max (Rik van Riel) [548457]
- [mm] vmscan: kill hibernation specific reclaim logic and unify it (Rik van Riel) [548457]
- [mm] vmscan: separate sc.swap_cluster_max and sc.nr_max_reclaim (Rik van Riel) [548457]
- [mm] vmscan: stop kswapd waiting on congestion when the min watermark is not being met (Rik van Riel) [548457]
- [mm] vmscan: have kswapd sleep for a short interval and double check it should be asleep (Rik van Riel) [548457]
- [mm] pass address down to rmap ones (Rik van Riel) [548457]
- [mm] CONFIG_MMU for PG_mlocked (Rik van Riel) [548457]
- [mm] mlocking in try_to_unmap_one (Rik van Riel) [548457]
- [mm] define PAGE_MAPPING_FLAGS (Rik van Riel) [548457]
- [mm] swap_info: note SWAP_MAP_SHMEM (Rik van Riel) [548457]
- [mm] swap_info: swap count continuations (Rik van Riel) [548457]
- [mm] swap_info: swap_map of chars not shorts (Rik van Riel) [548457]
- [mm] swap_info: SWAP_HAS_CACHE cleanups (Rik van Riel) [548457]
- [mm] swap_info: miscellaneous minor cleanups (Rik van Riel) [548457]
- [mm] swap_info: include first_swap_extent (Rik van Riel) [548457]
- [mm] swap_info: change to array of pointers (Rik van Riel) [548457]
- [mm] swap_info: private to swapfile.c (Rik van Riel) [548457]
- [mm] move inc_zone_page_state(NR_ISOLATED) to just isolated place (Rik van Riel) [548457]
- [xen] support MAXSMP (Andrew Jones) [547129]
- [xen] wait up to 5 minutes for device connetion and fix fallout (Paolo Bonzini) [523630]
- [uv] x86 SGI: Map low MMR ranges (George Beshers) [548181]
- [uv] gru: function to generate chipset IPI values (George Beshers) [548181]
- [uv] x86 RTC: Clean up error handling (George Beshers) [548181]
- [uv] x86: RTC: Add clocksource only boot option (George Beshers) [548181]
- [uv] x86: RTC: Fix early expiry handling (George Beshers) [548181]
- [uv] x86: introduce uv_gpa_is_mmr (George Beshers) [548181]
- [uv] x86: function to translate from gpa -> socket_paddr (George Beshers) [548181]
- [uv] x86: SGI UV: Fix BAU initialization (George Beshers) [548181]
- [s390] zfcp: Block SCSI EH thread for rport state BLOCKED (Hendrik Brueckner) [547413]
- [scsi] scsi_transport_fc: Introduce helper function for blocking scsi_eh (Hendrik Brueckner) [547413]
- [s390] zfcp: improve FSF error reporting (Hendrik Brueckner) [547386]
- [s390] zfcp: fix ELS ADISC handling to prevent QDIO errors (Hendrik Brueckner) [547385]
- [s390] zfcp: Assign scheduled work to driver queue (Hendrik Brueckner) [547377]
- [s390] zfcp: Don't fail SCSI commands when transitioning to blocked fc_rport (Hendrik Brueckner) [547379]
- [s390] ctcm: suspend has to wait for outstanding I/O (Hendrik Brueckner) [546633]
- [s390] cmm: free pages on hibernate (Hendrik Brueckner) [546407]
- [s390] iucv: add work_queue cleanup for suspend (Hendrik Brueckner) [546319]
- [s390] dasd: let device initialization wait for LCU setup (Hendrik Brueckner) [547735]
- [s390] dasd: remove strings from s390dbf (Hendrik Brueckner) [547735]
- [s390] dasd: enable prefix independent of pav support (Hendrik Brueckner) [547735]
- [sound] ALSA HDA driver update 2009-12-15 (Jaroslav Kysela) [525391]
- [utrace] utrace core (Roland McGrath) [549491]
- [utrace] implement utrace-ptrace (Roland McGrath) [549491]
- [ptrace] reorder the code in kernel/ptrace.c (Roland McGrath) [549491]
- [ptrace] export __ptrace_detach() and do_notify_parent_cldstop() (Roland McGrath) [549491]
- [ptrace_signal] check PT_PTRACED before reporting a signal (Roland McGrath) [549491]
- [tracehooks] check PT_PTRACED before reporting the single-step (Roland McGrath) [549491]
- [tracehooks] kill some PT_PTRACED checks (Roland McGrath) [549491]
- [signals] check ->group_stop_count after tracehook_get_signal() (Roland McGrath) [549491]
- [ptrace] x86: change syscall_trace_leave() to rely on tracehook when stepping (Roland McGrath) [549491]
- [ptrace] x86: implement user_single_step_siginfo() (Roland McGrath) [549491]
- [ptrace] change tracehook_report_syscall_exit() to handle stepping (Roland McGrath) [549491]
- [ptrace] powerpc: implement user_single_step_siginfo() (Roland McGrath) [549491]
- [ptrace] introduce user_single_step_siginfo() helper (Roland McGrath) [549491]
- [ptrace] copy_process() should disable stepping (Roland McGrath) [549491]
- [ptrace] cleanup ptrace_init_task()->ptrace_link() path (Roland McGrath) [549491]

* Thu Dec 17 2009 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-3.el6]
- [modsign] Don't check e_entry in ELF header (David Howells) [548027]
- [pci] pciehp: Provide an option to disable native PCIe hotplug (Matthew Garrett) [517050]
- [s390] OSA QDIO data connection isolation (Hendrik Brueckner) [537496]
- [s390] zcrypt: adjust speed rating of cex3 adapters (Hendrik Brueckner) [537495]
- [s390] zcrypt: adjust speed rating between cex2 and pcixcc (Hendrik Brueckner) [537495]
- [s390] zcrypt: use definitions for cex3 (Hendrik Brueckner) [537495]
- [s390] zcrypt: add support for cex3 device types (Hendrik Brueckner) [537495]
- [s390] zcrypt: special command support for cex3 exploitation (Hendrik Brueckner) [537495]
- [s390] zcrypt: initialize ap_messages for cex3 exploitation (Hendrik Brueckner) [537495]
- [s390] kernel: performance counter fix and page fault optimization (Hendrik Brueckner) [546396]
- [s390] kernel: fix dump indicator (Hendrik Brueckner) [546285]
- [s390] dasd: support DIAG access for read-only devices (Hendrik Brueckner) [546309]
- [s390] zcrypt: Do not simultaneously schedule hrtimer (Hendrik Brueckner) [546291]
- [s390] kernel: clear high-order bits after switching to 64-bit mode (Hendrik Brueckner) [546314]
- [virt] vhost: add missing architectures (Michael S. Tsirkin) [540389]
- [virt] vhost_net: a kernel-level virtio server (Michael S. Tsirkin) [540389]
- [virt] mm: export use_mm/unuse_mm to modules (Michael S. Tsirkin) [540389]
- [virt] tun: export underlying socket (Michael S. Tsirkin) [540389]
- [dm] snapshot-merge support from 2.6.33 (Mike Snitzer) [547563]
- [dm] snapshot changes from 2.6.33 (Mike Snitzer) [547563]
- [dm] crypt changes from 2.6.33 (Mike Snitzer) [547563]
- [dm] raid1 changes from 2.6.33 (Mike Snitzer) [547563]
- [dm] core and mpath changes from 2.6.33 (Mike Snitzer) [547563]
- [scsi] fix dma handling when using virtual hosts (Mike Christie) [525241]
- [nfs] convert proto= option to use netids rather than a protoname (Jeff Layton) [545973]

* Fri Dec 11 2009 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-2.el6]
- [block] config: enable CONFIG_BLK_DEV_INTEGRITY (Jeff Moyer) [490732]
- [block] config: enable CONFIG_BLK_CGROUP (Jeff Moyer) [425895]
- [libata] Clarify ata_set_lba_range_entries function (Jeff Moyer) [528046]
- [libata] Report zeroed read after Trim and max discard size (Jeff Moyer) [528046]
- [scsi] Correctly handle thin provisioning write error (Jeff Moyer) [528046]
- [scsi] sd: WRITE SAME(16) / UNMAP support (Jeff Moyer) [528046]
- [scsi] scsi_debug: Thin provisioning support (Jeff Moyer) [528046]
- [scsi] Add missing command definitions (Jeff Moyer) [528046]
- [block] Add support for the ATA TRIM command in libata. (Jeff Moyer) [528046]
- [block] dio: fix performance regression (Jeff Moyer) [545507]
- [block] cfq-iosched: Do not access cfqq after freeing it (Jeff Moyer) [425895]
- [block] include linux/err.h to use ERR_PTR (Jeff Moyer) [425895]
- [block] cfq-iosched: use call_rcu() instead of doing grace period stall on queue exit (Jeff Moyer) [425895]
- [block] blkio: Allow CFQ group IO scheduling even when CFQ is a module (Jeff Moyer) [425895]
- [block] blkio: Implement dynamic io controlling policy registration (Jeff Moyer) [425895]
- [block] blkio: Export some symbols from blkio as its user CFQ can be a module (Jeff Moyer) [425895]
- [block] cfq-iosched: make nonrot check logic consistent (Jeff Moyer) [545225]
- [block] io controller: quick fix for blk-cgroup and modular CFQ (Jeff Moyer) [425895]
- [block] cfq-iosched: move IO controller declerations to a header file (Jeff Moyer) [425895]
- [block] cfq-iosched: fix compile problem with !CONFIG_CGROUP (Jeff Moyer) [425895]
- [block] blkio: Documentation (Jeff Moyer) [425895]
- [block] blkio: Wait on sync-noidle queue even if rq_noidle = 1 (Jeff Moyer) [425895]
- [block] blkio: Implement group_isolation tunable (Jeff Moyer) [425895]
- [block] blkio: Determine async workload length based on total number of queues (Jeff Moyer) [425895]
- [block] blkio: Wait for cfq queue to get backlogged if group is empty (Jeff Moyer) [425895]
- [block] blkio: Propagate cgroup weight updation to cfq groups (Jeff Moyer) [425895]
- [block] blkio: Drop the reference to queue once the task changes cgroup (Jeff Moyer) [425895]
- [block] blkio: Provide some isolation between groups (Jeff Moyer) [425895]
- [block] blkio: Export disk time and sectors used by a group to user space (Jeff Moyer) [425895]
- [block] blkio: Some debugging aids for CFQ (Jeff Moyer) [425895]
- [block] blkio: Take care of cgroup deletion and cfq group reference counting (Jeff Moyer) [425895]
- [block] blkio: Dynamic cfq group creation based on cgroup tasks belongs to (Jeff Moyer) [425895]
- [block] blkio: Group time used accounting and workload context save restore (Jeff Moyer) [425895]
- [block] blkio: Implement per cfq group latency target and busy queue avg (Jeff Moyer) [425895]
- [block] blkio: Introduce per cfq group weights and vdisktime calculations (Jeff Moyer) [425895]
- [block] blkio: Introduce blkio controller cgroup interface (Jeff Moyer) [425895]
- [block] blkio: Introduce the root service tree for cfq groups (Jeff Moyer) [425895]
- [block] blkio: Keep queue on service tree until we expire it (Jeff Moyer) [425895]
- [block] blkio: Implement macro to traverse each service tree in group (Jeff Moyer) [425895]
- [block] blkio: Introduce the notion of cfq groups (Jeff Moyer) [425895]
- [block] blkio: Set must_dispatch only if we decided to not dispatch the request (Jeff Moyer) [425895]
- [block] cfq-iosched: no dispatch limit for single queue (Jeff Moyer) [425895]
- [block] Allow devices to indicate whether discarded blocks are zeroed (Jeff Moyer) [545203]
- [block] Revert "cfq: Make use of service count to estimate the rb_key offset" (Jeff Moyer) [425895]
- [block] cfq-iosched: fix corner cases in idling logic (Jeff Moyer) [425895]
- [block] cfq-iosched: idling on deep seeky sync queues (Jeff Moyer) [425895]
- [block] cfq-iosched: fix no-idle preemption logic (Jeff Moyer) [425895]
- [block] cfq-iosched: fix ncq detection code (Jeff Moyer) [425895]
- [block] cfq-iosched: cleanup unreachable code (Jeff Moyer) [425895]
- [block] cfq: Make use of service count to estimate the rb_key offset (Jeff Moyer) [425895]
- [block] partitions: read whole sector with EFI GPT header (Jeff Moyer) [463632]
- [block] partitions: use sector size for EFI GPT (Jeff Moyer) [463632]
- [block] Expose discard granularity (Jeff Moyer) [545203]
- [block] cfq-iosched: fix next_rq computation (Jeff Moyer) [425895]
- [block] cfq-iosched: simplify prio-unboost code (Jeff Moyer) [425895]
- [block] blkdev: flush disk cache on ->fsync (Jeff Moyer) [545199]
- [block] cfq-iosched: fix style issue in cfq_get_avg_queues() (Jeff Moyer) [425895]
- [block] cfq-iosched: fairness for sync no-idle queues (Jeff Moyer) [425895]
- [block] cfq-iosched: enable idling for last queue on priority class (Jeff Moyer) [425895]
- [block] cfq-iosched: reimplement priorities using different service trees (Jeff Moyer) [425895]
- [block] cfq-iosched: preparation to handle multiple service trees (Jeff Moyer) [425895]
- [block] cfq-iosched: adapt slice to number of processes doing I/O (Jeff Moyer) [425895]
- [block] cfq-iosched: improve hw_tag detection (Jeff Moyer) [425895]
- [block] cfq: break apart merged cfqqs if they stop cooperating (Jeff Moyer) [533932]
- [block] cfq: change the meaning of the cfqq_coop flag (Jeff Moyer) [533932]
- [block] cfq: merge cooperating cfq_queues (Jeff Moyer) [533932]
- [block] cfq: calculate the seek_mean per cfq_queue not per cfq_io_context (Jeff Moyer) [533932]
- [block] CFQ is more than a desktop scheduler (Jeff Moyer) [533932]
- [block] revert: cfq-iosched: limit coop preemption (Jeff Moyer) [533932]
- perf: Don't free perf_mmap_data until work has been done (Aristeu Rozanski) [547432]
- ext4: Fix insuficient checks in EXT4_IOC_MOVE_EXT (Aristeu Rozanski) [547432]
- agp: clear GTT on intel (Aristeu Rozanski) [547432]
- drm/i915: Fix sync to vblank when VGA output is turned off (Aristeu Rozanski) [547432]
- drm: nouveau fixes (Aristeu Rozanski) [547432]
- drm: radeon dp support (Aristeu Rozanski) [547432]
- drm: radeon fixes (Aristeu Rozanski) [547432]
- KVM: allow userspace to adjust kvmclock offset (Aristeu Rozanski) [547432]
- ath9k backports (Aristeu Rozanski) [547432]
- intel-iommu backport (Aristeu Rozanski) [547432]
- updating patch linux-2.6-nfsd4-proots.patch (2.6.32-8.fc13 reference) (Aristeu Rozanski) [547432]
- updating linux-2.6-execshield.patch (2.6.32-8.fc13 reference) (Aristeu Rozanski) [547432]

* Tue Dec 08 2009 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-1.el6]
- [rebase] Rebased to 2.6.32

* Mon Dec 07 2009 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-0.54.el6]
- [edac] amd64_edac: disabling temporarily (Aristeu Rozanski)
- [x86] Enable CONFIG_SPARSE_IRQ (Prarit Bhargava) [543174]
- [x86] panic if AMD cpu_khz is wrong (Prarit Bhargava) [523468]
- [infiniband] Rewrite SG handling for RDMA logic (Mike Christie) [540269]

* Wed Nov 25 2009 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-0.53.el6]
- [net] Add acession counts to all datagram protocols (Neil Horman) [445366]
- [modsign] Enable module signing in the RHEL RPM (David Howells) [517341]
- [modsign] Don't include .note.gnu.build-id in the digest (David Howells) [517341]
- [modsign] Apply signature checking to modules on module load (David Howells) [517341]
- [modsign] Module signature checker and key manager (David Howells) [517341]
- [modsign] Module ELF verifier (David Howells) [517341]
- [modsign] Add indications of module ELF types (David Howells) [517341]
- [modsign] Multiprecision maths library (David Howells) [517341]
- [procfs] add ability to modify proc file limits from outside a processes own context (Neil Horman) [461946]
- [s390x] fix build failure with CONFIG_FTRACE_SYSCALLS (Aristeu Rozanski) [538978]

* Wed Nov 25 2009 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-0.52.el6]
- [x86] AMD Northbridge: Verify NB's node is online (Prarit Bhargava) [536769]
- [scsi] devinfo update for Hitachi entries (Takahiro Yasui) [526763]
- [net] export device speed and duplex via sysfs (Andy Gospodarek) [453432]
- [ppc64] Fix kcrctab_ sections to undo undesireable relocations that break kdump (Neil Horman) [509012]
- [mm] Limit 32-bit x86 systems to 16GB and prevent panic on boot when system has more than ~30GB (Larry Woodman) [532039]

* Mon Nov 23 2009 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-0.51.el6]
- [kernel] Set panic_on_oops to 1 (Prarit Bhargava) [529963]
- [kdump] kexec: allow to shrink reserved memory (Amerigo Wang) [523091]
- [kdump] doc: update the kdump document (Amerigo Wang) [523091]
- [kdump] powerpc: implement crashkernel=auto (Amerigo Wang) [523091]
- [kdump] powerpc: add CONFIG_KEXEC_AUTO_RESERVE (Amerigo Wang) [523091]
- [kdump] ia64: implement crashkernel=auto (Amerigo Wang) [523091]
- [kdump] ia64: add CONFIG_KEXEC_AUTO_RESERVE (Amerigo Wang) [523091]
- [kdump] x86: implement crashkernel=auto (Amerigo Wang) [523091]
- [kdump] x86: add CONFIG_KEXEC_AUTO_RESERVE (Amerigo Wang) [523091]
- [block] aio: implement request batching (Jeff Moyer) [533931]
- [block] get rid of the WRITE_ODIRECT flag (Jeff Moyer) [533931]

* Sat Nov 21 2009 Aristeu Rozanski <arozansk@redhat.com> [2.6.32-0.50.el6]
- [crypto] padlock-aes: Use the correct mask when checking whether copying is required (Chuck Ebbert)
- [rfkill] add support to a key to control all radios (Aristeu Rozanski)
- [acpi] be less verbose about old BIOSes (Aristeu Rozanski)
- [drm] intel big hammer (Aristeu Rozanski)
- [e1000] add quirk for ich9 (Aristeu Rozanski)
- [pci] cacheline sizing (Dave Jones)
- [crash] add crash driver (Dave Anderson)
- [fb] disable fbcon logo with parameter (Aristeu Rozanski)
- [pci] silence some PCI resource allocation errors (Aristeu Rozanski)
- [serio] disable error messages when i8042 isn't found (Peter Jones)
- [serial] Enable higher baud rates for 16C95x (Aristeu Rozanski)
- [input] remove pcspkr modalias (Aristeu Rozanski)
- [floppy] remove the floppy pnp modalias (Aristeu Rozanski)
- [input] remove unwanted messages on spurious events (Aristeu Rozanski)
- [sound] hda intel prealloc 4mb dmabuffer (Aristeu Rozanski)
- [sound] disables hda beep by default (Aristeu Rozanski)
- [pci] sets PCIE ASPM default policy to POWERSAVE (Aristeu Rozanski)
- [pci] add config option to control the default state of PCI MSI interrupts (Aristeu Rozanski)
- [debug] always inline kzalloc (Aristeu Rozanski)
- [debug] add would_have_oomkilled procfs ctl (Aristeu Rozanski)
- [debug] add calls to print_tainted() on spinlock functions (Aristeu Rozanski)
- [debug] display tainted information on other places (Aristeu Rozanski)
- [x86] add option to control the NMI watchdog timeout (Aristeu Rozanski)
- [debug] print common struct sizes at boot time (Aristeu Rozanski)
- [acpi] Disable firmware video brightness change by default (Matthew Garrett)
- [acpi] Disable brightness switch by default (Aristeu Rozanski)
- [usb] enable autosuspend on UVC by default (Matthew Garrett)
- [usb] enable autosuspend by default on qcserial (Matthew Garrett)
- [usb] Allow drivers to enable USB autosuspend on a per-device basis (Matthew Garrett)
- [nfs] make nfs4 callback hidden (Steve Dickson)
- [nfsd4] proots (Aristeu Rozanski)
- [execshield] introduce execshield (Aristeu Rozanski)
- [powerpc] add modalias_show operation (Aristeu Rozanski)
- [hwmon] add VIA hwmon temperature sensor support (Aristeu Rozanski)
- [utrace] introduce utrace implementation (Aristeu Rozanski)
- [build] introduce AFTER_LINK variable (Aristeu Rozanski)


###
# The following Emacs magic makes C-c C-e use UTC dates.
# Local Variables:
# rpm-change-log-uses-utc: t
# End:
###
