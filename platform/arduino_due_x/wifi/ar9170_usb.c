/*
 * ar9170_usb.c
 *
 * Created: 2/1/2014 8:41:59 PM
 *  Author: Ioannis Glaropoulos
 */ 
#include "ar9170.h"
#include "ar9170_cmd.h"
#include "ar9170-drv.h"
#include "contiki.h"
#include <sys\errno.h>
#include "ar9170_fwcmd.h"
#include "interrupt\interrupt_sam_nvic.h"

#include "usbstack.h"

#include <stdlib.h>
#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#include "platform-conf.h"
#if WITH_WIFI_SUPPORT



static void 
ar9170_usb_cancel_urbs(struct ar9170 *ar)
{
	ar9170_set_state(ar, AR9170_UNKNOWN_STATE);
	/* Cancel all listening */
	USB_STACK.cancel_bulk_in();
	USB_STACK.cancel_int_in();
}


void
ar9170_usb_stop(struct ar9170 *ar)
{
	ar9170_set_state_when(ar, AR9170_IDLE, AR9170_STOPPED);
	
	/*
	 * Note:
	 * So far we freed all tx urbs, but we won't dare to touch any rx urbs.
	 * Else we would end up with a unresponsive device...
	 */
}

int 
ar9170_usb_open(struct ar9170 *ar)
{	
	printf("AR9170-usb: open\n");
	ar9170_set_state_when(ar, AR9170_STOPPED, AR9170_IDLE);
	if(ar->state != AR9170_IDLE) {
		printf("AR9170_USB: state should be idle\n");
		return -EINVAL;
	}
	return 0;
}

static int 
ar9170_usb_load_firmware(struct ar9170 *ar)
{
	const uint8_t *data;
	uint8_t *buf;
	unsigned int transfer;
	size_t len;
	uint32_t addr;
	int err = 0;
	
	buf = malloc(4096);
	if (!buf) {
		err = -ENOMEM;
		goto err_out;
	}
	PRINTF("AR9170-usb: load fw\n");
	data = ar->fw.fw->fw_data;
	len = ar->fw.fw->fw_size;
	addr = ar->fw.address;
	
	/* this removes the miniboot image */
	data += ar->fw.offset;
	len -= ar->fw.offset;
	
	/* Set the FW wait flag */
	AR9170_GET_LOCK(&ar->fw_boot_wait);
	
	while (len) {
		transfer = min(len, 4096u);
		memcpy(buf, data, transfer);
		
		err = USB_STACK.send_ctrl(buf, transfer, 0x30, addr >> 8);
		
		//err = usb_control_msg(ar->udev, usb_sndctrlpipe(ar->udev, 0),
		                                       //0x30 /* FW DL */, 0x40 | USB_DIR_OUT,
		                                       //addr >> 8, 0, buf, transfer, 100);
		
		if (err < 0) {
			free(buf);
			goto err_out;
		}
		
		len -= transfer;
		data += transfer;
		addr += transfer;
	}
	free(buf);
	
	err = USB_STACK.send_ctrl(NULL, 0, 0x31, 0);
	
	//err = usb_control_msg(ar->udev, usb_sndctrlpipe(ar->udev, 0),
	  // 0x31 /* FW DL COMPLETE */,
	  // 0x40 | USB_DIR_OUT, 0, 0, NULL, 0, 200);
	
	AR9170_WAIT_FOR_LOCK_CLEAR_TIMEOUT_MS(&ar->fw_boot_wait, (100*AR9170_RSP_TIMEOUT_MS));
	if (AR9170_IS_RSP_TIMEOUT) {
		PRINTF("AR9170-usb: FW boot timeout\n");
		err = -ETIMEDOUT;
		goto err_out;
	}
	err = ar9170_echo_test(ar, 0x4a110123);
	if (err)
		goto err_out;
	
	/* now, start the command response counter */
	ar->cmd_seq = -1;
	
	return 0;
	
err_out:
	PRINTF("firmware upload failed (%d).\n", err);
	return err;
}


