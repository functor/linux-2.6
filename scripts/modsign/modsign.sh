#!/bin/bash


if [ $# = "0" ] ; then
	echo
#	echo "usage: $0 <module_to_sign> <key_name>"
	echo "usage: $0 <module_to_sign>"
	echo
	exit 1
fi

module=$1
#key=$2

# strip out only the sections that we care about
sh scripts/modsign/mod-extract.sh $module $module.out

# sign the sections
#gpg --no-greeting --default-key $key -b $module.out
gpg --no-greeting --no-default-keyring --secret-keyring ../kernel.sec --keyring ../kernel.pub -b $module.out

# check the signature
#gpg --verify rxrpc.ko.out.sig rxrpc.ko.out

## sha1 the sections
#sha1sum $module.out | awk "{print \$1}" > $module.sha1
#
## encrypt the sections
#gpg --no-greeting -e -o - -r $key $module.sha1 > $module.crypt

# add the encrypted data to the module
#objcopy --add-section module_sig=$module.crypt $module $module.signed
objcopy --add-section module_sig=$module.out.sig $module $module.signed
objcopy --set-section-flags module_sig=alloc $module.signed
rm -f $module.out*
