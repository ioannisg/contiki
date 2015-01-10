/*
 * netdev_features.h
 *
 * Created: 1/30/2014 1:54:59 PM
 *  Author: Ioannis Glaropoulos
 */ 
#ifndef NETDEV_FEATURES_H_
#define NETDEV_FEATURES_H_


typedef uint64_t netdev_features_t; 
 
enum  {
         NETIF_F_SG_BIT,                 /* Scatter/gather IO. */
         NETIF_F_IP_CSUM_BIT,            /* Can checksum TCP/UDP over IPv4. */
         __UNUSED_NETIF_F_1,
         NETIF_F_HW_CSUM_BIT,            /* Can checksum all the packets. */
         NETIF_F_IPV6_CSUM_BIT,          /* Can checksum TCP/UDP over IPV6 */
         NETIF_F_HIGHDMA_BIT,            /* Can DMA to high memory. */
         NETIF_F_FRAGLIST_BIT,           /* Scatter/gather IO. */
         NETIF_F_HW_VLAN_CTAG_TX_BIT,    /* Transmit VLAN CTAG HW acceleration */
         NETIF_F_HW_VLAN_CTAG_RX_BIT,    /* Receive VLAN CTAG HW acceleration */
         NETIF_F_HW_VLAN_CTAG_FILTER_BIT,/* Receive filtering on VLAN CTAGs */
         NETIF_F_VLAN_CHALLENGED_BIT,    /* Device cannot handle VLAN packets */
         NETIF_F_GSO_BIT,                /* Enable software GSO. */
         NETIF_F_LLTX_BIT,               /* LockLess TX - deprecated. Please */
                                         /* do not use LLTX in new drivers */
         NETIF_F_NETNS_LOCAL_BIT,        /* Does not change network namespaces */
         NETIF_F_GRO_BIT,                /* Generic receive offload */
         NETIF_F_LRO_BIT,                /* large receive offload */
 
         /**/NETIF_F_GSO_SHIFT,          /* keep the order of SKB_GSO_* bits */
         NETIF_F_TSO_BIT                 /* ... TCPv4 segmentation */
                 = NETIF_F_GSO_SHIFT,
         NETIF_F_UFO_BIT,                /* ... UDPv4 fragmentation */
         NETIF_F_GSO_ROBUST_BIT,         /* ... ->SKB_GSO_DODGY */
         NETIF_F_TSO_ECN_BIT,            /* ... TCP ECN support */
         NETIF_F_TSO6_BIT,               /* ... TCPv6 segmentation */
         NETIF_F_FSO_BIT,                /* ... FCoE segmentation */
         NETIF_F_GSO_GRE_BIT,            /* ... GRE with TSO */
         NETIF_F_GSO_IPIP_BIT,           /* ... IPIP tunnel with TSO */
         NETIF_F_GSO_SIT_BIT,            /* ... SIT tunnel with TSO */
         NETIF_F_GSO_UDP_TUNNEL_BIT,     /* ... UDP TUNNEL with TSO */
         NETIF_F_GSO_MPLS_BIT,           /* ... MPLS segmentation */
         /**/NETIF_F_GSO_LAST =          /* last bit, see GSO_MASK */
                 NETIF_F_GSO_MPLS_BIT,
         NETIF_F_FCOE_CRC_BIT,           /* FCoE CRC32 */
         NETIF_F_SCTP_CSUM_BIT,          /* SCTP checksum offload */
         NETIF_F_FCOE_MTU_BIT,           /* Supports max FCoE MTU, 2158 bytes*/
         NETIF_F_NTUPLE_BIT,             /* N-tuple filters supported */
         NETIF_F_RXHASH_BIT,             /* Receive hashing offload */
         NETIF_F_RXCSUM_BIT,             /* Receive checksumming offload */
         NETIF_F_NOCACHE_COPY_BIT,       /* Use no-cache copyfromuser */
         NETIF_F_LOOPBACK_BIT,           /* Enable loopback */
         NETIF_F_RXFCS_BIT,              /* Append FCS to skb pkt data */
         NETIF_F_RXALL_BIT,              /* Receive errored frames too */
         NETIF_F_HW_VLAN_STAG_TX_BIT,    /* Transmit VLAN STAG HW acceleration */
         NETIF_F_HW_VLAN_STAG_RX_BIT,    /* Receive VLAN STAG HW acceleration */
         NETIF_F_HW_VLAN_STAG_FILTER_BIT,/* Receive filtering on VLAN STAGs */
         NETIF_F_HW_L2FW_DOFFLOAD_BIT,   /* Allow L2 Forwarding in Hardware */
  
         /*
          * Add your fresh new feature above and remember to update
          * netdev_features_strings[] in net/core/ethtool.c and maybe
          * some feature mask #defines below. Please also describe it
          * in Documentation/networking/netdev-features.txt.
          */
  
         /**/NETDEV_FEATURE_COUNT,
 };


#endif /* NETDEV_FEATURES_H_ */