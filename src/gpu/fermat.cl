/*
 * fermat.cl
 *
 *  Created on: 26.12.2013
 *      Author: mad
 */





#define N 11

#define SIZE 4096
#define LSIZE 256
#define STRIPES 420
#define WIDTH 20
#define PCOUNT (40960)
#define SCOUNT PCOUNT
#define TARGET 10


typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

__constant uint32_t binvert_limb_table[128] = {
  0x01, 0xAB, 0xCD, 0xB7, 0x39, 0xA3, 0xC5, 0xEF,
  0xF1, 0x1B, 0x3D, 0xA7, 0x29, 0x13, 0x35, 0xDF,
  0xE1, 0x8B, 0xAD, 0x97, 0x19, 0x83, 0xA5, 0xCF,
  0xD1, 0xFB, 0x1D, 0x87, 0x09, 0xF3, 0x15, 0xBF,
  0xC1, 0x6B, 0x8D, 0x77, 0xF9, 0x63, 0x85, 0xAF,
  0xB1, 0xDB, 0xFD, 0x67, 0xE9, 0xD3, 0xF5, 0x9F,
  0xA1, 0x4B, 0x6D, 0x57, 0xD9, 0x43, 0x65, 0x8F,
  0x91, 0xBB, 0xDD, 0x47, 0xC9, 0xB3, 0xD5, 0x7F,
  0x81, 0x2B, 0x4D, 0x37, 0xB9, 0x23, 0x45, 0x6F,
  0x71, 0x9B, 0xBD, 0x27, 0xA9, 0x93, 0xB5, 0x5F,
  0x61, 0x0B, 0x2D, 0x17, 0x99, 0x03, 0x25, 0x4F,
  0x51, 0x7B, 0x9D, 0x07, 0x89, 0x73, 0x95, 0x3F,
  0x41, 0xEB, 0x0D, 0xF7, 0x79, 0xE3, 0x05, 0x2F,
  0x31, 0x5B, 0x7D, 0xE7, 0x69, 0x53, 0x75, 0x1F,
  0x21, 0xCB, 0xED, 0xD7, 0x59, 0xC3, 0xE5, 0x0F,
  0x11, 0x3B, 0x5D, 0xC7, 0x49, 0x33, 0x55, 0xFF
};


typedef struct {
	
	uint index;
	uchar origin;
	uchar chainpos;
	uchar type;
	uchar hashid;
	
} fermat_t;


typedef struct {
	
	uint N_;
	uint SIZE_;
	uint STRIPES_;
	uint WIDTH_;
	uint PCOUNT_;
	uint TARGET_;
	
} config_t;



__kernel void getconfig(__global config_t* conf)
{
	config_t c;
	c.N_ = N;
	c.SIZE_ = SIZE;
	c.STRIPES_ = STRIPES;
	c.WIDTH_ = WIDTH;
	c.PCOUNT_ = PCOUNT;
	c.TARGET_ = TARGET;
	*conf = c;
}



/*void mad_211(uint* restrict c, const uint* restrict a, const uint* restrict b) {
	
	#pragma unroll
	for(uint i = 0; i < N; ++i){
		
		uint tmp = a[i] * b[0] + c[i];
		
		#pragma unroll
		for(uint k = 1; k < N; ++k){
			
			const uint ind = i+k;
			const uint tmp2 = a[i] * b[k] + c[ind] + (tmp < c[ind-1]);
			c[ind-1] = tmp;
			tmp = tmp2;
			
		}
		
		c[i+N] += (tmp < c[i+N-1]);
		c[i+N-1] = tmp;
		
		tmp = mul_hi(a[i], b[0]) + c[i+1];
		
		#pragma unroll
		for(uint k = 1; k < N; ++k){
			
			const uint ind = i+k+1;
			const uint tmp2 = mul_hi(a[i], b[k]) + c[ind] + (tmp < c[ind-1]);
			c[ind-1] = tmp;
			tmp = tmp2;
			
		}
		
		if(i+N+1 < 2*N)
			c[i+N+1] += (tmp < c[i+N]);
		
		c[i+N] = tmp;
		
	}
	
}*/


void mad_211(uint* restrict c, const uint* restrict a, const uint* restrict b) {
	
	#pragma unroll
	for(uint i = 0; i < N; ++i){
		
		bool carry = (c[i] + a[i] * b[0]) < c[i];
		c[i] += a[i] * b[0];
		
		#pragma unroll
		for(uint k = 1; k < N; ++k){
			
			const uint ind = i+k;
			bool carry2 = (c[ind] + a[i] * b[k] + carry) < c[ind];
			c[ind] = c[ind] + a[i] * b[k] + carry;
			carry = carry2;
			
		}
		
		c[i+N] += carry;
		
		carry = (c[i+1] + mul_hi(a[i], b[0])) < c[i+1];
		c[i+1] += mul_hi(a[i], b[0]);
		
		#pragma unroll
		for(uint k = 1; k < N; ++k){
			
			const uint ind = i+k+1;
			bool carry2 = (c[ind] + mul_hi(a[i], b[k]) + carry) < c[ind];
			c[ind] = c[ind] + mul_hi(a[i], b[k]) + carry;
			carry = carry2;
			
		}
		
		if(i+N+1 < 2*N)
			c[i+N+1] += carry;
		
	}
	
	
}


void add(uint* restrict c, const uint* restrict a, const uint* restrict b) {
	
	c[0] = a[0] + b[0];
	
	#pragma unroll
	for(uint i = 1; i < N; ++i)
		c[i] = a[i] + b[i] + (c[i-1] < a[i-1]);
	
}

void add_ui(uint* a, uint b) {
	
	const uint tmp = a[0] + b;
	a[1] += (tmp < a[0]);
	a[0] = tmp;
	
}


void sub(uint* restrict c, const uint* restrict a, const uint* restrict b) {
	
	c[0] = a[0] - b[0];
	
	#pragma unroll
	for(uint i = 1; i < N; ++i)
		c[i] = a[i] - b[i] - (c[i-1] > a[i-1]);
		//c[i] = a[i] - b[i] - (a[i-1] < b[i-1]);
	
}

void sub_ui(uint* a, uint b) {
	
	a[1] -= (a[0] < b);
	a[0] -= b;
	
}


void shr(uint* a) {
	
	#pragma unroll
	for(uint i = 0; i < N-1; ++i)
		a[i] = (a[i] >> 1) | (a[i+1] << 31);
	
	a[N-1] = a[N-1] >> 1;
	
}

void shr2(uint* restrict a, const uint* restrict b) {
	
	#pragma unroll
	for(uint i = 0; i < N-1; ++i)
		a[i] = (b[i] >> 1) | (b[i+1] << 31);
	
	a[N-1] = b[N-1] >> 1;
	
}


void shl(uint* restrict a) {
	
	#pragma unroll
	for(int i = N-1; i > 0; --i)
		a[i] = (a[i] << 1) | (a[i-1] >> 31);
	
	a[0] = a[0] << 1;
	
}

void shl2(uint* restrict a, const uint* restrict b) {
	
	#pragma unroll
	for(int i = N-1; i > 0; --i)
		a[i] = (b[i] << 1) | (b[i-1] >> 31);
	
	a[0] = b[0] << 1;
	
}

void shlx(uint* restrict a, uint bits) {
	
	#pragma unroll
	for(int i = N-1; i > 0; --i)
		a[i] = (a[i] << bits) | (a[i-1] >> (32-bits));
	
	a[0] = a[0] << bits;
	
}


void xor(uint* restrict c, const uint* restrict a, const uint* restrict b) {
	
	#pragma unroll
	for(uint i = 0; i < N; ++i)
		c[i] = a[i] ^ b[i];
	
}


void and(uint* restrict c, const uint* restrict a, const uint* restrict b) {
	
	#pragma unroll
	for(uint i = 0; i < N; ++i)
		c[i] = a[i] & b[i];
	
}


void or(uint* restrict c, const uint* restrict a, const uint* restrict b) {
	
	#pragma unroll
	for(uint i = 0; i < N; ++i)
		c[i] = a[i] | b[i];
	
}


void mov(uint* restrict a, const uint* restrict b) {
	
	#pragma unroll
	for(uint i = 0; i < N; ++i)
		a[i] = b[i];
	
}


void xbinGCD(const uint* restrict a, const uint* restrict b, uint* restrict u, uint* restrict v) {
	
	u[0] = 1;
	for(uint i = 1; i < N; ++i)
		u[i] = 0;
	
	for(uint i = 0; i < N; ++i)
		v[i] = 0;
	
	uint t[N];
	
	for(uint i = 0; i < N*32; ++i){
		
		if((u[0] & 1) == 0){
			
			shr(u);
			shr(v);
			
		}else{
			
			add(t, u, b);
			shr2(u, t);
			
			shr2(t, v);
			add(v, t, a);
			
		}
		
	}
	
}


