#include "helper_key.h"
#include <openssl/obj_mac.h>
#include <openssl/ripemd.h>
#include <ccan/array_size/array_size.h>
#include "../protocol.h"

static const unsigned char private_key1[] = {
	0x30,0x81,0xd3,0x02,0x01,0x01,0x04,0x20,0x53,0x2d,0x24,0xc8,
	0x1c,0x6f,0xd2,0xc1,0x6c,0x5e,0x4b,0xcf,0x9b,0x0d,0x4f,0x3a,
	0xcc,0xad,0xed,0x67,0xc4,0x32,0xfc,0x3a,0x6d,0x4c,0xaf,0xd9,
	0x43,0xea,0x37,0xc7,0xa0,0x81,0x85,0x30,0x81,0x82,0x02,0x01,
	0x01,0x30,0x2c,0x06,0x07,0x2a,0x86,0x48,0xce,0x3d,0x01,0x01,
	0x02,0x21,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xff,0xff,0xfc,0x2f,0x30,
	0x06,0x04,0x01,0x00,0x04,0x01,0x07,0x04,0x21,0x02,0x79,0xbe,
	0x66,0x7e,0xf9,0xdc,0xbb,0xac,0x55,0xa0,0x62,0x95,0xce,0x87,
	0x0b,0x07,0x02,0x9b,0xfc,0xdb,0x2d,0xce,0x28,0xd9,0x59,0xf2,
	0x81,0x5b,0x16,0xf8,0x17,0x98,0x02,0x21,0x00,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xfe,0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,
	0x8c,0xd0,0x36,0x41,0x41,0x02,0x01,0x01,0xa1,0x24,0x03,0x22,
	0x00,0x03,0xf0,0x14,0x0b,0x29,0xb0,0x76,0xf7,0x4f,0x9e,0x04,
	0x4a,0xf0,0x93,0x33,0x97,0x75,0xf0,0x45,0x74,0xf4,0xd3,0xfe,
	0x05,0xe3,0x8e,0x9a,0x97,0x74,0xbf,0xfd,0x3d,0xdf
};

static const struct protocol_pubkey public_key1 = {
	.key = { 0x03,0xf0,0x14,0x0b,0x29,0xb0,0x76,0xf7,0x4f,0x9e,0x04,
		 0x4a,0xf0,0x93,0x33,0x97,0x75,0xf0,0x45,0x74,0xf4,0xd3,0xfe,
		 0x05,0xe3,0x8e,0x9a,0x97,0x74,0xbf,0xfd,0x3d,0xdf }
};

static const unsigned char private_key2[] = {
	0x30,0x81,0xd3,0x02,0x01,0x01,0x04,0x20,0xd3,0x91,0x33,0x38,
	0xbd,0x9c,0x60,0x70,0x35,0x29,0xb8,0xc1,0x3f,0xf3,0xe6,0x59,
	0xc1,0xc5,0x0f,0xb7,0x83,0xdf,0x0d,0x3e,0x40,0x8e,0x90,0xcf,
	0x1f,0x57,0x78,0x34,0xa0,0x81,0x85,0x30,0x81,0x82,0x02,0x01,
	0x01,0x30,0x2c,0x06,0x07,0x2a,0x86,0x48,0xce,0x3d,0x01,0x01,
	0x02,0x21,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xff,0xff,0xfc,0x2f,0x30,
	0x06,0x04,0x01,0x00,0x04,0x01,0x07,0x04,0x21,0x02,0x79,0xbe,
	0x66,0x7e,0xf9,0xdc,0xbb,0xac,0x55,0xa0,0x62,0x95,0xce,0x87,
	0x0b,0x07,0x02,0x9b,0xfc,0xdb,0x2d,0xce,0x28,0xd9,0x59,0xf2,
	0x81,0x5b,0x16,0xf8,0x17,0x98,0x02,0x21,0x00,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xfe,0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,
	0x8c,0xd0,0x36,0x41,0x41,0x02,0x01,0x01,0xa1,0x24,0x03,0x22,
	0x00,0x02,0x46,0x29,0x02,0x23,0x29,0x88,0x5f,0x70,0xc8,0x89,
	0x10,0xda,0x1d,0x9c,0x64,0x16,0x0d,0xff,0x9d,0xf1,0xa6,0xdf,
	0xb6,0x51,0xb4,0xa4,0xd1,0x0f,0xfe,0xd9,0x5a,0x12
};

