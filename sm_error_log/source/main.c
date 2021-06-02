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

#include <lv2_syscall.h>
#include <udp_printf.h>

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

/*
 * main
 */
int main(int argc, char **argv)
{

	PRINTF("%s:%d: Start\n", __func__, __LINE__);

	uint32_t ret;
	uint8_t value0;
	uint32_t value1;
	uint32_t value2;
	uint32_t value3;

	PRINTF("\n%s:%d: sys_sm_request_error_log\n", __func__, __LINE__);
	for(int i = 0; i < 0x20; i++) {
		ret = sys_sm_request_error_log(i, &value0, &value1, &value2);
		PRINTF("%02d 0x%02X 0x%08X 0x%08X (0x%08X)\n", i, value0, value1, value2, ret);
	}

	PRINTF("\n%s:%d: sys_sm_request_be_count\n", __func__, __LINE__);
	ret = sys_sm_request_be_count(&value0, &value1, &value2, &value3);
	PRINTF("0x%02X 0x%08X 0x%08X 0x%08X (0x%08X)\n", value0, value1, value2, value3, ret);


	PRINTF("\n%s:%d: End\n", __func__, __LINE__);


	return 0;
}