void modul(uint* restrict x, uint* restrict y, const uint* restrict z) {
	
	uint v[N];
	
	for(uint k = 0; k < N*32; ++k){
		
		const bool t = x[N-1] >> 31;
		
		shl(x);
		x[0] |= y[N-1] >> 31;
		shl(y);
		
		sub(v, x, z);
		const bool test = v[N-1] <= x[N-1];
		
		if(test || t){
			
			mov(x, v);
			add_ui(y, 1);
			
		}
		
	}
	
}

void modulz(uint* restrict x, /*uint* restrict y,*/ const uint* restrict z) {
	
	uint v[N];
	
	for(uint k = 0; k < N*32; ++k){
		
		const bool t = x[N-1] >> 31;
		
		shl(x);
		
		sub(v, x, z);
		const bool test = v[N-1] <= x[N-1];
		
		if(test || t){
			
			mov(x, v);
			
		}
		
	}
	
}


/*uint tdiv_ui(uint* y, uint z) {
	
	uint x[N];
	for(uint i = 0; i < N; ++i)
		x[i] = 0;
	
	for(uint k = 0; k < N*32; ++k){
		
		const bool t = x[N-1] >> 31;
		
		shl(x);
		x[0] |= y[N-1] >> 31;
		shl(y);
		
		const bool test = x[1] > 0 || x[0] >= z;
		
		if(test || t){
			
			sub_ui(x, z);
			add_ui(y, 1);
			
		}
		
	}
	
	return x[0];
	
}*/

uint tdiv_ui(const uint* y, uint z) {
	
	uint x = 0;
	
	#pragma unroll
	for(uint k = 0; k < N*32; ++k){
		
		x = x << 1;
		x |= (y[N-1-(k/32)] >> (31-(k%32))) & 1u;
		
		if(x >= z)
			x -= z;
		
	}
	
	return x;
	
}


void montymul(uint* restrict c, const uint* restrict a, const uint* restrict b, const uint* restrict m, const uint* restrict mp) {
	
	uint t[2*N];
	uint s[2*N];
	for(int i = 0; i < 2*N; ++i){
		t[i] = 0;
		s[i] = 0;
	}
	
	mad_211(t, a, b);
	mad_211(s, t, mp);
	mad_211(t, s, m);
	
	for(int i = 0; i < N; ++i)
		c[i] = t[N+i];
	
	sub(t, c, m);
	
	if(t[N-1] <= c[N-1])
		mov(c, t);
	
}

uint32_t add128(uint4 *A, uint4 B)
{
  *A += B; 
  uint4 carry = -convert_uint4((*A) < B);
  
  (*A).y += carry.x; carry.y += ((*A).y < carry.x);
  (*A).z += carry.y; carry.z += ((*A).z < carry.y);
  (*A).w += carry.z;
  return carry.w + ((*A).w < carry.z); 
}

uint32_t add128Carry(uint4 *A, uint4 B, uint32_t externalCarry)
{
  *A += B;
  uint4 carry = -convert_uint4((*A) < B);
  
  (*A).x += externalCarry; carry.x += ((*A).x < externalCarry);
  (*A).y += carry.x; carry.y += ((*A).y < carry.x);
  (*A).z += carry.y; carry.z += ((*A).z < carry.y);
  (*A).w += carry.z;
  return carry.w + ((*A).w < carry.z); 
}

uint32_t add256(uint4 *a0, uint4 *a1, uint4 b0, uint4 b1)
{
  return add128Carry(a1, b1, add128(a0, b0));
}

uint32_t add384(uint4 *a0, uint4 *a1, uint4 *a2, uint4 b0, uint4 b1, uint4 b2)
{
  return add128Carry(a2, b2, add128Carry(a1, b1, add128(a0, b0)));
}

uint32_t add512(uint4 *a0, uint4 *a1, uint4 *a2, uint4 *a3, uint4 b0, uint4 b1, uint4 b2, uint4 b3)
{
  return add128Carry(a3, b3, add128Carry(a2, b2, add128Carry(a1, b1, add128(a0, b0))));
}

uint32_t sub64Borrow(uint2 *A, uint2 B, uint32_t externalBorrow)
{
  uint2 borrow = -convert_uint2((*A) < B);
  *A -= B;
  
  borrow.x += (*A).x < externalBorrow; (*A).x -= externalBorrow;
  borrow.y += (*A).y < borrow.x; (*A).y -= borrow.x;
  return borrow.y;
}

uint32_t sub96Borrow(uint4 *A, uint4 B, uint32_t externalBorrow)
{
  //   uint2 borrow = -convert_uint2((*A) < B);
  uint4 borrow = {
    (*A).x < B.x,
      (*A).y < B.y,
      (*A).z < B.z,
      0
  };
  (*A).x -= B.x;
  (*A).y -= B.y;
  (*A).z -= B.z;
  
  borrow.x += (*A).x < externalBorrow; (*A).x -= externalBorrow;
  borrow.y += (*A).y < borrow.x; (*A).y -= borrow.x;
  borrow.z += (*A).z < borrow.y; (*A).z -= borrow.y;
  
  return borrow.z;
}

uint32_t sub128(uint4 *A, uint4 B)
{
  uint4 borrow = {
    (*A).x < B.x,
      (*A).y < B.y,
      (*A).z < B.z,
      (*A).w < B.w
  };
  (*A).x -= B.x;
  (*A).y -= B.y;
  (*A).z -= B.z;
  (*A).w -= B.w;  
  
  borrow.y += (*A).y < borrow.x; (*A).y -= borrow.x;
  borrow.z += (*A).z < borrow.y; (*A).z -= borrow.y;
  borrow.w += (*A).w < borrow.z; (*A).w -= borrow.z;
  return borrow.w;
}

uint32_t sub128Borrow(uint4 *A, uint4 B, uint32_t externalBorrow)
{
  uint4 borrow = -convert_uint4((*A) < B);
  *A -= B;
  
  borrow.x += (*A).x < externalBorrow; (*A).x -= externalBorrow;
  borrow.y += (*A).y < borrow.x; (*A).y -= borrow.x;
  borrow.z += (*A).z < borrow.y; (*A).z -= borrow.y;
  borrow.w += (*A).w < borrow.z; (*A).w -= borrow.z;
  return borrow.w;
}

uint32_t sub256(uint4 *a0, uint4 *a1, uint4 b0, uint4 b1)
{
  return sub128Borrow(a1, b1, sub128(a0, b0));
}

uint32_t sub352(uint4 *a0, uint4 *a1, uint4 *a2, uint4 b0, uint4 b1, uint4 b2)
{
  return sub96Borrow(a2, b2, sub128Borrow(a1, b1, sub128(a0, b0)));
}

uint32_t sub384(uint4 *a0, uint4 *a1, uint4 *a2, uint4 b0, uint4 b1, uint4 b2)
{
  return sub128Borrow(a2, b2, sub128Borrow(a1, b1, sub128(a0, b0)));
}

uint32_t sub448(uint4 *a0, uint4 *a1, uint4 *a2, uint2 *a3, uint4 b0, uint4 b1, uint4 b2, uint2 b3)
{
  return sub64Borrow(a3, b3, sub128Borrow(a2, b2, sub128Borrow(a1, b1, sub128(a0, b0))));
}

uint32_t invert_limb(uint32_t limb)
{
  uint32_t inv = binvert_limb_table[(limb/2) & 0x7F];
  inv = 2*inv - inv*inv*limb;
  inv = 2*inv - inv*inv*limb;
  return -inv;
}

void lshiftByLimb2(uint4 *limbs1,
                   uint4 *limbs2)
{
  (*limbs2).yzw = (*limbs2).xyz; (*limbs2).x = (*limbs1).w;
  (*limbs1).yzw = (*limbs1).xyz; (*limbs1).x = 0;
}

void rshiftByLimb2(uint4 *limbs1,
                   uint4 *limbs2)
{
  (*limbs1).xyz = (*limbs1).yzw; (*limbs1).w = (*limbs2).x;
  (*limbs2).xyz = (*limbs2).yzw; (*limbs2).w = 0;
}

void lshiftByLimb3(uint4 *limbs1,
                   uint4 *limbs2,
                   uint4 *limbs3)
{
  (*limbs3).yzw = (*limbs3).xyz; (*limbs3).x = (*limbs2).w;
  (*limbs2).yzw = (*limbs2).xyz; (*limbs2).x = (*limbs1).w;
  (*limbs1).yzw = (*limbs1).xyz; (*limbs1).x = 0;
}

void rshiftByLimb3(uint4 *limbs1,
                   uint4 *limbs2,
                   uint4 *limbs3)
{
  (*limbs1).xyz = (*limbs1).yzw; (*limbs1).w = (*limbs2).x;
  (*limbs2).xyz = (*limbs2).yzw; (*limbs2).w = (*limbs3).x;
  (*limbs3).xyz = (*limbs3).yzw; (*limbs3).w = 0;
}

void lshiftByLimb4(uint4 *limbs1,
                   uint4 *limbs2,
                   uint4 *limbs3,
                   uint4 *limbs4)
{
  (*limbs4).yzw = (*limbs4).xyz; (*limbs4).x = (*limbs3).w;  
  (*limbs3).yzw = (*limbs3).xyz; (*limbs3).x = (*limbs2).w;
  (*limbs2).yzw = (*limbs2).xyz; (*limbs2).x = (*limbs1).w;
  (*limbs1).yzw = (*limbs1).xyz; (*limbs1).x = 0;
}