static int 
ar9170_usb_init_device(struct ar9170 *ar)
{
	printf("AR9170-usb: init-device\n");
	int err;
	/*
	 * The carl9170 firmware let's the driver know when it's
	 * ready for action. But we have to be prepared to gracefully
	 * handle all spurious [flushed] messages after each (re-)boot.
	 * Thus the command response counter remains disabled until it
	 * can be safely synchronized.
	 */
	ar->cmd_seq = -2;
	
	/* Initialize listening on the BULK endpoint */
	err = USB_STACK.init_bulk_in();
	if (err)
		goto err_out;
		
	/* Initialize listening on the INT endpoint */
	err = USB_STACK.init_int_in();
	if (err)
		goto err_unrx;
	
	/* Open USB device */
	err = ar9170_usb_open(ar);
	if (err)
		goto err_out;
			
	/* Start firmware upload */		
	err = ar9170_usb_load_firmware(ar);
	if (err)
		goto err_stop;
		
	return 0;
	
err_stop:
	ar9170_usb_stop(ar);

err_unrx:
	ar9170_usb_cancel_urbs(ar);
	
err_out:
	return err;	
}

void 
ar9170_usb_firmware_finish(struct ar9170 *ar)
{
    int err;

	err = ar9170_parse_firmware(ar);
    if (err)
		goto err_freefw;
	
	err = ar9170_usb_init_device(ar);
	if (err)
		goto err_freefw;
	
	err = ar9170_register(ar);
	
	ar9170_usb_stop(ar);
	if (err)
		goto err_unrx;
		
	err = ar9170_usb_open(ar);
	if (err)
		goto err_unrx;
	//complete(&ar->fw_load_wait);
	//usb_put_dev(ar->udev);
	return;
	
err_unrx:
	//carl9170_usb_cancel_urbs(ar);
	;
err_freefw:;
	;
	//carl9170_release_firmware(ar);
	//carl9170_usb_firmware_failed(ar);
}


/** 
 * Buffer the command 
 */
int 
__ar9170_exec_cmd(struct ar9170 *ar, struct ar9170_cmd *cmd,
                         const uint8_t free_buf)
{		
	int err = 0;
	
	if (!IS_INITIALIZED(ar)) {
		err = -EPERM;
		goto err_free;
	}
	
	if (cmd->hdr.len > AR9170_MAX_CMD_LEN - 4) {
		err = -EINVAL;
		goto err_free;
	}
	
	AR9170_WAIT_FOR_LOCK_CLEAR_TIMEOUT_MS(&ar->cmd_wait, AR9170_RSP_TIMEOUT_MS);
	if(AR9170_IS_RSP_TIMEOUT) {
		printf("AR9170_DRV: cmd-rsp-timeout-why?\n");
		AR9170_RELEASE_LOCK(&ar->cmd_wait);
		return AR9170_DRV_TX_ERR_TIMEOUT;
	}
	AR9170_GET_LOCK(&ar->cmd_wait);
	
	/* Copy the command content to the driver command buffer. */
	//ar->cmd_buffer.cmd_data = cmd->data;
	memcpy(ar->cmd_buffer.cmd_data, cmd, cmd->hdr.len + 4);
	ar->cmd_buffer.cmd_len = cmd->hdr.len + 4;
	
	if(free_buf) {
		PRINTF("AR9170_USB: free cmd mem\n");
		cpu_irq_enter_critical();
		free(cmd);
		cpu_irq_leave_critical();
	}
	
	/* Queue the command to the pending command list */
	//return WIFI_STACK.queue_tx_cmd();
	return ar9170_driver.send_sync_cmd(0);
		
err_free:
	return err;
}

/** 
 * Send the command immediately 
 */
