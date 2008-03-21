/* enter_admin.c	Vsys script to switch a vserver into admin mode in which it has access 
 * 			to the Internet. Install in /vsys and invoke as echo $$ > /vsys/enter_admin.in
 * 			from within the slice.
 * 			3/21/2008	Sapan Bhatia
 */

#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifndef CLONE_NEWNET
#define CLONE_NEWNET    0x40000000      /* New network namespace (lo, device, names sockets, etc) */
#endif

#define 	__NR_set_space 		327
#define 	PATHLEN 		1024

int set_space(int pid, int id, int toggle, unsigned long unshare_flags) { 
	        return syscall(__NR_set_space, pid, id, toggle, unshare_flags);
}

int get_slice_xid(char *slice_name) {
	char slicepath[PATHLEN];
	FILE *fp;
	int xid;
	snprintf(slicepath, sizeof(slicepath), "/etc/vservers/%s/context");

	if ((fp = fopen(slicepath, "r")) == NULL) {
		printf("Could not open %s\n", slicepath);	
		return -1;
	}

	if (fscanf(fp, "%d", &xid)==0) {
		printf("Could not read ctx file\n");
		return -1;
	}

	fclose (fp);
	return xid;
}

int verify_ownership(int pid, int arg_xid) {
	char procpath[PATHLEN];
	FILE *fp;
	int xid;
	snprintf(procpath, sizeof(procpath), "/proc/%d/vinfo");

	if ((fp = fopen(procpath, "r")) == NULL) {
		printf("Could not open %s\n", procpath);	
		return -1;
	}

	if (fscanf(fp, "XID: %d", &xid)==0) {
		printf("Could not read ctx file\n");
		return -1;
	}

	fclose (fp);
	return (arg_xid==xid);

}

int main(int argc, char *argv[]) {
	int xid;
	int pid;

	if (argc < 1) {
		printf("Slice name missing. Was I invoked by vsys?\n");
		exit(1);
	}

	scanf("%d",&pid);

	if ((xid = get_slice_xid(argv[1]))==-1) {
		printf("Could not get xid for slice %s\n",argv[1]);
		exit(1);
	}

	if (!verify_ownership(pid, xid)) {
		printf("Does xid %d really own %d?\n",xid,pid);
		exit(1);
	}

	set_space(pid, xid, 0, CLONE_NEWNET);
}