void rshiftByLimb4(uint4 *limbs1,
                   uint4 *limbs2,
                   uint4 *limbs3,
                   uint4 *limbs4)
{
  (*limbs1).xyz = (*limbs1).yzw; (*limbs1).w = (*limbs2).x;
  (*limbs2).xyz = (*limbs2).yzw; (*limbs2).w = (*limbs3).x;
  (*limbs3).xyz = (*limbs3).yzw; (*limbs3).w = (*limbs4).x;
  (*limbs4).xyz = (*limbs4).yzw; (*limbs4).w = 0;
}

void lshiftByLimb5(uint4 *limbs1,
                   uint4 *limbs2,
                   uint4 *limbs3,
                   uint4 *limbs4,
                   uint4 *limbs5)
{
  (*limbs5).yzw = (*limbs5).xyz; (*limbs5).x = (*limbs4).w;
  (*limbs4).yzw = (*limbs4).xyz; (*limbs4).x = (*limbs3).w;  
  (*limbs3).yzw = (*limbs3).xyz; (*limbs3).x = (*limbs2).w;
  (*limbs2).yzw = (*limbs2).xyz; (*limbs2).x = (*limbs1).w;
  (*limbs1).yzw = (*limbs1).xyz; (*limbs1).x = 0;
}

void rshiftByLimb5(uint4 *limbs1,
                   uint4 *limbs2,
                   uint4 *limbs3,
                   uint4 *limbs4,
                   uint4 *limbs5)
{
  (*limbs1).xyz = (*limbs1).yzw; (*limbs1).w = (*limbs2).x;
  (*limbs2).xyz = (*limbs2).yzw; (*limbs2).w = (*limbs3).x;
  (*limbs3).xyz = (*limbs3).yzw; (*limbs3).w = (*limbs4).x;
  (*limbs4).xyz = (*limbs4).yzw; (*limbs4).w = (*limbs5).x;
  (*limbs5).xyz = (*limbs5).yzw; (*limbs5).w = 0;
}

void rshiftByLimb6(uint4 *limbs1,
                   uint4 *limbs2,
                   uint4 *limbs3,
                   uint4 *limbs4,
                   uint4 *limbs5,
                   uint4 *limbs6)
{
  (*limbs1).xyz = (*limbs1).yzw; (*limbs1).w = (*limbs2).x;
  (*limbs2).xyz = (*limbs2).yzw; (*limbs2).w = (*limbs3).x;
  (*limbs3).xyz = (*limbs3).yzw; (*limbs3).w = (*limbs4).x;
  (*limbs4).xyz = (*limbs4).yzw; (*limbs4).w = (*limbs5).x;
  (*limbs5).xyz = (*limbs5).yzw; (*limbs5).w = (*limbs6).x;
  (*limbs6).xyz = (*limbs6).yzw; (*limbs6).w = 0;
}

void lshift2(uint4 *limbs1, uint4 *limbs2, unsigned count)
{
  if (!count)
    return;  
  unsigned lowBitsCount = 32 - count;  
  
  {
    uint4 lowBits = {
      (*limbs1).w >> lowBitsCount,
      (*limbs2).x >> lowBitsCount,
      (*limbs2).y >> lowBitsCount,
      (*limbs2).z >> lowBitsCount
    };
    (*limbs2) = ((*limbs2) << count) | lowBits;
  }  
  
  {
    uint4 lowBits = {
      0,
      (*limbs1).x >> lowBitsCount,
      (*limbs1).y >> lowBitsCount,
      (*limbs1).z >> lowBitsCount
    };
    (*limbs1) = ((*limbs1) << count) | lowBits;
  }
}

void rshift2(uint4 *limbs1, uint4 *limbs2, unsigned count)
{
  if (!count)
    return;  
  unsigned lowBitsCount = 32 - count;  
  
  {
    uint4 lowBits = {
      (*limbs1).y << lowBitsCount,
      (*limbs1).z << lowBitsCount,
      (*limbs1).w << lowBitsCount,
      (*limbs2).x << lowBitsCount
    };
    (*limbs1) = ((*limbs1) >> count) | lowBits;
  }  
  
  {
    uint4 lowBits = {
      (*limbs2).y << lowBitsCount,
      (*limbs2).z << lowBitsCount,
      (*limbs2).w << lowBitsCount,
      0
    };
    (*limbs2) = ((*limbs2) >> count) | lowBits;
  }  
}

void lshift3(uint4 *limbs1, uint4 *limbs2, uint4 *limbs3, unsigned count)
{
  if (!count)
    return;  
  unsigned lowBitsCount = 32 - count;
  
  {
    uint4 lowBits = {
      (*limbs2).w >> lowBitsCount,
      (*limbs3).x >> lowBitsCount,
      (*limbs3).y >> lowBitsCount,
      (*limbs3).z >> lowBitsCount
    };
    (*limbs3) = ((*limbs3) << count) | lowBits;
  }    
  
  {
    uint4 lowBits = {
      (*limbs1).w >> lowBitsCount,
      (*limbs2).x >> lowBitsCount,
      (*limbs2).y >> lowBitsCount,
      (*limbs2).z >> lowBitsCount
    };
    (*limbs2) = ((*limbs2) << count) | lowBits;
  }  
  
  {
    uint4 lowBits = {
      0,
      (*limbs1).x >> lowBitsCount,
      (*limbs1).y >> lowBitsCount,
      (*limbs1).z >> lowBitsCount
    };
    (*limbs1) = ((*limbs1) << count) | lowBits;
  }
}

void rshift3(uint4 *limbs1, uint4 *limbs2, uint4 *limbs3, unsigned count)
{
  if (!count)
    return;
  unsigned lowBitsCount = 32 - count;  
  
  {
    uint4 lowBits = {
      (*limbs1).y << lowBitsCount,
      (*limbs1).z << lowBitsCount,
      (*limbs1).w << lowBitsCount,
      (*limbs2).x << lowBitsCount
    };
    (*limbs1) = ((*limbs1) >> count) | lowBits;
  }    
  
  {
    uint4 lowBits = {
      (*limbs2).y << lowBitsCount,
      (*limbs2).z << lowBitsCount,
      (*limbs2).w << lowBitsCount,
      (*limbs3).x << lowBitsCount
    };
    (*limbs2) = ((*limbs2) >> count) | lowBits;
  }  
  
  {
    uint4 lowBits = {
      (*limbs3).y << lowBitsCount,
      (*limbs3).z << lowBitsCount,
      (*limbs3).w << lowBitsCount,
      0
    };
    (*limbs3) = ((*limbs3) >> count) | lowBits;
  }  
}

void lshift4(uint4 *limbs1, uint4 *limbs2, uint4 *limbs3, uint4 *limbs4, unsigned count)
{
  if (!count)
    return;  
  unsigned lowBitsCount = 32 - count;
  
  {
    uint4 lowBits = {
      (*limbs3).w >> lowBitsCount,
      (*limbs4).x >> lowBitsCount,
      (*limbs4).y >> lowBitsCount,
      (*limbs4).z >> lowBitsCount
    };
    (*limbs4) = ((*limbs4) << count) | lowBits;
  }      
  
  {
    uint4 lowBits = {
      (*limbs2).w >> lowBitsCount,
      (*limbs3).x >> lowBitsCount,
      (*limbs3).y >> lowBitsCount,
      (*limbs3).z >> lowBitsCount
    };
    (*limbs3) = ((*limbs3) << count) | lowBits;
  }    
  
  {
    uint4 lowBits = {
      (*limbs1).w >> lowBitsCount,
      (*limbs2).x >> lowBitsCount,
      (*limbs2).y >> lowBitsCount,
      (*limbs2).z >> lowBitsCount
    };
    (*limbs2) = ((*limbs2) << count) | lowBits;
  }  
  
  {
    uint4 lowBits = {
      0,
      (*limbs1).x >> lowBitsCount,
      (*limbs1).y >> lowBitsCount,
      (*limbs1).z >> lowBitsCount
    };
    (*limbs1) = ((*limbs1) << count) | lowBits;
  }
}

void lshift5(uint4 *limbs1, uint4 *limbs2, uint4 *limbs3, uint4 *limbs4, uint4 *limbs5, unsigned count)
{
  if (!count)
    return;  
  unsigned lowBitsCount = 32 - count;
  
  {
    uint4 lowBits = {
      (*limbs4).w >> lowBitsCount,
      (*limbs5).x >> lowBitsCount,
      (*limbs5).y >> lowBitsCount,
      (*limbs5).z >> lowBitsCount
    };
    (*limbs5) = ((*limbs5) << count) | lowBits;
  }      
  
  {
    uint4 lowBits = {
      (*limbs3).w >> lowBitsCount,
      (*limbs4).x >> lowBitsCount,
      (*limbs4).y >> lowBitsCount,
      (*limbs4).z >> lowBitsCount
    };
    (*limbs4) = ((*limbs4) << count) | lowBits;
  }      
  
  {
    uint4 lowBits = {
      (*limbs2).w >> lowBitsCount,
      (*limbs3).x >> lowBitsCount,
      (*limbs3).y >> lowBitsCount,
      (*limbs3).z >> lowBitsCount
    };
    (*limbs3) = ((*limbs3) << count) | lowBits;
  }    
  
  {
    uint4 lowBits = {
      (*limbs1).w >> lowBitsCount,
      (*limbs2).x >> lowBitsCount,
      (*limbs2).y >> lowBitsCount,
      (*limbs2).z >> lowBitsCount
    };
    (*limbs2) = ((*limbs2) << count) | lowBits;
  }  
  
  {
    uint4 lowBits = {
      0,
      (*limbs1).x >> lowBitsCount,
      (*limbs1).y >> lowBitsCount,
      (*limbs1).z >> lowBitsCount
    };
    (*limbs1) = ((*limbs1) << count) | lowBits;
  }
}


