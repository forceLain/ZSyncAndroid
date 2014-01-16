#include <jni.h>
#include <android/log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <utime.h>

#include "zsyncandroid.h"
#include "zsglobal.h"
#include "http.h"
#include "libzsync/zsync.h"
#include "url.h"

const char* absolute_path;

jint
Java_com_zsync_android_ZSync_dosync(JNIEnv* env, jobject thiz, jstring url, jstring path){

	//Get the native string from javaString
	const char *c_url = (*env)->GetStringUTFChars(env, url, 0);
	absolute_path = (*env)->GetStringUTFChars(env, path, 0); //file path for work and store output files

	int code = doSync(c_url);

	//DON'T FORGET THIS LINE
	(*env)->ReleaseStringUTFChars(env, url, c_url);
	(*env)->ReleaseStringUTFChars(env, path, absolute_path);
	return code;
}
/*****************************************************************************************
 *
 *NATIVE CODE*/

/* A ptrlist is a very simple structure for storing lists of pointers. This is
 * the only function in its API. The structure (not actually a struct) consists
 * of a (pointer to a) void*[] and an int giving the number of entries.
 *
 * ptrlist = append_ptrlist(&entries, ptrlist, new_entry)
 * Like realloc(2), this returns the new location of the ptrlist array; the
 * number of entries is passed by reference and updated in place. The new entry
 * is appended to the list.
 */
static void **append_ptrlist(int *n, void **p, void *a) {
    if (!a)
        return p;
    p = realloc(p, (*n + 1) * sizeof *p);
    if (!p) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    p[*n] = a;
    (*n)++;
    return p;
}

static int set_mtime(char* filename, time_t mtime) {
    struct stat s;
    struct utimbuf u;

    /* Get the access time, which I don't want to modify. */
    if (stat(filename, &s) != 0) {
        perror("stat");
        return -1;
    }

    /* Set the modification time. */
    u.actime = s.st_atime;
    u.modtime = mtime;
    if (utime(filename, &u) != 0) {
        perror("utime");
        return -1;
    }
    return 0;
}

/* fetch_remaining_blocks_http(zs, url, type)
 * For the given zsync_state, using the given URL (which is a copy of the
 * actual content of the target file is type == 0, or a compressed copy of it
 * if type == 1), retrieve the parts of the target that are currently missing.
 * Returns true if this URL was useful, false if we crashed and burned.
 */
#define BUFFERSIZE 8192
int fetch_remaining_blocks_http(struct zsync_state *z, const char *url,
                                int type) {
    int ret = 0;
    struct range_fetch *rf;
    unsigned char *buf;
    struct zsync_receiver *zr;

    /* URL might be relative - we need an absolute URL to do a fetch */
    char *u = make_url_absolute(referer, url);
    if (!u) {
    	__android_log_write(ANDROID_LOG_ERROR, ZSYNC_ANDROID_TAG, "URL from the .zsync file is relative, but I don't know the referer URL");
        return -1;
    }

    /* Start a range fetch and a zsync receiver */
    rf = range_fetch_start(u);
    if (!rf) {
        free(u);
        return -1;
    }
    zr = zsync_begin_receive(z, type);
    if (!zr) {
        range_fetch_end(rf);
        free(u);
        return -1;
    }

    //downloading...

    /* Create a read buffer */
    buf = malloc(BUFFERSIZE);
    if (!buf) {
        zsync_end_receive(zr);
        range_fetch_end(rf);
        free(u);
        return -1;
    }

    /* Get a set of byte ranges that we need to complete the target */
	int nrange;
	off_t *zbyterange = zsync_needed_byte_ranges(z, &nrange, type);
	if (!zbyterange)
		return 1;
	if (nrange == 0)
		return 0;

	/* And give that to the range fetcher */
	range_fetch_addranges(rf, zbyterange, nrange);
	free(zbyterange);

	int len;
	off_t zoffset;

	/* Loop while we're receiving data, until we're done or there is an error */
	while (!ret
		   && (len = get_range_block(rf, &zoffset, buf, BUFFERSIZE)) > 0) {
		/* Pass received data to the zsync receiver, which writes it to the
		 * appropriate location in the target file */
		if (zsync_receive_data(zr, buf, zoffset, len) != 0)
			ret = 1;

		// Needed in case next call returns len=0 and we need to signal where the EOF was.
		zoffset += len;
	}

	/* If error, we need to flag that to our caller */
	if (len < 0)
		ret = -1;
	else    /* Else, let the zsync receiver know that we're at EOF; there
			 *could be data in its buffer that it can use or needs to process */
		zsync_receive_data(zr, NULL, zoffset, 0);

    /* Clean up */
    free(buf);
    zsync_end_receive(zr);
    range_fetch_end(rf);
    free(u);
    return ret;
}

