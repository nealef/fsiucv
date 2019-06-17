#ifndef _IUCV_H
# define _IUCV_H

/*
 * Mapping of external interrupt buffers should be used with the corresponding
 * interrupt types.
 * Names: iucv_ConnectionPending    ->  connection pending
 *        iucv_ConnectionComplete   ->  connection complete
 *        iucv_ConnectionSevered    ->  connection severed
 *        iucv_ConnectionQuiesced   ->  connection quiesced
 *        iucv_ConnectionResumed    ->  connection resumed
 *        iucv_MessagePending       ->  message pending
 *        iucv_MessageComplete      ->  message complete
 */
typedef struct {
	u16 ippathid;
	u8 ipflags1;
	u8 iptype;
	u16 ipmsglim;
	u16 res1;
	u8 ipvmid[8];
	u8 ipuser[16];
	u32 res3;
	u8 ippollfg;
	u8 res4[3];
} iucv_ConnectionPending;

typedef struct {
	u16 ippathid;
	u8 ipflags1;
	u8 iptype;
	u16 ipmsglim;
	u16 res1;
	u8 res2[8];
	u8 ipuser[16];
	u32 res3;
	u8 ippollfg;
	u8 res4[3];
} iucv_ConnectionComplete;

typedef struct {
	u16 ippathid;
	u8 res1;
	u8 iptype;
	u32 res2;
	u8 res3[8];
	u8 ipuser[16];
	u32 res4;
	u8 ippollfg;
	u8 res5[3];
} iucv_ConnectionSevered;

typedef struct {
	u16 ippathid;
	u8 res1;
	u8 iptype;
	u32 res2;
	u8 res3[8];
	u8 ipuser[16];
	u32 res4;
	u8 ippollfg;
	u8 res5[3];
} iucv_ConnectionQuiesced;

typedef struct {
	u16 ippathid;
	u8 res1;
	u8 iptype;
	u32 res2;
	u8 res3[8];
	u8 ipuser[16];
	u32 res4;
	u8 ippollfg;
	u8 res5[3];
} iucv_ConnectionResumed;

typedef struct {
	u16 ippathid;
	u8 ipflags1;
	u8 iptype;
	u32 ipmsgid;
	u32 iptrgcls;
	u8 iprmmsg1[4];
	union u1 {
		u32 ipbfln1f;
		u8 iprmmsg2[4];
	} ln1msg2;
	u32 res1[3];
	u32 ipbfln2f;
	u8 ippollfg;
	u8 res2[3];
} iucv_MessagePending;

typedef struct {
	u16 ippathid;
	u8 ipflags1;
	u8 iptype;
	u32 ipmsgid;
	u32 ipaudit;
	u8 iprmmsg[8];
	u32 ipsrccls;
	u32 ipmsgtag;
	u32 res;
	u32 ipbfln2f;
	u8 ippollfg;
	u8 res2[3];
} iucv_MessageComplete;

#endif