void rshift4(uint4 *limbs1, uint4 *limbs2, uint4 *limbs3, uint4 *limbs4, unsigned count)
{
  if (!count)
    return;
  unsigned lowBitsCount = 32 - count;  
  
  {
    uint4 lowBits = {
      (*limbs1).y << lowBitsCount,
      (*limbs1).z << lowBitsCount,
      (*limbs1).w << lowBitsCount,
      (*limbs2).x << lowBitsCount
    };
    (*limbs1) = ((*limbs1) >> count) | lowBits;
  }    
  
  {
    uint4 lowBits = {
      (*limbs2).y << lowBitsCount,
      (*limbs2).z << lowBitsCount,
      (*limbs2).w << lowBitsCount,
      (*limbs3).x << lowBitsCount
    };
    (*limbs2) = ((*limbs2) >> count) | lowBits;
  }  
  
  {
    uint4 lowBits = {
      (*limbs3).y << lowBitsCount,
      (*limbs3).z << lowBitsCount,
      (*limbs3).w << lowBitsCount,
      (*limbs4).x << lowBitsCount,
    };
    (*limbs3) = ((*limbs3) >> count) | lowBits;
  }
  
  {
    uint4 lowBits = {
      (*limbs4).y << lowBitsCount,
      (*limbs4).z << lowBitsCount,
      (*limbs4).w << lowBitsCount,
      0
    };
    (*limbs4) = ((*limbs4) >> count) | lowBits;
  }    
}


void rshift5(uint4 *limbs1, uint4 *limbs2, uint4 *limbs3, uint4 *limbs4, uint4 *limbs5, unsigned count)
{
  if (!count)
    return;
  unsigned lowBitsCount = 32 - count;  
  
  {
    uint4 lowBits = {
      (*limbs1).y << lowBitsCount,
      (*limbs1).z << lowBitsCount,
      (*limbs1).w << lowBitsCount,
      (*limbs2).x << lowBitsCount
    };
    (*limbs1) = ((*limbs1) >> count) | lowBits;
  }    
  
  {
    uint4 lowBits = {
      (*limbs2).y << lowBitsCount,
      (*limbs2).z << lowBitsCount,
      (*limbs2).w << lowBitsCount,
      (*limbs3).x << lowBitsCount
    };
    (*limbs2) = ((*limbs2) >> count) | lowBits;
  }  
  
  {
    uint4 lowBits = {
      (*limbs3).y << lowBitsCount,
      (*limbs3).z << lowBitsCount,
      (*limbs3).w << lowBitsCount,
      (*limbs4).x << lowBitsCount,
    };
    (*limbs3) = ((*limbs3) >> count) | lowBits;
  }
  
  {
    uint4 lowBits = {
      (*limbs4).y << lowBitsCount,
      (*limbs4).z << lowBitsCount,
      (*limbs4).w << lowBitsCount,
      (*limbs5).x << lowBitsCount,
    };
    (*limbs4) = ((*limbs4) >> count) | lowBits;
  }
  
  {
    uint4 lowBits = {
      (*limbs5).y << lowBitsCount,
      (*limbs5).z << lowBitsCount,
      (*limbs5).w << lowBitsCount,
      0
    };
    (*limbs5) = ((*limbs5) >> count) | lowBits;
  }    
}

// // Calculate (384bit){b2, b1, b0} -= (384bit)({a2, a1, a0} >> 32) * (32bit)M, returns highest product limb
void subMul1_v3(uint4 *b0, uint4 *b1, uint4 *b2, uint4 *b3,
                uint4 a0, uint4 a1, uint4 a2,
                uint32_t M)
{
  #define bringBorrow(Data, Borrow, NextBorrow) NextBorrow += (Data < Borrow); Data -= Borrow;
  
  uint4 Mv4 = {M, M, M, M};
  uint32_t clow;
  uint4 c1 = {0, 0, 0, 0};
  uint4 c2 = {0, 0, 0, 0};
  uint4 c3 = {0, 0, 0, 0};
  
  {
    uint4 a0M = a0*Mv4;
    uint4 a0Mhi = mul_hi(a0, Mv4);
    
    clow = (*b0).w < a0M.x;
    (*b0).w -= a0M.x;
    
    c1.xyz -= convert_uint3((*b1).xyz < a0M.yzw);
    (*b1).xyz -= a0M.yzw;
    
    c1 -= convert_uint4((*b1) < a0Mhi);
    (*b1) -= a0Mhi;
  }
  
  {
    uint4 a1M = a1*Mv4;
    uint4 a1Mhi = mul_hi(a1, Mv4);
    
    c1.w += ((*b1).w < a1M.x);
    (*b1).w -= a1M.x;
    
    c2.xyz -= convert_uint3((*b2).xyz < a1M.yzw);
    (*b2).xyz -= a1M.yzw;
    
    c2 -= convert_uint4((*b2) < a1Mhi);
    (*b2) -= a1Mhi;
  }
  
  {
    uint4 a2M = a2*Mv4;
    uint4 a2Mhi = mul_hi(a2, Mv4);
    
    c2.w += ((*b2).w < a2M.x);
    (*b2).w -= a2M.x;
    
    c3.xyz -= convert_uint3((*b3).xyz < a2M.yzw);
    (*b3).xyz -= a2M.yzw;
    c3 -= convert_uint4((*b3) < a2Mhi);
    (*b3) -= a2Mhi;
  }
  
  bringBorrow((*b1).x, clow, c1.x);
  bringBorrow((*b1).y, c1.x, c1.y);
  bringBorrow((*b1).z, c1.y, c1.z);
  bringBorrow((*b1).w, c1.z, c1.w);
  bringBorrow((*b2).x, c1.w, c2.x);
  bringBorrow((*b2).y, c2.x, c2.y);
  bringBorrow((*b2).z, c2.y, c2.z);
  bringBorrow((*b2).w, c2.z, c2.w);
  bringBorrow((*b3).x, c2.w, c3.x);
  bringBorrow((*b3).y, c3.x, c3.y);
  bringBorrow((*b3).z, c3.y, c3.z);
  bringBorrow((*b3).w, c3.z, c3.w);
  #undef bringBorrow
}

uint2 modulo512to384(uint4 dividendLimbs0,
                     uint4 dividendLimbs1,
                     uint4 dividendLimbs2,
                     uint4 dividendLimbs3,
                     uint4 divisorLimbs0,
                     uint4 divisorLimbs1,
                     uint4 divisorLimbs2,
                     uint4 *moduloLimbs0,
                     uint4 *moduloLimbs1,
                     uint4 *moduloLimbs2)
{
  // Detect dividend and divisor limbs count (remove trailing zero limbs)
  unsigned dividendLimbs = 16;
  unsigned divisorLimbs = 12;
  
  while (divisorLimbs && !divisorLimbs2.w) {
    lshiftByLimb3(&divisorLimbs0, &divisorLimbs1, &divisorLimbs2);
    divisorLimbs--;
  }  
  
  // Normalize dividend and divisor (high bit of divisor must be set to 1)
  unsigned normalizeShiftCount = 0;  
  uint32_t bit = 0x80000000;
  while (!(divisorLimbs2.w & bit)) {
    normalizeShiftCount++;
    bit >>= 1;  
  }    
  
  lshift4(&dividendLimbs0, &dividendLimbs1, &dividendLimbs2, &dividendLimbs3, normalizeShiftCount);
  lshift3(&divisorLimbs0, &divisorLimbs1, &divisorLimbs2, normalizeShiftCount);    
  
  
  while (dividendLimbs && !dividendLimbs3.w) {
    lshiftByLimb4(&dividendLimbs0, &dividendLimbs1, &dividendLimbs2, &dividendLimbs3);
    dividendLimbs--;
  }  
  
  for (unsigned i = 0; i < (dividendLimbs - divisorLimbs); i++) {
    uint32_t i32quotient;
    if (dividendLimbs3.w == divisorLimbs2.w) {
      i32quotient = 0xFFFFFFFF;
    } else {
      uint64_t i64dividend = (((uint64_t)dividendLimbs3.w) << 32) | dividendLimbs3.z;
      i32quotient = i64dividend / divisorLimbs2.w;
    }
    
    subMul1_v3(&dividendLimbs0, &dividendLimbs1, &dividendLimbs2, &dividendLimbs3,
               divisorLimbs0, divisorLimbs1, divisorLimbs2,
               i32quotient);    
    uint32_t borrow = dividendLimbs3.w;    
    lshiftByLimb4(&dividendLimbs0, &dividendLimbs1, &dividendLimbs2, &dividendLimbs3);
    if (borrow) {
      add384(&dividendLimbs1, &dividendLimbs2, &dividendLimbs3, divisorLimbs0, divisorLimbs1, divisorLimbs2);
      if (dividendLimbs3.w > divisorLimbs2.w)
        add384(&dividendLimbs1, &dividendLimbs2, &dividendLimbs3, divisorLimbs0, divisorLimbs1, divisorLimbs2);
    }
  }
  
  rshift4(&dividendLimbs0, &dividendLimbs1, &dividendLimbs2, &dividendLimbs3, normalizeShiftCount);
  for (unsigned i = 0; i < (12-divisorLimbs); i++)
    rshiftByLimb4(&dividendLimbs0, &dividendLimbs1, &dividendLimbs2, &dividendLimbs3);
  
  *moduloLimbs0 = dividendLimbs1;
  *moduloLimbs1 = dividendLimbs2;
  *moduloLimbs2 = dividendLimbs3;
  return (uint2){divisorLimbs, 32-normalizeShiftCount};
}


