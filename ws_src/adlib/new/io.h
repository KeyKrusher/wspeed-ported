#ifndef bqIO_h
#define bqIO_h

#include <sys/io.h>

#include "sizes.h"

static __inline__ void outportb(const WORD port, const BYTE value)
{
   __asm__ volatile ("outb %0,%1" ::"a" ((char) value),"d" ((WORD) port));
}
static __inline__ void outport(const WORD port,
                const WORD value)
{
   __asm__ volatile ("outw %0,%1" ::"a" ((short) value),"d" ((WORD) port));
}
static __inline__ void outportd(const WORD port,
                const unsigned long value)
{
   __asm__ volatile ("outl %0,%1" ::"a" ((long) value),"d" ((WORD) port));
}

static __inline__ BYTE inportb(const WORD port)
{
   BYTE _v;
   __asm__ volatile ("inb %1,%0" :"=a" (_v):"d" ((WORD) port));
   return _v;
}
static __inline__ WORD inport(const WORD port)
{
   WORD _v;
   __asm__ volatile ("inw %1,%0" :"=a" (_v):"d" ((WORD) port));
   return _v;
}
static __inline__ unsigned long inportd(const WORD port)
{
   unsigned long _v;
   __asm__ volatile ("inl %1,%0" :"=a" (_v):"d" ((WORD) port));
   return _v;
}

#define outpb outportb
#define outpw outport
#define outpd outportd
#define inpb inportb
#define inpw inport
#define inpd inportd

#endif
