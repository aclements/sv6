#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include "fstest.h"

#ifndef XV6_USER
#define O_ANYFD 0
#endif

static int __attribute__((unused)) xerrno(int r) {
#ifdef XV6_USER
  return r;
#else
  return -errno;
#endif
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, True], [1, True], True], u'_map': [{u'off': 0, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: False
 *        b.close.fd: 1
 *        b.close.pid: False
 */
static void setup_0_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_0_proc0(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 0, SEEK_SET);
  dup2(fd, 5);
  close(fd);
  fd = open("__i0", O_RDWR);
  lseek(fd, 0, SEEK_SET);
  dup2(fd, 6);
  close(fd);
}

static void setup_0_proc1(void) {
  int fd __attribute__((unused));
}

static void setup_0_final(void) {
  unlink("__i0");
}

static int test_0_0(void) {
  return close(5);
}

static int test_0_1(void) {
  return close(6);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, True], [1, False], True], u'_map': [{u'off': 0, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: False
 *        b.close.fd: 1
 *        b.close.pid: False
 */
static void setup_1_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_1_proc0(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 0, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_1_proc1(void) {
  int fd __attribute__((unused));
}

static void setup_1_final(void) {
  unlink("__i0");
}

static int test_1_0(void) {
  return close(5);
}

static int test_1_1(void) {
  return close(6);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, True], [2, False], True], u'_map': [[3, {u'off': 4, u'inum': 0}], {u'off': 4, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[1, True], [2, True], True], u'_map': [[3, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: False
 *        b.close.fd: 1
 *        b.close.pid: True
 */
static void setup_2_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_2_proc0(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 4, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_2_proc1(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 2, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_2_final(void) {
  unlink("__i0");
}

static int test_2_0(void) {
  return close(5);
}

static int test_2_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, True], [1, True], True], u'_map': [[2, {u'off': 3, u'inum': 0}], {u'off': 3, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[0, True], [1, False], True], u'_map': [[2, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: False
 *        b.close.fd: 0
 *        b.close.pid: True
 */
static void setup_3_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_3_proc0(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 3, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_3_proc1(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 2, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_3_final(void) {
  unlink("__i0");
}

static int test_3_0(void) {
  return close(5);
}

static int test_3_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, True], [2, True], True], u'_map': [[3, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[1, False], [2, False], False], u'_map': [[3, {u'off': 4, u'inum': 0}], {u'off': 4, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: False
 *        b.close.fd: 1
 *        b.close.pid: True
 */
static void setup_4_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_4_proc0(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 2, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_4_proc1(void) {
  int fd __attribute__((unused));
}

static void setup_4_final(void) {
  unlink("__i0");
}

static int test_4_0(void) {
  return close(5);
}

static int test_4_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, True], [1, False], True], u'_map': [[2, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[0, False], [1, True], False], u'_map': [[2, {u'off': 3, u'inum': 0}], {u'off': 3, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: False
 *        b.close.fd: 0
 *        b.close.pid: True
 */
static void setup_5_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_5_proc0(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 2, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_5_proc1(void) {
  int fd __attribute__((unused));
}

static void setup_5_final(void) {
  unlink("__i0");
}

static int test_5_0(void) {
  return close(5);
}

static int test_5_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, False], [1, True], False], u'_map': [{u'off': 0, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: False
 *        b.close.fd: 1
 *        b.close.pid: False
 */
static void setup_6_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_6_proc0(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 0, SEEK_SET);
  dup2(fd, 6);
  close(fd);
}

static void setup_6_proc1(void) {
  int fd __attribute__((unused));
}

static void setup_6_final(void) {
  unlink("__i0");
}

static int test_6_0(void) {
  return close(5);
}

static int test_6_1(void) {
  return close(6);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, False], [1, False], False], u'_map': [{u'off': 0, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: False
 *        b.close.fd: 1
 *        b.close.pid: False
 */
static void setup_7_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
}

static void setup_7_proc0(void) {
  int fd __attribute__((unused));
}

static void setup_7_proc1(void) {
  int fd __attribute__((unused));
}

static void setup_7_final(void) {
}

static int test_7_0(void) {
  return close(5);
}

static int test_7_1(void) {
  return close(6);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, False], False], u'_map': [{u'off': 0, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: False
 *        b.close.fd: 0
 *        b.close.pid: False
 */
static void setup_8_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
}

static void setup_8_proc0(void) {
  int fd __attribute__((unused));
}

static void setup_8_proc1(void) {
  int fd __attribute__((unused));
}

static void setup_8_final(void) {
}

static int test_8_0(void) {
  return close(5);
}

static int test_8_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, False], [2, False], False], u'_map': [[3, {u'off': 4, u'inum': 0}], {u'off': 4, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[1, True], [2, True], True], u'_map': [[3, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: False
 *        b.close.fd: 1
 *        b.close.pid: True
 */
static void setup_9_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_9_proc0(void) {
  int fd __attribute__((unused));
}

static void setup_9_proc1(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 2, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_9_final(void) {
  unlink("__i0");
}

static int test_9_0(void) {
  return close(5);
}

static int test_9_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, False], [1, True], False], u'_map': [[2, {u'off': 3, u'inum': 0}], {u'off': 3, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[0, True], [1, False], True], u'_map': [[2, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: False
 *        b.close.fd: 0
 *        b.close.pid: True
 */
static void setup_10_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_10_proc0(void) {
  int fd __attribute__((unused));
}

static void setup_10_proc1(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 2, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_10_final(void) {
  unlink("__i0");
}

static int test_10_0(void) {
  return close(5);
}

static int test_10_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, False], [2, True], False], u'_map': [[3, {u'off': 4, u'inum': 0}], {u'off': 4, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[1, False], [2, False], False], u'_map': [[3, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: False
 *        b.close.fd: 1
 *        b.close.pid: True
 */
static void setup_11_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
}

static void setup_11_proc0(void) {
  int fd __attribute__((unused));
}

static void setup_11_proc1(void) {
  int fd __attribute__((unused));
}

static void setup_11_final(void) {
}

static int test_11_0(void) {
  return close(5);
}

static int test_11_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, False], [1, False], False], u'_map': [[2, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[0, False], [1, True], False], u'_map': [[2, {u'off': 3, u'inum': 0}], {u'off': 3, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: False
 *        b.close.fd: 0
 *        b.close.pid: True
 */
static void setup_12_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
}

static void setup_12_proc0(void) {
  int fd __attribute__((unused));
}

static void setup_12_proc1(void) {
  int fd __attribute__((unused));
}

static void setup_12_final(void) {
}

static int test_12_0(void) {
  return close(5);
}

static int test_12_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[1, True], [2, False], True], u'_map': [[3, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[0, True], [2, True], True], u'_map': [[3, {u'off': 4, u'inum': 0}], {u'off': 4, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: True
 *        b.close.fd: 1
 *        b.close.pid: False
 */
static void setup_13_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_13_proc0(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 2, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_13_proc1(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 4, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_13_final(void) {
  unlink("__i0");
}

static int test_13_0(void) {
  return close(5);
}

static int test_13_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, True], [1, True], True], u'_map': [[2, {u'off': 3, u'inum': 0}], {u'off': 3, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[0, True], [1, False], True], u'_map': [[2, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: True
 *        b.close.fd: 0
 *        b.close.pid: False
 */
static void setup_14_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_14_proc0(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 3, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_14_proc1(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 2, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_14_final(void) {
  unlink("__i0");
}

static int test_14_0(void) {
  return close(5);
}

static int test_14_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[1, False], [2, False], False], u'_map': [[3, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[0, True], [2, True], True], u'_map': [[3, {u'off': 4, u'inum': 0}], {u'off': 4, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: True
 *        b.close.fd: 1
 *        b.close.pid: False
 */
static void setup_15_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_15_proc0(void) {
  int fd __attribute__((unused));
}

static void setup_15_proc1(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 4, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_15_final(void) {
  unlink("__i0");
}

static int test_15_0(void) {
  return close(5);
}

static int test_15_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, False], [1, False], False], u'_map': [[2, {u'off': 3, u'inum': 0}], {u'off': 3, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[0, True], [1, True], True], u'_map': [[2, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: True
 *        b.close.fd: 0
 *        b.close.pid: False
 */
static void setup_16_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_16_proc0(void) {
  int fd __attribute__((unused));
}

static void setup_16_proc1(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 2, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_16_final(void) {
  unlink("__i0");
}

static int test_16_0(void) {
  return close(5);
}

static int test_16_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc1: {u'fd_map': {u'_valid': [[0, True], [1, True], True], u'_map': [{u'off': 0, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: True
 *        b.close.fd: 1
 *        b.close.pid: True
 */
static void setup_17_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_17_proc0(void) {
  int fd __attribute__((unused));
}

static void setup_17_proc1(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 0, SEEK_SET);
  dup2(fd, 5);
  close(fd);
  fd = open("__i0", O_RDWR);
  lseek(fd, 0, SEEK_SET);
  dup2(fd, 6);
  close(fd);
}

static void setup_17_final(void) {
  unlink("__i0");
}

static int test_17_0(void) {
  return close(5);
}

static int test_17_1(void) {
  return close(6);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc1: {u'fd_map': {u'_valid': [[0, True], [1, False], True], u'_map': [{u'off': 0, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: True
 *        b.close.fd: 1
 *        b.close.pid: True
 */
static void setup_18_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_18_proc0(void) {
  int fd __attribute__((unused));
}

static void setup_18_proc1(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 0, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_18_final(void) {
  unlink("__i0");
}

static int test_18_0(void) {
  return close(5);
}

static int test_18_1(void) {
  return close(6);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[1, True], [2, False], True], u'_map': [[3, {u'off': 4, u'inum': 0}], {u'off': 4, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[0, False], [2, True], False], u'_map': [[3, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: True
 *        b.close.fd: 1
 *        b.close.pid: False
 */
static void setup_19_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_19_proc0(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 4, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_19_proc1(void) {
  int fd __attribute__((unused));
}

static void setup_19_final(void) {
  unlink("__i0");
}

static int test_19_0(void) {
  return close(5);
}

static int test_19_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, True], [1, True], True], u'_map': [[2, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[0, False], [1, False], False], u'_map': [[2, {u'off': 3, u'inum': 0}], {u'off': 3, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: True
 *        b.close.fd: 0
 *        b.close.pid: False
 */
static void setup_20_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_20_proc0(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 2, SEEK_SET);
  dup2(fd, 5);
  close(fd);
}

static void setup_20_proc1(void) {
  int fd __attribute__((unused));
}

static void setup_20_final(void) {
  unlink("__i0");
}

static int test_20_0(void) {
  return close(5);
}

static int test_20_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[1, False], [2, True], False], u'_map': [[3, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[0, False], [2, False], False], u'_map': [[3, {u'off': 4, u'inum': 0}], {u'off': 4, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: True
 *        b.close.fd: 1
 *        b.close.pid: False
 */
static void setup_21_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
}

static void setup_21_proc0(void) {
  int fd __attribute__((unused));
}

static void setup_21_proc1(void) {
  int fd __attribute__((unused));
}

static void setup_21_final(void) {
}

static int test_21_0(void) {
  return close(5);
}

static int test_21_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc0: {u'fd_map': {u'_valid': [[0, False], [1, False], False], u'_map': [[2, {u'off': 2, u'inum': 0}], {u'off': 2, u'inum': 0}]}}
 *        Fs.proc1: {u'fd_map': {u'_valid': [[0, False], [1, True], False], u'_map': [[2, {u'off': 3, u'inum': 0}], {u'off': 3, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: True
 *        b.close.fd: 0
 *        b.close.pid: False
 */
static void setup_22_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
}

static void setup_22_proc0(void) {
  int fd __attribute__((unused));
}

static void setup_22_proc1(void) {
  int fd __attribute__((unused));
}

static void setup_22_final(void) {
}

static int test_22_0(void) {
  return close(5);
}

static int test_22_1(void) {
  return close(5);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc1: {u'fd_map': {u'_valid': [[0, False], [1, True], False], u'_map': [{u'off': 0, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: True
 *        b.close.fd: 1
 *        b.close.pid: True
 */
static void setup_23_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
  fd = open("__i0", O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd);
}

static void setup_23_proc0(void) {
  int fd __attribute__((unused));
}

static void setup_23_proc1(void) {
  int fd __attribute__((unused));
  fd = open("__i0", O_RDWR);
  lseek(fd, 0, SEEK_SET);
  dup2(fd, 6);
  close(fd);
}

static void setup_23_final(void) {
  unlink("__i0");
}

static int test_23_0(void) {
  return close(5);
}

static int test_23_1(void) {
  return close(6);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc1: {u'fd_map': {u'_valid': [[0, False], [1, False], False], u'_map': [{u'off': 0, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: True
 *        b.close.fd: 1
 *        b.close.pid: True
 */
static void setup_24_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
}

static void setup_24_proc0(void) {
  int fd __attribute__((unused));
}

static void setup_24_proc1(void) {
  int fd __attribute__((unused));
}

static void setup_24_final(void) {
}

static int test_24_0(void) {
  return close(5);
}

static int test_24_1(void) {
  return close(6);
}

/*
 * calls: [u'close', u'close']
 * vars:  Fs.proc1: {u'fd_map': {u'_valid': [[0, False], False], u'_map': [{u'off': 0, u'inum': 0}]}}
 *        a.close.fd: 0
 *        a.close.pid: True
 *        b.close.fd: 0
 *        b.close.pid: True
 */
static void setup_25_common(void) {
  int fd __attribute__((unused));
  char c __attribute__((unused));
}

static void setup_25_proc0(void) {
  int fd __attribute__((unused));
}

static void setup_25_proc1(void) {
  int fd __attribute__((unused));
}

static void setup_25_final(void) {
}

static int test_25_0(void) {
  return close(5);
}

static int test_25_1(void) {
  return close(5);
}

static void cleanup(void) {
  unlink("__f0");
  unlink("__f1");
  unlink("__f2");
  unlink("__f3");
  unlink("__f4");
  unlink("__f5");
  close(3);
  close(4);
  close(5);
  close(6);
  close(7);
  close(8);
  close(9);
}

struct fstest fstests[] = {
  { "fs-1364677196-0",
    &setup_0_common, { { &setup_0_proc0 }, { &setup_0_proc1 } }, &setup_0_final,
    { { &test_0_0, 0, "close" },
      { &test_0_1, 0, "close" } },
    &cleanup },
  { "fs-1364677196-1",
    &setup_1_common, { { &setup_1_proc0 }, { &setup_1_proc1 } }, &setup_1_final,
    { { &test_1_0, 0, "close" },
      { &test_1_1, 0, "close" } },
    &cleanup },
  { "fs-1364677196-2",
    &setup_2_common, { { &setup_2_proc0 }, { &setup_2_proc1 } }, &setup_2_final,
    { { &test_2_0, 0, "close" },
      { &test_2_1, 1, "close" } },
    &cleanup },
  { "fs-1364677196-3",
    &setup_3_common, { { &setup_3_proc0 }, { &setup_3_proc1 } }, &setup_3_final,
    { { &test_3_0, 0, "close" },
      { &test_3_1, 1, "close" } },
    &cleanup },
  { "fs-1364677196-4",
    &setup_4_common, { { &setup_4_proc0 }, { &setup_4_proc1 } }, &setup_4_final,
    { { &test_4_0, 0, "close" },
      { &test_4_1, 1, "close" } },
    &cleanup },
  { "fs-1364677196-5",
    &setup_5_common, { { &setup_5_proc0 }, { &setup_5_proc1 } }, &setup_5_final,
    { { &test_5_0, 0, "close" },
      { &test_5_1, 1, "close" } },
    &cleanup },
  { "fs-1364677196-6",
    &setup_6_common, { { &setup_6_proc0 }, { &setup_6_proc1 } }, &setup_6_final,
    { { &test_6_0, 0, "close" },
      { &test_6_1, 0, "close" } },
    &cleanup },
  { "fs-1364677196-7",
    &setup_7_common, { { &setup_7_proc0 }, { &setup_7_proc1 } }, &setup_7_final,
    { { &test_7_0, 0, "close" },
      { &test_7_1, 0, "close" } },
    &cleanup },
  { "fs-1364677196-8",
    &setup_8_common, { { &setup_8_proc0 }, { &setup_8_proc1 } }, &setup_8_final,
    { { &test_8_0, 0, "close" },
      { &test_8_1, 0, "close" } },
    &cleanup },
  { "fs-1364677196-9",
    &setup_9_common, { { &setup_9_proc0 }, { &setup_9_proc1 } }, &setup_9_final,
    { { &test_9_0, 0, "close" },
      { &test_9_1, 1, "close" } },
    &cleanup },
  { "fs-1364677196-10",
    &setup_10_common, { { &setup_10_proc0 }, { &setup_10_proc1 } }, &setup_10_final,
    { { &test_10_0, 0, "close" },
      { &test_10_1, 1, "close" } },
    &cleanup },
  { "fs-1364677196-11",
    &setup_11_common, { { &setup_11_proc0 }, { &setup_11_proc1 } }, &setup_11_final,
    { { &test_11_0, 0, "close" },
      { &test_11_1, 1, "close" } },
    &cleanup },
  { "fs-1364677196-12",
    &setup_12_common, { { &setup_12_proc0 }, { &setup_12_proc1 } }, &setup_12_final,
    { { &test_12_0, 0, "close" },
      { &test_12_1, 1, "close" } },
    &cleanup },
  { "fs-1364677196-13",
    &setup_13_common, { { &setup_13_proc0 }, { &setup_13_proc1 } }, &setup_13_final,
    { { &test_13_0, 1, "close" },
      { &test_13_1, 0, "close" } },
    &cleanup },
  { "fs-1364677196-14",
    &setup_14_common, { { &setup_14_proc0 }, { &setup_14_proc1 } }, &setup_14_final,
    { { &test_14_0, 1, "close" },
      { &test_14_1, 0, "close" } },
    &cleanup },
  { "fs-1364677196-15",
    &setup_15_common, { { &setup_15_proc0 }, { &setup_15_proc1 } }, &setup_15_final,
    { { &test_15_0, 1, "close" },
      { &test_15_1, 0, "close" } },
    &cleanup },
  { "fs-1364677196-16",
    &setup_16_common, { { &setup_16_proc0 }, { &setup_16_proc1 } }, &setup_16_final,
    { { &test_16_0, 1, "close" },
      { &test_16_1, 0, "close" } },
    &cleanup },
  { "fs-1364677196-17",
    &setup_17_common, { { &setup_17_proc0 }, { &setup_17_proc1 } }, &setup_17_final,
    { { &test_17_0, 1, "close" },
      { &test_17_1, 1, "close" } },
    &cleanup },
  { "fs-1364677196-18",
    &setup_18_common, { { &setup_18_proc0 }, { &setup_18_proc1 } }, &setup_18_final,
    { { &test_18_0, 1, "close" },
      { &test_18_1, 1, "close" } },
    &cleanup },
  { "fs-1364677196-19",
    &setup_19_common, { { &setup_19_proc0 }, { &setup_19_proc1 } }, &setup_19_final,
    { { &test_19_0, 1, "close" },
      { &test_19_1, 0, "close" } },
    &cleanup },
  { "fs-1364677196-20",
    &setup_20_common, { { &setup_20_proc0 }, { &setup_20_proc1 } }, &setup_20_final,
    { { &test_20_0, 1, "close" },
      { &test_20_1, 0, "close" } },
    &cleanup },
  { "fs-1364677196-21",
    &setup_21_common, { { &setup_21_proc0 }, { &setup_21_proc1 } }, &setup_21_final,
    { { &test_21_0, 1, "close" },
      { &test_21_1, 0, "close" } },
    &cleanup },
  { "fs-1364677196-22",
    &setup_22_common, { { &setup_22_proc0 }, { &setup_22_proc1 } }, &setup_22_final,
    { { &test_22_0, 1, "close" },
      { &test_22_1, 0, "close" } },
    &cleanup },
  { "fs-1364677196-23",
    &setup_23_common, { { &setup_23_proc0 }, { &setup_23_proc1 } }, &setup_23_final,
    { { &test_23_0, 1, "close" },
      { &test_23_1, 1, "close" } },
    &cleanup },
  { "fs-1364677196-24",
    &setup_24_common, { { &setup_24_proc0 }, { &setup_24_proc1 } }, &setup_24_final,
    { { &test_24_0, 1, "close" },
      { &test_24_1, 1, "close" } },
    &cleanup },
  { "fs-1364677196-25",
    &setup_25_common, { { &setup_25_proc0 }, { &setup_25_proc1 } }, &setup_25_final,
    { { &test_25_0, 1, "close" },
      { &test_25_1, 1, "close" } },
    &cleanup },
  { 0 }
};