uint2 divq640to384(uint4 dividendLimbs0,
                   uint4 dividendLimbs1,
                   uint4 dividendLimbs2,
                   uint4 dividendLimbs3,
                   uint4 dividendLimbs4,
                   uint4 divisorLimbs0,
                   uint4 divisorLimbs1,
                   uint4 divisorLimbs2,
                   uint4 *q0,
                   uint4 *q1)
{
  // Detect dividend and divisor limbs count (remove trailing zero limbs)
  unsigned dividendLimbs = 20;
  unsigned divisorLimbs = 12;
  
  while (divisorLimbs && !divisorLimbs2.w) {
    lshiftByLimb3(&divisorLimbs0, &divisorLimbs1, &divisorLimbs2);
    divisorLimbs--;
  }  
  
  // Normalize dividend and divisor (high bit of divisor must be set to 1)
  unsigned normalizeShiftCount = 0;  
  uint32_t bit = 0x80000000;
  while (!(divisorLimbs2.w & bit)) {
    normalizeShiftCount++;
    bit >>= 1;  
  }    
  
  lshift5(&dividendLimbs0, &dividendLimbs1, &dividendLimbs2, &dividendLimbs3, &dividendLimbs4, normalizeShiftCount);
  lshift3(&divisorLimbs0, &divisorLimbs1, &divisorLimbs2, normalizeShiftCount);    
  
  
  while (dividendLimbs && !dividendLimbs4.w) {
    lshiftByLimb5(&dividendLimbs0, &dividendLimbs1, &dividendLimbs2, &dividendLimbs3, &dividendLimbs4);
    dividendLimbs--;
  }  
  
  for (unsigned i = 0; i < (dividendLimbs - divisorLimbs); i++) {
    uint32_t i32quotient;
    if (dividendLimbs4.w == divisorLimbs2.w) {
      i32quotient = 0xFFFFFFFF;
    } else {
      uint64_t i64dividend = (((uint64_t)dividendLimbs4.w) << 32) | dividendLimbs4.z;
      i32quotient = i64dividend / divisorLimbs2.w;
    }
    
    subMul1_v3(&dividendLimbs1, &dividendLimbs2, &dividendLimbs3, &dividendLimbs4,
               divisorLimbs0, divisorLimbs1, divisorLimbs2,
               i32quotient);    
    uint32_t borrow = dividendLimbs4.w;
    lshiftByLimb5(&dividendLimbs0, &dividendLimbs1, &dividendLimbs2, &dividendLimbs3, &dividendLimbs4);
    if (borrow) {
      i32quotient--;
      add384(&dividendLimbs2, &dividendLimbs3, &dividendLimbs4, divisorLimbs0, divisorLimbs1, divisorLimbs2);
      if (dividendLimbs4.w > divisorLimbs2.w) {
        i32quotient--;        
        add384(&dividendLimbs2, &dividendLimbs3, &dividendLimbs4, divisorLimbs0, divisorLimbs1, divisorLimbs2);
      }
    }
    
    lshiftByLimb2(q0, q1);
    (*q0).x = i32quotient;
}

return (uint2){divisorLimbs, 32-normalizeShiftCount};
}


void mul352round_v3(uint4 op1l0, uint4 op1l1, uint4 op1l2, uint32_t m1, uint32_t m2,
                    uint64_t *R0, uint64_t *R1, uint64_t *R2, uint64_t *R3,
                    uint64_t *R4, uint64_t *R5, uint64_t *R6, uint64_t *R7,
                    uint64_t *R8, uint64_t *R9, uint64_t *R10)
{
  uint4 m1l0 = mul_hi(op1l0, m1);
  uint4 m1l1 = mul_hi(op1l1, m1);
  uint4 m1l2 = {
    mul_hi(op1l2.x, m1),
       mul_hi(op1l2.y, m1),
       mul_hi(op1l2.z, m1),
       0
  };
  uint4 m2l0 = op1l0 * m2;
  uint4 m2l1 = op1l1 * m2;
  uint4 m2l2 = {
    op1l2.x * m2,
    op1l2.y * m2,
    op1l2.z * m2,
    0
  };
  
  union {
    uint2 v32;
    ulong v64;
  } Int;
  *R0 += m1l0.x; *R0 += m2l0.x;
  *R1 += m1l0.y; *R1 += m2l0.y; Int.v64 = *R0; *R1 += Int.v32.y;
  *R2 += m1l0.z; *R2 += m2l0.z;
  *R3 += m1l0.w; *R3 += m2l0.w;
  *R4 += m1l1.x; *R4 += m2l1.x;
  *R5 += m1l1.y; *R5 += m2l1.y;
  *R6 += m1l1.z; *R6 += m2l1.z;
  *R7 += m1l1.w; *R7 += m2l1.w;  
  *R8 += m1l2.x; *R8 += m2l2.x;
  *R9 += m1l2.y; *R9 += m2l2.y;
  *R10 += m1l2.z; *R10 += m2l2.z;
}

void mul352schoolBook_v3(uint4 op1l0, uint4 op1l1, uint4 op1l2, // low --> hi
                         uint4 op2l0, uint4 op2l1, uint4 op2l2, // low --> hi
                         uint4 *rl0, uint4 *rl1, uint4 *rl2, uint4 *rl3, uint4 *rl4, uint4 *rl5)
{ 
  #define b1 op2l2.z
  #define b2 op2l2.y
  #define b3 op2l2.x
  #define b4 op2l1.w
  #define b5 op2l1.z
  #define b6 op2l1.y
  #define b7 op2l1.x
  #define b8 op2l0.w
  #define b9 op2l0.z
  #define b10 op2l0.y
  #define b11 op2l0.x  
  ulong R0x = op1l0.x * b11;
  ulong R0y = op1l0.y * b11;
  ulong R0z = op1l0.z * b11;
  ulong R0w = op1l0.w * b11;
  ulong R1x = op1l1.x * b11;
  ulong R1y = op1l1.y * b11;
  ulong R1z = op1l1.z * b11;
  ulong R1w = op1l1.w * b11;  
  ulong R2x = op1l2.x * b11;
  ulong R2y = op1l2.y * b11;
  ulong R2z = op1l2.z * b11;
  ulong R2w = mul_hi(op1l0.x, b1);
  ulong R3x = mul_hi(op1l0.y, b1);
  ulong R3y = mul_hi(op1l0.z, b1);
  ulong R3z = mul_hi(op1l0.w, b1);
  ulong R3w = mul_hi(op1l1.x, b1);
  ulong R4x = mul_hi(op1l1.y, b1);
  ulong R4y = mul_hi(op1l1.z, b1);
  ulong R4z = mul_hi(op1l1.w, b1);
  ulong R4w = mul_hi(op1l2.x, b1);
  ulong R5x = mul_hi(op1l2.y, b1);
  ulong R5y = mul_hi(op1l2.z, b1);
  
  mul352round_v3(op1l0, op1l1, op1l2, b11, b10, &R0y, &R0z, &R0w, &R1x, &R1y, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w);
  mul352round_v3(op1l0, op1l1, op1l2, b10, b9, &R0z, &R0w, &R1x, &R1y, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x);
  mul352round_v3(op1l0, op1l1, op1l2, b9,  b8, &R0w, &R1x, &R1y, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x, &R3y);
  mul352round_v3(op1l0, op1l1, op1l2,  b8,  b7, &R1x, &R1y, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x, &R3y, &R3z);
  mul352round_v3(op1l0, op1l1, op1l2,  b7,  b6, &R1y, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x, &R3y, &R3z, &R3w);
  mul352round_v3(op1l0, op1l1, op1l2,  b6,  b5, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x, &R3y, &R3z, &R3w, &R4x);
  mul352round_v3(op1l0, op1l1, op1l2,  b5,  b4, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x, &R3y, &R3z, &R3w, &R4x, &R4y);
  mul352round_v3(op1l0, op1l1, op1l2,  b4,  b3, &R2x, &R2y, &R2z, &R2w, &R3x, &R3y, &R3z, &R3w, &R4x, &R4y, &R4z);
  mul352round_v3(op1l0, op1l1, op1l2,  b3,  b2, &R2y, &R2z, &R2w, &R3x, &R3y, &R3z, &R3w, &R4x, &R4y, &R4z, &R4w);
  mul352round_v3(op1l0, op1l1, op1l2,  b2,  b1, &R2z, &R2w, &R3x, &R3y, &R3z, &R3w, &R4x, &R4y, &R4z, &R4w, &R5x);
  
  union {
    uint2 v32;
    ulong v64;
  } Int;
  
  Int.v64 = R2w; R3x += Int.v32.y;  
  Int.v64 = R3x; R3y += Int.v32.y;
  Int.v64 = R3y; R3z += Int.v32.y;
  Int.v64 = R3z; R3w += Int.v32.y;
  Int.v64 = R3w; R4x += Int.v32.y;
  Int.v64 = R4x; R4y += Int.v32.y;
  Int.v64 = R4y; R4z += Int.v32.y;
  Int.v64 = R4z; R4w += Int.v32.y;
  Int.v64 = R4w; R5x += Int.v32.y;
  Int.v64 = R5x; R5y += Int.v32.y;
  Int.v64 = R5y;
  
  *rl0 = (uint4){R0x, R0y, R0z, R0w};
  *rl1 = (uint4){R1x, R1y, R1z, R1w};
  *rl2 = (uint4){R2x, R2y, R2z, R2w};
  *rl3 = (uint4){R3x, R3y, R3z, R3w};
  *rl4 = (uint4){R4x, R4y, R4z, R4w};
  *rl5 = (uint4){R5x, R5y, 0, 0};
  
  #undef b1
  #undef b2
  #undef b3
  #undef b4
  #undef b5
  #undef b6
  #undef b7
  #undef b8
  #undef b9
  #undef b10
  #undef b11
  #undef b12
}

