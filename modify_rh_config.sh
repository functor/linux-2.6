#!/bin/bash

pushd SOURCES > /dev/null

sed -i -e "s,CONFIG_IPV6=m,CONFIG_IPV6=y,g" config-generic
sed -i -e "s,CONFIG_SOUND=m,CONFIG_SOUND=n,g" config-generic

sed -i -e "s,CONFIG_MODULE_SIG=y,CONFIG_MODULE_SIG=n,g" config-generic-rhel

sed -i -e "s,CONFIG_CRYPTO_SIGNATURE=y,CONFIG_CRYPTO_SIGNATURE=n,g" config-generic-rhel
sed -i -e "s,CONFIG_CRYPTO_SIGNATURE_DSA=y,CONFIG_CRYPTO_SIGNATURE_DSA=n,g" config-generic-rhel

# Do not enable CONFIG_SYSFS_DEPRECATED options for Scientific Linux
if ! grep -qiE "CentOS.*6.3|Scientific" /etc/redhat-release; then
	sed -i -e "s,# CONFIG_SYSFS_DEPRECATED is not set,CONFIG_SYSFS_DEPRECATED=y,g" config-generic
	sed -i -e "s,# CONFIG_SYSFS_DEPRECATED_V2 is not set,CONFIG_SYSFS_DEPRECATED_V2=y,g" config-generic
fi

sed -i -e "s,CONFIG_BUILD_DOCSRC=y,CONFIG_BUILD_DOCSRC=n,g" config-generic

popd > /dev/null
