#include <malloc.h>
#include <stdint.h>
#include "./nec/necintrf.h"
#include "ws_io.h"
#include "ws_initialIo.h"
#include "ws_audio.h"

uint8_t	*ws_ioRam=NULL;

void ws_io_init(void)
{
	if (ws_ioRam == NULL)
		ws_ioRam = (uint8_t*)malloc(0x100);
}

void ws_io_reset(void)
{
	int i;
	for (i=0;i<0x100;i++)
		ws_ioRam[i]=initialIoValue[i];
	for (i=0;i<0xc9;i++)
		cpu_writeport(i,initialIoValue[i]);
}

void ws_io_done(void)
{
	if (ws_ioRam)
		free(ws_ioRam);
	ws_ioRam = NULL;
}

uint8_t cpu_readport(uint8_t port)
{
	switch (port)
	{
//	case 0x4e:
//	case 0x4f:
//	case 0x50:
//	case 0x51:
	case 0x80:
	case 0x81:
	case 0x82:
	case 0x83:
	case 0x84:
	case 0x85:
	case 0x86:
	case 0x87:
	case 0x88:
	case 0x89:
	case 0x8a:
	case 0x8b:
	case 0x8c:
	case 0x8d:
	case 0x8e:
	case 0x8f:
	case 0x90:
	case 0x91:
	case 0x92:
	case 0x93:
	case 0x94:
				return (ws_audio_port_read(port));
	case 0xaa:	return 0xff;
	case 0xb3:	return 0;
	case 0xb5:	return ws_ioRam[0xb5];
	case 0xbe:	return ws_ioRam[0xbe]|3;
	case 0xba:	return 0;
	case 0xbb:	return 0;
	case 0xc0:	return ((ws_ioRam[0xc0]&0xf)|0x20);

	case 0xc4:	return 0;
	case 0xc5:	return 0;
	case 0xc8:	return ws_ioRam[0xc8]|3;

	case 0xca:	return ws_ioRam[0xca]|0x80;
	case 0xcb:	return 0;
	}
	return (ws_ioRam[port]);
}

void cpu_writeport(uint8_t port,uint8_t value)
{
//	if (ws_ioRam[port] == value)
//		return;

	switch (port)
	{
//	case 0x4e:
//	case 0x4f:
//	case 0x50:
//	case 0x51:
	case 0x52:
	case 0x80:
	case 0x81:
	case 0x82:
	case 0x83:
	case 0x84:
	case 0x85:
	case 0x86:
	case 0x87:
	case 0x88:
	case 0x89:
	case 0x8a:
	case 0x8b:
	case 0x8c:
	case 0x8d:
	case 0x8e:
	case 0x8f:
	case 0x90:
	case 0x91:
	case 0x92:
	case 0x93:
	case 0x94:
				ws_audio_port_write(port,value);
				return;
	case 0x48:	// DMA
				// bit 7 set to start dma transfer
				if(value&0x80)
				{
					int ix;
					uint32_t dma_src  = (((uint32_t)ws_ioRam[0x41])<<8)|(((uint32_t)ws_ioRam[0x40]))|(((uint32_t)ws_ioRam[0x42])<<16);
					uint32_t dma_dest = (((uint32_t)ws_ioRam[0x45])<<8)|(((uint32_t)ws_ioRam[0x44]))|(((uint32_t)ws_ioRam[0x43])<<16);
					uint32_t dma_size = (((uint32_t)ws_ioRam[0x47])<<8)|(((uint32_t)ws_ioRam[0x46]));
					for(ix=0;ix<dma_size;ix++)
						cpu_writemem20(dma_dest++,cpu_readmem20(dma_src++));
					ws_ioRam[0x47]=0;
					ws_ioRam[0x46]=0;
					ws_ioRam[0x41]=(uint8_t)(dma_src>>8);
					ws_ioRam[0x40]=(uint8_t)(dma_src&0xff);
					ws_ioRam[0x45]=(uint8_t)(dma_dest>>8);
					ws_ioRam[0x44]=(uint8_t)(dma_dest&0xff);
					value=0;
				}
				break;
	case 0xb6:
				//�u1=���荞�ݗv���t���O�N���A, 0=�������Ȃ��v�Ƃ����悤�ɉ��߂��Ă݂�
				ws_ioRam[port] &= ~value;
				return;
	case 0xba:	break;
	case 0xbb:	break;
	case 0xc4:	break;
	case 0xc5:	break;
	case 0xca:	break;
	}

	ws_ioRam[port] = value;
}

