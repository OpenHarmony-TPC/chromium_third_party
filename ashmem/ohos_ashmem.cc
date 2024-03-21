/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ashmem.h"

#include <base/logging.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/ashmem.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "graphic_adapter.h"

#define ASHMEM_DEVICE "/dev/ashmem"

static pthread_once_t s_ashmem_dev_once = PTHREAD_ONCE_INIT;
static dev_t s_ashmem_dev;

static void get_ashmem_dev() {
  struct stat st;
  if (stat(ASHMEM_DEVICE, &st) == 0 && S_ISCHR(st.st_mode)) {
    s_ashmem_dev = st.st_dev;
    LOG(INFO) << "get_ashmem_dev execute success";
    return;
  }

  LOG_IF(FATAL, 1) << "get_ashmem_dev execute failed";
}

static int ashmem_dev_fd_check(int fd) {
  pthread_once(&s_ashmem_dev_once, get_ashmem_dev);

  struct stat st;
  if (fstat(fd, &st) == 0 && S_ISCHR(st.st_mode) && st.st_dev != 0 &&
      st.st_dev == s_ashmem_dev) {
    return 1;
  }

  LOG_IF(FATAL, 1) << "ashmem_dev_fd_check failed";
  return 0;
}

static int ashmem_check_failure(int fd, int result) {
  if (result == -1 && errno == ENOTTY) {
    return ashmem_dev_fd_check(fd);
  }
  return result;
}

int ashmem_device_is_supported(void) {
  return 1;
}

int ashmem_create_region(const char* name, size_t size) {
  // use OHOS implementation to avoid race.
  return OHOS::NWeb::AshmemAdapter::AshmemCreate(name, size);
}

int ashmem_set_prot_region(int fd, int prot) {
  return ashmem_check_failure(
      fd, TEMP_FAILURE_RETRY(ioctl(fd, ASHMEM_SET_PROT_MASK, prot)));
}

int ashmem_get_prot_region(int fd) {
  return ashmem_check_failure(
      fd, TEMP_FAILURE_RETRY(ioctl(fd, ASHMEM_GET_PROT_MASK)));
}

int ashmem_pin_region(int fd, size_t offset, size_t len) {
  LOG(DEBUG) << "ashmem_pin_region";
  struct ashmem_pin pin = {static_cast<uint32_t>(offset),
                           static_cast<uint32_t>(len)};
  return ashmem_check_failure(fd,
                              TEMP_FAILURE_RETRY(ioctl(fd, ASHMEM_PIN, &pin)));
}

int ashmem_unpin_region(int fd, size_t offset, size_t len) {
  LOG(DEBUG) << "ashmem_unpin_region";
  struct ashmem_pin pin = {static_cast<uint32_t>(offset),
                           static_cast<uint32_t>(len)};
  return ashmem_check_failure(
      fd, TEMP_FAILURE_RETRY(ioctl(fd, ASHMEM_UNPIN, &pin)));
}

int ashmem_get_size_region(int fd) {
  return ashmem_check_failure(
      fd, TEMP_FAILURE_RETRY(ioctl(fd, ASHMEM_GET_SIZE, NULL)));
}
 
