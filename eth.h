/* eth.h
 */ 
#ifndef _SENDIP_ETH_H
#define _SENDIP_ETH_H

/* ETH HEADER
 * Taken from glibc 2.2, and modified
 */


#include <sys/types.h>
#include <stdint.h>

#include <linux/if_ether.h>     /* IEEE 802.3 Ethernet constants */

__BEGIN_DECLS

/* This is a name for the 48 bit ethernet address available on many
   systems.  */
struct ether_addr
{
  u_int8_t ether_addr_octet[ETH_ALEN];
} __attribute__ ((__packed__));

/* 10Mb/s ethernet header */
typedef struct 
{
  u_int8_t  ether_dhost[ETH_ALEN];	/* destination eth addr	*/
  u_int8_t  ether_shost[ETH_ALEN];	/* source ether addr	*/
  u_int16_t ether_type;		        /* packet type ID field	*/
} eth_header;

/* Defines for which parts have been modified
 */
#define ETH_MOD_HEADERLEN (1)
#define ETH_MOD_DHOST     (1<<1)
#define ETH_MOD_SHOST     (1<<2)
#define ETH_MOD_TYPE      (1<<3)

/* Options
 */
sendip_option eth_opts[] = {
    {"s",1,"Source MAC address","Correct"},
    {"d",1,"Destination MAC address","ff:ff:ff:ff:ff:ff (ffffffffffff)"},
    {"t",1,"Packet type","Correct"}
};

#define	ETHERTYPE_PUP		0x0200          /* Xerox PUP */
#define ETHERTYPE_SPRITE	0x0500		/* Sprite */
#define	ETHERTYPE_IP		0x0800		/* IP */
#define	ETHERTYPE_ARP		0x0806		/* Address resolution */
#define	ETHERTYPE_REVARP	0x8035		/* Reverse ARP */
#define ETHERTYPE_AT		0x809B		/* AppleTalk protocol */
#define ETHERTYPE_AARP		0x80F3		/* AppleTalk ARP */
#define	ETHERTYPE_VLAN		0x8100		/* IEEE 802.1Q VLAN tagging */
#define ETHERTYPE_IPX		0x8137		/* IPX */
#define	ETHERTYPE_IPV6		0x86dd		/* IP protocol version 6 */
#define ETHERTYPE_LOOPBACK	0x9000		/* used to test interfaces */


#endif  /* _SENDIP_ETH_H */
