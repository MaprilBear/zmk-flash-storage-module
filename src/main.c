/*
 * Copyright (c) 2016 Intel Corporation.
 * Copyright (c) 2019-2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/class/usbd_msc.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>

LOG_MODULE_REGISTER(main);

#if CONFIG_DISK_DRIVER_FLASH
#include <zephyr/storage/flash_map.h>
#endif

#if CONFIG_FAT_FILESYSTEM_ELM
#include <ff.h>
#endif

#if CONFIG_FILE_SYSTEM_LITTLEFS
#include <zephyr/fs/littlefs.h>
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
#endif

#define STORAGE_PARTITION storage_partition
#define STORAGE_PARTITION_ID FIXED_PARTITION_ID(STORAGE_PARTITION)

static struct fs_mount_t fs_mnt;

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
USBD_CONFIGURATION_DEFINE(config_1, USB_SCD_SELF_POWERED, 200);

USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_MANUFACTURER_DEFINE(sample_mfr, "ZEPHYR");
USBD_DESC_PRODUCT_DEFINE(sample_product, "Zephyr USBD MSC");
USBD_DESC_SERIAL_NUMBER_DEFINE(sample_sn, "0123456789ABCDEF");

USBD_DEVICE_DEFINE(sample_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
                   0x2fe3, 0x0008);

#if CONFIG_DISK_DRIVER_RAM
USBD_DEFINE_MSC_LUN(RAM, "Zephyr", "RAMDisk", "0.00");
#endif

#if CONFIG_DISK_DRIVER_FLASH
USBD_DEFINE_MSC_LUN(NAND, "Zephyr", "FlashDisk", "0.00");
#endif

#if CONFIG_DISK_DRIVER_SDMMC
USBD_DEFINE_MSC_LUN(SD, "Zephyr", "SD", "0.00");
#endif

static int enable_usb_device_next(void) {
  int err;

  err = usbd_add_descriptor(&sample_usbd, &sample_lang);
  if (err) {
    LOG_ERR("Failed to initialize language descriptor (%d)", err);
    return err;
  }

  err = usbd_add_descriptor(&sample_usbd, &sample_mfr);
  if (err) {
    LOG_ERR("Failed to initialize manufacturer descriptor (%d)", err);
    return err;
  }

  err = usbd_add_descriptor(&sample_usbd, &sample_product);
  if (err) {
    LOG_ERR("Failed to initialize product descriptor (%d)", err);
    return err;
  }

  err = usbd_add_descriptor(&sample_usbd, &sample_sn);
  if (err) {
    LOG_ERR("Failed to initialize SN descriptor (%d)", err);
    return err;
  }

  err = usbd_add_configuration(&sample_usbd, &config_1);
  if (err) {
    LOG_ERR("Failed to add configuration (%d)", err);
    return err;
  }

  err = usbd_register_class(&sample_usbd, "msc_0", 1);
  if (err) {
    LOG_ERR("Failed to register MSC class (%d)", err);
    return err;
  }

  err = usbd_init(&sample_usbd);
  if (err) {
    LOG_ERR("Failed to initialize device support");
    return err;
  }

  err = usbd_enable(&sample_usbd);
  if (err) {
    LOG_ERR("Failed to enable device support");
    return err;
  }

  LOG_DBG("USB device support enabled");

  return 0;
}
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK_NEXT) */

static int setup_flash(struct fs_mount_t *mnt) {
  int rc = 0;
#if CONFIG_DISK_DRIVER_FLASH
  unsigned int id;
  const struct flash_area *pfa;

  mnt->storage_dev = (void *)STORAGE_PARTITION_ID;
  id = STORAGE_PARTITION_ID;

  rc = flash_area_open(id, &pfa);
  printk("Area %u at 0x%x on %s for %u bytes\n", id, (unsigned int)pfa->fa_off,
         pfa->fa_dev->name, (unsigned int)pfa->fa_size);

  if (rc < 0 && IS_ENABLED(CONFIG_APP_WIPE_STORAGE)) {
    printk("Erasing flash area ... ");
    rc = flash_area_erase(pfa, 0, pfa->fa_size);
    printk("%d\n", rc);
  }

  if (rc < 0) {
    flash_area_close(pfa);
  }
#endif
  return rc;
}

static int mount_app_fs(struct fs_mount_t *mnt) {
  int rc;

#if CONFIG_FAT_FILESYSTEM_ELM
  static FATFS fat_fs;

  mnt->type = FS_FATFS;
  mnt->fs_data = &fat_fs;
  if (IS_ENABLED(CONFIG_DISK_DRIVER_RAM)) {
    mnt->mnt_point = "/RAM:";
  } else if (IS_ENABLED(CONFIG_DISK_DRIVER_SDMMC)) {
    mnt->mnt_point = "/SD:";
  } else {
    mnt->mnt_point = "/E:";
  }

#elif CONFIG_FILE_SYSTEM_LITTLEFS
  mnt->type = FS_LITTLEFS;
  mnt->mnt_point = "/lfs";
  mnt->fs_data = &storage;
#endif

  mnt->mnt_point = "/NAND:";

  rc = fs_mount(mnt);

  return rc;
}

static int setup_disk(void) {
  struct fs_mount_t *mp = &fs_mnt;
  struct fs_dir_t dir;
  int rc;

  fs_dir_t_init(&dir);

  if (IS_ENABLED(CONFIG_DISK_DRIVER_FLASH)) {
    rc = setup_flash(mp);
    if (rc < 0) {
      LOG_ERR("Failed to setup flash area");
      return rc;
    }
  }

  if (!IS_ENABLED(CONFIG_FILE_SYSTEM_LITTLEFS) &&
      !IS_ENABLED(CONFIG_FAT_FILESYSTEM_ELM)) {
    LOG_INF("No file system selected");
    return -1;
  }

  rc = mount_app_fs(mp);
  if (rc < 0) {
    LOG_ERR("Failed to mount filesystem");
    return rc;
  }

  return 0;
}

SYS_INIT(setup_disk, APPLICATION, 99);
