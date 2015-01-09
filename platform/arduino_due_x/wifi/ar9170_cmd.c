/*
 * ar9170_cmd.c
 *
 * Created: 1/31/2014 5:40:46 PM
 *  Author: Ioannis Glaropoulos
 */ 
#include "ar9170_cmd.h"
#include "ar9170_fwcmd.h"
#include "ar9170.h"
#include "interrupt\interrupt_sam_nvic.h"
#include <sys\errno.h>
#include <stdlib.h>

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif
#include "wire_digital.h"

uint32_t __attribute__((weak)) __div64_32(uint64_t *n, uint32_t base)
{
	         uint64_t rem = *n;
	         uint64_t b = base;
	         uint64_t res, d = 1;
	         uint32_t high = rem >> 32;
	
	         /* Reduce the thing a bit first */
	         res = 0;
	    if (high >= base) {
		                 high /= base;
		                 res = (uint64_t) high << 32;
		                 rem -= (uint64_t) (high*base) << 32;
	         }
	
	         while ((int64_t)b > 0 && b < rem) {
		                 b = b+b;
		                 d = d+d;
	         }
	
	        do {
		   if (rem >= b) {
			rem -= b;
			res += d;
		}
		b >>= 1;
		d >>= 1;
	} while (d);
	
	*n = res;
	return rem;
}

#define do_div(n,base) ({                              \
	uint32_t __base = (base);                       \
	uint32_t __rem;                                 \
	(void)(((typeof((n)) *)0) == ((uint64_t *)0));  \
	if (likely(((n) >> 32) == 0)) {                 \
		__rem = (uint32_t)(n) % __base;         \
		(n) = (uint32_t)(n) / __base;           \
	} else                                          \
		__rem = __div64_32(&(n), __base);       \
	__rem;                                          \
})

int 
ar9170_echo_test(struct ar9170 *ar, const uint32_t v)
{
	PRINTF("AR9170-usb: echo-test\n");
	uint32_t echores;
	int err;
	
	err = ar9170_exec_cmd(ar, AR9170_CMD_ECHO, 4, (uint8_t *)&v, 4, (uint8_t *)&echores);
	if (err) {
		PRINTF("AR9170-usb: cmd-error %u\n",err); 	
	 	return err;
	}
	if (v != echores) {
		PRINTF("wrong echo %x != %x", v, echores);
		return -EINVAL;
	}
	PRINTF("AR9170-usb: ECHO success\n");
	return 0;
}

 
 int 
 ar9170_write_reg(struct ar9170 *ar, const uint32_t reg, const uint32_t val)
{
	const le32_t buf[2] = {
		 cpu_to_le32(reg),
		 cpu_to_le32(val),
	 };
	 int err;
	 
	 err = ar9170_exec_cmd(ar, AR9170_CMD_WREG, sizeof(buf), (uint8_t *) buf, 0, NULL);
	 if (err) {
		 PRINTF("AR9170_CMD: cmd-error\n");
		/* 
		if (net_ratelimit()) {
			wiphy_err(ar->hw->wiphy, "writing reg %#x "
			  "(val %#x) failed (%d)\n", reg, val, err);
		 }
		 */
	 }
	 return err;
}

int 
ar9170_read_mreg(struct ar9170 *ar, const int nregs, 
	const uint32_t *regs, uint32_t *out)
{
	int i, err;
	le32_t *offs, *res;
	
	/* abuse "out" for the register offsets, must be same length */
	offs = (le32_t *)out;
	for (i = 0; i < nregs; i++)
		offs[i] = cpu_to_le32(regs[i]);
	
	/* also use the same buffer for the input */
	res = (le32_t *)out;
	
	err = ar9170_exec_cmd(ar, AR9170_CMD_RREG, 
		4 * nregs, (uint8_t *)offs, 4 * nregs, (uint8_t *)res);
	
	if (err) {
	/*	
		if (net_ratelimit()) {
			wiphy_err(ar->hw->wiphy, "reading regs failed (%d)\n", err);
		}
	*/
		PRINTF("AR9170_CMD: reading regs failed (%d)\n",err);	
		return err;
	}
	
	/* convert result to cpu endian */
	for (i = 0; i < nregs; i++)
		out[i] = le32_to_cpu(res[i]);
	
	return 0;
}