static const struct protocol_pubkey public_key2 = {
	.key = { 0x02,0x46,0x29,0x02,0x23,0x29,0x88,0x5f,0x70,0xc8,0x89,0x10,
		 0xda,0x1d,0x9c,0x64,0x16,0x0d,0xff,0x9d,0xf1,0xa6,0xdf,0xb6,
		 0x51,0xb4,0xa4,0xd1,0x0f,0xfe,0xd9,0x5a,0x12 }
};

static unsigned char private_key3[] = {
	0x30,0x81,0xd3,0x02,0x01,0x01,0x04,0x20,0x51,0x1e,0xd8,0xe6,
	0x41,0x71,0x10,0x0a,0x41,0x98,0x44,0x59,0xf6,0x2b,0x36,0xf9,
	0x9b,0x4e,0x8b,0x55,0x74,0x01,0xe9,0xcb,0x3e,0x79,0xc9,0x26,
	0x2d,0x39,0x65,0x8e,0xa0,0x81,0x85,0x30,0x81,0x82,0x02,0x01,
	0x01,0x30,0x2c,0x06,0x07,0x2a,0x86,0x48,0xce,0x3d,0x01,0x01,
	0x02,0x21,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xff,0xff,0xfc,0x2f,0x30,
	0x06,0x04,0x01,0x00,0x04,0x01,0x07,0x04,0x21,0x02,0x79,0xbe,
	0x66,0x7e,0xf9,0xdc,0xbb,0xac,0x55,0xa0,0x62,0x95,0xce,0x87,
	0x0b,0x07,0x02,0x9b,0xfc,0xdb,0x2d,0xce,0x28,0xd9,0x59,0xf2,
	0x81,0x5b,0x16,0xf8,0x17,0x98,0x02,0x21,0x00,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xfe,0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,
	0x8c,0xd0,0x36,0x41,0x41,0x02,0x01,0x01,0xa1,0x24,0x03,0x22,
	0x00,0x02,0xc0,0x79,0xe6,0xe1,0xe2,0x90,0x3c,0xe9,0xe2,0x4d,
	0x4d,0xa5,0x12,0x14,0xa2,0x7c,0xa6,0x90,0x7f,0xc4,0x4f,0x26,
	0xd4,0x46,0x43,0x26,0x8f,0x35,0x30,0x8d,0x7c,0x6f
};

static const struct protocol_pubkey public_key3 = {
	.key = { 0x02,0xc0,0x79,0xe6,0xe1,0xe2,0x90,0x3c,0xe9,0xe2,0x4d,0x4d,
		 0xa5,0x12,0x14,0xa2,0x7c,0xa6,0x90,0x7f,0xc4,0x4f,0x26,0xd4,
		 0x46,0x43,0x26,0x8f,0x35,0x30,0x8d,0x7c,0x6f }
};

