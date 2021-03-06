/* sendip.c - main program code for sendip
 * Copyright 2001 Mike Ricketts <mike@earth.li>
 * Distributed under the GPL.  See LICENSE.
 * Bug reports, patches, comments etc to mike@earth.li
 * ChangeLog since 2.0 release:
 * 27/11/2001 compact_string() moved to compact.c
 * 27/11/2001 change search path for libs to include <foo>.so
 * 23/01/2002 make random fields more random (Bryan Croft <bryan@gurulabs.com>)
 * 10/08/2002 detect attempt to use multiple -d and -f options
 * ChangeLog since 2.2 release:
 * 24/11/2002 compile on archs requiring alignment
 * ChangeLog since 2.3 release:
 * 21/04/2003 random data (Anand (Andy) Rao <andyrao@nortelnetworks.com>)
 * ChangeLog since 2.4 release:
 * 21/04/2003 fix errors detected by valgrind
 * 28/07/2003 fix compile error on solaris
 * ChangeLog from 2.5 release:
 * 12/12/2019 add save/append packet to pcap file function by Fisher Wu <fisherwky@gmail.com>
 */

#define _SENDIP_MAIN

/* socket stuff */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

/* everything else */
#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ctype.h> /* isprint */
#include <pcap.h>
#include "sendip_module.h"

#ifdef __sun__  /* for EVILNESS workaround */
#include "ipv4.h"
#endif /* __sun__ */

/* Use our own getopt to ensure consistant behaviour on all platforms */
#include "gnugetopt.h"

typedef struct _s_m {
	struct _s_m *next;
	struct _s_m *prev;
	char *name;
	char optchar;
	sendip_data * (*initialize)(void);
	bool (*do_opt)(const char *optstring, const char *optarg,
	               sendip_data *pack);
	bool (*set_addr)(char *hostname, sendip_data *pack);
	bool (*finalize)(char *hdrs, sendip_data *headers[], sendip_data *data,
	                 sendip_data *pack);
	sendip_data *pack;
	void *handle;
	sendip_option *opts;
	int num_opts;
} sendip_module;

/* sockaddr_storage struct is not defined everywhere, so here is our own
	nasty version
*/
typedef struct {
	u_int16_t ss_family;
	u_int32_t ss_align;
	char ss_padding[122];
} _sockaddr_storage;

static int num_opts=0;
static sendip_module *first;
static sendip_module *last;

static char *progname;
static int fd = -1;

//	return 0, success
static int save_pcap_header(int fd, char opt_char, size_t len)
{
	struct pcap_file_header hdr;
	hdr.magic = 0xa1b2c3d4;
	hdr.version_major = 2;
	hdr.version_minor = 4;
	hdr.thiszone = 0;
	hdr.sigfigs = 0;
	hdr.snaplen = 0x40000;
	if (opt_char == 'e')
		hdr.linktype = DLT_EN10MB;
	else //if (opt_char == 'i' || opt_char == '6')
		hdr.linktype = DLT_RAW;
	return write(fd, &hdr, sizeof(hdr)) != sizeof(hdr);
};

//	return 0, success
static int save_pkt_header(int fd, size_t len)
{
	struct my_pkthdr {
		bpf_u_int32 tv_sec;
		bpf_u_int32 tv_usec;
		bpf_u_int32 caplen;
		bpf_u_int32 len;	
	} hdr;
	
	struct timeval tv;
	gettimeofday(&tv, NULL);
	hdr.tv_sec = tv.tv_sec;
	hdr.tv_usec = tv.tv_usec;
	hdr.caplen = len;
	hdr.len = len;
	return write(fd, &hdr, sizeof(hdr)) != sizeof(hdr);
};

