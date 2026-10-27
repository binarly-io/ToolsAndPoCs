#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("binarly");
MODULE_DESCRIPTION("The mm_smi module allows SMI handlers to be triggered with "
                   "user-specified data.");
MODULE_VERSION("0.1");

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

#define NS_COMM_BUFFER 0x933e0000ULL
#define NS_COMM_BUFFER_SIZE 0x20000ULL
#define FFA_SP_ID 0x8004
#define HEADER_SIZE 0x24 // sizeof(efi_smm_communicate_header_t)

#define FFA_MSG_SEND_DIRECT_REQ 0xC400006FUL
#define FFA_MSG_SEND_DIRECT_RESP 0xC4000070UL
#define MM_HANDLER_INVOKE 0xC4000041UL

static int trigger_smi(void) {
  register uint64_t x0 asm("x0") = FFA_MSG_SEND_DIRECT_REQ;
  register uint64_t x1 asm("x1") = FFA_SP_ID;
  register uint64_t x2 asm("x2") = 0;
  register uint64_t x3 asm("x3") = MM_HANDLER_INVOKE;
  register uint64_t x4 asm("x4") = 0;
  register uint64_t x5 asm("x5") = NS_COMM_BUFFER;
  register uint64_t x6 asm("x6") = 0;
  register uint64_t x7 asm("x7") = 0;

  asm volatile("smc #0"
               : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4), "+r"(x5),
                 "+r"(x6), "+r"(x7)
               :
               : "memory");

  if (x0 != FFA_MSG_SEND_DIRECT_RESP) {
    pr_err("mm_smi: bad response 0x%llx\n", x0);
    return -EIO;
  }

  if (x4 != 0) {
    pr_err("mm_smi: handler error %lld\n", (int64_t)x4);
    return -EIO;
  }

  pr_info("mm_smi: handler success\n");
  return 0;
}

static long mm_smi_ioctl(struct file *file, unsigned int cmd,
                         unsigned long arg) {
  void __iomem *ns_buf;
  efi_smm_communicate_header_t header;
  uint64_t message_length;
  uint64_t total_size;
  void *kernel_buf = NULL;
  int ret;

  if (!arg)
    return -EINVAL;

  if (copy_from_user(&header, (void __user *)arg, sizeof(header)))
    return -EFAULT;

  message_length = header.message_length;

  if (message_length > NS_COMM_BUFFER_SIZE - (HEADER_SIZE + 1)) {
    pr_err("mm_smi: message too large: %llu\n", message_length);
    return -EINVAL;
  }

  // we are adding 1 to total_size and allow to use
  // message_length = 0 to mimic behaviour implemented
  // in StandaloneMmCpu
  total_size = message_length + HEADER_SIZE + 1;

  if (total_size < message_length) {
    pr_err("mm_smi: invalid size\n");
    return -EINVAL;
  }

  kernel_buf = kzalloc(total_size, GFP_KERNEL);
  if (!kernel_buf)
    return -ENOMEM;

  ns_buf = ioremap(NS_COMM_BUFFER, NS_COMM_BUFFER_SIZE);
  if (!ns_buf) {
    kfree(kernel_buf);
    return -ENOMEM;
  }

  if (copy_from_user(ns_buf, (void __user *)arg, total_size)) {
    ret = -EFAULT;
    goto cleanup;
  }

  wmb(); // ensure writes are visible to secure world

  ret = trigger_smi();

  rmb(); // ensure reads get updated data from secure world

  if (ret != 0)
    goto cleanup;

  memcpy_fromio(kernel_buf, ns_buf, total_size);

  if (copy_to_user((void __user *)arg, kernel_buf, total_size)) {
    pr_err("mm_smi: failed to copy response to user\n");
    ret = -EFAULT;
    goto cleanup;
  }

  pr_info("mm_smi: response copied to user (%llu bytes)\n", total_size);
  ret = 0;

cleanup:
  if (ns_buf) {
    memset_io(ns_buf, 0, total_size);
    iounmap(ns_buf);
  }

  if (kernel_buf) {
    memzero_explicit(kernel_buf, total_size);
    kfree(kernel_buf);
  }

  return ret;
}

static const struct file_operations mm_smi_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = mm_smi_ioctl,
};

static struct miscdevice mm_smi_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "mm_smi",
    .fops = &mm_smi_fops,
};

static int __init mm_smi_init(void) {
  int ret;

  pr_info("mm_smi: NS buffer=0x%llx size=0x%llx\n", NS_COMM_BUFFER,
          NS_COMM_BUFFER_SIZE);

  ret = misc_register(&mm_smi_dev);
  if (ret) {
    pr_err("mm_smi: registration failed\n");
    return ret;
  }

  pr_info("mm_smi: ready at /dev/mm_smi\n");
  return 0;
}

static void __exit mm_smi_exit(void) {
  misc_deregister(&mm_smi_dev);
  pr_info("mm_smi: unloaded\n");
}

module_init(mm_smi_init);
module_exit(mm_smi_exit);
