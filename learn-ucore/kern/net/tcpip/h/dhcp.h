#ifndef __KERN_NET_TCPIP_H_DHCP_H__
#define __KERN_NET_TCPIP_H_DHCP_H__

#define DHCP_RETRY 5

#define DHCP_PADDING 0
#define DHCP_SUBNET_MASK 1
#define DHCP_ROUTER 3
#define DHCP_DNS_SERVER 6
#define DHCP_DOMAIN_NAME 15
#define DHCP_VENDER_OPTIONS 43
#define DHCP_REQUESTED_IP 50
#define DHCP_IP_ADDR_LEASE_TIME 51
#define DHCP_OPTION_OVERLOAD 52
#define DHCP_MESSAGE_TYPE 53
#define DHCP_SERVER_ID 54
#define DHCP_PARAMETER_REQUEST_LIST 55
#define DHCP_MESSAGE 56
#define DHCP_MAXIMUM_DHCP_MESSAGE_SIZE 57
#define DHCP_RENEWAL_TIME_VALUE 58
#define DHCP_REBINDING_TIME_VALUE 59
#define DHCP_VENDOR_CLASS_ID 60
#define DHCP_CLIENT_ID 61
#define DHCP_TFTP_SERVER_NAME 66
#define DHCP_BOOTFILE_NAME 67
#define DHCP_CLIENT_SYS_ARCH 93
#define DHCP_CLIENT_NET_ID 94
#define DHCP_CLIENT_MACHINE_ID 97
#define DHCP_MESSAGE_END 255

#pragma pack(2)
struct dhcpmsg
{
        uint8_t dc_bop;        /* DHCP bootp op 1=req 2=reply  */
        uint8_t dc_htype;      /* DHCP hardware type           */
        uint8_t dc_hlen;       /* DHCP hardware address length */
        uint8_t dc_hops;       /* DHCP hop count               */
        uint32_t dc_xid;       /* DHCP xid                     */
        uint16_t dc_secs;      /* DHCP seconds                 */
        uint16_t dc_flags;     /* DHCP flags                   */
        uint32_t dc_cip;       /* DHCP client IP address       */
        uint32_t dc_yip;       /* DHCP your IP address         */
        uint32_t dc_sip;       /* DHCP server IP address       */
        uint32_t dc_gip;       /* DHCP gateway IP address      */
        uint8_t dc_chaddr[16]; /* DHCP client hardware address	*/
        union {
                uint8_t dc_bootp[192]; /* DHCP bootp area (zero) */
                struct
                {
                        uint8_t sname[64];     /* TFTP server name */
                        uint8_t bootfile[128]; /* TFTP File name */
                };
        };
        uint32_t dc_cookie;   /* DHCP cookie */
        uint8_t dc_opt[1024]; /* DHCP options area (large	*/
                              /*  enough to hold more than	*/
                              /*  reasonable options		*/
};

#pragma pack()

#endif //!__KERN_NET_TCPIP_H_DHCP_H__