int 
___ar9170_exec_cmd(struct ar9170 *ar, struct ar9170_cmd *cmd,
                         const uint8_t is_sync)
{	
	int err = 0;
	
	if (!IS_INITIALIZED(ar)) {
		err = -EPERM;
		goto err_free;
	}
	
	if (cmd->hdr.len > AR9170_MAX_CMD_LEN - 4) {
		err = -EINVAL;
		goto err_free;
	}
	/* This is really redundant TODO FIXME */
	memcpy(ar->cmd_buffer.cmd_data, cmd, cmd->hdr.len + 4);
	ar->cmd_buffer.cmd_len = cmd->hdr.len + 4;
	
	/* Send the command with or without the sync flag */
	return ar9170_driver.send_sync_cmd(is_sync);
		
err_free:
	printf("AR9170-usb: ___exec_cmd error %u\n",err);
	return err;
}


int
ar9170_exec_cmd(struct ar9170 *ar, const enum ar9170_cmd_oids cmd,
	unsigned int plen, void *payload, unsigned int outlen, void *out)
{
	int err = -ENOMEM;
	
	if (!IS_ACCEPTING_CMD(ar))
		return -EIO;
	
	if (!(cmd & AR9170_CMD_ASYNC_FLAG)) {
		//might_sleep();
	}
	
	/* Wait for the response of the previous command. 
	 * The response will come in a USB interrupt, so
	 * this function should be called in thread mode,
	 * or inside interrupt context with low-priority.
	 */
	AR9170_WAIT_FOR_LOCK_CLEAR_TIMEOUT_MS(&ar->cmd_wait, AR9170_RSP_TIMEOUT_MS);
	if(AR9170_IS_RSP_TIMEOUT) {
		printf("AR9170_DRV: cmd-rsp-timeout-why?\n");
		AR9170_RELEASE_LOCK(&ar->cmd_wait);
		return AR9170_DRV_TX_ERR_TIMEOUT;
	}
	AR9170_GET_LOCK(&ar->cmd_wait);
	/* From now on, we have locked the cmd_wait, so if
	 * a command shall be sent with higher priority it
	 * will have to wait and queue up. This also means
	 * that we don't really expect a command response.
	 */
		
	/* Place the command inside the driver command buffer */
	ar->cmd.hdr.len = plen;
	ar->cmd.hdr.cmd = cmd;
	/* writing multiple regs fills this buffer already */
	if (plen && payload != (uint8_t *)(ar->cmd.data))
		memcpy(ar->cmd.data, payload, plen);
	cpu_irq_enter_critical();
	ar->readbuf = (uint8_t *)out;
	ar->readlen = outlen;
	cpu_irq_leave_critical();
	
	if (!(cmd & AR9170_CMD_ASYNC_FLAG)) {
		
		err = ___ar9170_exec_cmd(ar, &ar->cmd, 1);
	
	} else {

		err = ___ar9170_exec_cmd(ar, &ar->cmd, 0);
	}
	
	if (err != 0) {
		PRINTF("AR9170-usb: ___exec_cmd possible timeout? %u (%u)\n",err,cmd);
		err = -ETIMEDOUT;
		goto err_unbuf;
	}
		
	if (ar->readlen != outlen) {
		err = -EMSGSIZE;
		goto err_unbuf;
	}
	
	return 0;
	
err_unbuf:
	/* Maybe the device was removed in the moment we were waiting? */
	if (IS_STARTED(ar)) {
		PRINTF("no command feedback received (%d).\n", err);
		
		/* provide some maybe useful debug information */
		//print_hex_dump_bytes("ar9170 cmd: ", DUMP_PREFIX_NONE,
		  //                                    &ar->cmd, plen + 4);
		
		//ar9170_restart(ar, AR9170_RR_COMMAND_TIMEOUT);
	}
	
	/* invalidate to avoid completing the next command prematurely */
	cpu_irq_enter_critical();
	ar->readbuf = NULL;
	ar->readlen = 0;
	cpu_irq_leave_critical();
	
	return err;
}
#endif /* WITH_WIFI_SUPPORT */