static int sendpacket(sendip_data *data, char *interface, int af_type, bool verbose) {
	int sent = -1;
	if (fd >= 0) {
		if (lseek(fd, 0, SEEK_END) == 0)
			save_pcap_header(fd, first->optchar, data->alloc_len);
		save_pkt_header(fd, data->alloc_len);
	    sent = write(fd, (char *)data->data, data->alloc_len);

	    if (sent == data->alloc_len) {
		  if(verbose) printf("Write %d bytes to fd %d\n",sent, fd);
	    } 
	    else {
			if (sent < 0) {
				perror("write");
				printf("fd = %d\n", fd);
			}
			else {
				if(verbose) fprintf(stderr, "Only write %d of %d bytes to fd %d\n",
							sent, data->alloc_len, fd);
			}
		}
		close(fd);
		fd = -1;
	}
	else {
		char pcap_errbuf[PCAP_ERRBUF_SIZE] = {0};
		pcap_t* pcap = pcap_open_live(interface, 96, 0, 0, pcap_errbuf);
		if(pcap) {
			if(pcap_inject(pcap, data->data, data->alloc_len)==-1) {
				fprintf(stderr,"pcap_inject failure! (%s)",pcap_errbuf);
			}
			else {
				sent = data->alloc_len;
			}
			pcap_close(pcap);
		}
		else {
			fprintf(stderr,"pcap_open_live failure! (%s)",pcap_errbuf);
		}
	}

	if(verbose && sent > 0 ) {
		int i, j;
		printf("Final packet data:\n");
		for(i=0; i<data->alloc_len; ) {
			for(j=0; j<4 && i+j<data->alloc_len; j++)
				printf("%02X ", ((unsigned char *)(data->data))[i+j]);
			printf("  ");
			for(j=0; j<4 && i+j<data->alloc_len; j++) {
				int c=(int) ((unsigned char *)(data->data))[i+j];
				printf("%c", isprint(c)?((char *)(data->data))[i+j]:'.');
			}
			printf("\n");
			i+=j;
		}
	}
	if (sent == data->alloc_len) {
		if(verbose) {
			printf("Sent %d bytes from %s\n",sent, interface);
		}
	} 
	else {
		if (sent < 0) {
			perror("sendto");
		}
		else {
			if(verbose) {
				fprintf(stderr, "Only sent %d of %d bytes from %s\n", sent, data->alloc_len, interface);
			}
		}
	}
	return sent;
}


static void unload_modules(bool freeit, int verbosity) {
	sendip_module *mod, *p;
	p = NULL;
	for(mod=first; mod!=NULL; mod=mod->next) {
		if(verbosity) printf("Freeing module %s\n",mod->name);
		if(p) free(p);
		p = mod;
		free(mod->name);
		if(freeit) free(mod->pack->data);
		free(mod->pack);
		(void)dlclose(mod->handle);
		/* Do not free options - TODO should we? */
	}
	if(p) free(p);
}

static bool load_module(char *modname) {
	sendip_module *newmod = malloc(sizeof(sendip_module));
	sendip_module *cur;
	int (*n_opts)(void);
	sendip_option * (*get_opts)(void);
	char (*get_optchar)(void);

	for(cur=first; cur!=NULL; cur=cur->next) {
		if(!strcmp(modname,cur->name)) {
			memcpy(newmod,cur,sizeof(sendip_module));
			newmod->num_opts=0;
			goto out;
		}
	}
	newmod->name=malloc(strlen(modname)+strlen(SENDIP_LIBS)+strlen(".so")+2);
	strcpy(newmod->name,modname);
	if(NULL==(newmod->handle=dlopen(newmod->name,RTLD_NOW))) {
		char *error0=strdup(dlerror());
		sprintf(newmod->name,"./%s.so",modname);
		if(NULL==(newmod->handle=dlopen(newmod->name,RTLD_NOW))) {
			char *error1=strdup(dlerror());
			sprintf(newmod->name,"%s/%s.so",SENDIP_LIBS,modname);
			if(NULL==(newmod->handle=dlopen(newmod->name,RTLD_NOW))) {
				char *error2=strdup(dlerror());
				sprintf(newmod->name,"%s/%s",SENDIP_LIBS,modname);
				if(NULL==(newmod->handle=dlopen(newmod->name,RTLD_NOW))) {
					char *error3=strdup(dlerror());
					fprintf(stderr,"Couldn't open module %s, tried:\n",modname);
					fprintf(stderr,"  %s\n  %s\n  %s\n  %s\n", error0, error1,
					        error2, error3);
					free(newmod);
					free(error3);
					return FALSE;
				}
				free(error2);
			}
			free(error1);
		}
		free(error0);
	}
	strcpy(newmod->name,modname);
	if(NULL==(newmod->initialize=dlsym(newmod->handle,"initialize"))) {
		fprintf(stderr,"%s doesn't have an initialize function: %s\n",modname,
		        dlerror());
		dlclose(newmod->handle);
		free(newmod);
		return FALSE;
	}
	if(NULL==(newmod->do_opt=dlsym(newmod->handle,"do_opt"))) {
		fprintf(stderr,"%s doesn't contain a do_opt function: %s\n",modname,
		        dlerror());
		dlclose(newmod->handle);
		free(newmod);
		return FALSE;
	}
	newmod->set_addr=dlsym(newmod->handle,"set_addr"); // don't care if fails
	if(NULL==(newmod->finalize=dlsym(newmod->handle,"finalize"))) {
		fprintf(stderr,"%s\n",dlerror());
		dlclose(newmod->handle);
		free(newmod);
		return FALSE;
	}
	if(NULL==(n_opts=dlsym(newmod->handle,"num_opts"))) {
		fprintf(stderr,"%s\n",dlerror());
		dlclose(newmod->handle);
		free(newmod);
		return FALSE;
	}
	if(NULL==(get_opts=dlsym(newmod->handle,"get_opts"))) {
		fprintf(stderr,"%s\n",dlerror());
		dlclose(newmod->handle);
		free(newmod);
		return FALSE;
	}
	if(NULL==(get_optchar=dlsym(newmod->handle,"get_optchar"))) {
		fprintf(stderr,"%s\n",dlerror());
		dlclose(newmod->handle);
		free(newmod);
		return FALSE;
	}
	newmod->num_opts = n_opts();
	newmod->optchar=get_optchar();
	/* TODO: check uniqueness */
	newmod->opts = get_opts();

	num_opts+=newmod->num_opts;

out:
	newmod->pack=NULL;
	newmod->prev=last;
	newmod->next=NULL;
	last = newmod;
	if(last->prev) last->prev->next = last;
	if(!first) first=last;

	return TRUE;
}