/* fetch_remaining_blocks(zs)
 * Using the URLs in the supplied zsync state, downloads data to complete the
 * target file.
 */
int fetch_remaining_blocks(struct zsync_state *zs) {
    int n, utype;
    const char *const *url = zsync_get_urls(zs, &n, &utype);
    int *status;        /* keep status for each URL - 0 means no error */
    int ok_urls = n;

    if (!url) {
        fprintf(stderr, "no URLs available from zsync?");
        return 1;
    }
    status = calloc(n, sizeof *status);

    /* Keep going until we're done or have no useful URLs left */
    while (zsync_status(zs) < 2 && ok_urls) {
        /* Still need data; pick a URL to use. */
        int try = rand() % n;

        if (!status[try]) {
            const char *tryurl = url[try];

            /* Try fetching data from this URL */
            int rc = fetch_remaining_blocks_http(zs, tryurl, utype);
            if (rc != 0) {
                fprintf(stderr, "failed to retrieve from %s\n", tryurl);
                status[try] = 1;
                ok_urls--;
            }
        }
    }
    free(status);
    return 0;
}

/* FILE* f = open_zcat_pipe(file_str)
 * Returns a (popen) filehandle which when read returns the un-gzipped content
 * of the given file. Or NULL on error; or the filehandle may fail to read. It
 * is up to the caller to call pclose() on the handle and check the return
 * value of that.
 */
FILE* open_zcat_pipe(const char* fname)
{
    /* Get buffer to build command line */
    char *cmd = malloc(6 + strlen(fname) * 2);
    if (!cmd)
        return NULL;

    strcpy(cmd, "zcat ");
    {   /* Add filename to commandline, escaping any characters that the shell
         *might consider special. */
        int i, j;

        for (i = 0, j = 5; fname[i]; i++) {
            if (!isalnum(fname[i]))
                cmd[j++] = '\\';
            cmd[j++] = fname[i];
        }
        cmd[j] = 0;
    }

   /* Finally, open the subshell for reading, and return the handle */
	FILE* f = popen(cmd, "r");
	free(cmd);
	return f;
}

void read_seed_file(struct zsync_state *z, const char *fname) {
    /* If we should decompress this file */
    if (zsync_hint_decompress(z) && strlen(fname) > 3
        && !strcmp(fname + strlen(fname) - 3, ".gz")) {
        /* Open for reading */
        FILE *f = open_zcat_pipe(fname);
        if (!f) {
            perror("popen");
            fprintf(stderr, "not using seed file %s\n", fname);
        }
        else {

            /* Give the contents to libzsync to read and find any useful
             * content */
            zsync_submit_source_file(z, f, 0);

            /* Close and check for errors */
            if (pclose(f) != 0) {
                perror("close");
            }
        }
    }
    else {
        /* Simple uncompressed file - open it */
        FILE *f = fopen(fname, "r");
        if (!f) {
            perror("open");
            fprintf(stderr, "not using seed file %s\n", fname);
        }
        else {

            /* Give the contents to libzsync to read, to find any content that
             * is part of the target file. */
            zsync_submit_source_file(z, f, 0);

            /* And close */
            if (fclose(f) != 0) {
                perror("close");
            }
        }
    }

   /* And print how far we've progressed towards the target file */
	long long done, total;
	zsync_progress(z, &done, &total);
}

/* zs = read_zsync_control_file(location_str)
 * Reads a zsync control file from a URL specified in
 * location_str; only http URLs are supported.
 * NOTE: zsync control file is not a target file to sync
 */
