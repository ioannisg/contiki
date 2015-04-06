#define SERIAL_DEBUG_IRQ_LEVEL 5

#include "serial-debug.h"
#include "contiki-conf.h"
#include "wire_digital.h" //TODO Make this a GPIO core cpu-agnostic
#include <stdio.h>

serial_debug_t serial_debug;

/*---------------------------------------------------------------------------*/
static void
serial_debug_eth_handler(uint32_t first, uint32_t second)
{
  UNUSED(first);
  UNUSED(second);

  if (serial_debug.mac_debug & MAC_ETH_DEBUG) {
    serial_debug.mac_debug &= ~MAC_ETH_DEBUG;
  } else {
    serial_debug.mac_debug |= MAC_ETH_DEBUG;
  }
}
/*---------------------------------------------------------------------------*/
static void
serial_debug_xbee_handler(uint32_t first, uint32_t second)
{
  UNUSED(first);
  UNUSED(second);

  if (serial_debug.mac_debug & MAC_XBEE_DEBUG) {
    serial_debug.mac_debug &= ~MAC_XBEE_DEBUG;
  } else {
    serial_debug.mac_debug |= MAC_XBEE_DEBUG;
  }
  printf("serial-debug: XBEE:%u\n", serial_debug.mac_debug & MAC_XBEE_DEBUG);
}
/*---------------------------------------------------------------------------*/
static void
serial_debug_wifi_handler(uint32_t first, uint32_t second)
{
  UNUSED(first);
  UNUSED(second);

  if (serial_debug.mac_debug & MAC_WIFI_DEBUG) {
    serial_debug.mac_debug &= ~MAC_WIFI_DEBUG;
  } else {
    serial_debug.mac_debug |= MAC_WIFI_DEBUG;
  }
}
/*---------------------------------------------------------------------------*/
static void
serial_debug_8023_handler(uint32_t first, uint32_t second)
{
  UNUSED(first);
  UNUSED(second);

  if (serial_debug.mac_debug & MAC_8023_DEBUG) {
    serial_debug.mac_debug &= ~MAC_8023_DEBUG;
  } else {
    serial_debug.mac_debug |= MAC_8023_DEBUG;
  }
}
/*---------------------------------------------------------------------------*/
static void
serial_debug_80211_handler(uint32_t first, uint32_t second)
{
  UNUSED(first);
  UNUSED(second);

  if (serial_debug.mac_debug & MAC_80211_DEBUG) {
    serial_debug.mac_debug &= ~MAC_80211_DEBUG;
  } else {
    serial_debug.mac_debug |= MAC_80211_DEBUG;
  }
}
/*---------------------------------------------------------------------------*/
static void
serial_debug_6lowpan_handler(uint32_t first, uint32_t second)
{
  UNUSED(first);
  UNUSED(second);

  if (serial_debug.mac_debug & MAC_6LOWPAN_DEBUG) {
    serial_debug.mac_debug &= ~MAC_6LOWPAN_DEBUG;
  } else {
    serial_debug.mac_debug |= MAC_6LOWPAN_DEBUG;
  }
  printf("serial-debug: 6PAN:%u\n", serial_debug.mac_debug & MAC_6LOWPAN_DEBUG);
}
/*---------------------------------------------------------------------------*/
static void
serial_debug_uip6_handler(uint32_t first, uint32_t second)
{
  UNUSED(first);
  UNUSED(second);

  if (serial_debug.ip_debug & UIP6_UIP_DEBUG) {
    serial_debug.ip_debug &= ~UIP6_UIP_DEBUG;
  } else {
    serial_debug.ip_debug |= UIP6_UIP_DEBUG;
  }
  printf("serial-debug: UIP:%u\n", serial_debug.ip_debug & UIP6_UIP_DEBUG);
}
/*---------------------------------------------------------------------------*/
static void
serial_debug_nbr_handler(uint32_t first, uint32_t second)
{
  UNUSED(first);
  UNUSED(second);

  if (serial_debug.ip_debug & UIP6_NBR_DEBUG) {
    serial_debug.ip_debug &= ~UIP6_NBR_DEBUG;
  } else {
    serial_debug.ip_debug |= UIP6_NBR_DEBUG;
  }
  printf("serial-debug: NBR:%u\n", serial_debug.ip_debug & UIP6_NBR_DEBUG);
}
/*---------------------------------------------------------------------------*/
static void
serial_debug_icmp_handler(uint32_t first, uint32_t second)
{
  UNUSED(first);
  UNUSED(second);

  if (serial_debug.ip_debug & UIP6_ICMP_DEBUG) {
    serial_debug.ip_debug &= ~UIP6_ICMP_DEBUG;
  } else {
    serial_debug.ip_debug |= UIP6_ICMP_DEBUG;
  }
  printf("serial-debug: ICMP:%u\n", serial_debug.ip_debug & UIP6_ICMP_DEBUG);
}
/*---------------------------------------------------------------------------*/
static void
serial_debug_nd6_handler(uint32_t first, uint32_t second)
{
  UNUSED(first);
  UNUSED(second);

  if (serial_debug.ip_debug & UIP6_ND6_DEBUG) {
    serial_debug.ip_debug &= ~UIP6_ND6_DEBUG;
  } else {
    serial_debug.ip_debug |= UIP6_ND6_DEBUG;
  }
  printf("serial-debug: ND6:%u\n", serial_debug.ip_debug & UIP6_ND6_DEBUG);
}
/*---------------------------------------------------------------------------*/
static void
serial_debug_ds6_handler(uint32_t first, uint32_t second)
{
  UNUSED(first);
  UNUSED(second);

  if (serial_debug.ip_debug & UIP6_DS6_DEBUG) {
    serial_debug.ip_debug &= ~UIP6_DS6_DEBUG;
  } else {
    serial_debug.ip_debug |= UIP6_DS6_DEBUG;
  }
  printf("serial-debug: DS6:%u\n", serial_debug.ip_debug & UIP6_DS6_DEBUG);
}
/*---------------------------------------------------------------------------*/
static void
serial_debug_tcpip_handler(uint32_t first, uint32_t second)
{
  UNUSED(first);
  UNUSED(second);

  if (serial_debug.ip_debug & UIP6_TCPIP_DEBUG) {
    serial_debug.ip_debug &= ~UIP6_TCPIP_DEBUG;
  } else {
    serial_debug.ip_debug |= UIP6_TCPIP_DEBUG;
  }
  printf("serial-debug: TCPIP:%u\n", serial_debug.ip_debug & UIP6_TCPIP_DEBUG);
}
/*---------------------------------------------------------------------------*/
static void
serial_debug_rpl_handler(uint32_t first, uint32_t second)
{
  UNUSED(first);
  UNUSED(second);

  if(serial_debug.ip_debug & UIP6_RPL_DEBUG) {
    serial_debug.ip_debug &= ~UIP6_RPL_DEBUG;
  } else {
    serial_debug.ip_debug |= UIP6_RPL_DEBUG;
  }
  printf("serial-debug: RPL:%u\n", serial_debug.ip_debug & UIP6_RPL_DEBUG);
}
/*---------------------------------------------------------------------------*/
void
serial_debug_init(void)
{
  memset(&serial_debug, 0, sizeof(serial_debug_t));
  /* Configure base pin for low output */
  configure_output_pin(SERIAL_DEBUG_BASE_PIN, LOW, DISABLE, DISABLE);
  /* Configure all debug pins as input */
  configure_input_pin_interrupt_enable(ETH_DEBUG_PIN, PIO_PULLUP,
    PIO_IT_RISE_EDGE, serial_debug_eth_handler, SERIAL_DEBUG_IRQ_LEVEL);
  configure_input_pin_interrupt_enable(WIFI_DEBUG_PIN, PIO_PULLUP, 
    PIO_IT_RISE_EDGE, serial_debug_wifi_handler, SERIAL_DEBUG_IRQ_LEVEL);
  configure_input_pin_interrupt_enable(XBEE_DEBUG_PIN, PIO_PULLUP, 
    PIO_IT_RISE_EDGE, serial_debug_xbee_handler, SERIAL_DEBUG_IRQ_LEVEL);
  configure_input_pin_interrupt_enable(IEEE8023_DEBUG_PIN, PIO_PULLUP, 
    PIO_IT_RISE_EDGE, serial_debug_8023_handler, SERIAL_DEBUG_IRQ_LEVEL);
  configure_input_pin_interrupt_enable(IEEE80211_DEBUG_PIN, PIO_PULLUP, 
    PIO_IT_RISE_EDGE, serial_debug_80211_handler, SERIAL_DEBUG_IRQ_LEVEL);
  configure_input_pin_interrupt_enable(SICSLOWPAN_DEBUG_PIN, PIO_PULLUP, 
    PIO_IT_RISE_EDGE, serial_debug_6lowpan_handler, SERIAL_DEBUG_IRQ_LEVEL);
  configure_input_pin_interrupt_enable(UIP_DEBUG_PIN, PIO_PULLUP, 
    PIO_IT_RISE_EDGE, serial_debug_uip6_handler, SERIAL_DEBUG_IRQ_LEVEL);
  configure_input_pin_interrupt_enable(NBR_DEBUG_PIN, PIO_PULLUP, 
    PIO_IT_RISE_EDGE, serial_debug_nbr_handler, SERIAL_DEBUG_IRQ_LEVEL);
  configure_input_pin_interrupt_enable(ND6_DEBUG_PIN, PIO_PULLUP, 
    PIO_IT_RISE_EDGE, serial_debug_nd6_handler, SERIAL_DEBUG_IRQ_LEVEL);
  configure_input_pin_interrupt_enable(DS6_DEBUG_PIN, PIO_PULLUP, 
    PIO_IT_RISE_EDGE, serial_debug_ds6_handler, SERIAL_DEBUG_IRQ_LEVEL);
  configure_input_pin_interrupt_enable(TCPIP_DEBUG_PIN, PIO_PULLUP, 
    PIO_IT_RISE_EDGE, serial_debug_tcpip_handler, SERIAL_DEBUG_IRQ_LEVEL);
  configure_input_pin_interrupt_enable(RPL_DEBUG_PIN, PIO_PULLUP, 
    PIO_IT_RISE_EDGE, serial_debug_rpl_handler, SERIAL_DEBUG_IRQ_LEVEL);
  configure_input_pin_interrupt_enable(ICMP_DEBUG_PIN, PIO_PULLUP, 
    PIO_IT_RISE_EDGE, serial_debug_icmp_handler, SERIAL_DEBUG_IRQ_LEVEL);
}
/*---------------------------------------------------------------------------*/
