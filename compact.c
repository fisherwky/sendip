/* compact.c - function to convert hex/octal/decimal/raw string to raw
 * ChangeLog since initial release in sendip 2.1.
 */
#include <stdio.h>
#include <string.h>

int compact_string(char *data_out) {
	char *data_in = data_out;
	int i=0;
	if(*data_in=='0') {
		data_in++;
		if(*data_in=='x' || *data_in=='X') {
			/* Hex */
			char c='\0';
			data_in++;
			while(*data_in) {
				if(*data_in>='0' && *data_in<='9') {
					c+=*data_in-'0';
				} else if(*data_in>='A' && *data_in<='F') {
					c+=*data_in-'A'+10;
				} else if(*data_in>='a' && *data_in<='f') {
					c+=*data_in-'a'+10;
				} else {
					fprintf(stderr,"Character %c invalid in hex data stream\n",
					        *data_in);
					return 0;
				}
				if( i&1) {
					*(data_out++)=c;  // odd nibble - output it
					c='\0';
				} else {
					c<<=4;   // even nibble - shift to top of byte
				}
				data_in++;
				i++;
			}
			*data_out=c; // make sure last nibble is added
			i++;
			i>>=1;  // i was a nibble count...
			return i;
		} else {
			/* Octal */
			char c='\0';
			while(*data_in) {
				if(*data_in>='0' && *data_in<='7') {
					c+=*data_in-'0';
				} else {
					fprintf(stderr,"Character %c invalid in octal data stream\n",
					        *data_in);
					return 0;
				}
				if( (i&3) == 3 ) {
					*(data_out++)=c;  // output every 4th char
					c='\0';
				} else {        // otherwise just shift it up
					c<<=2;
				}
				data_in++;
				i++;
			}
			*data_out=c;     // add partial last byte
			i+=3;
			i>>=2;
			return i;
		}
	} else {
		/* String */
		return strlen(data_in);
	}
}


int compact_mac(char *data_out) {
	char *data_in = data_out;
	int ret = 0;
	int nibble = 0;

	char c='\0';
	while(*data_in) {
		if(*data_in>='0' && *data_in<='9') {
			c+=*data_in-'0';
		}
		else if(*data_in>='A' && *data_in<='F') {
			c+=*data_in-'A'+10;
		}
		else if(*data_in>='a' && *data_in<='f') {
			c+=*data_in-'a'+10;
		}
		else if(*data_in==':' && nibble==0) {
			data_in++;
			continue;
		}
		else {
			fprintf(stderr,"Character %c invalid in mac address format\n", *data_in);
			return 0;
		}
		nibble++;
		if (nibble == 2) {
			*(data_out++)=c;  // odd nibble - output it
			c='\0';
			ret++;
			nibble = 0;
		}
		else {
			c <<= 4;
		}
		data_in++;
	}
	if (nibble!=0 || ret != 6) {	// make sure last nibble is added
		fprintf(stderr,"Input length error\n");
		return 0;
	}
	return ret;
}

#if 0
//	test compact_mac()
#include <stdio.h>

void test(char *buf) {
	printf("before: %s\n", buf);
	int ret = compact_mac(buf);
	int i;
	printf("after: ");
	for (i = 0; i<ret; i++) {
		printf("%02x", (unsigned char)buf[i]);
	}
	printf("\n");
}


int main(void) {
	char buf[32];
	
	strcpy(buf, "1a:2b:3c:4d:5e:6f");
	test(buf);
	strcpy(buf, "1a2b3c4d5e6f7");
	test(buf);
	strcpy(buf, "1a2b3c:4d:5e:6f");
	test(buf);
	strcpy(buf, "1:2:3c:4d:5e:6f");
	test(buf);
	return 0;
}

#endif

