#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))

void usage(char *self) {
	fprintf(stderr, "Clip Studio Paint thumbnail file extractor\n"\
			"Usage: %s [-ofilename | -o filename] [-x] [-q] filename\n\n"\
			"-o: filename for output PNG\n"\
			"-x: extract external data objects\n"\
			"-q: do not print messages to STDERR while extracting\n"\
			"-p: accept input from a pipe, or the terminal\n", self);
}

int main(int argc, char** argv) {
	char *filename = NULL;
	char** curarg = &argv[1];
	ssize_t readamt;

	/* argument flags */
	bool extract = false;
	bool quiet = false;
	bool pipe = false;
	char* outfilename = "output.png";

	int infd = 0;
	int outfd;

	char type[8];
	uint64_t len;
	struct iovec record[2] = { {type, 8}, {&len, 8} };

	char dbfile[] = "cspsqlXXXXXX.db";
	int dbsuffix = strlen(".db");

	sqlite3 *db;
	sqlite3_stmt *stmt;
	int status = 0; /* exit code, for non-fatal errors */

	while (argv + argc > curarg) {
		if (curarg[0][0] == '-') {
			switch(curarg[0][1]) {
				case '-':
					fprintf(stderr, "GNU-style options not accepted, ignored '%s'.\n", curarg[0]);
					break;
				case 'h':
					usage(argv[0]);
					exit(0);
					break;
				case 'x':
					extract = true;
					break;
				case 'q':
					quiet = true;
					break;
				case 'p':
					pipe = true;
					break;
				case 'o':
					if (curarg[0][2] == '\0') {
						outfilename = curarg[1];
						curarg++;
					} else {
						outfilename = &curarg[0][2];
					}
					break;
				default:
					fprintf(stderr, "Unknown switch '-%c', ignoring.\n", curarg[0][1]);
			}
		} else if (!filename) {
			filename = curarg[0];
		} else {
			fprintf(stderr, "Extra argument: '%s', please use me on one file at a time.\n", curarg[0]);
			usage(argv[0]);
			exit(1);
		}
		curarg++;
	}
	if (!filename && !pipe) {
		fprintf(stderr, "No filename provided, and -p not used.\n");
		usage(argv[0]);
		exit(1);
	}
	if (filename) {
		if (!quiet) {
			fprintf(stderr, "opening '%s'\n", filename);
		}
		infd = open(filename, O_RDONLY);
		if (infd == -1) {
			fprintf(stderr, "opening %s: %s\n", filename, strerror(errno));
			exit(1);
		}
	}
	outfd = mkstemps(dbfile, dbsuffix);
	if (outfd == -1) {
		fprintf(stderr, "mkstemps: %s\n", strerror(errno));
		exit(1);
	}
	int extra = open("/dev/null", O_WRONLY);
	
	if (readv(infd, record, 2) != 16) {
		fprintf(stderr, "Failed to read header\n");
		exit(1);
	}
	if (strncmp(type, "CSFCHUNK", 8) != 0) {
		fprintf(stderr, "Not a CSFCHUNK-format file\n");
		exit(1);
	}
	read(infd, &len, 8);


	while (readv(infd, record, 2) == 16) {
		len = htonll(len);
		if (!quiet) {
			fprintf(stderr, "got a '%.8s' of length %llu\n", type, len);
		}
		int curfd = extra;
		if (strncmp(type, "CHNKSQLi", 8) == 0) {
			curfd = outfd;
		} else {
			curfd = extra;
		}
		int ret = sendfile(curfd, infd, NULL, len);
		if (ret == -1) {
			fprintf(stderr, "sendfile: %s\n", strerror(errno));
			exit(1);
		}
	}
	close(outfd);
	close(extra);
	if (!quiet) {
		fprintf(stderr, "Wrote to '%s'\n", dbfile);
	}

	sqlite3_open_v2(dbfile, &db, SQLITE_OPEN_READONLY, "unix-none");
	if (db == NULL) {
		fprintf(stderr, "sqlite3_open: %s\n", sqlite3_errmsg(db));
		exit(1);
	}

	sqlite3_prepare_v2(db, "select ImageData from CanvasPreview limit 1", -1, &stmt, NULL);
	while (sqlite3_step(stmt) != SQLITE_DONE) {
		outfd = open(outfilename, O_WRONLY | O_CREAT | O_EXCL, 0660);
		if (outfd == -1) {
			fprintf(stderr, "opening %s: %s\n", outfilename, strerror(errno));
			status = 1;
			break;
		}

		int pnglen = len = sqlite3_column_bytes(stmt, 0);
		if (!quiet) {
			fprintf(stderr, "Writing %llu bytes\n", pnglen);
		}
		write(outfd, sqlite3_column_blob(stmt, 0), pnglen);
		close(outfd);
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	unlink(dbfile);
	exit(status);
}