struct zsync_state *read_zsync_control_file(const char *p) {
    FILE *f;
    struct zsync_state *zs;
    char *lastpath = NULL;

	/* No such local file - if not a URL either, report error */
	if (!is_url_absolute(p)) {
		__android_log_write(ANDROID_LOG_ERROR, ZSYNC_ANDROID_TAG, "Bad URL");
		return NULL;
	}

	/* Try URL fetch */
	f = http_get(p, &lastpath, NULL);
	if (!f) {
		__android_log_write(ANDROID_LOG_ERROR, ZSYNC_ANDROID_TAG, "could not read control file from URL");
		return NULL;
	}
	referer = lastpath;

    /* Read the .zsync */
    if ((zs = zsync_begin(f)) == NULL) {
    	__android_log_write(ANDROID_LOG_ERROR, ZSYNC_ANDROID_TAG, "could not read .zsync");
    	return NULL;
    }

    /* And close it */
    if (fclose(f) != 0) {
    	__android_log_write(ANDROID_LOG_ERROR, ZSYNC_ANDROID_TAG, "could not close .zsync");
    	return NULL;
    }
    return zs;
}

/* str = get_filename_prefix(path_str)
 * Returns a (malloced) string of the alphanumeric leading segment of the
 * filename in the given file path.
 */
static char *get_filename_prefix(const char *p) {
    char *s = strdup(p);
    char *t = strrchr(s, '/');
    char *u;

    if (t)
        *t++ = 0;
    else
        t = s;
    u = t;
    while (isalnum(*u)) {
        u++;
    }
    *u = 0;
    if (*t > 0)
        t = strdup(t);
    else
        t = NULL;
    free(s);
    return t;
}

/* filename_str = get_filename(zs, source_filename_str)
 * Returns a (malloced string with a) suitable filename for a zsync download,
 * using the given zsync state and source filename strings as hints. */
char *get_filename(const struct zsync_state *zs, const char *source_name) {
    char *p = zsync_filename(zs);
    char *filename = NULL;

    if (p) {
        if (strchr(p, '/')) {
            fprintf(stderr,
                    "Rejected filename specified in %s, contained path component.\n",
                    source_name);
            free(p);
        }
        else {
            char *t = get_filename_prefix(source_name);

            if (t && !memcmp(p, t, strlen(t)))
                filename = p;
            else
                free(p);

            if (t && !filename) {
                fprintf(stderr,
                        "Rejected filename specified in %s - prefix %s differed from filename %s.\n",
                        source_name, t, p);
            }
            free(t);
        }
    }
    if (!filename) {
        filename = get_filename_prefix(source_name);
        if (!filename)
            filename = strdup("zsync-download");
    }

    return filename;
}

