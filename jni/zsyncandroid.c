#include <jni.h>
#include <android/log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <utime.h>

#include "zsyncandroid.h"
#include "zsglobal.h"
#include "http.h"
#include "libzsync/zsync.h"


char* temp_dir;
//long long http_down;

//PROTOTYPES
struct zsync_state *read_zsync_control_file(const char *p);
char *get_filename(const struct zsync_state *zs, const char *source_name);
void read_seed_file(struct zsync_state *z, const char *fname);

struct object_holder {
	struct zsync_state *zs;
	char* main_url;
	char* filename;
	char* temp_file_name;
	long long http_down;
};

jlong
Java_com_zsync_android_ZSync_init(JNIEnv* env, jobject thiz, jstring url, jstring jtemp_dir){

	const char* m_url = (*env)->GetStringUTFChars(env, url, 0);
	const char* m_temp_dir = (*env)->GetStringUTFChars(env, jtemp_dir, 0);

	struct object_holder *oh = calloc(sizeof *oh, 1);

	oh->main_url = strdup(m_url);
	temp_dir = strdup(m_temp_dir);

	oh->http_down = 0;
	char *pr = getenv("http_proxy");
	if (pr != NULL){
		set_proxy_from_string(pr);
	}

	(*env)->ReleaseStringUTFChars(env, url, m_url);
	(*env)->ReleaseStringUTFChars(env, jtemp_dir, m_temp_dir);

	return (long)oh;
}

jint
Java_com_zsync_android_ZSync_readControlFile(JNIEnv* env, jobject thiz, jlong lp){
	struct object_holder* oh = (struct oh*)lp;
	oh->zs = read_zsync_control_file(oh->main_url);
	if (oh->zs == NULL){
		return -1;
	} else {
		return 0;
	}
}

jstring
Java_com_zsync_android_ZSync_getOriginName(JNIEnv* env, jobject thiz, jlong lp){
	struct object_holder* oh = (struct oh*)lp;
	char *local_name = get_filename(oh->zs, oh->main_url);
	jstring j_local_name = (*env)->NewStringUTF(env, local_name);
	free(local_name);
	return j_local_name;
}

void
Java_com_zsync_android_ZSync_setFileName(JNIEnv* env, jobject thiz, jstring jfilename, jlong lp){
	struct object_holder* oh = (struct oh*)lp;
	const char* m_filename = (*env)->GetStringUTFChars(env, jfilename, 0);
	oh->filename = strdup(m_filename);
	(*env)->ReleaseStringUTFChars(env, jfilename, m_filename);
}

void
Java_com_zsync_android_ZSync_setTempFileName(JNIEnv* env, jobject thiz, jstring jtempfilename, jlong lp){
	struct object_holder* oh = (struct oh*)lp;
	const char* m_tempfilename = (*env)->GetStringUTFChars(env, jtempfilename, 0);
	oh->temp_file_name = strdup(m_tempfilename);
	(*env)->ReleaseStringUTFChars(env, jtempfilename, m_tempfilename);
}

void
Java_com_zsync_android_ZSync_fetchFromLocal(JNIEnv* env, jobject thiz, jstring jfilename, jlong lp){
	struct object_holder* oh = (struct oh*)lp;
	const char* m_filename = (*env)->GetStringUTFChars(env, jfilename, 0);

	if (zsync_status(oh->zs) < 2){
		read_seed_file(oh->zs, m_filename);
	}

	(*env)->ReleaseStringUTFChars(env, jfilename, m_filename);
}

void
Java_com_zsync_android_ZSync_restartZsyncTempFile(JNIEnv* env, jobject thiz, jlong lp){
	struct object_holder* oh = (struct oh*)lp;
	zsync_rename_file(oh->zs, oh->temp_file_name);
}

