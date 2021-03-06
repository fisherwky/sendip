/* ipv6.c - sendip IPv6 code
 * Taken from code by Antti Tuominen <ajtuomin@tml.hut.fi>
 * ChangeLog since 2.0 release:
 * 09/08/2002 Setting src/dst now works (Pekka Savola <pekkas@netcore.fi>)
 * ChangeLog from 2.5 release:
 * 12/12/2019 support previous layer is ethernet by Fisher Wu <fisherwky@gmail.com>
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <ifaddrs.h>

#include "sendip_module.h"
#include "ipv6.h"
#include "eth.h"

/* Character that identifies our options
 */
const char opt_char='6';

sendip_data *initialize(void) {
	sendip_data *ret = malloc(sizeof(sendip_data));
	ipv6_header *ipv6 = malloc(sizeof(ipv6_header));
	memset(ipv6,0,sizeof(ipv6_header));
	ret->alloc_len = sizeof(ipv6_header);
	ret->data = (void *)ipv6;
	ret->modified=0;
	return ret;
}


bool set_addr(char *interface, sendip_data *pack) {
	bool ret = TRUE;
	ipv6_header *ipv6 = (ipv6_header *)pack->data;

	if(!(pack->modified & IPV6_MOD_SRC)) {
		struct ifaddrs *ifa_list, *ifa;

		ipv6->ip6_src = in6addr_loopback;	//	default value
		if (getifaddrs(&ifa_list) == -1) {
			fprintf(stderr,"Get IPv6 address from %s failure! (getifaddrs)\n", interface);
			ret = FALSE;
		}
		else {
			for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next) {
				if (strcmp(ifa->ifa_name, interface) == 0) {
					if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET6) {
						memcpy(&ipv6->ip6_src, &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr, sizeof(ipv6->ip6_src));
						break;
					}
				}
			}
			freeifaddrs(ifa_list);
		}
	}
	if(!(pack->modified & IPV6_MOD_DST)) {
		ipv6->ip6_dst = in6addr_any;
	}
	return ret;
}


bool do_opt(char *opt, char *arg, sendip_data *pack) {
	ipv6_header *hdr = (ipv6_header *)pack->data;
	struct in6_addr addr;

	switch(opt[1]) {
	case 'f':
		/* TODO : This looks byte-order dependant */
		hdr->ip6_flow |= htonl((u_int32_t)strtoul(arg, (char **)NULL, 0) & 0xFFF00000);
		pack->modified |= IPV6_MOD_FLOW;
		break;
	case 'v':
		hdr->ip6_vfc &= 0x0F;
		hdr->ip6_vfc |= (u_int8_t)(strtoul(arg, (char **)NULL, 0) &0x0F) << 4;
		pack->modified |= IPV6_MOD_VERSION;
		break;
	case 'p':
		hdr->ip6_vfc &= 0xF0;
		hdr->ip6_vfc |= (u_int8_t)strtoul(arg, (char **)NULL, 0) & 0x0F;
		pack->modified |= IPV6_MOD_PRIORITY;
		break;
	case 't':
		/* TODO : This looks byte-order dependant */
		hdr->ip6_flow |= htonl(((u_int32_t)strtoul(arg, (char **)NULL, 0) << 20) & 0x0F000000);
		pack->modified |= IPV6_MOD_FLOW;
		break;
	case 'l':
		hdr->ip6_plen = htons((u_int16_t)strtoul(arg, (char **)NULL, 0));
		pack->modified |= IPV6_MOD_PLEN;
		break;
	case 'h':
		hdr->ip6_hlim = (u_int8_t)strtoul(arg, (char **)NULL, 0);
		pack->modified |= IPV6_MOD_HLIM;
		break;
	case 'n':
		hdr->ip6_nxt = (u_int8_t)strtoul(arg, (char **)NULL, 0);
		pack->modified |= IPV6_MOD_NXT;
		break;
	case 's':
		if (inet_pton(AF_INET6, arg, &addr)) {
			memcpy(&hdr->ip6_src, &addr, sizeof(struct in6_addr));
		}
		pack->modified |= IPV6_MOD_SRC;
		break;
	case 'd':
		if (inet_pton(AF_INET6, arg, &addr)) {
			memcpy(&hdr->ip6_dst, &addr, sizeof(struct in6_addr));
		}
		pack->modified |= IPV6_MOD_DST;
		break;
	}
	return TRUE;

}

bool finalize(char *hdrs, sendip_data *headers[], sendip_data *data,
              sendip_data *pack) {
	ipv6_header *ipv6 = (ipv6_header *)pack->data;

	if(!(pack->modified&IPV6_MOD_VERSION)) {
		ipv6->ip6_vfc &= 0x0F;
		ipv6->ip6_vfc |= (6 << 4);
	}
	if(!(pack->modified&IPV6_MOD_PLEN)) {
		ipv6->ip6_plen = htons(data->alloc_len);
	}
	if(!(pack->modified&IPV6_MOD_NXT)) {
		ipv6->ip6_nxt = (u_int8_t)IPPROTO_NONE;
	}
	if(!(pack->modified&IPV6_MOD_HLIM)) {
		ipv6->ip6_hlim = 32;
	}
    /* Find enclosing ETH header and modify ether_type */
    if(hdrs[strlen(hdrs)-1]=='e') {
        int i = strlen(hdrs)-1;
        if(!(headers[i]->modified&ETH_MOD_TYPE)) {
            ((eth_header *)(headers[i]->data))->ether_type=htons(ETHERTYPE_IPV6);
            headers[i]->modified |= ETH_MOD_TYPE;
        }
    }
	return TRUE;
}

int num_opts() {
	return sizeof(ipv6_opts)/sizeof(sendip_option);
}
sendip_option *get_opts() {
	return ipv6_opts;
}
char get_optchar() {
	return opt_char;
}