/* the main function */
int doSync(char* url){
	struct zsync_state *zs;
	char *temp_file = NULL;
	char **seedfiles = NULL;
	int nseedfiles = 0;
	char *filename = NULL;
	long long local_used;
	char *zfname = NULL;
	time_t mtime;
	char *remotefile = url;


	/* Get proxy setting from the environment */
	char *pr = getenv("http_proxy");
	if (pr != NULL) {set_proxy_from_string(pr);}

	/* STEP 1: Read the zsync control file */
	if ((zs = read_zsync_control_file(remotefile)) == NULL){
		__android_log_write(ANDROID_LOG_ERROR, ZSYNC_ANDROID_TAG, "could not read zsync control file");
		return -1;
	}

	/* Get eventual filename for output, and filename to write to while working */
	char *local_name = get_filename(zs, remotefile);
	filename = malloc(sizeof(absolute_path)+sizeof(local_name));
	strcpy(filename, absolute_path);
	strcat(filename, local_name);
	temp_file = malloc(strlen(filename)+6);
	strcpy(temp_file, filename);
	strcat(temp_file, ".part");


	/* STEP 2: read available local data and fill in what we know in the
	 *target file */
	int i;

	/* If the target file already exists, we're probably updating that file
	 * - so it's a seed file */
	if (!access(filename, R_OK)) {
		seedfiles = append_ptrlist(&nseedfiles, seedfiles, filename);
	}
	/* If the .part file exists, it's probably an interrupted earlier
	 * effort; a normal HTTP client would 'resume' from where it got to,
	 * but zsync can't (because we don't know this data corresponds to the
	 * current version on the remote) and doesn't need to, because we can
	 * treat it like any other local source of data. Use it now. */
	if (!access(temp_file, R_OK)) {
		seedfiles = append_ptrlist(&nseedfiles, seedfiles, temp_file);
	}

	/* Try any seed files supplied by the command line */
	for (i = 0; i < nseedfiles; i++) {
		int dup = 0, j;

		/* And stop reading seed files once the target is complete. */
		if (zsync_status(zs) >= 2) break;

		/* Skip dups automatically, to save the person running the program
		 * having to worry about this stuff. */
		for (j = 0; j < i; j++) {
			if (!strcmp(seedfiles[i],seedfiles[j])) dup = 1;
		}

		/* And now, if not a duplicate, read it */
		if (!dup)
			read_seed_file(zs, seedfiles[i]);
	}
	/* Show how far that got us */
	zsync_progress(zs, &local_used, NULL);


	/* libzsync has been writing to a randomely-named temp file so far -
	 * because we didn't want to overwrite the .part from previous runs. Now
	 * we've read any previous .part, we can replace it with our new
	 * in-progress run (which should be a superset of the old .part - unless
	 * the content changed, in which case it still contains anything relevant
	 * from the old .part). */
	if (zsync_rename_file(zs, temp_file) != 0) {
		__android_log_write(ANDROID_LOG_ERROR, ZSYNC_ANDROID_TAG, "could not rename zsync file");
		return -1;
	}

	/* STEP 3: fetch remaining blocks via the URLs from the .zsync */
	if (fetch_remaining_blocks(zs) != 0) {
		__android_log_write(ANDROID_LOG_ERROR, ZSYNC_ANDROID_TAG, "failed to retrieve all remaining blocks - no valid download URLs remain");
		return -1;
	}

	/* STEP 4: verify download */
	int r;
	r = zsync_complete(zs);
	if (r == -1){
		__android_log_write(ANDROID_LOG_ERROR, ZSYNC_ANDROID_TAG, "Aborting, download available in temp_file");
		return -1;
	}

	free(temp_file);

	/* Get any mtime that we is suggested to set for the file, and then shut
	 * down the zsync_state as we are done on the file transfer. Getting the
	 * current name of the file at the same time. */
	mtime = zsync_mtime(zs);
	temp_file = zsync_end(zs);

	/* STEP 5: Move completed .part file into place as the final target */
	if (filename) {
		char *oldfile_backup = malloc(strlen(filename) + 8);
		int ok = 1;

		strcpy(oldfile_backup, filename);
		strcat(oldfile_backup, ".zs-old");

		if (!access(filename, F_OK)) {
			/* Backup the old file. */
			/* First, remove any previous backup. We don't care if this fails -
			 * the link below will catch any failure */
			unlink(oldfile_backup);

			/* Try linking the filename to the backup file name, so we will
			   atomically replace the target file in the next step.
			   If that fails due to EPERM, it is probably a filesystem that
			   doesn't support hard-links - so try just renaming it to the
			   backup filename. */
			if (link(filename, oldfile_backup) != 0
				&& (errno != EPERM || rename(filename, oldfile_backup) != 0)) {
				perror("linkname");
				fprintf(stderr,
						"Unable to back up old file %s - completed download left in %s\n",
						filename, temp_file);
				ok = 0;         /* Prevent overwrite of old file below */
			}
		}
		if (ok) {
			/* Rename the file to the desired name */
			if (rename(temp_file, filename) == 0) {
				/* final, final thing - set the mtime on the file if we have one */
				if (mtime != -1) set_mtime(filename, mtime);
			}
			else {
				perror("rename");
				fprintf(stderr,
						"Unable to back up old file %s - completed download left in %s\n",
						filename, temp_file);
			}
		}
		free(oldfile_backup);
		free(filename);
	}
	else {
		printf
			("No filename specified for download - completed download left in %s\n",
			 temp_file);
	}

	if (local_name){
		free(local_name);
	}

	/* Final stats and cleanup */
	free(referer);
	free(temp_file);
	return 0;
}
