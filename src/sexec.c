/*
 *
 * Copyright (c) 2015, Gregory M. Kurtzer
 * All rights reserved.
 *
 *
 * Copyright (c) 2015, The Regents of the University of California,
 * through Lawrence Berkeley National Laboratory (subject to receipt of
 * any required approvals from the U.S. Dept. of Energy).
 * All rights reserved.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h> 
#include <string.h>
#include <fcntl.h>  
#include <grp.h>
#include "config.h"

int main(int argc, char **argv) {
    char *sappdir;
    char *singularitypath;
    struct stat sappdirstat;
    struct stat singularitystat;
    int cwd_fd;
    mode_t process_mask = umask(0);
    uid_t uid = getuid();
    gid_t gid = getgid();


    // We don't run as root...
    if ( uid == 0 || gid == 0 ) {
        fprintf(stderr, "ERROR: Do not run singularities as root!\n");
        return(255);
    }

    // Lets start off as the right user.
    if ( seteuid(uid) != 0 ) {
        fprintf(stderr, "ERROR: Could not set effective user privledges to %d!\n", uid);
        return(255);
    }

    // Open a FD to the current working dir.
    if ( (cwd_fd = open(".", O_RDONLY)) < 0 ) {
        fprintf(stderr, "ERROR: Could not open cwd fd (%s)!\n", strerror(errno));
        return(1);
    }

    // Get sappdir from the environment (we check on this shortly)
    sappdir = getenv("SAPPCONTAINER");


    /*
     * Sanity Checks, exit if any don't match.
     */

    // Make sure SAPPCONTAINER is defined
    if ( sappdir == NULL ) {
        fprintf(stderr, "ERROR: SAPPCONTAINER undefined!\n");
        return(1);
    }

    // Check SAPPCONTAINER
    if (lstat(sappdir, &sappdirstat) < 0) {
        fprintf(stderr, "ERROR: Could not stat %s!\n", sappdir);
        return(1);
    }
    if ( ! S_ISDIR(sappdirstat.st_mode) ) {
        fprintf(stderr, "ERROR: SAPPCONTAINER (%s) must be a SAPP directory!\n", sappdir);
        return(1);
    }
    if ( uid != (int)sappdirstat.st_uid ) {
        fprintf(stderr, "ERROR: Will not execute in a SAPPCONTAINER you don't own. (%s:%d)!\n", sappdir, (int)sappdirstat.st_uid);
        return(255);
    }

    // Check the singularity within the SAPPCONTAINER
    singularitypath = (char *) malloc(strlen(sappdir) + 13);
    snprintf(singularitypath, strlen(sappdir) + 13, "%s/singularity", sappdir);
    if (stat(singularitypath, &singularitystat) < 0) {
        fprintf(stderr, "ERROR: Could not stat %s!\n", singularitypath);
        return(1);
    }
    if ( ! S_ISREG(singularitystat.st_mode) ) {
        fprintf(stderr, "ERROR: The singularity is not found in SAPPCONTAINER!\n");
        return(1);
    }
    if ( uid != (int)singularitystat.st_uid ) {
        fprintf(stderr, "ERROR: Will not execute a singularity you don't own. (%d)!\n", (int)sappdirstat.st_uid);
        return(255);
    }
    if ( ! (S_IXUSR & singularitystat.st_mode) ) {
        fprintf(stderr, "ERROR: The singularity can not be executed!\n");
        return(1);
    }


    /*
     * Warning! Danger! Entering the privledged zone!
     */

    // Get root
    if ( seteuid(0) != 0 ) {
        fprintf(stderr, "ERROR: Could not escalate privledges!\n");
        return(1);
    }

    // Do the chroot
    if ( chroot(sappdir) != 0 ) {
        fprintf(stderr, "ERROR: failed enter SAPPCONTAINER: %s\n", sappdir);
        return(255);
    }

    // Failure is acceptable here
    mkdir("/dev", 0755);
    chown("/dev", uid, gid);
    mknod("/dev/null", S_IFCHR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, makedev(1,3));
    chown("/dev/null", uid, gid);
    mknod("/dev/random", S_IFCHR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, makedev(1,8));
    chown("/dev/random", uid, gid);
    mknod("/dev/urandom", S_IFCHR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, makedev(1,9));
    chown("/dev/urandom", uid, gid);
    umask(process_mask);

    // Dump all privs
    if ( setregid(gid, gid) != 0 ) {
        fprintf(stderr, "ERROR: Could not dump real/effective group privledges!\n");
        return(255);
    }
    if ( setreuid(uid, uid) != 0 ) {
        fprintf(stderr, "ERROR: Could not dump real/effective user privledges!\n");
        return(255);
    }



    /*
     * Out of the immediate danger zone... whew!
     */

    // Confirm we no longer have any escalated privledges
    if ( setuid(0) == 0 ) {
        fprintf(stderr, "ERROR: Root not allowed here!\n");
        return(1);
    }

    // change directory back to starting point
    if ( fchdir(cwd_fd) != 0 ) {
        fprintf(stderr, "ERROR: Could not fchdir!\n");
        return(1);
    }
    if ( close(cwd_fd) != 0 ) {
        fprintf(stderr, "ERROR: Could not close cwd_fd!\n");
        return(1);
    }

    // Exec the singularity
    if ( execv("/singularity", argv) != 0 ) {
        fprintf(stderr, "ERROR: Failed to exec SAPP envrionment\n");
        return(2);
    }

    // We should *never* reach here, but if we do... error out hard!
    fprintf(stderr, "ERROR: We should not be here!\n");
    return(255);
}
