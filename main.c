/*
 * main.c
 * FuseHFS
 *
 * Created by Zydeco on 27/2/2010.
 * Copyright 2010 namedfork.net. All rights reserved.
 *
 * Edited by Joel Cretan 7/19/2014
 * Still licensed under GPLv2: https://www.gnu.org/licenses/gpl-2.0.html
 */
#define FUSE_USE_VERSION 31
#define DEBUG 1

#include <fuse/fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <iconv.h>
#include <libhfs/hfs.h>
#include "fusefs_hfs.h"

#define FUSEHFS_VERSION "1"

//#define DEBUG

extern struct fuse_operations FuseHFS_operations;
extern int log_to_file();
extern void log_invoking_command(int argc, char *argv[]);
extern void log_fuse_call(struct fuse_args *args);


struct fusehfs_options options = {
    .path =         NULL,
    .encoding =		NULL,
	.readonly =		0
};

enum {
	KEY_VERSION,
	KEY_HELP,
	KEY_ENCODING,
	KEY_READONLY,
};

static struct fuse_opt FuseHFS_opts[] = {
	FUSE_OPT_KEY("-V",			KEY_VERSION),
	FUSE_OPT_KEY("--version",	KEY_VERSION),
	FUSE_OPT_KEY("-h",			KEY_HELP),
	FUSE_OPT_KEY("--help",		KEY_HELP),
	FUSE_OPT_KEY("--encoding=",	KEY_ENCODING),
	FUSE_OPT_KEY("--readonly",	KEY_READONLY),
	FUSE_OPT_END
};

static int FuseHFS_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {
	switch (key) {
		case FUSE_OPT_KEY_NONOPT:
			if (options.path == NULL) {
				options.path = strdup(arg);
				return 0;
			}
			return 1;
		case KEY_ENCODING:
			if (options.encoding == NULL) {
				options.encoding = strdup(arg+11);
				return 0;
			}
			return 1;
		case KEY_VERSION:
			printf("FuseHFS %s, (c)2010 namedfork.net namedfork.net\n", FUSEHFS_VERSION);
			exit(1);
		case KEY_HELP:
			printf("usage: fusefs_hfs [fuse options] device mountpoint\n");
			exit(0);
		case KEY_READONLY:
			options.readonly = 1;
			return 0;
	}
	return 0;
}

char * iconv_convert(const char *src, const char *from, const char *to) {
	size_t inb = strlen(src);
	size_t outb = inb*4;
	char *out = malloc(outb+1);
	char *outp = out;
	
	// allocate conversion descriptor
	iconv_t cd = iconv_open(from, to);
	if (cd == (iconv_t)-1) return NULL;
	
	// convert
	if (iconv(cd, (char **restrict)&src, &inb, &outp, &outb) == (size_t)-1) {
		iconv_close(cd);
		free(out);
		return NULL;
	}
	*outp = '\0';
	
	// The End
	iconv_close(cd);
	return out;
}

#define ROOT_UID 0
static bool is_root() {
    int euid = geteuid();
    return euid == ROOT_UID;
}


int main(int argc, char* argv[], char* envp[], char** exec_path) {
#ifdef DEBUG
	int log = log_to_file();
    log_invoking_command(argc, argv);
#else
    log_to_file();
#endif
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	
	bzero(&options, sizeof options);
	if (fuse_opt_parse(&args, NULL, FuseHFS_opts, FuseHFS_opt_proc)) return 1;
	options.mountpoint = strdup(argv[1]);
	if (options.encoding == NULL) options.encoding = strdup("Macintosh");
	
	// mount volume
	hfsvolent vstat;
	int mode = options.readonly?HFS_MODE_RDONLY:HFS_MODE_ANY;
        fprintf(stderr,"options.path [%s] mode = %d\n",options.path,mode);
	if (NULL == hfs_mount(options.path, 0, mode)) {
	  if (NULL == hfs_mount(options.path, 1, mode)) {
		perror("ahfs_mount");
		return 1;
	  }
	}
	if (-1 == hfs_vstat(NULL, &vstat)) {
		perror("hfs_vstat");
		return 1;
	}
	
	// is it read-only?
	if (vstat.flags & HFS_ISLOCKED) {
		options.readonly = 1;
		fuse_opt_add_arg(&args, "-oro");
	}
	fuse_opt_add_arg(&args, "-s");
	hfs_umount(NULL);

	// MacFUSE options
    char volnameOption[128] = "-ovolname=";
	char *volname = iconv_convert(vstat.name, options.encoding, "UTF-8");
	if (volname == NULL) {
		perror("iconv");
		return 1;
	}
    strcpy(volnameOption+10, volname);
	free(volname);
    //fuse_opt_add_arg(&args, volnameOption);
    fuse_opt_add_arg(&args, "-s");
    //fuse_opt_add_arg(&args, "-ofstypename=hfs");
    if (is_root()) fuse_opt_add_arg(&args, "-oallow_other"); // this option requires privileges
    //fuse_opt_add_arg(&args, "-odefer_permissions");
    char *fsnameOption = malloc(strlen(options.path)+10);
    strcpy(fsnameOption, "-ofsname=");
    strcat(fsnameOption, options.path);
    fuse_opt_add_arg(&args, fsnameOption);
    //free(fsnameOption);
    //fuse_opt_add_arg(&args, "-debug");
    fuse_opt_add_arg(&args, "-d");
    fuse_opt_add_arg(&args, "-f");
    //fuse_opt_add_arg(&args, "-olocal"); // experimental option. See: https://code.google.com/p/macfuse/wiki/OPTIONS
     
	// run fuse
#ifdef DEBUG
    log_fuse_call(&args);
#endif
        int ret = fuse_main(args.argc, args.argv, &FuseHFS_operations, &options);
        fprintf(stderr,"fuse_main returned %d\n",ret);
	
	//free(options.path);
	//free(options.encoding);
	//fuse_opt_free_args(&args);
#ifdef DEBUG
    char *macfuse_mode = getenv("OSXFUSE_MACFUSE_MODE");
    dprintf(log, "MacFUSE mode: %s\n", macfuse_mode);
    dprintf(log, "Quitting fusefs_hfs, returning %d\n\n", ret);
    fflush(stdout);
#endif
    return ret;
}
