


#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <bfd.h>

/* This should be set in a Makefile somehow */
#define TARGET "i686-pc-linux-gnu"

static FILE *out;

static void dump_data(bfd *abfd)
{
	asection *section;
	bfd_byte *data = 0;
	bfd_size_type size;
	bfd_size_type addr_offset;
	bfd_size_type stop_offset;
	unsigned int opb = bfd_octets_per_byte(abfd);
	unsigned int cksum;

	for (section = abfd->sections; section != NULL; section = section->next) {
		if (section->flags & SEC_HAS_CONTENTS) {
			if (bfd_section_size(abfd, section) == 0)
				continue;

			/* We only care about sections with "text" or "data" in their names */
			if ((strstr(section->name, "text") == NULL) &&
			    (strstr(section->name, "data") == NULL))
				continue;

			cksum = 0;

			size = bfd_section_size(abfd, section) / opb;

			printf("Contents of section %s size %lu", section->name, size);

			data = (bfd_byte *) malloc(size);

			bfd_get_section_contents(abfd, section, (PTR) data, 0, size);

			stop_offset = size;

			printf(" idata %02x%02x%02x%02x", data[0], data[1], data[2], data[3]);

			for (addr_offset = 0; addr_offset < stop_offset; ++addr_offset) {
				cksum += (unsigned int) data[addr_offset];
				fputc(data[addr_offset], out);
			}
			free (data);

			printf(" checksum %08x\n", cksum);
		}
	}
}

void set_default_bfd_target(void)
{
	const char *target = TARGET;

	if (!bfd_set_default_target(target))
		fprintf(stderr, "can't set BFD default target to `%s': %s", target, bfd_errmsg (bfd_get_error ()));
}

int main (int argc, char *argv[])
{
	char *in_file;
	char *out_file;
	bfd *file;
	char **matching;

	if (argc != 3) {
		fprintf(stderr, "%s [infile] [outfile]\n", argv[0]);
		exit(1);
	}

	in_file = argv[1];
	out_file = argv[2];

	bfd_init();
	set_default_bfd_target();


//	file = bfd_openr(in_file, "elf32-i386");
	file = bfd_openr(in_file, NULL);
	if (file == NULL) {
		fprintf(stderr, "error \"%s\" trying to open %s\n", strerror(errno), in_file);
		exit(1);
	}

	out = fopen(out_file, "w");
	if (out == NULL) {
		fprintf(stderr, "error \"%s\" trying to create %s\n", strerror(errno), out_file);
		exit(1);
	}

	if (bfd_check_format_matches(file, bfd_object, &matching)) {
		dump_data (file);
	}

	fclose(out);

	return 0;
}