static unsigned char private_key4[] = {
	0x30,0x81,0xd3,0x02,0x01,0x01,0x04,0x20,0xf6,0x58,0x67,0x97,
	0xce,0x2e,0xc8,0x6f,0x87,0x40,0xe1,0xd7,0x53,0x15,0xb7,0xf4,
	0x52,0xf9,0xbc,0x9c,0xee,0x06,0x4f,0x3f,0xb0,0x93,0xa0,0xd4,
	0x63,0x00,0xa2,0x6f,0xa0,0x81,0x85,0x30,0x81,0x82,0x02,0x01,
	0x01,0x30,0x2c,0x06,0x07,0x2a,0x86,0x48,0xce,0x3d,0x01,0x01,
	0x02,0x21,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xff,0xff,0xfc,0x2f,0x30,
	0x06,0x04,0x01,0x00,0x04,0x01,0x07,0x04,0x21,0x02,0x79,0xbe,
	0x66,0x7e,0xf9,0xdc,0xbb,0xac,0x55,0xa0,0x62,0x95,0xce,0x87,
	0x0b,0x07,0x02,0x9b,0xfc,0xdb,0x2d,0xce,0x28,0xd9,0x59,0xf2,
	0x81,0x5b,0x16,0xf8,0x17,0x98,0x02,0x21,0x00,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xfe,0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,
	0x8c,0xd0,0x36,0x41,0x41,0x02,0x01,0x01,0xa1,0x24,0x03,0x22,
	0x00,0x03,0x62,0xbd,0x37,0x52,0xf2,0x8c,0x6d,0xaf,0xb5,0xa2,
	0x01,0xa3,0xe3,0x47,0x03,0xf5,0x63,0x76,0xa0,0x9d,0xa9,0xbe,
	0x79,0xfa,0xe1,0x6d,0xb2,0xc5,0x48,0xd1,0x92,0x08
};
static const struct protocol_pubkey public_key4 = {
	.key = { 0x03,0x62,0xbd,0x37,0x52,0xf2,0x8c,0x6d,0xaf,0xb5,0xa2,0x01,
		 0xa3,0xe3,0x47,0x03,0xf5,0x63,0x76,0xa0,0x9d,0xa9,0xbe,0x79,
		 0xfa,0xe1,0x6d,0xb2,0xc5,0x48,0xd1,0x92,0x08 }
};
static unsigned char private_key5[] = {
	0x30,0x81,0xd3,0x02,0x01,0x01,0x04,0x20,0xe7,0x45,0x08,0xc8,
	0x6e,0xaa,0x03,0x96,0xfd,0xb6,0xc4,0x5c,0xac,0x48,0x08,0x62,
	0x9a,0xe4,0xf3,0x24,0xab,0x36,0x4b,0xd4,0xf4,0xff,0x4d,0x7a,
	0xa1,0x2e,0xe5,0x0c,0xa0,0x81,0x85,0x30,0x81,0x82,0x02,0x01,
	0x01,0x30,0x2c,0x06,0x07,0x2a,0x86,0x48,0xce,0x3d,0x01,0x01,
	0x02,0x21,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xff,0xff,0xfc,0x2f,0x30,
	0x06,0x04,0x01,0x00,0x04,0x01,0x07,0x04,0x21,0x02,0x79,0xbe,
	0x66,0x7e,0xf9,0xdc,0xbb,0xac,0x55,0xa0,0x62,0x95,0xce,0x87,
	0x0b,0x07,0x02,0x9b,0xfc,0xdb,0x2d,0xce,0x28,0xd9,0x59,0xf2,
	0x81,0x5b,0x16,0xf8,0x17,0x98,0x02,0x21,0x00,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xfe,0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,
	0x8c,0xd0,0x36,0x41,0x41,0x02,0x01,0x01,0xa1,0x24,0x03,0x22,
	0x00,0x02,0x11,0x66,0x6a,0xc9,0x6b,0xda,0x62,0x9e,0x37,0xa2,
	0x0a,0x3d,0xf5,0xdd,0x9c,0xac,0xa6,0x54,0x76,0xcd,0x08,0x04,
	0x31,0xde,0x4d,0xd7,0x4f,0xd0,0xae,0x4e,0xb5,0xe4
};
static const struct protocol_pubkey public_key5 = {
	.key = { 0x02,0x11,0x66,0x6a,0xc9,0x6b,0xda,0x62,0x9e,0x37,0xa2,0x0a,
		 0x3d,0xf5,0xdd,0x9c,0xac,0xa6,0x54,0x76,0xcd,0x08,0x04,0x31,
		 0xde,0x4d,0xd7,0x4f,0xd0,0xae,0x4e,0xb5,0xe4 }
};
static unsigned char private_key6[] = {
	0x30,0x81,0xd3,0x02,0x01,0x01,0x04,0x20,0x80,0x32,0xbc,0x7b,
	0x1b,0x0f,0x19,0x91,0xa3,0xa0,0x22,0x1b,0xff,0x86,0xf2,0xcd,
	0x86,0xef,0x08,0x81,0x2a,0x2b,0xb9,0xec,0x94,0x1b,0x01,0xfc,
	0x03,0x83,0x76,0xdf,0xa0,0x81,0x85,0x30,0x81,0x82,0x02,0x01,
	0x01,0x30,0x2c,0x06,0x07,0x2a,0x86,0x48,0xce,0x3d,0x01,0x01,
	0x02,0x21,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xff,0xff,0xfc,0x2f,0x30,
	0x06,0x04,0x01,0x00,0x04,0x01,0x07,0x04,0x21,0x02,0x79,0xbe,
	0x66,0x7e,0xf9,0xdc,0xbb,0xac,0x55,0xa0,0x62,0x95,0xce,0x87,
	0x0b,0x07,0x02,0x9b,0xfc,0xdb,0x2d,0xce,0x28,0xd9,0x59,0xf2,
	0x81,0x5b,0x16,0xf8,0x17,0x98,0x02,0x21,0x00,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xfe,0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,
	0x8c,0xd0,0x36,0x41,0x41,0x02,0x01,0x01,0xa1,0x24,0x03,0x22,
	0x00,0x03,0x64,0xdd,0xef,0xd6,0x3c,0x3e,0x65,0x6d,0xa6,0x8c,
	0x2a,0x6d,0xf4,0xb3,0xe6,0x96,0x60,0x37,0xd1,0x04,0x53,0x40,
	0x2d,0x03,0xe2,0x13,0x73,0x90,0x46,0x24,0xec,0x03
};
static const struct protocol_pubkey public_key6 = {
	.key = { 0x03,0x64,0xdd,0xef,0xd6,0x3c,0x3e,0x65,0x6d,0xa6,0x8c,0x2a,
		 0x6d,0xf4,0xb3,0xe6,0x96,0x60,0x37,0xd1,0x04,0x53,0x40,0x2d,
		 0x03,0xe2,0x13,0x73,0x90,0x46,0x24,0xec,0x03 }
};
static unsigned char private_key7[] = {
	0x30,0x81,0xd3,0x02,0x01,0x01,0x04,0x20,0xa1,0x3d,0xcd,0xc6,
	0xa2,0x7e,0xd9,0xa1,0x54,0xa5,0x56,0xdb,0xb5,0xc2,0xfa,0x5e,
	0x30,0x23,0x74,0x4b,0xe3,0xb4,0x3a,0x49,0xc3,0x70,0xd9,0xd0,
	0x7f,0x95,0x7b,0x67,0xa0,0x81,0x85,0x30,0x81,0x82,0x02,0x01,
	0x01,0x30,0x2c,0x06,0x07,0x2a,0x86,0x48,0xce,0x3d,0x01,0x01,
	0x02,0x21,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xff,0xff,0xfc,0x2f,0x30,
	0x06,0x04,0x01,0x00,0x04,0x01,0x07,0x04,0x21,0x02,0x79,0xbe,
	0x66,0x7e,0xf9,0xdc,0xbb,0xac,0x55,0xa0,0x62,0x95,0xce,0x87,
	0x0b,0x07,0x02,0x9b,0xfc,0xdb,0x2d,0xce,0x28,0xd9,0x59,0xf2,
	0x81,0x5b,0x16,0xf8,0x17,0x98,0x02,0x21,0x00,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xfe,0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,
	0x8c,0xd0,0x36,0x41,0x41,0x02,0x01,0x01,0xa1,0x24,0x03,0x22,
	0x00,0x02,0x45,0x8f,0x2f,0xcf,0xed,0xd7,0xef,0x09,0xbd,0x30,
	0xcc,0xbb,0x3c,0x0a,0xa0,0xd3,0x07,0x0c,0x40,0x34,0x3d,0x0f,
	0x32,0xcb,0xb7,0x32,0x85,0xdd,0x86,0xc2,0x5d,0x8e
};
static const struct protocol_pubkey public_key7 = {
	.key = { 0x02,0x45,0x8f,0x2f,0xcf,0xed,0xd7,0xef,0x09,0xbd,0x30,0xcc,
		 0xbb,0x3c,0x0a,0xa0,0xd3,0x07,0x0c,0x40,0x34,0x3d,0x0f,0x32,
		 0xcb,0xb7,0x32,0x85,0xdd,0x86,0xc2,0x5d,0x8e }
};
static unsigned char private_key8[] = {
	0x30,0x81,0xd3,0x02,0x01,0x01,0x04,0x20,0x29,0x95,0x6c,0x08,
	0x4a,0x8d,0xe0,0x1b,0xa6,0xec,0x7c,0xdc,0x83,0x40,0x1f,0x79,
	0xff,0xa2,0xa2,0x5c,0x71,0xeb,0x80,0x13,0x88,0x74,0x37,0x25,
	0xd3,0x92,0x3c,0x63,0xa0,0x81,0x85,0x30,0x81,0x82,0x02,0x01,
	0x01,0x30,0x2c,0x06,0x07,0x2a,0x86,0x48,0xce,0x3d,0x01,0x01,
	0x02,0x21,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xff,0xff,0xfc,0x2f,0x30,
	0x06,0x04,0x01,0x00,0x04,0x01,0x07,0x04,0x21,0x02,0x79,0xbe,
	0x66,0x7e,0xf9,0xdc,0xbb,0xac,0x55,0xa0,0x62,0x95,0xce,0x87,
	0x0b,0x07,0x02,0x9b,0xfc,0xdb,0x2d,0xce,0x28,0xd9,0x59,0xf2,
	0x81,0x5b,0x16,0xf8,0x17,0x98,0x02,0x21,0x00,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xfe,0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,
	0x8c,0xd0,0x36,0x41,0x41,0x02,0x01,0x01,0xa1,0x24,0x03,0x22,
	0x00,0x03,0x5a,0xec,0x79,0x5f,0x49,0x93,0x99,0xaf,0x1e,0x94,
	0xdc,0xb0,0x80,0xf4,0xdd,0x43,0x86,0xb8,0x11,0xf9,0x34,0xe3,
	0x2d,0x3a,0x8c,0x75,0x7f,0x42,0x07,0xe2,0xfc,0x44
};
static const struct protocol_pubkey public_key8 = {
	.key = { 0x03,0x5a,0xec,0x79,0x5f,0x49,0x93,0x99,0xaf,0x1e,0x94,0xdc,
		 0xb0,0x80,0xf4,0xdd,0x43,0x86,0xb8,0x11,0xf9,0x34,0xe3,0x2d,
		 0x3a,0x8c,0x75,0x7f,0x42,0x07,0xe2,0xfc,0x44 }
};
static unsigned char private_key9[] = {
	0x30,0x81,0xd3,0x02,0x01,0x01,0x04,0x20,0x8d,0x88,0x34,0x99,
	0x04,0xee,0x3b,0x8b,0x88,0x7f,0x7f,0x25,0xdc,0xf5,0xab,0x01,
	0x85,0xe7,0xf8,0xd2,0x95,0xbb,0x34,0x35,0x9f,0x3c,0x3f,0x66,
	0xa7,0x91,0x9f,0xf2,0xa0,0x81,0x85,0x30,0x81,0x82,0x02,0x01,
	0x01,0x30,0x2c,0x06,0x07,0x2a,0x86,0x48,0xce,0x3d,0x01,0x01,
	0x02,0x21,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xff,0xff,0xfc,0x2f,0x30,
	0x06,0x04,0x01,0x00,0x04,0x01,0x07,0x04,0x21,0x02,0x79,0xbe,
	0x66,0x7e,0xf9,0xdc,0xbb,0xac,0x55,0xa0,0x62,0x95,0xce,0x87,
	0x0b,0x07,0x02,0x9b,0xfc,0xdb,0x2d,0xce,0x28,0xd9,0x59,0xf2,
	0x81,0x5b,0x16,0xf8,0x17,0x98,0x02,0x21,0x00,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xfe,0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,
	0x8c,0xd0,0x36,0x41,0x41,0x02,0x01,0x01,0xa1,0x24,0x03,0x22,
	0x00,0x02,0xc1,0x1a,0x74,0x32,0x49,0xa3,0xcc,0xc2,0x6b,0x20,
	0x63,0x6a,0x39,0x08,0x28,0x1a,0xb6,0x4f,0xf3,0xdd,0xda,0xc8,
	0xd4,0xab,0xd3,0x13,0xbb,0x24,0xfd,0x0e,0x2e,0xdb
};
static const struct protocol_pubkey public_key9 = {
	.key = { 0x02,0xc1,0x1a,0x74,0x32,0x49,0xa3,0xcc,0xc2,0x6b,0x20,0x63,
		 0x6a,0x39,0x08,0x28,0x1a,0xb6,0x4f,0xf3,0xdd,0xda,0xc8,0xd4,
		 0xab,0xd3,0x13,0xbb,0x24,0xfd,0x0e,0x2e,0xdb }
};
static unsigned char private_key10[] = {
	0x30,0x81,0xd3,0x02,0x01,0x01,0x04,0x20,0x73,0xe2,0xd9,0xa9,
	0x96,0x6a,0xa3,0xf2,0x14,0x93,0x33,0xb7,0xa4,0x53,0xb0,0x13,
	0xe4,0x3e,0x39,0xfc,0x50,0x38,0x8c,0xa0,0x71,0x88,0x22,0xba,
	0x6f,0xef,0xcd,0x0a,0xa0,0x81,0x85,0x30,0x81,0x82,0x02,0x01,
	0x01,0x30,0x2c,0x06,0x07,0x2a,0x86,0x48,0xce,0x3d,0x01,0x01,
	0x02,0x21,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xff,0xff,0xfc,0x2f,0x30,
	0x06,0x04,0x01,0x00,0x04,0x01,0x07,0x04,0x21,0x02,0x79,0xbe,
	0x66,0x7e,0xf9,0xdc,0xbb,0xac,0x55,0xa0,0x62,0x95,0xce,0x87,
	0x0b,0x07,0x02,0x9b,0xfc,0xdb,0x2d,0xce,0x28,0xd9,0x59,0xf2,
	0x81,0x5b,0x16,0xf8,0x17,0x98,0x02,0x21,0x00,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xfe,0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,
	0x8c,0xd0,0x36,0x41,0x41,0x02,0x01,0x01,0xa1,0x24,0x03,0x22,
	0x00,0x02,0x82,0xa7,0x0a,0x2e,0x35,0x5a,0x44,0x8e,0xee,0xa9,
	0xb2,0x9d,0x0a,0x9e,0x8b,0xc0,0xe5,0x37,0xad,0x42,0xe4,0xf8,
	0x97,0xa7,0x26,0x7e,0x95,0x23,0x36,0x6a,0xc4,0x99
};
static const struct protocol_pubkey public_key10 = {
	.key = { 0x02,0x82,0xa7,0x0a,0x2e,0x35,0x5a,0x44,0x8e,0xee,0xa9,0xb2,
		 0x9d,0x0a,0x9e,0x8b,0xc0,0xe5,0x37,0xad,0x42,0xe4,0xf8,0x97,
		 0xa7,0x26,0x7e,0x95,0x23,0x36,0x6a,0xc4,0x99 }
};

