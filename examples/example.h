#ifndef __EXAMPLE_H__

#define __EXAMPLE_H__
/*
 * Rather than using the iconv() API which would be 
 * more correct. We just use a couple of 256 byte 
 * translate tables
 */
static char E2ATBL[256] = {
0x00, 0x01, 0x02, 0x03, 0xec, 0x09, 0xca, 0x1c,  // 00
0xe2, 0xd2, 0xd3, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,  // 08
0x10, 0x11, 0x12, 0x13, 0xef, 0x0a, 0x08, 0xcb,  // 10
0x18, 0x19, 0xdc, 0xd8, 0x1a, 0x1d, 0x1e, 0x1f,  // 18
0xb7, 0xb8, 0xb9, 0xbb, 0xc4, 0x0a, 0x17, 0x1b,  // 20
0xcc, 0xcd, 0xcf, 0xd0, 0xd1, 0x05, 0x06, 0x07,  // 28
0xd9, 0xda, 0x16, 0xdd, 0xde, 0xdf, 0xe0, 0x04,  // 30
0xe3, 0xe5, 0xe9, 0xeb, 0xb0, 0xb1, 0x9e, 0x7f,  // 38
0x20, 0xff, 0x83, 0x84, 0x85, 0xa0, 0xf2, 0x86,  // 40
0x87, 0xa4, 0x9b, 0x2e, 0x3c, 0x28, 0x2b, 0x7c,  // 48
0x26, 0x82, 0x88, 0x89, 0x8a, 0xa1, 0x8c, 0x8b,  // 50
0x8d, 0xe1, 0x21, 0x24, 0x2a, 0x29, 0x3b, 0x5e,  // 58
0x2d, 0x2f, 0xb2, 0x8e, 0xb4, 0xb5, 0xb6, 0x8f,  // 60
0x80, 0xa5, 0xb3, 0x2c, 0x25, 0x5f, 0x3e, 0x3f,  // 68
0xba, 0x90, 0xbc, 0xbd, 0xbe, 0xf3, 0xc0, 0xc1,  // 70
0xc2, 0x60, 0x3a, 0x23, 0x40, 0x27, 0x3d, 0x22,  // 78
0xc3, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,  // 80
0x68, 0x69, 0xae, 0xaf, 0xc6, 0xc7, 0xc8, 0xf1,  // 88
0xf8, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,  // 90
0x71, 0x72, 0xa6, 0xa7, 0x91, 0xce, 0x92, 0xa9,  // 98
0xe6, 0x7e, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,  // a0
0x79, 0x7a, 0xad, 0xa8, 0xd4, 0x5b, 0xd6, 0xd7,  // a9
0xaa, 0x9c, 0x9d, 0xfa, 0x9f, 0x15, 0x14, 0xac,  // b0
0xab, 0xfc, 0xd5, 0xfe, 0xe4, 0x5d, 0xbf, 0xe7,  // b8
0x7b, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,  // c0
0x48, 0x49, 0xe8, 0x93, 0x94, 0x95, 0xa2, 0xed,  // c8
0x7d, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,  // d0
0x51, 0x52, 0xee, 0x96, 0x81, 0x97, 0xa3, 0x98,  // d8
0x5c, 0xf6, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,  // e0
0x59, 0x5a, 0xfd, 0xf5, 0x99, 0xf7, 0xf0, 0xf9,  // e8
0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  // f0
0x38, 0x39, 0xdb, 0xfb, 0x9a, 0xf4, 0xea, 0xc9   // f8
};

static char A2ETBL[256] = {
0x00, 0x01, 0x02, 0x03, 0x37, 0x2d, 0x2e, 0x2f,
0x16, 0x05, 0x15, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
0x10, 0x11, 0x12, 0x13, 0xb6, 0xb5, 0x32, 0x26,
0x18, 0x19, 0x1c, 0x27, 0x07, 0x1d, 0x1e, 0x1f,
0x40, 0x5a, 0x7f, 0x7b, 0x5b, 0x6c, 0x50, 0x7d,
0x4d, 0x5d, 0x5c, 0x4e, 0x6b, 0x60, 0x4b, 0x61,
0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
0xf8, 0xf9, 0x7a, 0x5e, 0x4c, 0x7e, 0x6e, 0x6f,
0x7c, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
0xc8, 0xc9, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
0xd7, 0xd8, 0xd9, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
0xe7, 0xe8, 0xe9, 0xad, 0xe0, 0xbd, 0x5f, 0x6d,
0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
0x97, 0x98, 0x99, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
0xa7, 0xa8, 0xa9, 0xc0, 0x4f, 0xd0, 0xa1, 0x3f,
0x68, 0xdc, 0x51, 0x42, 0x43, 0x44, 0x47, 0x48,
0x52, 0x53, 0x54, 0x57, 0x56, 0x58, 0x63, 0x67,
0x71, 0x9c, 0x9e, 0xcb, 0xcc, 0xcd, 0xdb, 0xdd,
0xdf, 0xec, 0xfc, 0x4a, 0xb1, 0xb2, 0x3e, 0xb4,
0x45, 0x55, 0xce, 0xde, 0x49, 0x69, 0x9a, 0x9b,
0xab, 0x9f, 0xb0, 0xb8, 0xb7, 0xaa, 0x8a, 0x8b,
0x3c, 0x3d, 0x62, 0x6a, 0x64, 0x65, 0x66, 0x20,
0x21, 0x22, 0x70, 0x23, 0x72, 0x73, 0x74, 0xbe,
0x76, 0x77, 0x78, 0x80, 0x24, 0x15, 0x8c, 0x8d,
0x8e, 0xff, 0x06, 0x17, 0x28, 0x29, 0x9d, 0x2a,
0x2b, 0x2c, 0x09, 0x0a, 0xac, 0xba, 0xae, 0xaf,
0x1b, 0x30, 0x31, 0xfa, 0x1a, 0x33, 0x34, 0x35,
0x36, 0x59, 0x08, 0x38, 0xbc, 0x39, 0xa0, 0xbf,
0xca, 0x3a, 0xfe, 0x3b, 0x04, 0xcf, 0xda, 0x14,
0xee, 0x8f, 0x46, 0x75, 0xfd, 0xeb, 0xe1, 0xed,
0x90, 0xef, 0xb3, 0xfb, 0xb9, 0xea, 0xbb, 0x41
};

/*
 * Translate to EBCDIC
 */ 
static __inline__ char *
a2e(char *data, size_t len)
{
	/*
	* Translate the data to be sent from ASCII to EBCDIC
	*/ 
	__asm__ ("	bctr	%1,0\n"
	     "	exrl	%1,1f\n"
	     ".data\n"
	     "1:	tr	0(1,%0),%2\n"
	     ".text\n"
	     : : "r" (data), "r" (len), "Q" (A2ETBL) : "memory", "cc"); 
	return (data);
}

/*
 * Translate to ASCII
 */ 
static __inline__ char *
e2a(char *data, size_t len)
{
	__asm__ ("	bctr	%1,0\n"
		 "	exrl	%1,1f\n"
		     ".data\n"
		     "1:	tr	0(1,%0),%2\n"
		     ".text\n"
		     : : "r" (data), "r" (len), "Q" (E2ATBL) : "memory", "cc"); 
	return (data);
}
#endif