void redc352_round_v3(uint4 op1l0, uint4 op1l1, uint4 op1l2, uint32_t m1, uint32_t *m2, uint32_t invm,
                      uint64_t *R0, uint64_t *R1, uint64_t *R2, uint64_t *R3,
                      uint64_t *R4, uint64_t *R5, uint64_t *R6, uint64_t *R7,
                      uint64_t *R8, uint64_t *R9, uint64_t *R10)
{
  uint4 m1l0 = mul_hi(op1l0, m1);
  uint4 m1l1 = mul_hi(op1l1, m1);
  uint4 m1l2 = {
    mul_hi(op1l2.x, m1),
       mul_hi(op1l2.y, m1),
       mul_hi(op1l2.z, m1),
       0
  };
  
  *m2 = invm * ((uint32_t)*R0 + m1l0.x);
  uint4 m2l0 = op1l0 * (*m2);
  uint4 m2l1 = op1l1 * (*m2);
  uint4 m2l2 = {
    op1l2.x * (*m2),
       op1l2.y * (*m2),
       op1l2.z * (*m2),    
       0
  };
  
  union {
    uint2 v32;
    ulong v64;
  } Int; 
  
  *R0 += m1l0.x; *R0 += m2l0.x;
  *R1 += m1l0.y; *R1 += m2l0.y; Int.v64 = *R0; *R1 += Int.v32.y;
  *R2 += m1l0.z; *R2 += m2l0.z;
  *R3 += m1l0.w; *R3 += m2l0.w;
  *R4 += m1l1.x; *R4 += m2l1.x;
  *R5 += m1l1.y; *R5 += m2l1.y;
  *R6 += m1l1.z; *R6 += m2l1.z;
  *R7 += m1l1.w; *R7 += m2l1.w;  
  *R8 += m1l2.x; *R8 += m2l2.x;
  *R9 += m1l2.y; *R9 += m2l2.y;
  *R10 += m1l2.z; *R10 += m2l2.z;
}

void redc1_352_v3(uint4 limbs0, uint4 limbs1, uint4 limbs2, uint4 limbs3, uint4 limbs4, uint4 limbs5,
                  uint4 moduloLimb0, uint4 moduloLimb1, uint4 moduloLimb2,
                  uint32_t invm,
                  uint4 *ResultLimb0, uint4 *ResultLimb1, uint4 *ResultLimb2)
{
  ulong R0x = limbs0.x;
  ulong R0y = limbs0.y;
  ulong R0z = limbs0.z;
  ulong R0w = limbs0.w;
  ulong R1x = limbs1.x;
  ulong R1y = limbs1.y;
  ulong R1z = limbs1.z;
  ulong R1w = limbs1.w;
  ulong R2x = limbs2.x;
  ulong R2y = limbs2.y;
  ulong R2z = limbs2.z;
  ulong R2w = limbs2.w;
  ulong R3x = limbs3.x;
  ulong R3y = limbs3.y;
  ulong R3z = limbs3.z;
  ulong R3w = limbs3.w;
  ulong R4x = limbs4.x;
  ulong R4y = limbs4.y;
  ulong R4z = limbs4.z;
  ulong R4w = limbs4.w;
  ulong R5x = limbs5.x;
  ulong R5y = limbs5.y;
  
  uint32_t i0, i1, i2, i3, i4, i5, i6, i7, i8, i9, i10/*, i11*/;
  
  union {
    uint2 v32;
    ulong v64;
  } Int;   
  
  i0 = limbs0.x * invm;
  {
    uint4 M1l0 = moduloLimb0 * i0;
    uint4 M1l1 = moduloLimb1 * i0;
    uint4 M1l2 = {
      moduloLimb2.x * i0,
       moduloLimb2.y * i0,
       moduloLimb2.z * i0,
       0
    };
    R0x += M1l0.x;
    R0y += M1l0.y; Int.v64 = R0x; R0y += Int.v32.y;
    R0z += M1l0.z;
    R0w += M1l0.w;
    R1x += M1l1.x;
    R1y += M1l1.y;
    R1z += M1l1.z;
    R1w += M1l1.w;
    R2x += M1l2.x;
    R2y += M1l2.y;
    R2z += M1l2.z;
  }
  
  redc352_round_v3(moduloLimb0, moduloLimb1, moduloLimb2, i0, &i1, invm, &R0y, &R0z, &R0w, &R1x, &R1y, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w);
  redc352_round_v3(moduloLimb0, moduloLimb1, moduloLimb2, i1, &i2, invm, &R0z, &R0w, &R1x, &R1y, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x);
  redc352_round_v3(moduloLimb0, moduloLimb1, moduloLimb2, i2, &i3, invm, &R0w, &R1x, &R1y, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x, &R3y);
  redc352_round_v3(moduloLimb0, moduloLimb1, moduloLimb2, i3, &i4, invm, &R1x, &R1y, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x, &R3y, &R3z);
  redc352_round_v3(moduloLimb0, moduloLimb1, moduloLimb2, i4, &i5, invm, &R1y, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x, &R3y, &R3z, &R3w);
  redc352_round_v3(moduloLimb0, moduloLimb1, moduloLimb2, i5, &i6, invm, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x, &R3y, &R3z, &R3w, &R4x);
  redc352_round_v3(moduloLimb0, moduloLimb1, moduloLimb2, i6, &i7, invm, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x, &R3y, &R3z, &R3w, &R4x, &R4y);
  redc352_round_v3(moduloLimb0, moduloLimb1, moduloLimb2, i7, &i8, invm, &R2x, &R2y, &R2z, &R2w, &R3x, &R3y, &R3z, &R3w, &R4x, &R4y, &R4z);
  redc352_round_v3(moduloLimb0, moduloLimb1, moduloLimb2, i8, &i9, invm, &R2y, &R2z, &R2w, &R3x, &R3y, &R3z, &R3w, &R4x, &R4y, &R4z, &R4w);
  redc352_round_v3(moduloLimb0, moduloLimb1, moduloLimb2, i9, &i10, invm, &R2z, &R2w, &R3x, &R3y, &R3z, &R3w, &R4x, &R4y, &R4z, &R4w, &R5x);
  
  {
    uint4 M1l0 = mul_hi(moduloLimb0, i10);
    uint4 M1l1 = mul_hi(moduloLimb1, i10);    
    uint4 M1l2 = {
      mul_hi(moduloLimb2.x, i10),
       mul_hi(moduloLimb2.y, i10),
       mul_hi(moduloLimb2.z, i10),
       0
    };
    R2w += M1l0.x;
    R3x += M1l0.y; Int.v64 = R2w; R3x += Int.v32.y;
    R3y += M1l0.z; Int.v64 = R3x; R3y += Int.v32.y;
    R3z += M1l0.w; Int.v64 = R3y; R3z += Int.v32.y;
    R3w += M1l1.x; Int.v64 = R3z; R3w += Int.v32.y;
    R4x += M1l1.y; Int.v64 = R3w; R4x += Int.v32.y;
    R4y += M1l1.z; Int.v64 = R4x; R4y += Int.v32.y;
    R4z += M1l1.w; Int.v64 = R4y; R4z += Int.v32.y;
    R4w += M1l2.x; Int.v64 = R4z; R4w += Int.v32.y;
    R5x += M1l2.y; Int.v64 = R4w; R5x += Int.v32.y;
    R5y += M1l2.z; Int.v64 = R5x; R5y += Int.v32.y;
  }
  
  *ResultLimb0 = (uint4){R2w, R3x, R3y, R3z};
  *ResultLimb1 = (uint4){R3w, R4x, R4y, R4z};  
  *ResultLimb2 = (uint4){R4w, R5x, R5y, 0};    
  
  
  {
    Int.v64 = R5y;
    uint4 l0 = Int.v32.y ? moduloLimb0 : (uint4){0, 0, 0, 0};
    uint4 l1 = Int.v32.y ? moduloLimb1 : (uint4){0, 0, 0, 0};
    uint4 l2 = Int.v32.y ? moduloLimb2 : (uint4){0, 0, 0, 0};    
    sub352(ResultLimb0, ResultLimb1, ResultLimb2, l0, l1, l2);
  }
}




