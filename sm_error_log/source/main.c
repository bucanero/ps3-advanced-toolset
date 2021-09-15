/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <net/net.h>

#include <lv2_syscall.h>
#include <udp_printf.h>

#define DUMP_FILENAME				"sm_error.log"

static const char *dump_path[] = {
	"/dev_usb000/" DUMP_FILENAME,
	"/dev_usb001/" DUMP_FILENAME,
	"/dev_usb002/" DUMP_FILENAME,
	"/dev_usb003/" DUMP_FILENAME,
	"/dev_usb004/" DUMP_FILENAME,
	"/dev_usb005/" DUMP_FILENAME,
	"/dev_usb006/" DUMP_FILENAME,
	"/app_home/" DUMP_FILENAME,
};

struct __attribute__((__packed__)) system_info {
	uint8_t firmware_version_high;
	uint16_t firmware_version_low;
	uint8_t reserved;
	uint8_t unk1[4];
	char platform_id[8];
	uint32_t firmware_build;
	uint8_t unk2[4];
};

static inline int sys_sm_get_system_info(struct system_info* unknown0)
{
	lv2syscall1(387, (uint64_t)unknown0);
	return_to_user_prog(int);
}

static inline int sys_sm_request_error_log(uint8_t offset, uint8_t* unknown0, uint32_t* unknown1, uint32_t* unknown2)
{
	lv2syscall4(390, (uint64_t)offset, (uint64_t)unknown0, (uint64_t)unknown1, (uint64_t)unknown2);
	return_to_user_prog(int);
}

static inline int sys_sm_request_be_count(uint8_t* unknown0, uint32_t* unknown1, uint32_t* unknown2, uint32_t* unknown3)
{
	lv2syscall4(391, (uint64_t)unknown0, (uint64_t)unknown1, (uint64_t)unknown2, (uint64_t)unknown3);
	return_to_user_prog(int);
}

static inline int sys_sm_get_hw_config(uint8_t* unknown0, uint64_t* unknown1)
{
	lv2syscall2(393, (uint64_t)unknown0, (uint64_t)unknown1);
	return_to_user_prog(int);	
}

static inline int sys_sm_request_scversion(uint64_t* unknown0, uint64_t* unknown1, uint64_t* unknown2)
{
	lv2syscall3(394, (uint64_t)unknown0, (uint64_t)unknown1, (uint64_t)unknown2);
	return_to_user_prog(int);	
}

static inline int sys_ss_appliance_info_manager_get_ps_code(uint8_t* unknown0)
{
	lv2syscall2(867, (uint64_t)0x19004, (uint64_t)unknown0);
	return_to_user_prog(int);
}

/*
 * open_dump
 */
static FILE *open_dump(void)
{
	FILE *fp;
	int i;

	fp = NULL;

	for (i = 0; i < 8; i++) {
		PRINTF("%s:%d: trying path '%s'\n", __func__, __LINE__, dump_path[i]);

		fp = fopen(dump_path[i], "w");
		if (fp)
			break;
	}

	if (fp)
		PRINTF("%s:%d: path '%s'\n", __func__, __LINE__, dump_path[i]);
	else
		PRINTF("%s:%d: file could not be opened\n", __func__, __LINE__);

	return fp;
}

/*
 * main
 */
int main(int argc, char **argv)
{
	FILE *fp;
	uint32_t ret;

	netInitialize();
	udp_printf_init();

	PRINTF("%s:%d: Start\n", __func__, __LINE__);

	fp = open_dump();
	if (!fp)
		goto done;

	struct system_info hwinfo = {0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, 0, {0, 0, 0, 0}};
	ret = sys_sm_get_system_info(&hwinfo);
	if(!ret) {
		fprintf(fp, "Firmware Version: %01x.%02x (build %d)\n", hwinfo.firmware_version_high, hwinfo.firmware_version_low >> 4, hwinfo.firmware_build);
		fprintf(fp, "Platform ID: %s\n", hwinfo.platform_id);
	} else {
		PRINTF("%s:%d: sys_sm_get_system_info failed\n", __func__, __LINE__);
	}
	
	uint8_t pscode[8];
	ret = sys_ss_appliance_info_manager_get_ps_code(pscode);
	if(!ret) {
		fprintf(fp, "Product Code: %02X %02X\n", pscode[2], pscode[3]);
		fprintf(fp, "Product Sub Code: %02X %02X\n", pscode[4], pscode[5]);
	} else {
		PRINTF("%s:%d: sys_ss_appliance_info_manager_get_ps_code failed\n", __func__, __LINE__);
	}

	uint8_t status;
	uint64_t hardware_info;
	ret = sys_sm_get_hw_config(&status, &hardware_info);
	if(!ret && !status) {
		fprintf(fp, "Hardware Config: %016lX\n", hardware_info);
	} else {
		PRINTF("%s:%d: sys_sm_get_hw_config failed\n", __func__, __LINE__);
	}

	uint64_t soft_id;
	uint64_t patch_id_rom;
	uint64_t patch_id_ram;
	ret = sys_sm_request_scversion(&soft_id, &patch_id_rom, &patch_id_ram);
	if(!ret){
		fprintf(fp, "Syscon Fimware Version: %04lX.%016lX (EEPROM: %016lX)\n", soft_id, patch_id_ram, patch_id_rom);
	} else {
		PRINTF("%s:%d: sys_sm_request_scversion failed\n", __func__, __LINE__);
	}

	uint32_t run_time;
	uint32_t bringup_ct;
	uint32_t shutdown_ct;
	ret = sys_sm_request_be_count(&status, &run_time, &bringup_ct, &shutdown_ct);
	if(!ret && !status) {
		fprintf(fp, "\nBringup Count: %d, Shutdown Count: %d\n", bringup_ct, shutdown_ct);
		uint32_t days = run_time/86400;
		uint32_t hours = (run_time-days*86400)/3600;
		uint32_t minutes = ((run_time-days*86400)-hours*3600)/60;
		uint32_t seconds = ((run_time-days*86400)-hours*3600)-minutes*60;
		fprintf(fp, "Runtime: %d Days, %d Hours, %d Minutes, %d Seconds\n", days, hours, minutes, seconds);
	} else {
		PRINTF("\n%s:%d: sys_sm_request_be_count failed\n", __func__, __LINE__);
	}

	uint32_t error_code;
	uint32_t error_time;
	time_t print_time;
	fprintf(fp, "\nError Log\n");
	for(int i = 0; i < 0x20; i++) {
		ret = sys_sm_request_error_log(i, &status, &error_code, &error_time);
		if(!ret && !status) {
			error_time += 946684800;
			print_time = (time_t) error_time;
			fprintf(fp, "%02d: %08X  %s", i + 1, error_code, ctime(&print_time));
		} else {
			PRINTF("\n%s:%d: sys_sm_request_error_log failed\n", __func__, __LINE__);
			break;
		}
	}

done:
	if (fp)
		fclose(fp);

	PRINTF("\n%s:%d: End\n", __func__, __LINE__);

	udp_printf_deinit();
	netDeinitialize();

	return 0;
}