static void print_usage(void) {
	sendip_module *mod;
	int i;
	printf("Usage: %s [-v] [-d data] [-h] [-f datafile] [-p module] [module options] [-w pcapfile] interface\n",progname);
	printf(" -d data\tadd this data as a string to the end of the packet\n");
	printf("\t\tData can be:\n");
	printf("\t\trN to generate N random(ish) data bytes;\n");
	printf("\t\t0x or 0X followed by hex digits;\n");
	printf("\t\t0 followed by octal digits;\n");
	printf("\t\tany other stream of bytes\n");
	printf(" -w pcapfile\twrite/append packet to PCAP file\n");
	printf(" -f datafile\tread packet data from file\n");
	printf(" -h\t\tprint this message\n");
	printf(" -p module\tload the specified module (see below)\n");
	printf(" -v\t\tbe verbose\n");

	printf("\n\nModules are loaded in the order the -p option appears.  The headers from\n");
	printf("each module are put immediately inside the headers from the previos model in\n");
	printf("the final packet.  For example, to embed bgp inside tcp inside ipv4, do\n");
	printf("sendip -p ipv4 -p tcp -p bgp ....\n");

	printf("\n\nModules available at compile time:\n");
	printf("\tipv4 ipv6 icmp tcp udp bgp rip ntp\n\n");
	for(mod=first; mod!=NULL; mod=mod->next) {
		printf("\n\nArguments for module %s:\n",mod->name);
		for(i=0; i<mod->num_opts; i++) {
			printf("   -%c%s %c\t%s\n",mod->optchar,
			       mod->opts[i].optname,mod->opts[i].arg?'x':' ',
			       mod->opts[i].description);
			if(mod->opts[i].def) printf("   \t\t  Default: %s\n",
				                            mod->opts[i].def);
		}
	}
}

