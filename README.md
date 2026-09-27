# Project B — Embedded Linux edge platform (BeagleBone Black + MPU6050)

## Pinned versions
| Thành phần | Giá trị |
|---|---|
| Yocto release | 5.2.4 "Walnascar" (poky tag `yocto-5.2.4`) |
| MACHINE | `beaglebone-yocto` |
| Kernel | linux-yocto 6.12 |
| Build host | container `yocto-build` (Ubuntu 24.04, user `builder` uid 1000), host Arch Linux |
| Threads | `BB_NUMBER_THREADS=3`, `PARALLEL_MAKE=-j 3` |

## Layout
- `docker/Dockerfile` — môi trường build (Ubuntu 24.04 + gói Yocto + user uid khớp host)
- `meta-projectb/` — layer của dự án
- `poky/` — upstream, **không commit** (`.gitignore`); clone tag `yocto-5.2.4`:
  `git clone --branch yocto-5.2.4 --depth 1 https://git.yoctoproject.org/poky`
- `downloads/` — cache mã nguồn (`DL_DIR`), **không commit**, có thể tải lại từ đầu

## Build

    docker build --build-arg UID=$(id -u) -t yocto-build docker/
    docker run --rm -it -v ~/workspace_projectb:/workspace yocto-build
    # trong container:
    cd poky && source oe-init-build-env build
    bitbake core-image-minimal

Output: `poky/build/tmp/deploy/images/beaglebone-yocto/`

## Sự cố đã gặp và cách xử lý

**pseudo + tar >= 1.35 / glibc >= 2.39 (Ubuntu 24.04 container):**
`do_package` của `linux-libc-headers` lỗi `got *at() syscall for unknown directory`
vì `pseudo` mặc định của poky 5.2.4 thiếu wrapper cho `openat2()`.
Sửa bằng `meta-projectb/recipes-devtools/pseudo/pseudo_git.bbappend`:
ghim `SRCREV` mới hơn (có wrapper `openat2`) + patch `older-glibc-symbols.patch`
đã rebase theo revision đó (patch gốc của poky không áp được lên source mới).

## Flash

    sudo bmaptool copy <image>.wic /dev/sdX   # kiểm tra kỹ bằng lsblk trước
