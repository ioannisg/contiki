/*
 * ar9170_cmd.h
 *
 * Created: 2/7/2014 6:11:31 PM
 *  Author: Ioannis Glaropoulos
 */ 
#include "ar9170.h"

#ifndef AR9170_CMD_H_
#define AR9170_CMD_H_

/* basic HW access */
int ar9170_write_reg(struct ar9170 *ar, const uint32_t reg, const uint32_t val);
int ar9170_read_mreg(struct ar9170 *ar, const int nregs, const uint32_t *regs, uint32_t *out);
int ar9170_read_reg(struct ar9170 *ar, uint32_t reg, uint32_t *val);

int ar9170_echo_test(struct ar9170 *ar, const uint32_t v);
int ar9170_reboot(struct ar9170 *ar);
int ar9170_mac_reset(struct ar9170 *ar);
int ar9170_powersave(struct ar9170 *ar, const bool ps);
int ar9170_bcn_ctrl(struct ar9170 *ar, const unsigned int vif_id, const uint32_t mode, const uint32_t addr, const uint32_t len);
int ar9170_collect_tally(struct ar9170 *ar);


static inline int ar9170_flush_cab(struct ar9170 *ar,
                                      const unsigned int vif_id)
{
	return ar9170_bcn_ctrl(ar, vif_id, AR9170_BCN_CTRL_DRAIN, 0, 0);
}

static inline int ar9170_rx_filter(struct ar9170 *ar,
	const unsigned int _rx_filter)
{
	le32_t rx_filter = cpu_to_le32(_rx_filter);
	
	return ar9170_exec_cmd(ar, AR9170_CMD_RX_FILTER,
		sizeof(rx_filter), (uint8_t *)&rx_filter,0, NULL);
}

struct ar9170_cmd* ar9170_cmd_buf(struct ar9170 *ar, 
	const enum ar9170_cmd_oids cmd, const unsigned int len);

/*
 * Macros to facilitate writing multiple registers in a single
 * write-combining USB command. Note that when the first group
 * fails the whole thing will fail without any others attempted,
 * but you won't know which write in the group failed.
 */
 #define ar9170_regwrite_begin(ar)                                     \
 do {                                                                    \
	int __nreg = 0, __err = 0;                                      \
    struct ar9170 *__ar = ar;
  
 #define ar9170_regwrite(r, v) do {                                    \
	__ar->cmd_buf[2 * __nreg + 1] = cpu_to_le32(r);                 \
    __ar->cmd_buf[2 * __nreg + 2] = cpu_to_le32(v);                 \
    __nreg++;                                                       \
    if ((__nreg >= PAYLOAD_MAX / 2)) {                              \
		if (IS_ACCEPTING_CMD(__ar))                             \
			__err = ar9170_exec_cmd(__ar,                 \
	        AR9170_CMD_WREG, 8 * __nreg,          \
            (uint8_t *) &__ar->cmd_buf[1], 0, NULL);     \
		else                                                    \
			goto __regwrite_out;                            \
                                                                         \
		__nreg = 0;                                             \
        if (__err)                                              \
			goto __regwrite_out;                            \
	}                                                               \
} while (0)
 
#define ar9170_regwrite_finish()                                      \
__regwrite_out :                                                        \
	if (__err == 0 && __nreg) {                                     \
		if (IS_ACCEPTING_CMD(__ar))                             \
			__err = ar9170_exec_cmd(__ar,                 \
				AR9170_CMD_WREG, 8 * __nreg,          \
				(uint8_t *) &__ar->cmd_buf[1], 0, NULL);     \
	             __nreg = 0;                                             \
	}
 
#define ar9170_regwrite_result()                                      \
	__err;                                                          \
} while (0)
 
 
#define ar9170_async_regwrite_get_buf()                               \
do {                                                                    \
	__nreg = 0;                                                     \
	__cmd = ar9170_cmd_buf(__carl, AR9170_CMD_WREG_ASYNC,       \
                                  AR9170_MAX_CMD_PAYLOAD_LEN);         \
   if (__cmd == NULL) {                                            \
		__err = -ENOMEM;                                        \
        goto __async_regwrite_out;                              \
   }                                                               \
} while (0)
 
#define ar9170_async_regwrite_begin(carl)                             \
do {                                                                    \
	struct ar9170 *__carl = carl;                                   \
    struct ar9170_cmd *__cmd;                                     \
    unsigned int __nreg;                                            \
	int  __err = 0;                                                 \
    ar9170_async_regwrite_get_buf();                              \
 
#define ar9170_async_regwrite_flush()                                 \
do {                                                                    \
	if (__cmd == NULL || __nreg == 0)                               \
    break;                                                  \
                                                                         \
	if (IS_ACCEPTING_CMD(__carl) && __nreg) {                       \
		__cmd->hdr.len = 8 * __nreg;                            \
		__err = __ar9170_exec_cmd(__carl, __cmd, true);       \
		__cmd = NULL;                                           \
		break;                                                  \
	}                                                               \
    goto __async_regwrite_out;                                      \
} while (0)
 
#define ar9170_async_regwrite(r, v) do {                              \
	if (__cmd == NULL)                                              \
		ar9170_async_regwrite_get_buf();                      \
		__cmd->wreg.regs[__nreg].addr = cpu_to_le32(r);                 \
		__cmd->wreg.regs[__nreg].val = cpu_to_le32(v);                  \
        __nreg++;                                                       \
		if ((__nreg >= PAYLOAD_MAX / 2))                                \
			ar9170_async_regwrite_flush();                        \
} while (0)
 
#define ar9170_async_regwrite_finish() do {                           \
__async_regwrite_out:                                                   \
	if (__cmd != NULL && __err == 0)                                \
		ar9170_async_regwrite_flush();                        \
		free(__cmd);                                                   \
} while (0)                                                             \
 
#define ar9170_async_regwrite_result()                                \
__err;                                                          \
} while (0)

#endif /* AR9170_CMD_H_ */