void mul352to192(uint4 op1l0, uint4 op1l1, uint4 op1l2, // low --> hi
                 uint4 op2l0, uint2 op2l1, // low --> hi
                 uint4 *rl0, uint4 *rl1, uint4 *rl2, uint4 *rl3, uint4 *rl4)
{ 
  #define b1 op2l1.y
  #define b2 op2l1.x
  #define b3 op2l0.w
  #define b4 op2l0.z
  #define b5 op2l0.y
  #define b6 op2l0.x  
  
  ulong R0x = op1l0.x * b6;
  ulong R0y = op1l0.y * b6;
  ulong R0z = op1l0.z * b6;
  ulong R0w = op1l0.w * b6;
  ulong R1x = op1l1.x * b6;
  ulong R1y = op1l1.y * b6;
  ulong R1z = op1l1.z * b6 + mul_hi(op1l0.x, b1);
  ulong R1w = op1l1.w * b6 + mul_hi(op1l0.y, b1);  
  ulong R2x = op1l2.x * b6 + mul_hi(op1l0.z, b1);
  ulong R2y = op1l2.y * b6 + mul_hi(op1l0.w, b1);
  ulong R2z = op1l2.z * b6 + mul_hi(op1l1.x, b1);
  ulong R2w = mul_hi(op1l1.y, b1);
  ulong R3x = mul_hi(op1l1.z, b1);
  ulong R3y = mul_hi(op1l1.w, b1);
  ulong R3z = mul_hi(op1l2.x, b1);
  ulong R3w = mul_hi(op1l2.y, b1);
  ulong R4x = mul_hi(op1l2.z, b1);
  
  mul352round_v3(op1l0, op1l1, op1l2, b6, b5, &R0y, &R0z, &R0w, &R1x, &R1y, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w);
  mul352round_v3(op1l0, op1l1, op1l2, b5, b4, &R0z, &R0w, &R1x, &R1y, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x);
  mul352round_v3(op1l0, op1l1, op1l2, b4, b3, &R0w, &R1x, &R1y, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x, &R3y);
  mul352round_v3(op1l0, op1l1, op1l2, b3, b2, &R1x, &R1y, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x, &R3y, &R3z);
  mul352round_v3(op1l0, op1l1, op1l2, b2, b1, &R1y, &R1z, &R1w, &R2x, &R2y, &R2z, &R2w, &R3x, &R3y, &R3z, &R3w);
  
  union {
    uint2 v32;
    ulong v64;
  } Int;
  
  Int.v64 = R1z; R1w += Int.v32.y;
  Int.v64 = R1w; R2x += Int.v32.y;
  Int.v64 = R2x; R2y += Int.v32.y;
  Int.v64 = R2y; R2z += Int.v32.y;
  Int.v64 = R2z; R2w += Int.v32.y;
  Int.v64 = R2w; R3x += Int.v32.y;
  Int.v64 = R3x; R3y += Int.v32.y;
  Int.v64 = R3y; R3z += Int.v32.y;
  Int.v64 = R3z; R3w += Int.v32.y;
  Int.v64 = R3w; R4x += Int.v32.y;
  
  *rl0 = (uint4){R0x, R0y, R0z, R0w};
  *rl1 = (uint4){R1x, R1y, R1z, R1w};
  *rl2 = (uint4){R2x, R2y, R2z, R2w};
  *rl3 = (uint4){R3x, R3y, R3z, R3w};
  (*rl4).x = R4x;
  
  #undef b1
  #undef b2
  #undef b3
  #undef b4
  #undef b5
  #undef b6
}


void redcify352(unsigned shiftCount,
                uint4 q0, uint4 q1,
                uint4 limbs0, uint4 limbs1, uint4 limbs2,
                uint4 *r0, uint4 *r1, uint4 *r2)
{
  uint4 mr0, mr1, mr2, mr3, mr4;
  
  for (unsigned  i = 0, ie = (128-shiftCount)/32; i < ie; i++)
    rshiftByLimb2(&q0, &q1);
  rshift2(&q0, &q1, (128-shiftCount) % 32);
  
  mul352to192(limbs0, limbs1, limbs2, q0, (uint2){q1.x, q1.y}, &mr0, &mr1, &mr2, &mr3, &mr4);
  
  // substract 2^(384+shiftCount) - q*R
  *r0 = ~mr0;
  *r1 = ~mr1;
  (*r2).x = ~mr2.x;
  (*r2).y = ~mr2.y;
  (*r2).z = ~mr2.z;
  
  // TODO: bring carry
  (*r0).x++;
}


void montgomeryMul352(uint4 *rl0, uint4 *rl1, uint4 *rl2,
                      uint4 ml0, uint4 ml1, uint4 ml2, 
                      uint4 modl0, uint4 modl1, uint4 modl2,
                      uint32_t inverted)
{
  uint4 m0, m1, m2, m3, m4, m5;
  mul352schoolBook_v3(*rl0, *rl1, *rl2, ml0, ml1, ml2, &m0, &m1, &m2, &m3, &m4, &m5);
  redc1_352_v3(m0, m1, m2, m3, m4, m5, modl0, modl1, modl2, inverted, rl0, rl1, rl2);
}

void FermatTest352(uint4 *restrict limbs,
                   uint4 *resultLimbs0, uint4 *resultLimbs1, uint4 *resultLimbs2)
{
  uint2 bitSize;
  uint4 redcl0, redcl1, redcl2;
  uint32_t inverted = invert_limb(limbs[0].x);  
  
  uint4 q0 = 0, q1 = 0;  
  {
    uint4 dl4 = {0, 0, 0, 0};    
    uint4 dl3 = {0, 0, 0, 1};
    uint4 dl2 = {0, 0, 0, 0};
    uint4 dl1 = {0, 0, 0, 0};
    uint4 dl0 = {0, 0, 0, 0};
    divq640to384(dl0, dl1, dl2, dl3, dl4, limbs[0], limbs[1], limbs[2], &q0, &q1);
  }
  
  
  // Retrieve of "2" in Montgomery representation
  {
    uint4 dl3 = {0, 0, 0, 0};
    uint4 dl2 = {0, 0, 0, 2};
    uint4 dl1 = {0, 0, 0, 0};
    uint4 dl0 = {0, 0, 0, 0};    
    bitSize = modulo512to384(dl0, dl1, dl2, dl3, limbs[0], limbs[1], limbs[2], &redcl0, &redcl1, &redcl2);
    --bitSize.y;
    if (bitSize.y == 0) {
      --bitSize.x;
      bitSize.y = 32;
    }
  }
  
  uint32_t *data = (uint32_t*)limbs;
  int remaining = (bitSize.x-1)*32 + bitSize.y;
  
  while (remaining > 0) {
    int bitPos = max(remaining-7, 0);
    int size = min(remaining, 7);
    
    uint64_t v64 = *(uint64_t*)(data+bitPos/32) - (remaining <= 7 ? 1 : 0);
    v64 >>= bitPos % 32;
    uint32_t index = ((uint32_t)v64) & ((1 << size) - 1);
    
    uint4 m0, m1, m2;
    for (unsigned i = 0; i < size; i++)
      montgomeryMul352(&redcl0, &redcl1, &redcl2, redcl0, redcl1, redcl2, limbs[0], limbs[1], limbs[2], inverted);
    redcify352(index, q0, q1, limbs[0], limbs[1], limbs[2], &m0, &m1, &m2);
    montgomeryMul352(&redcl0, &redcl1, &redcl2, m0, m1, m2, limbs[0], limbs[1], limbs[2], inverted);
    
    remaining -= 7;
  }
  
  redc1_352_v3(redcl0, redcl1, redcl2, 0, 0, 0, limbs[0], limbs[1], limbs[2], inverted, resultLimbs0, resultLimbs1, resultLimbs2); 
}

