/*
 * bitops.h
 *
 * Created: 4/12/2014 9:05:25 PM
 *  Author: ioannisg
 */ 


#ifndef BITOPS_H_
#define BITOPS_H_


#define BIT(nr)				(1UL << (nr))
#define BITS(_start, _end)	((BIT(_end) - BIT(_start)) + BIT(_end))
#define BITS_PER_BYTE		8
#define BITS_PER_LONG		(BITS_PER_BYTE)*sizeof(long)

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define BITS_TO_LONGS(nr)       DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))


#endif /* BITOPS_H_ */