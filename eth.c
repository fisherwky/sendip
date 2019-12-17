/* eth.c - ETH code for sendip
 * Author: Mike Ricketts <mike@earth.li>
 * ChangeLog from 2.5 release:
 * 12/12/2019 first version by Fisher Wu <fisherwky@gmail.com>
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>   //strncpy
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>   //ifreq
#include <unistd.h>   //close




#include "sendip_module.h"
#include "eth.h"

/* Character that identifies our options
 */
const char opt_char='e';

sendip_data *initialize(void) {
    sendip_data *ret = malloc(sizeof(sendip_data));
    eth_header *eth = malloc(sizeof(eth_header));
    memset(eth,0,sizeof(eth_header));
    ret->alloc_len = sizeof(eth_header);
    ret->data = (void *)eth;
    ret->modified=0;
    return ret;
}

bool set_addr(char *interface, sendip_data *pack) {
	bool ret = TRUE;
    eth_header *eth = (eth_header *)pack->data;
    if(!(pack->modified & ETH_MOD_DHOST)) {
		memcpy(&(eth->ether_dhost),"\xff\xff\xff\xff\xff\xff",6);
    }
    if(!(pack->modified & ETH_MOD_SHOST)) {
		struct ifreq ifr;
		//int fd = socket(AF_INET, SOCK_DGRAM, 0);
		int fd =socket(AF_PACKET,SOCK_RAW,IPPROTO_RAW);
		ifr.ifr_addr.sa_family = AF_INET;
		strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);
		if(ioctl(fd, SIOCGIFHWADDR, &ifr) != -1) {
			memcpy(&(eth->ether_shost), ifr.ifr_hwaddr.sa_data, sizeof(eth->ether_shost));
		}
		else {
			fprintf(stderr,"Get MAC address from %s failure!\n", interface);
			ret = FALSE;
		}
		close(fd);
    }
    return ret;
}

bool do_opt(char *opt, char *arg, sendip_data *pack) {
    eth_header *eth = (eth_header *)pack->data;
    u_int8_t *data;
	int len;

    switch(opt[1]) {
    case 's':
    case 'd':
		data = malloc(strlen(arg)+2);
		if(!data) {
			fprintf(stderr,"Out of memory!\n");
			return FALSE;
		}
		sprintf((char*)data,"0x%s",arg);
		len = compact_string((char*)data);
		if(len != sizeof(struct ether_addr)) {
			fprintf(stderr,"MAC address length error!\n");
			free(data);
			return FALSE;
		}

		if (opt[1] == 's') {
			memcpy(&(eth->ether_shost), data, sizeof(eth->ether_shost));
			pack->modified |= ETH_MOD_SHOST;
		}
		else {
			memcpy(&(eth->ether_dhost), data, sizeof(eth->ether_dhost));
			pack->modified |= ETH_MOD_DHOST;
		}
        free(data);
        break;
    case 't':
        eth->ether_type = htons((u_int16_t)strtoul(arg, (char **)NULL, 0));
        pack->modified |= ETH_MOD_TYPE;
        break;
    }
    return TRUE;
}




bool finalize(char *hdrs, sendip_data *headers[], sendip_data *data,
              sendip_data *pack) {
    eth_header *eth = (eth_header *)pack->data;

    if(!(pack->modified & ETH_MOD_TYPE)) {
        eth->ether_type = htons(ETHERTYPE_IP);
    }
	/* count total length for padding */
	int i = 10;
	i++;
    
    
    return TRUE;
}

int num_opts() {
    return sizeof(eth_opts)/sizeof(sendip_option);
}
sendip_option *get_opts() {
    return eth_opts;
}
char get_optchar() {
    return opt_char;
}
