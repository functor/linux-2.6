/* module-verify.c: description
 *
 * Written by David Howells (dhowells@redhat.com)
 * - Derived from GregKH's RSA module signer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/elf.h>
#include <linux/crypto.h>
#include <linux/crypto/ksign.h>
#include <asm/scatterlist.h>
#include "module-verify.h"

#if 0
#define _debug(FMT, ...) printk(KERN_DEBUG FMT, ##__VA_ARGS__)
#else
#define _debug(FMT, ...) do { ; } while (0)
#endif

static int signedonly;

/*****************************************************************************/
/*
 * verify the signature attached to a module
 */
int module_verify_sig(Elf_Ehdr *hdr, Elf_Shdr *sechdrs, const char *secstrings, struct module *mod)
{
	struct crypto_tfm *sha1_tfm;
	unsigned sig_index, sig_size;
	char *sig;
	int i;

	/* pull the signature out of the file */
	sig_index = 0;
	for (i = 1; i < hdr->e_shnum; i++) {
		if (strcmp(secstrings + sechdrs[i].sh_name,
			   "module_sig") == 0) {
			sig_index = i;
			break;
		}
	}

	if (sig_index <= 0)
		goto no_signature;

	_debug("sig in section %d (size %d)\n",
	       sig_index, sechdrs[sig_index].sh_size);

	sig = (char *) sechdrs[sig_index].sh_addr;
	sig_size = sechdrs[sig_index].sh_size;

	_debug("");




	/* grab an SHA1 transformation context
	 * - !!! if this tries to load the sha1.ko module, we will deadlock!!!
	 */
	sha1_tfm = crypto_alloc_tfm2("sha1", 0, 1);
	if (!sha1_tfm) {
		printk("Couldn't load module - SHA1 transform unavailable\n");
		return -EPERM;
	}

	crypto_digest_init(sha1_tfm);

	for (i = 1; i < hdr->e_shnum; i++) {
		uint8_t *data;
		int size;
		const char *name = secstrings + sechdrs[i].sh_name;

		/* We only care about sections with "text" or "data" in their names */
		if ((strstr(name, "text") == NULL) &&
		    (strstr(name, "data") == NULL))
			continue;

		/* avoid the ".rel.*" sections too. */
		if (strstr(name, ".rel.") != NULL)
			continue;

		/* avoid the ".rel.*" sections too. */
		if (strstr(name, ".rela.") != NULL)
			continue;

		data = (uint8_t *) sechdrs[i].sh_addr;
		size = sechdrs[i].sh_size;

		_debug("SHA1ing the %s section, size %d\n", name, size);
		_debug("idata [ %02x%02x%02x%02x ]\n",
		       data[0], data[1], data[2], data[3]);

		crypto_digest_update_kernel(sha1_tfm, data, size);
	}

	/* do the actual signature verification */
	i = ksign_verify_signature(sig, sig_size, sha1_tfm);
	if (!i)
                mod->gpgsig_ok = 1;
	
	return i;

	/* deal with the case of an unsigned module */
 no_signature:
 	if (!signedonly)
		return 0;
	printk("An attempt to load unsigned module was rejected\n");
	return -EPERM;
} /* end module_verify_sig() */

static int __init sign_setup(char *str)
{
	signedonly = 1;
	return 0;
}
__setup("enforcemodulesig", sign_setup);
                        
