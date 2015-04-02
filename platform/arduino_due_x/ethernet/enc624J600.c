/*
 * enc624J600.c
 *
 * Created: 2014-08-18 18:18:30
 *  Author: Ioannis Glaropoulos
 */ 
#include "platform/arduino_due_ath9170/arduino/wire_digital.h"
#include "enc424J600-conf.h"
#include "enc424j600.h"
#include "enc424j600-spi.h"

#include "delay.h"

#include "contiki-conf.h"
#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#ifdef ETHERNET_CONF_IRQ_LEVEL
#define ENC424J600_IRQ_LEVEL ETHERNET_CONF_IRQ_LEVEL
#else
#define ENC424J600_IRQ_LEVEL 4
#endif

// Binary constant identifiers for ReadMemoryWindow() and WriteMemoryWindow()
// functions
#define UDA_WINDOW		(0x1)
#define GP_WINDOW			(0x2)
#define RX_WINDOW			(0x4)

// Internal MAC level variables and flags.
static uint8_t current_bank;
static uint16_t next_pkt_ptr;

static uint8_t enc424j600_full_dpx_mod;
static uint8_t enc424j600_speed;

static uint8_t enc424j600_frame_buf[ENC424J600_MAX_RX_FRAME_LEN];
static uint16_t enc424j600_frame_len;

// Static functions
static uint8_t enc424j600_send_system_reset(void);

static uint8_t enc424j600_mac_is_tx_ready(void);
static uint8_t enc424j600_mac_flush(void);

static uint8_t enc424j600_conf_phy(void);
static uint8_t enc424j600_conf_duplex_mode(void);

static uint8_t enc424j600_checksum_calculation(uint16_t position, uint16_t length, 
  uint16_t seed, uint16_t* ret_val);

static uint8_t enc424j600_write_memory_window(uint8_t window, uint8_t *data, uint16_t length);
static uint8_t enc424j600_read_memory_window(uint8_t window, uint8_t *data, uint16_t length);

static uint8_t enc424j600_read_reg(uint16_t address, uint16_t* ret_val);
static uint8_t enc424j600_write_reg(uint16_t address, uint16_t data);

static uint8_t enc424j600_read_phy_reg(uint8_t address, uint16_t* ret_val);
static uint8_t enc424j600_write_phy_reg(uint8_t address, uint16_t Data);

static void enc424j600_BFS_reg(uint16_t address, uint16_t bitMask);
static void enc424j600_BFC_reg(uint16_t address, uint16_t bitMask);

static enc424j600_irq_cb recv_cb;