jint
Java_com_zsync_android_ZSync_fetchFromUrl(JNIEnv* env, jobject thiz, jlong lp){
	struct object_holder* oh = (struct oh*)lp;
	/* libzsync has been writing to a randomely-named temp file so far -
	 * because we didn't want to overwrite the .part from previous runs. Now
	 * we've read any previous .part, we can replace it with our new
	 * in-progress run (which should be a superset of the old .part - unless
	 * the content changed, in which case it still contains anything relevant
	 * from the old .part). */
	zsync_rename_file(oh->zs, oh->temp_file_name);

	/* And now start the blocks downloading */
	int result = fetch_remaining_blocks(oh);
	if (result == 0){
		return 0;
	} else {
		return -1;
	}
}

jint
Java_com_zsync_android_ZSync_applyDiffs(JNIEnv* env, jobject thiz, jlong lp){
	struct object_holder* oh = (struct oh*)lp;
	int result = zsync_complete(oh->zs);
	if (result == -1){
		return result;
	}
	return 0;
}

jlong
Java_com_zsync_android_ZSync_getZsyncMtime(JNIEnv* env, jobject thiz, jlong lp){
	struct object_holder* oh = (struct oh*)lp;
	time_t mtime = zsync_mtime(oh->zs);
	return mtime;
}

jlong
Java_com_zsync_android_ZSync_release(JNIEnv* env, jobject thiz, jlong lp){
	struct object_holder* oh = (struct oh*)lp;
	if (oh->zs){
		zsync_end(oh->zs);
	}
	if (oh->main_url){
		free(oh->main_url);
	}
	if (oh->filename){
		free(oh->filename);
	}
	if (oh->temp_file_name){
		free(oh->temp_file_name);
	}
	long long int loaded = oh->http_down;
	free(oh);
	//if (temp_dir){free(temp_dir);}
	//if (referer){free(referer);}
	return loaded;
}
/*****************************************************************************************
 *
 *NATIVE CODE*/

/* fetch_remaining_blocks_http(zs, url, type)
 * For the given zsync_state, using the given URL (which is a copy of the
 * actual content of the target file is type == 0, or a compressed copy of it
 * if type == 1), retrieve the parts of the target that are currently missing.
 * Returns true if this URL was useful, false if we crashed and burned.
 */
#define BUFFERSIZE 8192
int fetch_remaining_blocks_http(struct object_holder *oh, const char *url,
                                int type) {
	struct zsync_state *z = oh->zs;
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
    oh->http_down += range_fetch_bytes_down(rf);
    zsync_end_receive(zr);
    range_fetch_end(rf);
    free(u);
    return ret;
}

/* fetch_remaining_blocks(zs)
 * Using the URLs in the supplied zsync state, downloads data to complete the
 * target file.
 */
int fetch_remaining_blocks(struct object_holder *oh) {
    int n, utype;
    struct zsync_state *zs = oh->zs;
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
            int rc = fetch_remaining_blocks_http(oh, tryurl, utype);
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
 * main_url; only http URLs are supported.
 * NOTE: zsync control file is not a target file to sync
 */
struct zsync_state *read_zsync_control_file(const char *p) {
    FILE *f;
    struct zsync_state *zs;
    char *lastpath = NULL;

    f = http_get(p, &lastpath, NULL);
    if (!f){
    	__android_log_write(ANDROID_LOG_ERROR, ZSYNC_ANDROID_TAG, "could not download .zsync control file");
    	return NULL;
    }
    referer = lastpath;

    /* Read the .zsync */
    if ((zs = zsync_begin(f)) == NULL) {
    	__android_log_write(ANDROID_LOG_ERROR, ZSYNC_ANDROID_TAG, "could not read .zsync control file");
    	return NULL;
    }

    /* And close it */
    if (fclose(f) != 0) {
    	__android_log_write(ANDROID_LOG_ERROR, ZSYNC_ANDROID_TAG, "could not close .zsync control file");
    	return NULL;
    }
    return zs;
}

/* str = get_filename_prefix(path_str)
 * Returns a (malloced) string of the alphanumeric leading segment of the
 * filename in the given file path.
 */
const char *get_filename_prefix(const char *p) {
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
