#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <cuetools/cuefile.h>
#include <cuetools/cd.h>

int cueformat = CUE;

#define BLOCKSIZE (1024 * 1024)

Cd *getCdFromCueFile(char *name);
int updateStartsAndLengths(Cd *cd);
int makeBin(Cd *cd, const char *name);
void updateFileNames(Cd *cd, char *binname);
long bytesToFrames(int type, off_t bytes);

int main(int argc, char **argv) {
    Cd *cd;

    if(argc < 4) {
        fprintf(stderr, "USAGE: multi2singlecue <in-cue> <out-cue> <out-bin>\n");
        exit(EXIT_FAILURE);
    }

    cd = getCdFromCueFile(argv[1]);
    if(cd == NULL) {
        fprintf(stderr, "Failed to load cue file.\n");
        exit(EXIT_FAILURE);
    }

    if(updateStartsAndLengths(cd) != 0) {
        fprintf(stderr, "A problem occured while updating tracks.\n");
        exit(EXIT_FAILURE);
    }

    if(makeBin(cd, argv[3]) != 0) {
        fprintf(stderr, "Failed to make single bin.\n");
        exit(EXIT_FAILURE);
    }

    updateFileNames(cd, argv[3]);

    if(cf_print(argv[2], &cueformat, cd) != 0) {
        fprintf(stderr, "Failed to write cue sheet %s.", argv[2]);
        exit(EXIT_FAILURE);
    }

    return(EXIT_SUCCESS);
}

Cd *getCdFromCueFile(char *name) {
    Cd *cd;

    cd = cf_parse(name, &cueformat);
    if(cd == NULL) {
        fprintf(stderr, "Failed to parse %s.\n", name);
        return(NULL);
    }

    return(cd);
}

int updateStartsAndLengths(Cd *cd) {
    int tracks, i;
    off_t length;
    long start;
    FILE *inbin;
    Track *t;
    char *name;

    tracks = cd_get_ntrack(cd);
    start = 0;

    for(i = 0; i < tracks; i++) {
        t = cd_get_track(cd, i + 1); /* 1 indexed */
        name = track_get_filename(t);
        inbin = fopen(name, "rb");
        if(inbin == NULL) {
            fprintf(stderr, "Failed to open %s for reading: %s\n", name, strerror(errno));
            goto error0;
        }
        if(fseeko(inbin, 0, SEEK_END) == -1) {
            fprintf(stderr, "Failed to seek to end of %s: %s\n", name, strerror(errno));
            goto error1;
        }
        length = ftello(inbin);
        if(length == -1) {
            fprintf(stderr, "Failed to get position in %s: %s\n", name, strerror(errno));
            goto error1;
        }
        fclose(inbin);
        track_set_start(t, bytesToFrames(track_get_mode(t), start));
        track_set_length(t, bytesToFrames(track_get_mode(t), length));
        start += length;
    }

    return(0);

error1:
    fclose(inbin);
error0:
    return(-1);
}

int makeBin(Cd *cd, const char *name) {
    int tracks, i;
    Track *t;
    char *inname;
    FILE *inbin, *outbin;
    char *buffer;
    size_t dataread;

    buffer = malloc(BLOCKSIZE);
    if(buffer == NULL) {
        fprintf(stderr, "Couldn't allocate memory.\n");
        goto error0;
    }

    outbin = fopen(name, "wb");
    if(outbin == NULL) {
        fprintf(stderr, "Failed open %s for writing: %s\n", name, strerror(errno));
        goto error1;
    }

    tracks = cd_get_ntrack(cd);
    for(i = 0; i < tracks; i++) {
        t = cd_get_track(cd, i + 1); /* 1 indexed */
        inname = track_get_filename(t);

        inbin = fopen(inname, "rb");
        if(inbin == NULL) {
            fprintf(stderr, "Failed open %s for reading: %s\n", inname, strerror(errno));
            goto error2;
        }

        for(;;) {
            dataread = fread(buffer, 1, BLOCKSIZE, inbin);
            if(dataread < BLOCKSIZE) {
                if(feof(inbin)) {
                    if(dataread > 0) {
                        if(fwrite(buffer, 1, dataread, outbin) < dataread) {
                            fprintf(stderr, "Failed to write to %s.\n", name);
                            goto error3;
                        }
                    }
                    break;
                } else {
                    fprintf(stderr, "Failed to read %s.\n", inname);
                    goto error3;
                }
            }

            if(fwrite(buffer, 1, dataread, outbin) < dataread) {
                fprintf(stderr, "Failed to write to %s.\n", name);
                goto error3;
            }
        }

        fclose(inbin);
    }

    fclose(outbin);
    free(buffer);
    return(0);

error3:
    fclose(inbin);
error2:
    fclose(outbin);
error1:
    free(buffer);
error0:
    return(-1);
}

void updateFileNames(Cd *cd, char *binname) {
    int tracks, i;
    Track *t;

    tracks = cd_get_ntrack(cd);

    for(i = 0; i < tracks; i++) {
        t = cd_get_track(cd, i + 1); /* 1 indexed */
        track_set_filename(t, binname);
    }
}

long bytesToFrames(int type, off_t bytes) {
    switch(type) {
        case MODE_MODE1:
        case MODE_MODE2_FORM1:
            return(bytes / 2048);
        case MODE_MODE2_FORM2:
            return(bytes / 2324);
        case MODE_MODE2_FORM_MIX:
            return(bytes / 2332);
        case MODE_MODE2:
            return(bytes / 2336);
        case MODE_AUDIO:
        case MODE_MODE1_RAW:
        case MODE_MODE2_RAW:
            return(bytes / 2352);
        default:
            return(0);
    }
}
