/* module-verify.h: module verification definitions
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifdef CONFIG_MODULE_SIG
extern int module_verify_sig(Elf_Ehdr *hdr, Elf_Shdr *sechdrs,
			     const char *secstrings, struct module *mod);
#endif
