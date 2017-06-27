/*
 * This file is part of fast_crc32
 * https://github.com/johndoe31415/fast_crc32
 * (c) 2017, Johannes Bauer <joe@johannes-bauer.com>
 * License: Public Domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/if_alg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <unistd.h>
#include <bits/socket.h>

static void print_crc32(const char *path);

static bool calc_blockdev_crc;
static int kernel_crc32_socket;

static int initialize_kernel_crc32(uint32_t seed) {
	struct sockaddr_alg sa = {
		.salg_family = AF_ALG,
		.salg_type   = "hash",
		.salg_name   = "crc32c",
	};

	int sd = socket(AF_ALG, SOCK_SEQPACKET, 0);
	if (sd == -1) {
		perror("socket");
		return -1;
	}

	if (bind(sd, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
		perror("bind");
		close(sd);
		return -1;
	}

	if (setsockopt(sd, SOL_ALG, ALG_SET_KEY, &seed, 4) == -1) {
		perror("ALG_SET_KEY");
		close(sd);
		return -1;
	}

	return sd;
}

static bool calculate_kernel_crc32(int fd, size_t filesize, uint32_t *crc) {
	int sd;
    if ((sd = accept(kernel_crc32_socket, NULL, 0)) == -1) {
		perror("accept");
		return false;
	}

	int pipefd[2];
	if (pipe(pipefd) == -1) {
		perror("pipe");
		close(sd);
		return false;
	}

	size_t remaining = filesize;
	while (remaining > 0) {
		ssize_t into_pipe = splice(fd, NULL, pipefd[1], NULL, remaining, SPLICE_F_MORE);
		if (into_pipe < 0) {
			fprintf(stderr, "splice1 wrote %zd (expected %zu): %s\n", into_pipe, remaining, strerror(errno));
			close(pipefd[0]);
			close(pipefd[1]);
			close(sd);
			return false;
		}
		ssize_t written = splice(pipefd[0], NULL, sd, NULL, into_pipe, SPLICE_F_MORE);
		if (written < 0) {
			fprintf(stderr, "splice2 wrote %zd (expected %zu): %s\n", written, remaining, strerror(errno));
			close(pipefd[0]);
			close(pipefd[1]);
			close(sd);
			return false;
		}
		remaining -= written;
	}

	close(pipefd[0]);
	close(pipefd[1]);

	if (read(sd, crc, 4) != 4) {
		perror("read");
		close(sd);
		return false;
	}

	close(sd);
	return true;
}

static void print_crc32_file(const char *filename, size_t filesize) {
	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "open %s: %s\n", filename, strerror(errno));
		return;
	}
	uint32_t crc = 0;
	if (!calculate_kernel_crc32(fd, filesize, &crc)) {
		fprintf(stderr, "calculate_crc32 failed for %s\n", filename);
	} else {
		printf("%s %08x\n", filename, crc);
	}
	close(fd);
}

static void print_crc32_dir(const char *dirname) {
	DIR *dir = opendir(dirname);
	if (!dir) {
		fprintf(stderr, "opendir %s: %s\n", dirname, strerror(errno));
		return;
	}
	int baselen = strlen(dirname);
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if ((!strcmp(entry->d_name, ".")) || (!strcmp(entry->d_name, ".."))) {
			continue;
		}

		char full_dirname[baselen + 1 + strlen(entry->d_name) + 1];
		strcpy(full_dirname, dirname);
		strcat(full_dirname, "/");
		strcat(full_dirname, entry->d_name);
		print_crc32(full_dirname);
	}
	closedir(dir);
}

static void print_crc32(const char *path) {
	struct stat statbuf;
	if (stat(path, &statbuf) == -1) {
		fprintf(stderr, "stat %s: %s\n", path, strerror(errno));
		return;
	}
	if (S_ISREG(statbuf.st_mode) || (calc_blockdev_crc && S_ISBLK(statbuf.st_mode))) {
		size_t filesize = 0;
		if (S_ISREG(statbuf.st_mode)) {
			filesize = statbuf.st_size;
		} else {
			int fd = open(path, O_RDONLY);
			if (fd == -1) {
				perror("open");
				return;
			}
			filesize = lseek(fd, 0, SEEK_END);
			if (filesize == (off_t)-1) {
				perror("lseek");
				close(fd);
				return;
			}
			close(fd);
		}
		print_crc32_file(path, filesize);
	} else if (S_ISDIR(statbuf.st_mode)) {
		print_crc32_dir(path);
	}
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "%s (-d) [path] ([path] ...)\n", argv[0]);
		fprintf(stderr, "   -d     Also print the hash value of block device contents.\n");
		exit(EXIT_FAILURE);
	}
	int positional_index = 1;
	if (!strcmp(argv[positional_index], "-d")) {
		calc_blockdev_crc = true;
		positional_index++;
		fprintf(stderr, "Will also calculate CRC of block devices.\n");
	}
	kernel_crc32_socket = initialize_kernel_crc32(~0);
	if (kernel_crc32_socket == -1) {
		exit(EXIT_FAILURE);
	}
	for (int i = positional_index; i < argc; i++) {
		print_crc32(argv[i]);
	}
	close(kernel_crc32_socket);
	return 0;
}