bool fermat(const uint* p) {
  uint4 modpowl0, modpowl1, modpowl2;
  FermatTest352((const uint4*)p, &modpowl0, &modpowl1, &modpowl2);
  --modpowl0.x;
  modpowl0 |= modpowl1;
  modpowl0 |= modpowl2;
  modpowl0.xy |= modpowl0.zw;
  modpowl0.x |= modpowl0.y;
  return modpowl0.x == 0;
}



uint int_invert(uint a, uint nPrime)
{
    // Extended Euclidean algorithm to calculate the inverse of a in finite field defined by nPrime
    int rem0 = nPrime, rem1 = a % nPrime, rem2;
    int aux0 = 0, aux1 = 1, aux2;
    int quotient, inverse;
    
    while (1)
    {
        if (rem1 <= 1)
        {
            inverse = aux1;
            break;
        }
        
        rem2 = rem0 % rem1;
        quotient = rem0 / rem1;
        aux2 = -quotient * aux1 + aux0;
        
        if (rem2 <= 1)
        {
            inverse = aux2;
            break;
        }
        
        rem0 = rem1 % rem2;
        quotient = rem1 / rem2;
        aux0 = -quotient * aux2 + aux1;
        
        if (rem0 <= 1)
        {
            inverse = aux0;
            break;
        }
        
        rem1 = rem2 % rem0;
        quotient = rem2 / rem0;
        aux1 = -quotient * aux0 + aux2;
    }
    
    return (inverse + nPrime) % nPrime;
}



__kernel void setup_sieve(	__global uint* offset1,
							__global uint* offset2,
							__global const uint* vPrimes,
							__constant uint* hash,
							uint hashid )
{
	
	const uint id = get_global_id(0);
	const uint nPrime = vPrimes[id];
	
	uint tmp[N];
	for(int i = 0; i < N; ++i)
		tmp[i] = hash[hashid*N + i];
	
	const uint nFixedFactorMod = tdiv_ui(tmp, nPrime);
	if(nFixedFactorMod == 0){
		for(uint line = 0; line < WIDTH; ++line){
			offset1[PCOUNT*line + id] = 1u << 31;
			offset2[PCOUNT*line + id] = 1u << 31;
		}
		return;
		
	}
	
	uint nFixedInverse = int_invert(nFixedFactorMod, nPrime);
  for(uint layer = 0; layer < WIDTH; ++layer) {
    offset1[PCOUNT*layer + id] = nFixedInverse;
    offset2[PCOUNT*layer + id] = nPrime - nFixedInverse;
    nFixedInverse = (nFixedInverse & 0x1) ?
      (nFixedInverse + nPrime) / 2 : nFixedInverse / 2;
  }    
  
  /*
	const uint nTwoInverse = (nPrime + 1) / 2;
	
	// Check whether 32-bit arithmetic can be used for nFixedInverse
	const bool fUse32BArithmetic = (UINT_MAX / nTwoInverse) >= nPrime;
	
	if(fUse32BArithmetic){
		
		for(uint line = 0; line < WIDTH; ++line){
			
			offset1[PCOUNT*line + id] = nFixedInverse;
			offset2[PCOUNT*line + id] = nPrime - nFixedInverse;
			
			nFixedInverse = nFixedInverse * nTwoInverse % nPrime;
			
		}
		
	}else{
		
		for(uint line = 0; line < WIDTH; ++line){
			
			offset1[PCOUNT*line + id] = nFixedInverse;
			offset2[PCOUNT*line + id] = nPrime - nFixedInverse;
			
			nFixedInverse = (ulong)nFixedInverse * nTwoInverse % nPrime;
			
		}
		
	}*/
	
}



void mul384_1(uint4 l0, uint4 l1, uint4 l2, uint32_t m,
              uint4 *r0, uint4 *r1, uint4 *r2)
{
  *r0 = l0 * m;
  *r1 = l1 * m;
  *r2 = l2 * m;
  
  uint4 h0 = mul_hi(l0, m);
  uint4 h1 = mul_hi(l1, m);
  uint4 h2 = mul_hi(l2, m);
  
  add384(r0, r1, r2,
         (uint4){0, h0.x, h0.y, h0.z},
         (uint4){h0.w, h1.x, h1.y, h1.z},
         (uint4){h1.w, h2.x, h2.y, h2.z});
}

__kernel void setup_fermat( __global uint* fprimes,
              __global const fermat_t* info_all,
              __constant uint* hash )
{
  
  const uint id = get_global_id(0);
  const fermat_t info = info_all[id];
  
  uint h[N];
  uint m[N];
  uint r[2*N];
  
  __constant uint *H = &hash[info.hashid*N];
  uint4 h1 = {H[0], H[1], H[2], H[3]};
  uint4 h2 = {H[4], H[5], H[6], H[7]};
  uint4 h3 = {H[8], H[9], H[10], 0};

  uint line = info.origin;
  if(info.type < 2)
    line += info.chainpos;
  else
    line += info.chainpos/2;

  uint modifier = (info.type == 1 || (info.type == 2 && (info.chainpos & 1))) ? 1 : -1;
  uint4 m1, m2, m3;  
  mul384_1(h1, h2, h3, info.index, &m1, &m2, &m3);
  lshift3(&m1, &m2, &m3, line);
  m1.x += modifier;

  fprimes[id*N + 0] = m1.x;
  fprimes[id*N + 1] = m1.y;
  fprimes[id*N + 2] = m1.z;
  fprimes[id*N + 3] = m1.w;
  fprimes[id*N + 4] = m2.x;
  fprimes[id*N + 5] = m2.y;
  fprimes[id*N + 6] = m2.z;
  fprimes[id*N + 7] = m2.w;
  fprimes[id*N + 8] = m3.x;
  fprimes[id*N + 9] = m3.y;
  fprimes[id*N + 10] = m3.z;
  
}



__attribute__((reqd_work_group_size(64, 1, 1)))
__kernel void fermat_kernel(__global uchar* result,
							__global const uint* fprimes )
{
	
	const uint id = get_global_id(0);
	
	uint p[N+1];
	
	for(int i = 0; i < N; ++i)
		p[i] = fprimes[id*N+i];
  p[N] = 0;
	
	result[id] = fermat(p);
	
}



__kernel void check_fermat(	__global fermat_t* info_out,
							__global uint* count,
							__global fermat_t* info_fin_out,
							__global uint* count_fin,
							__global const uchar* results,
							__global const fermat_t* info_in,
							uint depth )
{
	
	const uint id = get_global_id(0);
	
	if(results[id] == 1){
		
		fermat_t info = info_in[id];
		info.chainpos++;
		
		if(info.chainpos < depth){
			
			const uint i = atomic_inc(count);
			info_out[i] = info;
			
		}else{
			
			const uint i = atomic_inc(count_fin);
			info_fin_out[i] = info;
			
		}
		
	}
	
}






/*
__attribute__((reqd_work_group_size(256, 1, 1)))
__kernel void test_monty(__global const uint* asrc, __global const uint* bsrc, __global const uint* csrc, __global const uint* dsrc, __global uint* esrc) {
	
	const uint id = get_global_id(0);
	
	uint a[N];
	uint b[N];
	uint c[N];
	uint d[N];
	uint e[N];
	
	for(int i = 0; i < N; ++i){
		a[i] = asrc[id*N+i];
		b[i] = bsrc[id*N+i];
		c[i] = csrc[id*N+i];
		d[i] = dsrc[id*N+i];
	}
	
	montymul(e, a, b, c, d);
	
	for(int i = 0; i < N; ++i){
		esrc[id*(N)+i] = e[i];
	}
	
}



__attribute__((reqd_work_group_size(64, 1, 1)))
__kernel void test_fermat(__global const uint* psrc, __global uint* dest) {
	
	const uint id = get_global_id(0);
	
	uint p[N];
	for(int i = 0; i < N; ++i){
		p[i] = psrc[id*N+i];
	}
	
	dest[id] = fermat(p);
	
}


__attribute__((reqd_work_group_size(256, 1, 1)))
__kernel void test_1(__global const uint* asrc, __global const uint* bsrc, __global uint* csrc, __global uint* dsrc) {
	
	const uint id = get_global_id(0);
	
	uint a[N];
	uint b[N];
	uint c[N];
	uint d[N];
	
	for(int i = 0; i < N; ++i){
		a[i] = asrc[id*N+i];
		b[i] = bsrc[id*N+i];
		c[i] = 0;
	}
	
	c[0] = int_invert(a[0], b[0]);
	
	for(int i = 0; i < N; ++i){
		csrc[id*(N)+i] = c[i];
		//dsrc[id*(N)+i] = d[i];
	}
	
}*/


__kernel void test_2(__global const uint* asrc, __global const uint* bsrc, __global uint* csrc) {
	
	const uint id = get_global_id(0);
	
	uint a[N];
	uint b[N];
	uint c[2*N];
	
	for(int i = 0; i < N; ++i){
		a[i] = asrc[id*N+i];
		b[i] = bsrc[id*N+i];
	}
	
	for(int i = 0; i < 2*N; ++i){
		//c[i] = 0;
		c[i] = csrc[id*(2*N)+i];
	}
	
	mad_211(c, a, b);
	
	for(int i = 0; i < 2*N; ++i)
		csrc[id*(2*N)+i] = c[i];
	
}