/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_get_speed(void)
{
	return enc424j600_speed;
}
/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_get_dpx_mode(void)
{
	return enc424j600_full_dpx_mod;
}
/*---------------------------------------------------------------------------*/
void
enc424j600_set_irq_callback(enc424j600_irq_cb cb)
{
	if (cb != NULL)
	recv_cb = cb;
}
/*---------------------------------------------------------------------------*/
uint16_t
enc424j600_get_recv_frame_len(void)
{
  return enc424j600_frame_len;
}
/*---------------------------------------------------------------------------*/
uint8_t*
enc424j600_get_recv_frame_ptr(void)
{
  return enc424j600_frame_buf;
}
/*---------------------------------------------------------------------------*/
static void
enc424j600_irq_handler(uint32_t first, uint32_t second)
{
  UNUSED(first);
  UNUSED(second);

  uint8_t result = ENC424J600_STATUS_OK;
  uint16_t reg_val;
  /* Clear the "Interrupt Enable" bit [DS39935C-page 117] */
  enc424j600_BFC_reg((EIE), (EIE_INTIE));
  
  /* Read the "Interrupt Status" register */
  uint16_t irq_status = 0;
  if ((result = enc424j600_read_reg((EIR), &irq_status)) != ENC424J600_STATUS_OK)
    goto err_exit;

  if (unlikely(irq_status == 0)) {
    printf("enc424j600: received-irq-status-0\n");
  }

  /* PHY Link Status Change Interrupt? */
  if (irq_status & EIR_LINKIF) {
   
    /* Clear "Link Update" interrupt */
    enc424j600_BFC_reg((EIR), (EIR_LINKIF));
  
    /* Read the PHY Link Status [ON or OFF] */
    if ((result = enc424j600_read_reg((ESTAT), &reg_val)) != ENC424J600_STATUS_OK)
      goto err_exit;
		
    /* Configure PHY if link is up */
	 uint16_t link_status;
	 
    if (reg_val & ESTAT_PHYLNK) {
      link_status = ENC424J600_IRQ_STATUS_LINK_UP;
      if (enc424j600_conf_phy() != ENC424J600_STATUS_OK) {
        PRINTF("enc424j600: phy-conf-err\n");
		}
	 } else {
      link_status = ENC424J600_IRQ_STATUS_LINK_DN;
	 }
    /* Execute handler for DRIVER */
    if (recv_cb != NULL)
      recv_cb((uint32_t)(link_status));
  }  

  /* Frame(s) is received? */
  if (irq_status & EIR_PKTIF) {
	  
    /* Process all pending received packets */
    uint16_t status_val;
	 /* Verify that a frame is really pending in the RX buffer */
    if ((result = enc424j600_read_reg(ESTAT, &status_val)) != ENC424J600_STATUS_OK)
	   goto err_exit;

    while ((status_val & 0xff)) {
		 /* Process a pending packet */
       if ((result = enc424j600_pkt_recv(ENC424J600_MAX_RX_FRAME_LEN, 
		   enc424j600_frame_buf, &enc424j600_frame_len)) != ENC424J600_STATUS_OK)
			PRINTF("enc424j600: rx-err\n");
		 else {
			 if (recv_cb != NULL)
			   recv_cb((uint32_t)ENC424J600_IRQ_STATUS_RX);
		 }
		/* Read again the status register to check if more packets are pending */
      if ((result = enc424j600_read_reg(ESTAT, &status_val)) != ENC424J600_STATUS_OK)
        goto err_exit;
	 }
	 /* Clear the "PKT RECV" interrupt flag */
	 enc424j600_BFC_reg((EIR), (EIR_PKTIF));
  }
  
  
  /* Frame transmission complete? */
  if (irq_status & EIR_TXIF) {
    if (recv_cb != NULL)
      recv_cb((uint32_t)ENC424J600_IRQ_STATUS_TX_COMP);
		
    enc424j600_BFC_reg((EIR), (EIR_TXIF));
  }
  
  /* Frame transmission aborted? */
  if (irq_status & EIR_TXABTIF) {
	  if (recv_cb != NULL)
	  recv_cb((uint32_t)ENC424J600_IRQ_STATUS_TX_ABRT);
	  
	  enc424j600_BFC_reg((EIR), (EIR_TXABTIF));
  }
  
  /* Perform a read on the Interrupt status register */
  result = enc424j600_read_reg(EIR, &irq_status);
    
  /* Re-enable interrupts from the peripheral once all source are serviced */
  enc424j600_BFS_reg((EIE), (EIE_INTIE));
  return;
err_exit:
  /* Maybe debug? */
  printf("enc424j600: irq-handler-err\n");
  return;
}
/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_init(void)
{
  uint8_t result = ENC424J600_STATUS_OK;

  /* Initialize SPI bus [SPI0] on Master mode,
   * and on default configuration [default CS]
	*/
  enc424j600_spi_init();
  
  delay_ms(10);
  
  /* Set default bank on init. */
  if ((result = enc424j600_execute_op0(B0SEL)) != ENC424J600_STATUS_OK)
    return result;
  
  current_bank = 0;
   
  /* Configure ENC624J600 interrupt pin for input and
   * register interrupt generation on a falling edge.
	*/
  configure_input_pin_interrupt_enable(ENC624J600_IRQ_PIN, PIO_PULLUP, 
    PIO_IT_FALL_EDGE, enc424j600_irq_handler, ENC424J600_IRQ_LEVEL);
  
  /* Initialize RX tracking variables and other control state flags */
  next_pkt_ptr = RXSTART;
   
  /* Perform a system reset */
  if ((result = enc424j600_send_system_reset()) != ENC424J600_STATUS_OK)
    return result;

  /* Disable CLOCKOUT output */
  if ((result = enc424j600_write_reg(ECON2, ECON2_ETHEN | ECON2_STRCH)) 
    != ENC424J600_STATUS_OK)
    return result;
  
  /* Set receiver buffer location */
  if ((result = enc424j600_write_reg(ERXST, RXSTART)) != ENC424J600_STATUS_OK)
    return result;
	 
  /* Program the RX tail */
  if ((result = enc424j600_write_reg(ERXTAIL, RAMSIZE - 2)) != ENC424J600_STATUS_OK)
    return result;

  /* Initialize the hash table */
  if ((result = enc424j600_write_reg(EHT1, 0)) != ENC424J600_STATUS_OK)
    return result;
  if ((result = enc424j600_write_reg(EHT2, 0)) != ENC424J600_STATUS_OK)
    return result;
  if ((result = enc424j600_write_reg(EHT3, 0)) != ENC424J600_STATUS_OK)
    return result;
  if ((result = enc424j600_write_reg(EHT4, 0)) != ENC424J600_STATUS_OK)
    return result; 

  /* Configure the receiver filters */
  if ((result = enc424j600_write_reg(ERXFCON,
    (ERXFCON_CRCEN | 
	  ERXFCON_RUNTEN | 
	  ERXFCON_HTEN |
	  ERXFCON_UCEN | 
	  ERXFCON_MCEN |
#ifdef PROMISCUOUS_MODE	  
	   //ERXFCON_NOTMEEN | 
#endif
#if NETSTACK_CONF_WITH_IP64
     ERXFCON_BCEN |
#endif
	  //ERXFCON_BCEN)))
	  0)))
    != ENC424J600_STATUS_OK)
    return result;  
  
  /* Set maximum receive frame length */
  if ((result = enc424j600_write_reg(MAMXFL, 1518)) != ENC424J600_STATUS_OK)
    return result;
  
  /* Set up TX/UDA buffer addresses */
  if ((result = enc424j600_write_reg(ETXST, TXSTART)) != ENC424J600_STATUS_OK)
    return result;    
  if ((result = enc424j600_write_reg(EUDAST, USSTART)) != ENC424J600_STATUS_OK)
    return result;
  if ((result = enc424j600_write_reg(EUDAND, USEND)) != ENC424J600_STATUS_OK)
    return result;

  // Set PHY Auto-negotiation to support 10BaseT Half duplex,
  // 10BaseT Full duplex, 100BaseTX Half Duplex, 100BaseTX Full Duplex,
  // and symmetric PAUSE capability
  if ((result = enc424j600_write_phy_reg(PHANA, 
    PHANA_ADPAUS0 | PHANA_AD10FD | PHANA_AD10 | PHANA_AD100FD | PHANA_AD100 | PHANA_ADIEEE0)) 
	 != ENC424J600_STATUS_OK)
    return result;
  
  /* Clear all interrupt flags */
  if ((result = enc424j600_write_reg(EIR, 0x0000)) != ENC424J600_STATUS_OK)
    return result;

  /* Configure interrupt sources */
  if ((result = enc424j600_write_reg(EIE, EIE_INTIE | EIE_LINKIE | EIE_PKTIE | EIE_TXIE | EIE_TXABTIE)) 
    != ENC424J600_STATUS_OK)
    return result;

  /* Enable packet reception */
  enc424j600_BFS_reg(ECON1, ECON1_TXRTS | ECON1_RXEN);
  
  /* Initialization is completed */
  return result;
}
/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_is_linked(void) {
  
  uint16_t reg_val;
  if(enc424j600_read_reg((ESTAT), &reg_val) != ENC424J600_STATUS_OK)
    return 0;
	 
  return (reg_val & ESTAT_PHYLNK) != 0u;
}
/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_set_macaddr(uint8_t* macAddr)
{
  /* Write a new MAC address on the Ethernet device */
  uint16_t reg_value;
  uint8_t result = ENC424J600_STATUS_OK;
  
  ((uint8_t*) & reg_value)[0] = *macAddr++;
  ((uint8_t*) & reg_value)[1] = *macAddr++;
  if ((result = enc424j600_write_reg(MAADR1, reg_value)) != ENC424J600_STATUS_OK)
    return result;
  
  ((uint8_t*) & reg_value)[0] = *macAddr++;
  ((uint8_t*) & reg_value)[1] = *macAddr++;
  if ((result = enc424j600_write_reg(MAADR2, reg_value)) != ENC424J600_STATUS_OK)
    return result;  

  ((uint8_t*) & reg_value)[0] = *macAddr++;
  ((uint8_t*) & reg_value)[1] = *macAddr++;
  result = enc424j600_write_reg(MAADR3, reg_value);
  
  return result; 
}
/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_get_macaddr(uint8_t* macAddr)
{
  /* Read the MAC address */
  uint16_t reg_value;
  uint8_t result = ENC424J600_STATUS_OK;
  
  if ((result = enc424j600_read_reg(MAADR1, &reg_value)) != ENC424J600_STATUS_OK)
    return result;

  *macAddr++ = ((uint8_t*) & reg_value)[0];
  *macAddr++ = ((uint8_t*) & reg_value)[1];

  if ((result = enc424j600_read_reg(MAADR2, &reg_value)) != ENC424J600_STATUS_OK)
    return result;
  
  *macAddr++ = ((uint8_t*) & reg_value)[0];
  *macAddr++ = ((uint8_t*) & reg_value)[1];
  
  if ((result = enc424j600_read_reg(MAADR3, &reg_value)) != ENC424J600_STATUS_OK)
    return result;

  *macAddr++ = ((uint8_t*) & reg_value)[0];
  *macAddr++ = ((uint8_t*) & reg_value)[1];
  
  return result;
}
/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_powersave_disable(void)
{
  uint8_t result = ENC424J600_STATUS_OK;
  
  //Wake-up Ethernet interface
  enc424j600_BFS_reg(ECON2, ECON2_ETHEN);
  enc424j600_BFS_reg(ECON2, ECON2_STRCH);
  //Wake-up PHY
  uint16_t state;
  if ((result = enc424j600_read_phy_reg(PHCON1, &state)) != ENC424J600_STATUS_OK)
    return result;
  
  if ((result =  enc424j600_write_phy_reg(PHCON1, state & ~PHCON1_PSLEEP)) != ENC424J600_STATUS_OK)
    return result;

  //Turn on packet reception
  enc424j600_BFS_reg(ECON1, ECON1_RXEN);
  
  return result;
}
/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_powersave_enable(void)
{
  uint8_t result = ENC424J600_STATUS_OK;
  uint16_t reg_value;
  
  //Turn off modular exponentiation and AES engine
  enc424j600_BFC_reg(EIR, EIR_CRYPTEN);
  //Turn off packet reception
  enc424j600_BFC_reg(ECON1, ECON1_RXEN);
  //Wait for any in-progress receptions to complete
  while(1) {
    if ((result = enc424j600_read_reg(ESTAT, &reg_value)) != ENC424J600_STATUS_OK)
      return result;
    
	 if (!(reg_value & ESTAT_RXBUSY))
	   break;
    
	  delay_us(100);
  }

  //Wait for any current transmissions to complete
  while (1) {
    if ((result = enc424j600_read_reg(ECON1, &reg_value)) != ENC424J600_STATUS_OK)
	   return result;
    
	 if (!(reg_value  & ECON1_TXRTS))
	   break;

   delay_us(100);
  }  
    
  //Power-down PHY
  uint16_t state;
  if ((result = enc424j600_read_phy_reg(PHCON1, &state)) != ENC424J600_STATUS_OK)
    return result;
  if ((result = enc424j600_write_phy_reg(PHCON1, state | PHCON1_PSLEEP)) != ENC424J600_STATUS_OK)
    return result;

  //Power-down Ethernet interface
  enc424j600_BFC_reg(ECON2, ECON2_ETHEN);
  enc424j600_BFC_reg(ECON2, ECON2_STRCH);
  
  return result;
}
/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_pkt_send(uint16_t len, uint8_t* packet)
{
  PRINTF("ENC424J600: TX[%u]\n",len);
  /* Ensure that the device is ready to send */
  uint8_t result;
  uint16_t reg_val;
  if ((result = enc424j600_read_reg(ECON1, &reg_val)) != ENC424J600_STATUS_OK)
    return result;

  if (reg_val & ECON1_TXRTS)
    return ENC424J600_STATUS_BUSY;

  // Set the Window Write Pointer to the beginning of the transmit buffer
  if ((enc424j600_write_memory_window(GP_WINDOW, packet, len)) != ENC424J600_STATUS_OK)
	  return ENC424J600_STATUS_ERR;

  if ((enc424j600_write_reg(EGPWRPT, TXSTART)) != ENC424J600_STATUS_OK)
    return ENC424J600_STATUS_ERR;
	 
  if ((enc424j600_write_reg(ETXLEN, len)) != ENC424J600_STATUS_OK)
    return ENC424J600_STATUS_ERR;

  return enc424j600_mac_flush();
}
/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_pkt_recv(uint16_t max_len, uint8_t* packet, uint16_t* length)
{
  /* RX status vector */
  RXSTATUS status_vector;
  uint8_t result = ENC424J600_STATUS_OK;
  uint16_t new_RX_tail;
  uint16_t pkt_len;
  
  // Set the RX Read Pointer to the beginning of the next unprocessed packet
  if ((result = enc424j600_write_reg(ERXRDPT, next_pkt_ptr)) != ENC424J600_STATUS_OK)
    return result;

  // Store the next packet pointer reference	
  if ((result = enc424j600_read_memory_window(RX_WINDOW, (uint8_t*) &next_pkt_ptr, sizeof(next_pkt_ptr)))
    != ENC424J600_STATUS_OK)
    return result;

  // Read the status vector
  if ((result = enc424j600_read_memory_window(RX_WINDOW, (uint8_t*) &status_vector, sizeof(status_vector)))
    != ENC424J600_STATUS_OK)
    return result; 

  // Check if we received a frame of acceptable length 
  pkt_len = (status_vector.bits.ByteCount <= max_len + 4) ? status_vector.bits.ByteCount - 4 : 0;
  
  /* Check if any error has occurred */
  if (!status_vector.bits.ReceiveOk) {
    result = ENC424J600_STATUS_ERR;
	 goto _continue;
  }
	 
  // Read frame data
  if ((result = enc424j600_read_memory_window(RX_WINDOW, packet, pkt_len)) != ENC424J600_STATUS_OK)
     return result;
	  
_continue:  
  new_RX_tail = next_pkt_ptr - 2;
  
  //Special situation if nextPacketPointer is exactly RXSTART
  if (next_pkt_ptr == RXSTART)
    new_RX_tail = RAMSIZE - 2;

  //Packet decrement
  enc424j600_BFS_reg(ECON1, ECON1_PKTDEC);

  //Write new RX tail
  if ((result = enc424j600_write_reg(ERXTAIL, new_RX_tail)) != ENC424J600_STATUS_OK)
    return result;

  /* All fine. Store packet length. */
  *length = pkt_len;
  return result;  
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_conf_phy(void)
{
  uint8_t result = ENC424J600_STATUS_OK;
  uint16_t reg_val;
  //Read PHY status register 3
  if ((result = enc424j600_read_phy_reg(PHSTAT3, &reg_val)) != ENC424J600_STATUS_OK)
    return result;
  //Get current speed
  enc424j600_speed = (reg_val & PHSTAT3_SPDDPX1) ? 1 : 0;
  //Determine the new duplex mode
  enc424j600_full_dpx_mod = (reg_val & PHSTAT3_SPDDPX2) ? 1 : 0;

  //Configure MAC duplex mode for proper operation
  return enc424j600_conf_duplex_mode();
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_conf_duplex_mode(void)
{
  uint8_t result = ENC424J600_STATUS_OK;
  uint16_t duplex_mode;
  
  /* Determine current mode */
  if ((result = enc424j600_read_reg(ESTAT, &duplex_mode)) != ENC424J600_STATUS_OK)
    return result;
  duplex_mode &= ESTAT_PHYDPX;
  if (duplex_mode) {
    //Configure the FULDPX bit to match the current duplex mode
    if ((result = enc424j600_write_reg(MACON2,
	                      MACON2_DEFER | 
	                      MACON2_PADCFG2 | 
								 MACON2_PADCFG0 | 
								 MACON2_TXCRCEN | 
								 MACON2_r1 | 
								 MACON2_FULDPX)) != ENC424J600_STATUS_OK)
      return result;
    //Configure the Back-to-Back Inter-Packet Gap register
    if ((result = enc424j600_write_reg(MABBIPG, 0x15)) != ENC424J600_STATUS_OK)
      return result;
		
  } else {
    //Configure the FULDPX bit to match the current duplex mode
	 if ((result = enc424j600_write_reg(MACON2, 
	                      MACON2_DEFER |
                         MACON2_PADCFG2 | 
								 MACON2_PADCFG0 | 
								 MACON2_TXCRCEN | 
								 MACON2_r1)) != ENC424J600_STATUS_OK)
      return result;
    //Configure the Back-to-Back Inter-Packet Gap register
    if ((result = enc424j600_write_reg(MABBIPG, 0x12)) != ENC424J600_STATUS_OK)
	   return result; 
  }
  return result;
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_send_system_reset(void)
{
  uint8_t result;
  uint16_t return_value;
  
  /* Perform a reset via the SPI interface */
  do {
    /* Set EUDAST register and check whether it is properly cleared by the reset.
	  * If you can not write to EUDAST, there is possibly a problem witht the SPI
	  * setup, wiring etc.
	  */
    
	 //sbi(PORTE, PE7); XXX check this out!
	 do {
      return_value = 0;
      if ((result = enc424j600_read_reg(EUDAST, &return_value)) != ENC424J600_STATUS_OK)
        goto _exit;
      
		delay_ms(5);
      if ((result = enc424j600_write_reg(EUDAST, 0x1234)) != ENC424J600_STATUS_OK)
		  goto _exit;
  
      return_value = 0;
      delay_ms(100);  
      if ((result = enc424j600_read_reg(EUDAST, &return_value)) != ENC424J600_STATUS_OK)
        goto _exit;
      
    } while (return_value != 0x1234);

    while (1) {
      return_value = 0;
      /* Poll the status register and wait until all required flags are set. */
		if ((result = enc424j600_read_reg(ESTAT, &return_value)) != ENC424J600_STATUS_OK)
		  goto _exit;

		if ((return_value & (ESTAT_CLKRDY | ESTAT_RSTDONE | ESTAT_PHYRDY)) ==
		    (ESTAT_CLKRDY | ESTAT_RSTDONE | ESTAT_PHYRDY))
			 break;
    }
	 /* Issue a reset command and wait for it to complete the device reset */
    enc424j600_BFS_reg(ECON2, ECON2_ETHRST);
	 /* Reset has changed the bank stored in the device to the ZERO one */
	 current_bank = 0;
	 
	 delay_ms(10);
    return_value = 0x00;
    /* Check if the reset were successful. Reset should have zeroed the EUDAST register */
	 if((result = enc424j600_read_reg(EUDAST, &return_value)) != ENC424J600_STATUS_OK)
	   goto _exit;
  
  } while (return_value != 0x0000u);

  // Really ensure reset is done and give some time for power to be stable
  delay_ms(10);
  return result;
_exit:
  PRINTF("enc424j600: sys-reset-error\n");
  return result;
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_mac_is_tx_ready(void)
{
  uint16_t return_value = 0;
  uint8_t result = enc424j600_read_reg(ECON1, &return_value);	

  if (result != ENC424J600_STATUS_OK)
		return !0; // XXX check this
  
  return (uint8_t)!(return_value & ECON1_TXRTS);
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_mac_flush(void)
{
  // Check to see if the duplex status has changed.  This can
  // change if the user unplugs the cable and plugs it into a
  // different node.  Auto-negotiation will automatically set
  // the duplex in the PHY, but we must also update the MAC
  // inter-packet gap timing and duplex state to match.
  
  uint8_t result = ENC424J600_STATUS_OK;
  uint16_t return_value = 0;

  result = enc424j600_read_reg(EIR, &return_value);
  
  if (result != ENC424J600_STATUS_OK)
    goto _exit;

  if (return_value & EIR_LINKIF) {
	  
    enc424j600_BFC_reg(EIR, EIR_LINKIF);
    
	 uint16_t w;
	 
    // Update MAC duplex settings to match PHY duplex setting
    result = enc424j600_read_reg(MACON2, &w);
	 if (result != ENC424J600_STATUS_OK)
      goto _exit;
		
    result = enc424j600_read_reg(ESTAT, &return_value);
    if (result != ENC424J600_STATUS_OK)
      goto _exit;
    
	 if (return_value & ESTAT_PHYDPX) {
      // Switching to full duplex
      result = enc424j600_write_reg(MABBIPG, 0x15); 
		if (result != ENC424J600_STATUS_OK)
		  goto _exit;
	   w |= MACON2_FULDPX;
		
	 } else {
      // Switching to half duplex
      result = enc424j600_write_reg(MABBIPG, 0x12);
      if (result != ENC424J600_STATUS_OK)
		  goto _exit;
		w &= ~MACON2_FULDPX; 
	 }
	 
	 result = enc424j600_write_reg(MACON2, w);
	 if (result != ENC424J600_STATUS_OK)
	   goto _exit;
		
  }

  // Start the transmission, but only if we are linked.
  if ((result = enc424j600_read_reg(ESTAT, &return_value)) != ENC424J600_STATUS_OK)
    goto _exit;

  if (return_value & ESTAT_PHYLNK)
    enc424j600_BFS_reg(ECON1, ECON1_TXRTS);
 
_exit:
  return result;	 
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_checksum_calculation(uint16_t position, uint16_t length, 
  uint16_t seed, uint16_t* ret_val)
{
  uint8_t result = ENC424J600_STATUS_OK;
  uint16_t return_value = 0;
  
  // Wait until module is idle
  while (1) {
    result = enc424j600_read_reg(ECON1, &return_value);
	 
	 if (result != ENC424J600_STATUS_OK)
	   goto _exit;
    
    if (!(return_value & ECON1_DMAST))
	   break;
  }
  
  // Clear DMACPY to prevent a copy operation
  enc424j600_BFC_reg(ECON1, ECON1_DMACPY);
  // Clear DMANOCS to select a checksum operation
  enc424j600_BFC_reg(ECON1, ECON1_DMANOCS);
  // Clear DMACSSD to use the default seed of 0000h
  enc424j600_BFC_reg(ECON1, ECON1_DMACSSD);
  // Set EDMAST to source address
  result = enc424j600_write_reg(EDMAST, position);
  if (result != ENC424J600_STATUS_OK)
    goto _exit;

  // Set EDMALEN to length
  result = enc424j600_write_reg(EDMALEN, length);
  if (result != ENC424J600_STATUS_OK)
    goto _exit;
  
  //If we have a seed, now it's time
  if (seed) {
    enc424j600_BFS_reg(ECON1, ECON1_DMACSSD);
    result = enc424j600_write_reg(EDMACS, seed);
    if (result != ENC424J600_STATUS_OK)
      goto _exit;
	}
	
  // Initiate operation
  enc424j600_BFS_reg(ECON1, ECON1_DMAST);
  // Wait until done
  while (1) {
    result = enc424j600_read_reg(ECON1, &return_value);
		
	 if (result != ENC424J600_STATUS_OK)
	   goto _exit;
		
	 if (!(return_value & ECON1_DMAST))
      break;
  }
  
  result =  enc424j600_read_reg(EDMACS, &return_value);
  
  *ret_val = return_value;

_exit:
  return result;  
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_read_memory_window(uint8_t window, uint8_t *data, uint16_t length)
{
  uint8_t result = ENC424J600_STATUS_OK;
  
  if (length == 0)
    return result;

  uint8_t op = RBMUDA;

  if (window & GP_WINDOW)
    op = RBMGP;
  
  if (window & RX_WINDOW)
    op = RBMRX;

  return enc424j600_read_n(op, data, length);
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_write_memory_window(uint8_t window, uint8_t *data, uint16_t length)
{
  uint8_t op = WBMUDA;
    
  if (window & GP_WINDOW)
    op = WBMGP;
	 
  if (window & RX_WINDOW)
    op = WBMRX;

	return  enc424j600_write_n(op, data, length);
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_write_reg(uint16_t address, uint16_t data)
{
  uint8_t bank;
  uint16_t temp;
  uint8_t result = ENC424J600_STATUS_OK;

  /* See if we need to change register banks */
  bank = ((uint8_t) address) & 0xE0;
  //If address is banked, we will use banked access
  if (bank <= (0x3u << 5)) {
    if (bank != current_bank) {
      if (bank == (0x0u << 5))
        result = enc424j600_execute_op0(B0SEL);
      else if (bank == (0x1u << 5))
        result = enc424j600_execute_op0(B1SEL);
      else if (bank == (0x2u << 5))
        result = enc424j600_execute_op0(B2SEL);
      else if (bank == (0x3u << 5))
        result = enc424j600_execute_op0(B3SEL);
		
		if (result != ENC424J600_STATUS_OK)
		  goto _exit;
		    
	   current_bank = bank;
    }
    result = enc424j600_execute_op16(WCR | (address & 0x1F), data, &temp);

  } else {

	 uint32_t data32 = 0;
	 uint32_t temp32;
	 
    ((uint8_t*) & data32)[0] = (uint8_t) address;
    ((uint8_t*) & data32)[1] = ((uint8_t*) & data)[0];
    ((uint8_t*) & data32)[2] = ((uint8_t*) & data)[1];
    result = enc424j600_execute_op32(WCRU, data32, &temp32);
  }
  
_exit:  
  return result;
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_read_reg(uint16_t address, uint16_t *ret_val)
{
  uint16_t return_value = 0;
  uint8_t bank;
  uint8_t result = ENC424J600_STATUS_OK;

  /* See if we need to change register banks */
  bank = ((uint8_t) address) & 0xE0;
  //If address is banked, we will use banked access
  if (bank <= (0x3u << 5)) {
    if (bank != current_bank) {
      if (bank == (0x0u << 5))
        result = enc424j600_execute_op0(B0SEL);
      else if (bank == (0x1u << 5))
        result = enc424j600_execute_op0(B1SEL);
      else if (bank == (0x2u << 5))
        result = enc424j600_execute_op0(B2SEL);
      else if (bank == (0x3u << 5))
        result = enc424j600_execute_op0(B3SEL);
      
		if (result != ENC424J600_STATUS_OK)
        goto _exit;
		
	   current_bank = bank;
    }
    result = enc424j600_execute_op16(RCR | (address & 0x1F), 0x0000, &return_value);
	 
	 if (result != ENC424J600_STATUS_OK)
      goto _exit;
	  
	 *ret_val = return_value;
  
  } else {

      uint32_t return_value32 = 0;
		result = enc424j600_execute_op32(RCRU, (uint32_t) address, &return_value32);
		
		if (result != ENC424J600_STATUS_OK)
        goto _exit;
				  
		((uint8_t*) & return_value)[0] = ((uint8_t*) & return_value32)[1];
		((uint8_t*) & return_value)[1] = ((uint8_t*) & return_value32)[2];
		
		*ret_val = (uint16_t) (return_value32 & 0xffff);
	}
	
_exit:
	return result;
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_read_phy_reg(uint8_t address, uint16_t *ret_val)
{
  uint16_t return_value = 0;
  uint8_t result = ENC424J600_STATUS_OK;
  
  // Set the right address and start the register read operation
  result = enc424j600_write_reg(MIREGADR, 0x0100 | address);
  
  if (result != ENC424J600_STATUS_OK)
    goto _exit;
  
  result = enc424j600_write_reg(MICMD, MICMD_MIIRD);
  
  if (result != ENC424J600_STATUS_OK)
    goto _exit;
	 	 
  // Loop to wait until the PHY register has been read through the MII
  // This requires 25.6us
  while(1) {
	  result = enc424j600_read_reg(MISTAT, &return_value);
	  
	  if (result != ENC424J600_STATUS_OK)
	    goto _exit;
		 
	  if (!(return_value & MISTAT_BUSY)) {
		  break;
	  }	  
  }

  // Stop reading
  result = enc424j600_write_reg(MICMD, 0x0000);
  
  if (result != ENC424J600_STATUS_OK)
    goto _exit;
  
  // Obtain results and return
  result = enc424j600_read_reg(MIRD, &return_value);

  if (result != ENC424J600_STATUS_OK)
    goto _exit;

  *ret_val = return_value;
  	 
_exit:
  return result;  
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_write_phy_reg(uint8_t address, uint16_t data)
{
  uint8_t result = ENC424J600_STATUS_OK;
  
  // Write the register address
  result = enc424j600_write_reg(MIREGADR, 0x0100 | address);
  
  if (result != ENC424J600_STATUS_OK)
    goto _exit;
  
   // Write the data
  result = enc424j600_write_reg(MIWR, data);
  
  if (result != ENC424J600_STATUS_OK)
    goto _exit;
 
  // Wait until the PHY register has been written
  uint16_t return_value = 0;
  while(1) {
	  
	   result = enc424j600_read_reg(MISTAT, &return_value);
		
		if (result != ENC424J600_STATUS_OK)
		  goto _exit;
		  
		if (!(return_value  & MISTAT_BUSY))
		  break; 	
  }
  
_exit:
  return result;	
}
/*---------------------------------------------------------------------------*/
static void 
enc424j600_BFS_reg(uint16_t address, uint16_t bit_mask) {
	
  uint8_t bank;

  /* Check if register bank needs change */
  bank = ((uint8_t) address) & 0xE0;
	
  if (bank != current_bank) {
    if (bank == (0x0u << 5))
      enc424j600_execute_op0(B0SEL);
    else if (bank == (0x1u << 5))
      enc424j600_execute_op0(B1SEL);
    else if (bank == (0x2u << 5))
      enc424j600_execute_op0(B2SEL);
    else if (bank == (0x3u << 5))
      enc424j600_execute_op0(B3SEL);

    current_bank = bank;
  }
  uint16_t temp;
  enc424j600_execute_op16(BFS | (address & 0x1F), bit_mask, &temp);
}
/*---------------------------------------------------------------------------*/
static void
enc424j600_BFC_reg(uint16_t address, uint16_t bit_mask)
{
  uint8_t bank;
  
  bank = ((uint8_t) address) & 0xE0;
  /* Check if register bank needs change */
  if (bank != current_bank) {
    if (bank == (0x0u << 5))
	   enc424j600_execute_op0(B0SEL);
    else if (bank == (0x1u << 5))
      enc424j600_execute_op0(B1SEL);
    else if (bank == (0x2u << 5))
      enc424j600_execute_op0(B2SEL);
    else if (bank == (0x3u << 5))
	   enc424j600_execute_op0(B3SEL);

    current_bank = bank;
  }
  uint16_t temp;
  enc424j600_execute_op16(BFC | (address & 0x1F), bit_mask, &temp);
}
/*---------------------------------------------------------------------------*/