static const unsigned char *private_keys[] = {
	private_key1,
	private_key2,
	private_key3,
	private_key4,
	private_key5,
	private_key6,
	private_key7,
	private_key8,
	private_key9,
	private_key10
};

static const struct protocol_pubkey *public_keys[] = {
	&public_key1,
	&public_key2,
	&public_key3,
	&public_key4,
	&public_key5,
	&public_key6,
	&public_key7,
	&public_key8,
	&public_key9,
	&public_key10
};

static struct protocol_address public_addr[ARRAY_SIZE(public_keys)];

EC_KEY *helper_private_key(int index)
{
	const unsigned char *p = private_keys[index];
	EC_KEY *priv = EC_KEY_new_by_curve_name(NID_secp256k1);

	if (!d2i_ECPrivateKey(&priv, &p, sizeof(private_key1)))
		abort();

	/* We *always* used compressed form keys. */
	EC_KEY_set_conv_form(priv, POINT_CONVERSION_COMPRESSED);
	return priv;
}

const struct protocol_pubkey *helper_public_key(int index)
{
	return public_keys[index];
}

const struct protocol_address *helper_addr(int index)
{
	RIPEMD160(helper_public_key(index)->key,
		  sizeof(struct protocol_pubkey),
		  public_addr[index].addr);
	return &public_addr[index];
}
