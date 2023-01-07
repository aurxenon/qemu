/*
 * SA-1110-based moon3 platform.
 *
 * Copyright (C) 2011 Dmitry Eremin-Solenikov
 * Copyright (C) 2023 Edward Humes
 *
 * This code is licensed under GNU GPL v2.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */
#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "strongarm.h"
#include "hw/arm/boot.h"
#include "hw/sd/sd.h"
#include "hw/ssi/ssi.h"
#include "hw/block/flash.h"
#include "exec/address-spaces.h"
#include "cpu.h"
#include "qom/object.h"

struct moon3MachineState {
    MachineState parent;

    StrongARMState *sa1110;
};

#define TYPE_MOON3_MACHINE MACHINE_TYPE_NAME("moon3")
OBJECT_DECLARE_SIMPLE_TYPE(moon3MachineState, MOON3_MACHINE)

static struct arm_boot_info moon3_binfo = {
    .loader_start = SA_SDCS0,
    .ram_size = 0x20000000,
};

static void moon3_init(MachineState *machine)
{
    DriveInfo *dinfo;
    MachineClass *mc = MACHINE_GET_CLASS(machine);
    moon3MachineState *cms = MOON3_MACHINE(machine);

    BlockBackend *blk;
    DeviceState *sd_dev, *card_dev;

    if (machine->ram_size != mc->default_ram_size) {
        char *sz = size_to_str(mc->default_ram_size);
        error_report("Invalid RAM size, should be %s", sz);
        g_free(sz);
        exit(EXIT_FAILURE);
    }

    cms->sa1110 = sa1110_init(machine->cpu_type);

    memory_region_add_subregion(get_system_memory(), SA_SDCS0, machine->ram);

    dinfo = drive_get(IF_PFLASH, 0, 0);
    pflash_cfi01_register(SA_CS0, "moon3.fl1", 0x02000000,
                    dinfo ? blk_by_legacy_dinfo(dinfo) : NULL,
                    64 * KiB, 4, 0x00, 0x00, 0x00, 0x00, 0);

    dinfo = drive_get(IF_PFLASH, 0, 1);
    pflash_cfi01_register(SA_CS1, "moon3.fl2", 0x02000000,
                    dinfo ? blk_by_legacy_dinfo(dinfo) : NULL,
                    64 * KiB, 4, 0x00, 0x00, 0x00, 0x00, 0);

    /* Connect an SD card to SPI2 */
    sd_dev = ssi_create_peripheral(cms->sa1110->ssp_bus, "ssi-sd");

    dinfo = drive_get(IF_SD, 0, 0);
    blk = dinfo ? blk_by_legacy_dinfo(dinfo) : NULL;
    card_dev = qdev_new(TYPE_SD_CARD);
    qdev_prop_set_drive_err(card_dev, "drive", blk, &error_fatal);
    qdev_prop_set_bit(card_dev, "spi", true);
    qdev_realize_and_unref(card_dev,
                           qdev_get_child_bus(sd_dev, "sd-bus"),
                           &error_fatal);

    moon3_binfo.board_id = 0x208;
    arm_load_kernel(cms->sa1110->cpu, machine, &moon3_binfo);
}

static void moon3_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "moon3 (SA-1110)";
    mc->init = moon3_init;
    mc->ignore_memory_transaction_failures = true;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("sa1110");
    mc->default_ram_size = 0x20000000;
    mc->default_ram_id = "strongarm.sdram";
}

static const TypeInfo moon3_machine_typeinfo = {
    .name = TYPE_MOON3_MACHINE,
    .parent = TYPE_MACHINE,
    .class_init = moon3_machine_class_init,
    .instance_size = sizeof(moon3MachineState),
};

static void moon3_machine_register_types(void)
{
    type_register_static(&moon3_machine_typeinfo);
}
type_init(moon3_machine_register_types);