int 
ar9170_read_reg(struct ar9170 *ar, uint32_t reg, uint32_t *val)
{
	return ar9170_read_mreg(ar, 1, &reg, val);
}

int 
ar9170_reboot(struct ar9170 *ar)
{
	struct ar9170_cmd *cmd;
	int err;
	
	cmd = ar9170_cmd_buf(ar, AR9170_CMD_REBOOT_ASYNC, 0);
	if (!cmd)
		return -ENOMEM;
	
	err = __ar9170_exec_cmd(ar, cmd, 1);
	return err;
}

struct ar9170_cmd*
ar9170_cmd_buf(struct ar9170 *ar,
	const enum ar9170_cmd_oids cmd, const unsigned int len)
{
	struct ar9170_cmd *tmp;
	cpu_irq_enter_critical();
	tmp = calloc(1, sizeof(struct ar9170_cmd_head) + len);
	cpu_irq_leave_critical();
	if (tmp) {
		tmp->hdr.cmd = cmd;
	    tmp->hdr.len = len;
	}
	
	return tmp;
}

int 
ar9170_mac_reset(struct ar9170 *ar)
{
	return ar9170_exec_cmd(ar, AR9170_CMD_SWRST, 0, NULL, 0, NULL);
}

int 
ar9170_bcn_ctrl(struct ar9170 *ar, const unsigned int vif_id,
	const uint32_t mode, const uint32_t addr, const uint32_t len)
{
	PRINTF("AR9170_CMD: bcn-ctrl\n");
	struct ar9170_cmd *cmd;
	
	cmd = ar9170_cmd_buf(ar, AR9170_CMD_BCN_CTRL_ASYNC,
			sizeof(struct ar9170_bcn_ctrl_cmd));
	if (!cmd)
		return -ENOMEM;
	
	cmd->bcn_ctrl.vif_id = cpu_to_le32(vif_id);
	cmd->bcn_ctrl.mode = cpu_to_le32(mode);
	cmd->bcn_ctrl.bcn_addr = cpu_to_le32(addr);
	cmd->bcn_ctrl.bcn_len = cpu_to_le32(len);
	
	return __ar9170_exec_cmd(ar, cmd, 1);
}

int 
ar9170_collect_tally(struct ar9170 *ar)
{
	struct ar9170_tally_rsp tally;
	struct survey_info *info;
	unsigned int tick;
	int err;
	
	err = ar9170_exec_cmd(ar, AR9170_CMD_TALLY, 0, NULL, sizeof(tally), (uint8_t *)&tally);
	if (err)
		return err;
	
	tick = le32_to_cpu(tally.tick);
	if (tick) {
		ar->tally.active += le32_to_cpu(tally.active) / tick;
		ar->tally.cca += le32_to_cpu(tally.cca) / tick;
		ar->tally.tx_time += le32_to_cpu(tally.tx_time) / tick;
		ar->tally.rx_total += le32_to_cpu(tally.rx_total);
		ar->tally.rx_overrun += le32_to_cpu(tally.rx_overrun);
		
		if (ar->channel) {
			info = &ar->survey[ar->channel->hw_value];
			info->channel_time = ar->tally.active;
			info->channel_time_busy = ar->tally.cca;
			info->channel_time_tx = ar->tally.tx_time;
			do_div(info->channel_time, 1000);
			do_div(info->channel_time_busy, 1000);
			do_div(info->channel_time_tx, 1000);
		}
	}
	return 0;
}


int 
ar9170_powersave(struct ar9170 *ar, const bool ps)
{	
	wire_digital_write(DOZE_ACTIVE_PIN, HIGH);	
	struct ar9170_cmd *cmd;
	uint32_t state;
	
	cmd = 1;//ar9170_cmd_buf(ar, AR9170_CMD_PSM_ASYNC, sizeof(struct ar9170_psm)); XXX
	if (!cmd)
		return -ENOMEM;
	
	if (ps) {
		/* Sleep until next TBTT */
		state = AR9170_PSM_SLEEP | 1;
	} else {
		/* wake up immediately */
		state = 1;
	}
	
	cmd->psm.state = cpu_to_le32(state);
	return 0;//__ar9170_exec_cmd(ar, cmd, 1); XXX
}