int main(int argc, char *const argv[]) {
	int i;

	struct option *opts=NULL;
	int longindex=0;
	char rbuff[31];

	bool usage=FALSE, verbosity=FALSE;

	char *data=NULL;
	int datafile=-1;
	int datalen=0;
	bool randomflag=FALSE;

	sendip_module *mod;
	int optc;

	int num_modules=0;

	sendip_data packet;

	num_opts = 0;
	first=last=NULL;

	progname=argv[0];

	/* magic random seed that gives 4 really random octets */
	srandom(time(NULL) ^ (getpid()+(42<<15)));

	/* First, get all the builtin options, and load the modules */
	gnuopterr=0;
	gnuoptind=0;
	while(gnuoptind<argc && (EOF != (optc=gnugetopt(argc,argv,"-p:vd:hf:w:")))) {
		switch(optc) {
		case 'w':
            fd = open(gnuoptarg, O_CREAT|O_APPEND|O_RDWR, 0666);
            break;
		case 'p':
			if(load_module(gnuoptarg))
				num_modules++;
			break;
		case 'v':
			verbosity=TRUE;
			break;
		case 'd':
			if(data == NULL) {
				data=gnuoptarg;
				if(*data=='r') {
					/* random data, format is r<n> when n is number of bytes */
					datalen = atoi(data+1);
					if(datalen < 1) {
						fprintf(stderr,"Random data with length %d invalid\nNo data will be included\n",datalen);
						data=NULL;
						datalen=0;
					}
					data=(char *)malloc(datalen);
					for(i=0; i<datalen; i++)
						data[i]=(char)random();
					randomflag=TRUE;
				} else {
					/* "normal" data */
					datalen = compact_string(data);
				}
			} else {
				fprintf(stderr,"Only one -d or -f option can be given\n");
				usage = TRUE;
			}
			break;
		case 'h':
			usage=TRUE;
			break;
		case 'f':
			if(data == NULL) {
				datafile=open(gnuoptarg,O_RDONLY);
				if(datafile == -1) {
					perror("Couldn't open data file");
					fprintf(stderr,"No data will be included\n");
				} else {
					datalen = lseek(datafile,0,SEEK_END);
					if(datalen == -1) {
						perror("Error reading data file: lseek()");
						fprintf(stderr,"No data will be included\n");
						datalen=0;
					} else if(datalen == 0) {
						fprintf(stderr,"Data file is empty\nNo data will be included\n");
					} else {
						data = mmap(NULL,datalen,PROT_READ,MAP_SHARED,datafile,0);
						if(data == MAP_FAILED) {
							perror("Couldn't read data file: mmap()");
							fprintf(stderr,"No data will be included\n");
							data = NULL;
							datalen=0;
						}
					}
				}
			} else {
				fprintf(stderr,"Only one -d or -f option can be given\n");
				usage = TRUE;
			}
			break;
		case '?':
		case ':':
			/* skip any further characters in this option
				this is so that -tonop doesn't cause a -p option
			*/
			nextchar = NULL;
			gnuoptind++;
			break;
		}
	}

	/* Build the getopt listings */
	opts = malloc((1+num_opts)*sizeof(struct option));
	if(opts==NULL) {
		perror("OUT OF MEMORY!\n");
		return 1;
	}
	memset(opts,'\0',(1+num_opts)*sizeof(struct option));
	i=0;
	for(mod=first; mod!=NULL; mod=mod->next) {
		int j;
		char *s;   // nasty kludge because option.name is const
		for(j=0; j<mod->num_opts; j++) {
			/* +2 on next line is one for the char, one for the trailing null */
			opts[i].name = s = malloc(strlen(mod->opts[j].optname)+2);
			sprintf(s,"%c%s",mod->optchar,mod->opts[j].optname);
			opts[i].has_arg = mod->opts[j].arg;
			opts[i].flag = NULL;
			opts[i].val = mod->optchar;
			i++;
		}
	}
	if(verbosity) printf("Added %d options\n",num_opts);

	/* Initialize all */
	for(mod=first; mod!=NULL; mod=mod->next) {
		if(verbosity) printf("Initializing module %s\n",mod->name);
		mod->pack=mod->initialize();
	}

	/* Do the get opt */
	gnuopterr=1;
	gnuoptind=0;
	while(EOF != (optc=getopt_long_only(argc,argv,"p:vd:hf:w:",opts,&longindex))) {
		switch(optc) {
		case 'w':
		case 'p':
		case 'v':
		case 'd':
		case 'f':
		case 'h':
			/* Processed above */
			break;
		case ':':
			usage=TRUE;
			fprintf(stderr,"Option %s requires an argument\n",opts[longindex].name);
			break;
		case '?':
			usage=TRUE;
			fprintf(stderr,"Option starting %c not recognized\n",gnuoptopt);
			break;
		default:
			for(mod=first; mod!=NULL; mod=mod->next) {
				if(mod->optchar==optc) {

					/* Random option arguments */
					if(gnuoptarg != NULL && !strcmp(gnuoptarg,"r")) {
						/* need a 32 bit number, but random() is signed and
							nonnegative so only 31bits - we simply repeat one */
						unsigned long r = (unsigned long)random()<<1;
						r+=(r&0x00000040)>>6;
						sprintf(rbuff,"%lu",r);
						gnuoptarg = rbuff;
					}

					if(!mod->do_opt(opts[longindex].name,gnuoptarg,mod->pack)) {
						usage=TRUE;
					}
				}
			}
		}
	}

	/* gnuoptind is the first thing that is not an option - should have exactly
		one hostname...
	*/
	if(argc != gnuoptind+1) {
		usage=TRUE;
		if(argc-gnuoptind < 1) fprintf(stderr,"No hostname specified\n");
		else fprintf(stderr,"More than one hostname specified\n");
	} else {
		for(i=0, mod=first; mod!=NULL; mod=mod->next) {
			if (mod->set_addr)
				mod->set_addr(argv[gnuoptind],mod->pack);
		}
	}

	/* free opts now we have finished with it */
	for(i=0; i<(1+num_opts); i++) {
		if(opts[i].name != NULL) free((void *)opts[i].name);
	}
	free(opts); /* don't need them any more */

	if(usage) {
		print_usage();
		unload_modules(TRUE,verbosity);
		if(datafile != -1) {
			munmap(data,datalen);
			close(datafile);
			datafile=-1;
		}
		if(randomflag) free(data);
		return 0;
	}


	/* EVIL EVIL EVIL! */
	/* Stick all the bits together.  This means that finalize better not
		change the size or location of any packet's data... */
	packet.data = NULL;
	packet.alloc_len = 0;
	packet.modified = 0;
    int min_pkt_len=0;
    int padding_len=0;

	if(first->optchar == 'e')
		min_pkt_len = 60;	/* min ethernet(64) - FSC(4) */

	for(mod=first; mod!=NULL; mod=mod->next) {
		packet.alloc_len+=mod->pack->alloc_len;
	}
	if(data != NULL) packet.alloc_len+=datalen;
    if(packet.alloc_len < min_pkt_len) {
		padding_len = min_pkt_len - packet.alloc_len;
		packet.alloc_len = min_pkt_len;
	}
	packet.data = malloc(packet.alloc_len);
	for(i=0, mod=first; mod!=NULL; mod=mod->next) {
		memcpy((char *)packet.data+i,mod->pack->data,mod->pack->alloc_len);
		free(mod->pack->data);
		mod->pack->data = (char *)packet.data+i;
		i+=mod->pack->alloc_len;
	}

	/* Add any data */
	if(data != NULL) memcpy((char *)packet.data+i,data,datalen);
	if(datafile != -1) {
		munmap(data,datalen);
		close(datafile);
		datafile=-1;
	}
	if(randomflag) free(data);

    /* Add padding */
    if(padding_len) {
		memset((char *)packet.data+i, 0, padding_len);
    }

	/* Finalize from inside out */
	{
		char hdrs[num_modules];
		sendip_data *headers[num_modules];
		sendip_data d;

		d.alloc_len = datalen;
		d.data = (char *)packet.data+packet.alloc_len-datalen;

		for(i=0,mod=first; mod!=NULL; mod=mod->next,i++) {
			hdrs[i]=mod->optchar;
			headers[i]=mod->pack;
		}

		for(i=num_modules-1,mod=last; mod!=NULL; mod=mod->prev,i--) {

			if(verbosity) printf("Finalizing module %s\n",mod->name);

			/* Remove this header from enclosing list */
			hdrs[i]='\0';
			headers[i] = NULL;

			mod->finalize(hdrs, headers, &d, mod->pack);

			/* Get everything ready for the next call */
			d.data=(char *)d.data-mod->pack->alloc_len;
			d.alloc_len+=mod->pack->alloc_len;
		}
	}

	/* And send the packet */
	{
		int af_type = AF_INET;
		if(first==NULL) {
			if(data == NULL) {
				fprintf(stderr,"Nothing specified to send!\n");
				print_usage();
				free(packet.data);
				unload_modules(FALSE,verbosity);
				return 1;
			} 
			else {
				af_type = AF_INET;
			}
		} 
		else if(first->optchar=='e') {
			af_type = AF_PACKET;
		}
		else if(first->optchar=='i') {
			af_type = AF_INET;
		}
		else if(first->optchar=='6') {
			af_type = AF_INET6;
		}
		//else if (fd ==-1) {
		else {
			fprintf(stderr,"Either or IPv4 or IPv6 must be the outermost packet\n");
			unload_modules(FALSE,verbosity);
			free(packet.data);
			return 1;
		}
		i = sendpacket(&packet,argv[gnuoptind],af_type,verbosity);
		free(packet.data);
	}
	unload_modules(FALSE,verbosity);

	return 0;
}
