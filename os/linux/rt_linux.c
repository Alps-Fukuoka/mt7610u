/*
 *************************************************************************
 * Ralink Tech Inc.
 * 5F., No.36, Taiyuan St., Jhubei City,
 * Hsinchu County 302,
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2010, Ralink Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                       *
 *************************************************************************/


#define RTMP_MODULE_OS
#define RTMP_MODULE_OS_UTIL

#include "rtmp_comm.h"
#include "rtmp_osabl.h"
#include "rt_os_util.h"

/* TODO */
#undef RT_CONFIG_IF_OPMODE_ON_AP
#undef RT_CONFIG_IF_OPMODE_ON_STA

#define RT_CONFIG_IF_OPMODE_ON_AP(__OpMode)
#define RT_CONFIG_IF_OPMODE_ON_STA(__OpMode)

ULONG RTDebugLevel = RT_DEBUG_ERROR;
ULONG RTDebugFunc = 0;

#ifdef VENDOR_FEATURE4_SUPPORT
ULONG OS_NumOfMemAlloc = 0, OS_NumOfMemFree = 0;
#endif /* VENDOR_FEATURE4_SUPPORT */
#ifdef VENDOR_FEATURE2_SUPPORT
ULONG OS_NumOfPktAlloc = 0, OS_NumOfPktFree = 0;
#endif /* VENDOR_FEATURE2_SUPPORT */

/*
 * the lock will not be used in TX/RX
 * path so throughput should not be impacted
 */
BOOLEAN FlgIsUtilInit = FALSE;
OS_NDIS_SPIN_LOCK UtilSemLock;


BOOLEAN RTMP_OS_Alloc_RscOnly(void *pRscSrc, UINT32 RscLen);
BOOLEAN RTMP_OS_Remove_Rsc(LIST_HEADER *pRscList, void *pRscSrc);
/*
========================================================================
Routine Description:
	Initialize something in UTIL module.

Arguments:
	None

Return Value:
	None

Note:
========================================================================
*/
void RtmpUtilInit(void)
{
	if (FlgIsUtilInit == FALSE) {
		OS_NdisAllocateSpinLock(&UtilSemLock);
		FlgIsUtilInit = TRUE;
	}
}

/* timeout -- ms */
static inline void __RTMP_SetPeriodicTimer(
	IN OS_NDIS_MINIPORT_TIMER * pTimer,
	IN unsigned long timeout)
{
	timeout = ((timeout * OS_HZ) / 1000);
	pTimer->expires = jiffies + timeout;
	add_timer(pTimer);
}

/* convert NdisMInitializeTimer --> RTMP_OS_Init_Timer */
static inline void __RTMP_OS_Init_Timer(
	IN void *pReserved,
	IN OS_NDIS_MINIPORT_TIMER * pTimer,
	IN TIMER_FUNCTION function,
	IN void *data)
{
	if (!timer_pending(pTimer)) {
		init_timer(pTimer);
		pTimer->data = (unsigned long)data;
		pTimer->function = function;
	}
}

static inline void __RTMP_OS_Add_Timer(
	IN OS_NDIS_MINIPORT_TIMER * pTimer,
	IN unsigned long timeout)
{
	if (timer_pending(pTimer))
		return;

	timeout = ((timeout * OS_HZ) / 1000);
	pTimer->expires = jiffies + timeout;
	add_timer(pTimer);
}

static inline void __RTMP_OS_Mod_Timer(
	IN OS_NDIS_MINIPORT_TIMER * pTimer,
	IN unsigned long timeout)
{
	timeout = ((timeout * OS_HZ) / 1000);
	mod_timer(pTimer, jiffies + timeout);
}

static inline void __RTMP_OS_Del_Timer(
	IN OS_NDIS_MINIPORT_TIMER * pTimer,
	OUT BOOLEAN *pCancelled)
{
	if (timer_pending(pTimer))
		*pCancelled = del_timer_sync(pTimer);
	else
		*pCancelled = TRUE;
}

static inline void __RTMP_OS_Release_Timer(
	IN OS_NDIS_MINIPORT_TIMER * pTimer)
{
	/* nothing to do */
}


/* Unify all delay routine by using udelay */
void RTMPusecDelay(ULONG usec)
{
	ULONG i;

	for (i = 0; i < (usec / 50); i++)
		udelay(50);

	if (usec % 50)
		udelay(usec % 50);
}


/* Unify all delay routine by using udelay */
void RtmpOsUsDelay(ULONG value)
{
	ULONG i;

	udelay(value);
}

void RtmpOsMsDelay(ULONG msec)
{
	mdelay(msec);
}

void RTMP_GetCurrentSystemTime(LARGE_INTEGER * time)
{
	time->u.LowPart = jiffies;
}

void RTMP_GetCurrentSystemTick(ULONG *pNow)
{
	*pNow = jiffies;
}

ULONG RTMPMsecsToJiffies(UINT32 m)
{

	return msecs_to_jiffies(m);
}

/* pAd MUST allow to be NULL */

NDIS_STATUS os_alloc_mem(
	IN void *pReserved,
	OUT UCHAR **mem,
	IN ULONG size)
{
	*mem = (PUCHAR) kmalloc(size, GFP_ATOMIC);
	if (*mem) {
#ifdef VENDOR_FEATURE4_SUPPORT
		OS_NumOfMemAlloc++;
#endif /* VENDOR_FEATURE4_SUPPORT */

		return NDIS_STATUS_SUCCESS;
	} else
		return NDIS_STATUS_FAILURE;
}

NDIS_STATUS os_alloc_mem_suspend(
	IN void *pReserved,
	OUT UCHAR **mem,
	IN ULONG size)
{
	*mem = (PUCHAR) kmalloc(size, GFP_KERNEL);
	if (*mem) {
#ifdef VENDOR_FEATURE4_SUPPORT
		OS_NumOfMemAlloc++;
#endif /* VENDOR_FEATURE4_SUPPORT */

		return NDIS_STATUS_SUCCESS;
	} else
		return NDIS_STATUS_FAILURE;
}

#if defined(RTMP_RBUS_SUPPORT) || defined(RTMP_FLASH_SUPPORT)
/* The flag "CONFIG_RALINK_FLASH_API" is used for APSoC Linux SDK */
#ifdef CONFIG_RALINK_FLASH_API

int32_t FlashRead(
	uint32_t *dst,
	uint32_t *src,
	uint32_t count);

int32_t FlashWrite(
	uint16_t *source,
	uint16_t *destination,
	uint32_t numBytes);
#else /* CONFIG_RALINK_FLASH_API */

#ifdef RA_MTD_RW_BY_NUM
#if defined(CONFIG_RT2880_FLASH_32M)
#define MTD_NUM_FACTORY 5
#else
#define MTD_NUM_FACTORY 2
#endif
extern int ra_mtd_write(int num, loff_t to, size_t len, const u_char *buf);
extern int ra_mtd_read(int num, loff_t from, size_t len, u_char *buf);
#else
extern int ra_mtd_write_nm(char *name, loff_t to, size_t len, const u_char *buf);
extern int ra_mtd_read_nm(char *name, loff_t from, size_t len, u_char *buf);
#endif

#endif /* CONFIG_RALINK_FLASH_API */

void RtmpFlashRead(
	UCHAR *p,
	ULONG a,
	ULONG b)
{
#ifdef CONFIG_RALINK_FLASH_API
	FlashRead((uint32_t *) p, (uint32_t *) a, (uint32_t) b);
#else
#ifdef RA_MTD_RW_BY_NUM
	ra_mtd_read(MTD_NUM_FACTORY, 0, (size_t) b, p);
#else
	ra_mtd_read_nm("Factory", a&0xFFFF, (size_t) b, p);
#endif
#endif /* CONFIG_RALINK_FLASH_API */
}

void RtmpFlashWrite(
	UCHAR * p,
	ULONG a,
	ULONG b)
{
#ifdef CONFIG_RALINK_FLASH_API
	FlashWrite((uint16_t *) p, (uint16_t *) a, (uint32_t) b);
#else
#ifdef RA_MTD_RW_BY_NUM
	ra_mtd_write(MTD_NUM_FACTORY, 0, (size_t) b, p);
#else
	ra_mtd_write_nm("Factory", a&0xFFFF, (size_t) b, p);
#endif
#endif /* CONFIG_RALINK_FLASH_API */
}
#endif /* defined(RTMP_RBUS_SUPPORT) || defined(RTMP_FLASH_SUPPORT) */


PNDIS_PACKET RtmpOSNetPktAlloc(void *dummy, int size)
{
	struct sk_buff *skb;
	/* Add 2 more bytes for ip header alignment */
	skb = dev_alloc_skb(size + 2);
	if (skb != NULL)
		MEM_DBG_PKT_ALLOC_INC(skb);

	return ((PNDIS_PACKET) skb);
}

PNDIS_PACKET RTMP_AllocateFragPacketBuffer(void *dummy, ULONG len)
{
	struct sk_buff *pkt;

	pkt = dev_alloc_skb(len);

	if (pkt == NULL) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("can't allocate frag rx %ld size packet\n", len));
	}

	if (pkt) {
		MEM_DBG_PKT_ALLOC_INC(pkt);
		RTMP_SET_PACKET_SOURCE(OSPKT_TO_RTPKT(pkt), PKTSRC_NDIS);
	}

	return (PNDIS_PACKET) pkt;
}



/*
	The allocated NDIS PACKET must be freed via RTMPFreeNdisPacket()
*/
NDIS_STATUS RTMPAllocateNdisPacket(
	IN void *pReserved,
	OUT PNDIS_PACKET *ppPacket,
	IN UCHAR *pHeader,
	IN UINT HeaderLen,
	IN UCHAR *pData,
	IN UINT DataLen)
{
	struct sk_buff *pPacket;


	ASSERT(pData);
	ASSERT(DataLen);

	pPacket = dev_alloc_skb(HeaderLen + DataLen + RTMP_PKT_TAIL_PADDING);
	if (pPacket == NULL) {
		*ppPacket = NULL;
#ifdef DEBUG
		printk(KERN_ERR "RTMPAllocateNdisPacket Fail\n\n");
#endif
		return NDIS_STATUS_FAILURE;
	}
	MEM_DBG_PKT_ALLOC_INC(pPacket);

	/* Clone the frame content and update the length of packet */
	if (HeaderLen > 0)
		NdisMoveMemory(pPacket->data, pHeader, HeaderLen);
	if (DataLen > 0)
		NdisMoveMemory(pPacket->data + HeaderLen, pData, DataLen);
	skb_put(pPacket, HeaderLen + DataLen);
/* printk(KERN_ERR "%s : pPacket = %p, len = %d\n", __FUNCTION__, pPacket, GET_OS_PKT_LEN(pPacket));*/

	RTMP_SET_PACKET_SOURCE(pPacket, PKTSRC_NDIS);
	*ppPacket = (PNDIS_PACKET)pPacket;

	return NDIS_STATUS_SUCCESS;
}


/*
  ========================================================================
  Description:
	This routine frees a miniport internally allocated NDIS_PACKET and its
	corresponding NDIS_BUFFER and allocated memory.
  ========================================================================
*/
void RTMPFreeNdisPacket(
	IN void *pReserved,
	IN PNDIS_PACKET pPacket)
{
	dev_kfree_skb_any(RTPKT_TO_OSPKT(pPacket));
	MEM_DBG_PKT_FREE_INC(pPacket);
}


/* IRQL = DISPATCH_LEVEL */
/* NOTE: we do have an assumption here, that Byte0 and Byte1
 * always reasid at the same scatter gather buffer
 */
NDIS_STATUS Sniff2BytesFromNdisBuffer(
	IN PNDIS_BUFFER pFirstBuffer,
	IN UCHAR DesiredOffset,
	OUT PUCHAR pByte0,
	OUT PUCHAR pByte1)
{
	*pByte0 = *(PUCHAR) (pFirstBuffer + DesiredOffset);
	*pByte1 = *(PUCHAR) (pFirstBuffer + DesiredOffset + 1);

	return NDIS_STATUS_SUCCESS;
}


void RTMP_QueryPacketInfo(
	IN PNDIS_PACKET pPacket,
	OUT PACKET_INFO *info,
	OUT UCHAR **pSrcBufVA,
	OUT UINT *pSrcBufLen)
{
	info->BufferCount = 1;
	info->pFirstBuffer = (PNDIS_BUFFER) GET_OS_PKT_DATAPTR(pPacket);
	info->PhysicalBufferCount = 1;
	info->TotalPacketLength = GET_OS_PKT_LEN(pPacket);

	*pSrcBufVA = GET_OS_PKT_DATAPTR(pPacket);
	*pSrcBufLen = GET_OS_PKT_LEN(pPacket);

#ifdef TX_PKT_SG
	if (RTMP_GET_PKT_SG(pPacket)) {
		struct sk_buff *skb = (struct sk_buff *)pPacket;
		int i, nr_frags = skb_shinfo(skb)->nr_frags;

		info->BufferCount =  nr_frags + 1;
		info->PhysicalBufferCount = info->BufferCount;
		info->sg_list[0].data = (void *)GET_OS_PKT_DATAPTR(pPacket);
		info->sg_list[0].len = skb_headlen(skb);
		for (i = 0; i < nr_frags; i++) {
			skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

			info->sg_list[i+1].data = ((void *) page_address(frag->page) +
									frag->page_offset);
			info->sg_list[i+1].len = frag->size;
		}
	}
#endif /* TX_PKT_SG */
}




PNDIS_PACKET DuplicatePacket(
	IN struct net_device *pNetDev,
	IN PNDIS_PACKET pPacket,
	IN UCHAR FromWhichBSSID)
{
	struct sk_buff *skb;
	PNDIS_PACKET pRetPacket = NULL;
	USHORT DataSize;
	UCHAR *pData;

	DataSize = (USHORT) GET_OS_PKT_LEN(pPacket);
	pData = (PUCHAR) GET_OS_PKT_DATAPTR(pPacket);

	skb = skb_clone(RTPKT_TO_OSPKT(pPacket), MEM_ALLOC_FLAG);
	if (skb) {
		MEM_DBG_PKT_ALLOC_INC(skb);
		skb->dev = pNetDev;	/*get_netdev_from_bssid(pAd, FromWhichBSSID); */
		pRetPacket = OSPKT_TO_RTPKT(skb);
	}

	return pRetPacket;

}


PNDIS_PACKET duplicate_pkt(
	IN struct net_device *pNetDev,
	IN PUCHAR pHeader802_3,
	IN UINT HdrLen,
	IN PUCHAR pData,
	IN ULONG DataSize,
	IN UCHAR FromWhichBSSID)
{
	struct sk_buff *skb;
	PNDIS_PACKET pPacket = NULL;

	if ((skb =
	     __dev_alloc_skb(HdrLen + DataSize + 2, MEM_ALLOC_FLAG)) != NULL) {
		MEM_DBG_PKT_ALLOC_INC(skb);

		skb_reserve(skb, 2);
		NdisMoveMemory(GET_OS_PKT_DATATAIL(skb), pHeader802_3, HdrLen);
		skb_put(skb, HdrLen);
		NdisMoveMemory(GET_OS_PKT_DATATAIL(skb), pData, DataSize);
		skb_put(skb, DataSize);
		skb->dev = pNetDev;	/*get_netdev_from_bssid(pAd, FromWhichBSSID); */
		pPacket = OSPKT_TO_RTPKT(skb);
	}

	return pPacket;
}


#define TKIP_TX_MIC_SIZE		8
PNDIS_PACKET duplicate_pkt_with_TKIP_MIC(
	IN void *pReserved,
	IN PNDIS_PACKET pPacket)
{
	struct sk_buff *skb, *newskb;

	skb = RTPKT_TO_OSPKT(pPacket);
	if (skb_tailroom(skb) < TKIP_TX_MIC_SIZE) {
		/* alloc a new skb and copy the packet */
		newskb = skb_copy_expand(skb, skb_headroom(skb), TKIP_TX_MIC_SIZE, GFP_ATOMIC);

		dev_kfree_skb_any(skb);
		MEM_DBG_PKT_FREE_INC(skb);

		if (newskb == NULL) {
			DBGPRINT(RT_DEBUG_ERROR, ("Extend Tx.MIC for packet failed!, dropping packet!\n"));
			return NULL;
		}
		skb = newskb;
		MEM_DBG_PKT_ALLOC_INC(skb);
	}

	return OSPKT_TO_RTPKT(skb);


}


/*
	========================================================================

	Routine Description:
		Send a L2 frame to upper daemon to trigger state machine

	Arguments:
		pAd - pointer to our pAdapter context

	Return Value:

	Note:

	========================================================================
*/
BOOLEAN RTMPL2FrameTxAction(
	IN void * pCtrlBkPtr,
	IN struct net_device *pNetDev,
	IN RTMP_CB_8023_PACKET_ANNOUNCE _announce_802_3_packet,
	IN UCHAR apidx,
	IN PUCHAR pData,
	IN UINT32 data_len,
	IN	UCHAR			OpMode)
{
	struct sk_buff *skb = dev_alloc_skb(data_len + 2);

	if (!skb) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("%s : Error! Can't allocate a skb.\n", __FUNCTION__));
		return FALSE;
	}

	MEM_DBG_PKT_ALLOC_INC(skb);
	/*get_netdev_from_bssid(pAd, apidx)); */
	SET_OS_PKT_NETDEV(skb, pNetDev);

	/* 16 byte align the IP header */
	skb_reserve(skb, 2);

	/* Insert the frame content */
	NdisMoveMemory(GET_OS_PKT_DATAPTR(skb), pData, data_len);

	/* End this frame */
	skb_put(GET_OS_PKT_TYPE(skb), data_len);

	DBGPRINT(RT_DEBUG_TRACE, ("%s doen\n", __FUNCTION__));

	_announce_802_3_packet(pCtrlBkPtr, skb, OpMode);

	return TRUE;

}


PNDIS_PACKET ExpandPacket(
	IN void *pReserved,
	IN PNDIS_PACKET pPacket,
	IN UINT32 ext_head_len,
	IN UINT32 ext_tail_len)
{
	struct sk_buff *skb, *newskb;

	skb = RTPKT_TO_OSPKT(pPacket);
	if (skb_cloned(skb) || (skb_headroom(skb) < ext_head_len)
	    || (skb_tailroom(skb) < ext_tail_len)) {
		UINT32 head_len =
		    (skb_headroom(skb) <
		     ext_head_len) ? ext_head_len : skb_headroom(skb);
		UINT32 tail_len =
		    (skb_tailroom(skb) <
		     ext_tail_len) ? ext_tail_len : skb_tailroom(skb);

		/* alloc a new skb and copy the packet */
		newskb = skb_copy_expand(skb, head_len, tail_len, GFP_ATOMIC);

		dev_kfree_skb_any(skb);
		MEM_DBG_PKT_FREE_INC(skb);

		if (newskb == NULL) {
			DBGPRINT(RT_DEBUG_ERROR,
				 ("Extend Tx buffer for WPI failed!, dropping packet!\n"));
			return NULL;
		}
		skb = newskb;
		MEM_DBG_PKT_ALLOC_INC(skb);
	}

	return OSPKT_TO_RTPKT(skb);

}

PNDIS_PACKET ClonePacket(
	IN void *pReserved,
	IN PNDIS_PACKET pPacket,
	IN PUCHAR pData,
	IN ULONG DataSize)
{
	struct sk_buff *pRxPkt;
	struct sk_buff *pClonedPkt;

	ASSERT(pPacket);
	pRxPkt = RTPKT_TO_OSPKT(pPacket);

	/* clone the packet */
	pClonedPkt = skb_clone(pRxPkt, MEM_ALLOC_FLAG);

	if (pClonedPkt) {
		/* set the correct dataptr and data len */
		MEM_DBG_PKT_ALLOC_INC(pClonedPkt);
		pClonedPkt->dev = pRxPkt->dev;
		pClonedPkt->data = pData;
		pClonedPkt->len = DataSize;
                SET_OS_PKT_DATATAIL(pClonedPkt, pClonedPkt->data, pClonedPkt->len);
		ASSERT(DataSize < 1530);
	}
	return pClonedPkt;
}

void RtmpOsPktInit(
	IN PNDIS_PACKET pNetPkt,
	IN struct net_device *pNetDev,
	IN UCHAR *pData,
	IN USHORT DataSize)
{
	PNDIS_PACKET pRxPkt;

	pRxPkt = RTPKT_TO_OSPKT(pNetPkt);

	SET_OS_PKT_NETDEV(pRxPkt, pNetDev);
	SET_OS_PKT_DATAPTR(pRxPkt, pData);
	SET_OS_PKT_LEN(pRxPkt, DataSize);
	SET_OS_PKT_DATATAIL(pRxPkt, pData, DataSize);
}


void wlan_802_11_to_802_3_packet(
	IN struct net_device *pNetDev,
	IN UCHAR OpMode,
	IN USHORT VLAN_VID,
	IN USHORT VLAN_Priority,
	IN PNDIS_PACKET pRxPacket,
	IN UCHAR *pData,
	IN ULONG DataSize,
	IN PUCHAR pHeader802_3,
	IN UCHAR FromWhichBSSID,
	IN UCHAR *TPID)
{
	struct sk_buff *pOSPkt;

	ASSERT(pHeader802_3);

	pOSPkt = RTPKT_TO_OSPKT(pRxPacket);

	/*get_netdev_from_bssid(pAd, FromWhichBSSID); */
	pOSPkt->dev = pNetDev;
	pOSPkt->data = pData;
	pOSPkt->len = DataSize;
	SET_OS_PKT_DATATAIL(pOSPkt, pOSPkt->data, pOSPkt->len);

	/* copy 802.3 header */

#ifdef CONFIG_STA_SUPPORT
	RT_CONFIG_IF_OPMODE_ON_STA(OpMode)
	{
	    NdisMoveMemory(skb_push(pOSPkt, LENGTH_802_3), pHeader802_3, LENGTH_802_3);
	}
#endif /* CONFIG_STA_SUPPORT */

}


void hex_dump(char *str, UCHAR *pSrcBufVA, UINT SrcBufLen)
{
#ifdef DBG
	unsigned char *pt;
	int x;

	if (RTDebugLevel < RT_DEBUG_TRACE)
		return;

	pt = pSrcBufVA;
	printk("%s: %p, len = %d\n", str, pSrcBufVA, SrcBufLen);
	for (x = 0; x < SrcBufLen; x++) {
		if (x % 16 == 0)
			printk("0x%04x : ", x);
		printk("%02x ", ((unsigned char)pt[x]));
		if (x % 16 == 15)
			printk("\n");
	}
	printk("\n");
#endif /* DBG */
}

#ifdef CONFIG_STA_SUPPORT
INT32 ralinkrate[] = {
	2,  4, 11, 22,
	12, 18, 24, 36, 48, 72, 96, 108, 109, 110, 111, 112, /* CCK and OFDM */
	13, 26, 39, 52, 78, 104, 117, 130, 26, 52, 78, 104, 156, 208, 234, 260,
	39, 78, 117, 156, 234, 312, 351, 390, /* BW 20, 800ns GI, MCS 0~23 */
	27, 54, 81, 108, 162, 216, 243, 270, 54, 108, 162, 216, 324, 432, 486, 540,
	81, 162, 243, 324, 486, 648, 729, 810, /* BW 40, 800ns GI, MCS 0~23 */
	14, 29, 43, 57, 87, 115, 130, 144, 29, 59, 87, 115, 173, 230, 260, 288,
	43, 87, 130, 173, 260, 317, 390, 433, /* BW 20, 400ns GI, MCS 0~23 */
	30, 60, 90, 120, 180, 240, 270, 300, 60, 120, 180, 240, 360, 480, 540, 600,
	90, 180, 270, 360, 540, 720, 810, 900, /* BW 40, 400ns GI, MCS 0~23 */
	13, 26, 39, 52, 78, 104, 117, 130, 156, /* AC: 20Mhz, 800ns GI, MCS: 0~8 */
	27, 54, 81, 108, 162, 216, 243, 270, 324, 360, /* AC: 40Mhz, 800ns GI, MCS: 0~9 */
	59, 117, 176, 234, 351, 468, 527, 585, 702, 780, /* AC: 80Mhz, 800ns GI, MCS: 0~9 */
	14, 29, 43, 57, 87, 115, 130, 144, 173, /* AC: 20Mhz, 400ns GI, MCS: 0~8 */
	30, 60, 90, 120, 180, 240, 270, 300, 360, 400, /* AC: 40Mhz, 400ns GI, MCS: 0~9 */
	65, 130, 195, 260, 390, 520, 585, 650, 780, 867, /* AC: 80Mhz, 400ns GI, MCS: 0~9 */
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47 /* 3*3 */
};

UINT32 RT_RateSize = sizeof (ralinkrate);

void send_monitor_packets(IN struct net_device *pNetDev,
			  IN PNDIS_PACKET pRxPacket,
			  IN PHEADER_802_11 pHeader,
			  IN UCHAR * pData,
			  IN USHORT DataSize,
			  IN UCHAR L2PAD,
			  IN UCHAR PHYMODE,
			  IN UCHAR BW,
			  IN UCHAR ShortGI,
			  IN UCHAR MCS,
			  IN UCHAR AMPDU,
			  IN UCHAR STBC,
			  IN UCHAR RSSI1,
			  IN UCHAR BssMonitorFlag11n,
			  IN UCHAR * pDevName,
			  IN UCHAR Channel,
			  IN UCHAR CentralChannel,
			  IN UINT32 MaxRssi) {
	struct sk_buff *pOSPkt;
	wlan_ng_prism2_header *ph;
#ifdef MONITOR_FLAG_11N_SNIFFER_SUPPORT
	ETHEREAL_RADIO h,
	*ph_11n33;		/* for new 11n sniffer format */
#endif /* MONITOR_FLAG_11N_SNIFFER_SUPPORT */
	int rate_index = 0;
	USHORT header_len = 0;
	UCHAR temp_header[40] = {
	0};

	MEM_DBG_PKT_FREE_INC(pRxPacket);


	pOSPkt = RTPKT_TO_OSPKT(pRxPacket);	/*pRxBlk->pRxPacket); */
	pOSPkt->dev = pNetDev;	/*get_netdev_from_bssid(pAd, BSS0); */
	if (pHeader->FC.Type == BTYPE_DATA) {
		DataSize -= LENGTH_802_11;
		if ((pHeader->FC.ToDs == 1) && (pHeader->FC.FrDs == 1))
			header_len = LENGTH_802_11_WITH_ADDR4;
		else
			header_len = LENGTH_802_11;

		/* QOS */
		if (pHeader->FC.SubType & 0x08) {
			header_len += 2;
			/* Data skip QOS contorl field */
			DataSize -= 2;
		}

		/* Order bit: A-Ralink or HTC+ */
		if (pHeader->FC.Order) {
			header_len += 4;
			/* Data skip HTC contorl field */
			DataSize -= 4;
		}

		/* Copy Header */
		if (header_len <= 40)
			NdisMoveMemory(temp_header, pData, header_len);

		/* skip HW padding */
		if (L2PAD)
			pData += (header_len + 2);
		else
			pData += header_len;
	}

	if (DataSize < pOSPkt->len) {
		skb_trim(pOSPkt, DataSize);
	} else {
		skb_put(pOSPkt, (DataSize - pOSPkt->len));
	}

	if ((pData - pOSPkt->data) > 0) {
		skb_put(pOSPkt, (pData - pOSPkt->data));
		skb_pull(pOSPkt, (pData - pOSPkt->data));
	}

	if (skb_headroom(pOSPkt) < (sizeof (wlan_ng_prism2_header) + header_len)) {
		if (pskb_expand_head(pOSPkt, (sizeof (wlan_ng_prism2_header) + header_len), 0, GFP_ATOMIC)) {
			DBGPRINT(RT_DEBUG_ERROR,
				 ("%s : Reallocate header size of sk_buff fail!\n",
				  __FUNCTION__));
			goto err_free_sk_buff;
		}
	}

	if (header_len > 0)
		NdisMoveMemory(skb_push(pOSPkt, header_len), temp_header,
			       header_len);

#ifdef MONITOR_FLAG_11N_SNIFFER_SUPPORT
	if (BssMonitorFlag11n == 0)
#endif /* MONITOR_FLAG_11N_SNIFFER_SUPPORT */
	{
		ph = (wlan_ng_prism2_header *) skb_push(pOSPkt,
							sizeof(wlan_ng_prism2_header));
		memset(ph, 0, sizeof(wlan_ng_prism2_header));

		ph->msgcode = DIDmsg_lnxind_wlansniffrm;
		ph->msglen = sizeof (wlan_ng_prism2_header);
		strcpy((char *) ph->devname, (char *) pDevName);

		ph->hosttime.did = DIDmsg_lnxind_wlansniffrm_hosttime;
		ph->hosttime.status = 0;
		ph->hosttime.len = 4;
		ph->hosttime.data = jiffies;

		ph->mactime.did = DIDmsg_lnxind_wlansniffrm_mactime;
		ph->mactime.status = 0;
		ph->mactime.len = 0;
		ph->mactime.data = 0;

		ph->istx.did = DIDmsg_lnxind_wlansniffrm_istx;
		ph->istx.status = 0;
		ph->istx.len = 0;
		ph->istx.data = 0;

		ph->channel.did = DIDmsg_lnxind_wlansniffrm_channel;
		ph->channel.status = 0;
		ph->channel.len = 4;

		ph->channel.data = (u_int32_t) Channel;

		ph->rssi.did = DIDmsg_lnxind_wlansniffrm_rssi;
		ph->rssi.status = 0;
		ph->rssi.len = 4;
		ph->rssi.data = MaxRssi;
		ph->signal.did = DIDmsg_lnxind_wlansniffrm_signal;
		ph->signal.status = 0;
		ph->signal.len = 4;
		ph->signal.data = 0;	/*rssi + noise; */

		ph->noise.did = DIDmsg_lnxind_wlansniffrm_noise;
		ph->noise.status = 0;
		ph->noise.len = 4;
		ph->noise.data = 0;

#ifdef DOT11_N_SUPPORT
		if (PHYMODE >= MODE_HTMIX) {
			rate_index = 12 + ((UCHAR) BW * 24) + ((UCHAR) ShortGI * 48) + ((UCHAR) MCS);
		} else
#endif /* DOT11_N_SUPPORT */
		if (PHYMODE == MODE_OFDM)
			rate_index = (UCHAR) (MCS) + 4;
		else
			rate_index = (UCHAR) (MCS);

		if (rate_index < 0)
			rate_index = 0;
		if (rate_index >= (sizeof (ralinkrate) / sizeof (ralinkrate[0])))
			rate_index = (sizeof (ralinkrate) / sizeof (ralinkrate[0])) - 1;

		ph->rate.did = DIDmsg_lnxind_wlansniffrm_rate;
		ph->rate.status = 0;
		ph->rate.len = 4;
		/* real rate = ralinkrate[rate_index] / 2 */
		ph->rate.data = ralinkrate[rate_index];

		ph->frmlen.did = DIDmsg_lnxind_wlansniffrm_frmlen;
		ph->frmlen.status = 0;
		ph->frmlen.len = 4;
		ph->frmlen.data = (u_int32_t) DataSize;
	}
#ifdef MONITOR_FLAG_11N_SNIFFER_SUPPORT
	else {
		ph_11n33 = &h;
		memset((unsigned char *)ph_11n33, 0,
			       sizeof (ETHEREAL_RADIO));

		/*802.11n fields */
		if (MCS > 15)
			ph_11n33->Flag_80211n |= WIRESHARK_11N_FLAG_3x3;

		if (PHYMODE == MODE_HTGREENFIELD)
			ph_11n33->Flag_80211n |= WIRESHARK_11N_FLAG_GF;

		if (BW == 1) {
			ph_11n33->Flag_80211n |= WIRESHARK_11N_FLAG_BW40;
		} else if (Channel < CentralChannel) {
			ph_11n33->Flag_80211n |= WIRESHARK_11N_FLAG_BW20U;
		} else if (Channel > CentralChannel) {
			ph_11n33->Flag_80211n |= WIRESHARK_11N_FLAG_BW20D;
		} else {
			ph_11n33->Flag_80211n |=
			    (WIRESHARK_11N_FLAG_BW20U |
			     WIRESHARK_11N_FLAG_BW20D);
		}

		if (ShortGI == 1)
			ph_11n33->Flag_80211n |= WIRESHARK_11N_FLAG_SGI;

		if (AMPDU)
			ph_11n33->Flag_80211n |= WIRESHARK_11N_FLAG_AMPDU;

		if (STBC)
			ph_11n33->Flag_80211n |= WIRESHARK_11N_FLAG_STBC;

		ph_11n33->signal_level = (UCHAR) RSSI1;

		/* data_rate is the rate index in the wireshark rate table */
		if (PHYMODE >= MODE_HTMIX) {
			if (MCS == 32) {
				if (ShortGI)
					ph_11n33->data_rate = 16;
				else
					ph_11n33->data_rate = 4;
			} else if (MCS > 15)
				ph_11n33->data_rate =
				    (16 * 4 + ((UCHAR) BW * 16) +
				     ((UCHAR) ShortGI * 32) + ((UCHAR) MCS));
			else
				ph_11n33->data_rate =
				    16 + ((UCHAR) BW * 16) +
				    ((UCHAR) ShortGI * 32) + ((UCHAR) MCS);
		} else if (PHYMODE == MODE_OFDM)
			ph_11n33->data_rate = (UCHAR) (MCS) + 4;
		else
			ph_11n33->data_rate = (UCHAR) (MCS);

		/*channel field */
		ph_11n33->channel = (UCHAR) Channel;

		NdisMoveMemory(skb_put(pOSPkt, sizeof (ETHEREAL_RADIO)),
			       (UCHAR *) ph_11n33, sizeof (ETHEREAL_RADIO));
	}
#endif /* MONITOR_FLAG_11N_SNIFFER_SUPPORT */

	pOSPkt->pkt_type = PACKET_OTHERHOST;
	pOSPkt->protocol = eth_type_trans(pOSPkt, pOSPkt->dev);
	pOSPkt->ip_summed = CHECKSUM_NONE;
	netif_rx(pOSPkt);

	return;

      err_free_sk_buff:
	RELEASE_NDIS_PACKET(NULL, pRxPacket, NDIS_STATUS_FAILURE);
	return;

}
#endif /* CONFIG_STA_SUPPORT */


/*******************************************************************************

	File open/close related functions.

 *******************************************************************************/
RTMP_OS_FD RtmpOSFileOpen(char *pPath, int flag, int mode)
{
	struct file *filePtr;

	if (flag == RTMP_FILE_RDONLY)
		flag = O_RDONLY;
	else if (flag == RTMP_FILE_WRONLY)
		flag = O_WRONLY;
	else if (flag == RTMP_FILE_CREAT)
		flag = O_CREAT;
	else if (flag == RTMP_FILE_TRUNC)
		flag = O_TRUNC;

	filePtr = filp_open(pPath, flag, 0);
	if (IS_ERR(filePtr)) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("%s(): Error %ld opening %s\n", __FUNCTION__,
			  -PTR_ERR(filePtr), pPath));
	}

	return (RTMP_OS_FD) filePtr;
}

int RtmpOSFileClose(RTMP_OS_FD osfd)
{
	filp_close(osfd, NULL);
	return 0;
}

void RtmpOSFileSeek(RTMP_OS_FD osfd, int offset)
{
	osfd->f_pos = offset;
}


int RtmpOSFileRead(RTMP_OS_FD osfd, char *pDataPtr, int readLen)
{
	/* The object must have a read method */
	if (osfd->f_op && osfd->f_op->read) {
		return osfd->f_op->read(osfd, pDataPtr, readLen, &osfd->f_pos);
	} else {
		DBGPRINT(RT_DEBUG_ERROR, ("no file read method\n"));
		return -1;
	}
}

int RtmpOSFileWrite(RTMP_OS_FD osfd, char *pDataPtr, int writeLen)
{
	return osfd->f_op->write(osfd, pDataPtr, (size_t) writeLen, &osfd->f_pos);
}

static inline void __RtmpOSFSInfoChange(OS_FS_INFO * pOSFSInfo, BOOLEAN bSet)
{
	if (bSet) {
		/* Save uid and gid used for filesystem access. */
		/* Set user and group to 0 (root) */
		pOSFSInfo->fsuid = current_fsuid();
		pOSFSInfo->fsgid = current_fsgid();
		pOSFSInfo->fs = get_fs();
		set_fs(KERNEL_DS);
	} else {
		set_fs(pOSFSInfo->fs);
	}
}


/*******************************************************************************

	Task create/management/kill related functions.

 *******************************************************************************/
static inline NDIS_STATUS __RtmpOSTaskKill(OS_TASK *pTask)
{
	int ret = NDIS_STATUS_FAILURE;

#ifdef KTHREAD_SUPPORT
	if (pTask->kthread_task) {
		kthread_stop(pTask->kthread_task);
		ret = NDIS_STATUS_SUCCESS;
	}
#else
	CHECK_PID_LEGALITY(pTask->taskPID) {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("Terminate the task(%s) with pid(%d)!\n",
			  pTask->taskName, GET_PID_NUMBER(pTask->taskPID)));
		mb();
		pTask->task_killed = 1;
		mb();
		ret = KILL_THREAD_PID(pTask->taskPID, SIGTERM, 1);
		if (ret) {
			printk(KERN_WARNING
			       "kill task(%s) with pid(%d) failed(retVal=%d)!\n",
			       pTask->taskName, GET_PID_NUMBER(pTask->taskPID),
			       ret);
		} else {
			wait_for_completion(&pTask->taskComplete);
			pTask->taskPID = THREAD_PID_INIT_VALUE;
			pTask->task_killed = 0;
			RTMP_SEM_EVENT_DESTORY(&pTask->taskSema);
			ret = NDIS_STATUS_SUCCESS;
		}
	}
#endif

	return ret;

}

static inline INT __RtmpOSTaskNotifyToExit(OS_TASK *pTask)
{
#ifndef KTHREAD_SUPPORT
	pTask->taskPID = THREAD_PID_INIT_VALUE;
	complete_and_exit(&pTask->taskComplete, 0);
#endif

	return 0;
}

static inline void __RtmpOSTaskCustomize(OS_TASK *pTask)
{
#ifndef KTHREAD_SUPPORT

	daemonize((char *) & pTask->taskName[0] /*"%s",pAd->net_dev->name */ );

	allow_signal(SIGTERM);
	allow_signal(SIGKILL);
	current->flags |= PF_NOFREEZE;

	RTMP_GET_OS_PID(pTask->taskPID, current->pid);

	/* signal that we've started the thread */
	complete(&pTask->taskComplete);

#endif
}

static inline NDIS_STATUS __RtmpOSTaskAttach(
	IN OS_TASK *pTask,
	IN RTMP_OS_TASK_CALLBACK fn,
	IN ULONG arg)
{
	NDIS_STATUS status = NDIS_STATUS_SUCCESS;
#ifndef KTHREAD_SUPPORT
	pid_t pid_number = -1;
#endif /* KTHREAD_SUPPORT */

#ifdef KTHREAD_SUPPORT
	pTask->task_killed = 0;
	pTask->kthread_task = NULL;
	pTask->kthread_task =
	    kthread_run((cast_fn) fn, (void *)arg, pTask->taskName);
	if (IS_ERR(pTask->kthread_task))
		status = NDIS_STATUS_FAILURE;
#else
	pid_number =
	    kernel_thread((cast_fn) fn, (void *)arg, RTMP_OS_MGMT_TASK_FLAGS);
	if (pid_number < 0) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("Attach task(%s) failed!\n", pTask->taskName));
		status = NDIS_STATUS_FAILURE;
	} else {
		/* Wait for the thread to start */
		wait_for_completion(&pTask->taskComplete);
		status = NDIS_STATUS_SUCCESS;
	}
#endif
	return status;
}

static inline NDIS_STATUS __RtmpOSTaskInit(
	IN OS_TASK *pTask,
	IN char *pTaskName,
	IN void *pPriv,
	IN LIST_HEADER *pSemList)
{
	int len;

	ASSERT(pTask);

#ifndef KTHREAD_SUPPORT
	memset((PUCHAR) (pTask), 0, sizeof (OS_TASK));
#endif

	len = strlen(pTaskName);
	len = len > (RTMP_OS_TASK_NAME_LEN - 1) ? (RTMP_OS_TASK_NAME_LEN - 1) : len;
	NdisMoveMemory(&pTask->taskName[0], pTaskName, len);
	pTask->priv = pPriv;

#ifndef KTHREAD_SUPPORT
	RTMP_SEM_EVENT_INIT_LOCKED(&(pTask->taskSema), pSemList);
	pTask->taskPID = THREAD_PID_INIT_VALUE;
	init_completion(&pTask->taskComplete);
#endif

#ifdef KTHREAD_SUPPORT
	init_waitqueue_head(&(pTask->kthread_q));
#endif /* KTHREAD_SUPPORT */

	return NDIS_STATUS_SUCCESS;
}

BOOLEAN __RtmpOSTaskWait(
	IN void *pReserved,
	IN OS_TASK *pTask,
	IN INT32 *pStatus)
{
#ifdef KTHREAD_SUPPORT
	RTMP_WAIT_EVENT_INTERRUPTIBLE((*pStatus), pTask);

	if ((pTask->task_killed == 1) || ((*pStatus) != 0))
		return FALSE;
#else

	RTMP_SEM_EVENT_WAIT(&(pTask->taskSema), (*pStatus));

	/* unlock the device pointers */
	if ((*pStatus) != 0) {
/*		RTMP_SET_FLAG_(*pFlags, fRTMP_ADAPTER_HALT_IN_PROGRESS); */
		return FALSE;
	}
#endif /* KTHREAD_SUPPORT */

	return TRUE;
}


static UINT32 RtmpOSWirelessEventTranslate(IN UINT32 eventType)
{
	switch (eventType) {
	case RT_WLAN_EVENT_CUSTOM:
		eventType = IWEVCUSTOM;
		break;

	case RT_WLAN_EVENT_CGIWAP:
		eventType = SIOCGIWAP;
		break;

	case RT_WLAN_EVENT_ASSOC_REQ_IE:
		eventType = IWEVASSOCREQIE;
		break;

	case RT_WLAN_EVENT_SCAN:
		eventType = SIOCGIWSCAN;
		break;

	case RT_WLAN_EVENT_EXPIRED:
		eventType = IWEVEXPIRED;
		break;

	default:
		printk("Unknown event: 0x%x\n", eventType);
		break;
	}

	return eventType;
}

int RtmpOSWrielessEventSend(
	IN struct net_device *pNetDev,
	IN UINT32 eventType,
	IN INT flags,
	IN PUCHAR pSrcMac,
	IN PUCHAR pData,
	IN UINT32 dataLen)
{
	union iwreq_data wrqu;

	/* translate event type */
	eventType = RtmpOSWirelessEventTranslate(eventType);

	memset(&wrqu, 0, sizeof (wrqu));

	if (flags > -1)
		wrqu.data.flags = flags;

	if (pSrcMac)
		memcpy(wrqu.ap_addr.sa_data, pSrcMac, MAC_ADDR_LEN);

	if ((pData != NULL) && (dataLen > 0))
		wrqu.data.length = dataLen;
	else
		wrqu.data.length = 0;

	wireless_send_event(pNetDev, eventType, &wrqu, (char *)pData);
	return 0;
}

int RtmpOSWrielessEventSendExt(
	IN struct net_device *pNetDev,
	IN UINT32 eventType,
	IN INT flags,
	IN PUCHAR pSrcMac,
	IN PUCHAR pData,
	IN UINT32 dataLen,
	IN UINT32 family)
{
	union iwreq_data wrqu;

	/* translate event type */
	eventType = RtmpOSWirelessEventTranslate(eventType);

	/* translate event type */
	memset(&wrqu, 0, sizeof (wrqu));

	if (flags > -1)
		wrqu.data.flags = flags;

	if (pSrcMac)
		memcpy(wrqu.ap_addr.sa_data, pSrcMac, MAC_ADDR_LEN);

	if ((pData != NULL) && (dataLen > 0))
		wrqu.data.length = dataLen;

	wrqu.addr.sa_family = family;

	wireless_send_event(pNetDev, eventType, &wrqu, (char *)pData);
	return 0;
}

int RtmpOSNetDevAddrSet(
	IN UCHAR OpMode,
	IN struct net_device *pNetDev,
	IN PUCHAR pMacAddr,
	IN PUCHAR dev_name)
{
	struct net_device *net_dev;

	net_dev = pNetDev;
/*	GET_PAD_FROM_NET_DEV(pAd, net_dev); */

#ifdef CONFIG_STA_SUPPORT
	/* work-around for the SuSE due to it has it's own interface name management system. */
	RT_CONFIG_IF_OPMODE_ON_STA(OpMode) {
/*		memset(pAd->StaCfg.dev_name, 0, 16); */
/*		NdisMoveMemory(pAd->StaCfg.dev_name, net_dev->name, strlen(net_dev->name)); */
		if (dev_name != NULL) {
			memset(dev_name, 0, 16);
			NdisMoveMemory(dev_name, net_dev->name, strlen(net_dev->name));
		}
	}
#endif /* CONFIG_STA_SUPPORT */

	NdisMoveMemory(net_dev->dev_addr, pMacAddr, 6);

	return 0;
}

/*
  *	Assign the network dev name for created Ralink WiFi interface.
  */
static int RtmpOSNetDevRequestName(
	IN INT32 MC_RowID,
	IN UINT32 *pIoctlIF,
	IN struct net_device *dev,
	IN char *pPrefixStr,
	IN INT devIdx)
{
	struct net_device *existNetDev;
	STRING suffixName[IFNAMSIZ];
	STRING desiredName[IFNAMSIZ];
	int ifNameIdx,
	 prefixLen,
	 slotNameLen;
	int Status;

	prefixLen = strlen(pPrefixStr);
	ASSERT((prefixLen < IFNAMSIZ));

	for (ifNameIdx = devIdx; ifNameIdx < 32; ifNameIdx++) {
		memset(suffixName, 0, IFNAMSIZ);
		memset(desiredName, 0, IFNAMSIZ);
		strncpy(&desiredName[0], pPrefixStr, prefixLen);

			sprintf(suffixName, "%d", ifNameIdx);

		slotNameLen = strlen(suffixName);
		ASSERT(((slotNameLen + prefixLen) < IFNAMSIZ));
		strcat(desiredName, suffixName);

		existNetDev = RtmpOSNetDevGetByName(dev, &desiredName[0]);
		if (existNetDev == NULL)
			break;
		else
			RtmpOSNetDeviceRefPut(existNetDev);
	}

	if (ifNameIdx < 32) {
#ifdef HOSTAPD_SUPPORT
		*pIoctlIF = ifNameIdx;
#endif /*HOSTAPD_SUPPORT */
		strcpy(&dev->name[0], &desiredName[0]);
		Status = NDIS_STATUS_SUCCESS;
	} else {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("Cannot request DevName with preifx(%s) and in range(0~32) as suffix from OS!\n",
			  pPrefixStr));
		Status = NDIS_STATUS_FAILURE;
	}

	return Status;
}

void RtmpOSNetDevClose(struct net_device *pNetDev)
{
	dev_close(pNetDev);
}

void RtmpOSNetDevFree(struct net_device *pNetDev)
{
	struct dev_priv_info *pDevInfo = NULL;


	ASSERT(pNetDev);

	/* free assocaited private information */
	pDevInfo = (struct dev_priv_info *) netdev_priv(pNetDev);
	if (pDevInfo != NULL)
		kfree(pDevInfo);

	free_netdev(pNetDev);

#ifdef VENDOR_FEATURE4_SUPPORT
	printk("OS_NumOfMemAlloc = %ld, OS_NumOfMemFree = %ld\n",
			OS_NumOfMemAlloc, OS_NumOfMemFree);
#endif /* VENDOR_FEATURE4_SUPPORT */
#ifdef VENDOR_FEATURE2_SUPPORT
	printk("OS_NumOfPktAlloc = %ld, OS_NumOfPktFree = %ld\n",
			OS_NumOfPktAlloc, OS_NumOfPktFree);
#endif /* VENDOR_FEATURE2_SUPPORT */
}

INT RtmpOSNetDevAlloc(
	IN struct net_device **new_dev_p,
	IN UINT32 privDataSize)
{
	*new_dev_p = NULL;

	DBGPRINT(RT_DEBUG_TRACE,
		 ("Allocate a net device with private data size=%d!\n",
		  privDataSize));
	*new_dev_p = alloc_etherdev(privDataSize);

	if (*new_dev_p)
		return NDIS_STATUS_SUCCESS;
	else
		return NDIS_STATUS_FAILURE;
}


INT RtmpOSNetDevOpsAlloc(void **pNetDevOps)
{
	*pNetDevOps = (void *) vmalloc(sizeof (struct net_device_ops));
	if (*pNetDevOps) {
		memset(*pNetDevOps, 0, sizeof (struct net_device_ops));
		return NDIS_STATUS_SUCCESS;
	} else {
		return NDIS_STATUS_FAILURE;
	}
}


struct net_device *RtmpOSNetDevGetByName(struct net_device *pNetDev, char *pDevName)
{
	struct net_device *pTargetNetDev = NULL;

	pTargetNetDev = dev_get_by_name(dev_net(pNetDev), pDevName);

	return pTargetNetDev;
}


void RtmpOSNetDeviceRefPut(struct net_device *pNetDev)
{
	/*
	   every time dev_get_by_name is called, and it has returned a valid struct
	   net_device*, dev_put should be called afterwards, because otherwise the
	   machine hangs when the device is unregistered (since dev->refcnt > 1).
	 */
	if (pNetDev)
		dev_put(pNetDev);
}


INT RtmpOSNetDevDestory(void *pReserved, struct net_device *pNetDev)
{

	/* TODO: Need to fix this */
	printk("WARNING: This function(%s) not implement yet!!!\n",
	       __FUNCTION__);
	return 0;
}


void RtmpOSNetDevDetach(struct net_device *pNetDev)
{
	struct net_device_ops *pNetDevOps = (struct net_device_ops *)pNetDev->netdev_ops;

	unregister_netdev(pNetDev);

	vfree(pNetDevOps);
}

void RtmpOSNetDevProtect(BOOLEAN lock_it)
{

/*
	if (lock_it)
		rtnl_lock();
	else
		rtnl_unlock();
*/
}

static void RALINK_ET_DrvInfoGet(
	struct net_device *pDev,
	struct ethtool_drvinfo *pInfo)
{
	strcpy(pInfo->driver, "RALINK WLAN");


	sprintf(pInfo->bus_info, "CSR 0x%lx", pDev->base_addr);
}

static struct ethtool_ops RALINK_Ethtool_Ops = {
	.get_drvinfo = RALINK_ET_DrvInfoGet,
};


int RtmpOSNetDevAttach(
	IN UCHAR OpMode,
	IN struct net_device *pNetDev,
	IN RTMP_OS_NETDEV_OP_HOOK *pDevOpHook)
{
	int ret,
	 rtnl_locked = FALSE;

	struct net_device_ops *pNetDevOps = (struct net_device_ops *)pNetDev->netdev_ops;

	DBGPRINT(RT_DEBUG_TRACE, ("RtmpOSNetDevAttach()--->\n"));

	/* If we need hook some callback function to the net device structrue, now do it. */
	if (pDevOpHook) {
/*		struct rtmp_adapter *pAd = NULL; */

/*		GET_PAD_FROM_NET_DEV(pAd, pNetDev); */

		pNetDevOps->ndo_open = pDevOpHook->open;
		pNetDevOps->ndo_stop = pDevOpHook->stop;
		pNetDevOps->ndo_start_xmit =
		    (HARD_START_XMIT_FUNC) (pDevOpHook->xmit);
		pNetDevOps->ndo_do_ioctl = pDevOpHook->ioctl;

		pNetDev->ethtool_ops = &RALINK_Ethtool_Ops;

		/* if you don't implement get_stats, just leave the callback function as NULL, a dummy
		   function will make kernel panic.
		 */
		if (pDevOpHook->get_stats)
			pNetDevOps->ndo_get_stats = pDevOpHook->get_stats;

		/* OS specific flags, here we used to indicate if we are virtual interface */
/*		pNetDev->priv_flags = pDevOpHook->priv_flags; */
		RT_DEV_PRIV_FLAGS_SET(pNetDev, pDevOpHook->priv_flags);

#ifdef CONFIG_STA_SUPPORT
		if (OpMode == OPMODE_STA) {
/*			pNetDev->wireless_handlers = &rt28xx_iw_handler_def; */
			pNetDev->wireless_handlers = &rt28xx_iw_handler_def;
		}
#endif /* CONFIG_STA_SUPPORT */

		/* copy the net device mac address to the net_device structure. */
		NdisMoveMemory(pNetDev->dev_addr, &pDevOpHook->devAddr[0],
			       MAC_ADDR_LEN);

		rtnl_locked = pDevOpHook->needProtcted;

	}
	pNetDevOps->ndo_validate_addr = NULL;
	/*pNetDev->netdev_ops = ops; */

	if (rtnl_locked)
		ret = register_netdevice(pNetDev);
	else
		ret = register_netdev(pNetDev);

	netif_stop_queue(pNetDev);

	DBGPRINT(RT_DEBUG_TRACE, ("<---RtmpOSNetDevAttach(), ret=%d\n", ret));
	if (ret == 0)
		return NDIS_STATUS_SUCCESS;
	else
		return NDIS_STATUS_FAILURE;
}

struct net_device *RtmpOSNetDevCreate(
	IN INT32 MC_RowID,
	IN UINT32 *pIoctlIF,
	IN INT devType,
	IN INT devNum,
	IN INT privMemSize,
	IN char *pNamePrefix)
{
	struct net_device *pNetDev = NULL;
	struct net_device_ops *pNetDevOps = NULL;
	int status;

	/* allocate a new network device */
	status = RtmpOSNetDevAlloc(&pNetDev, sizeof(struct dev_priv_info *) /*privMemSize */ );
	if (status != NDIS_STATUS_SUCCESS) {
		/* allocation fail, exit */
		DBGPRINT(RT_DEBUG_ERROR, ("Allocate network device fail (%s)...\n", pNamePrefix));
		return NULL;
	}
	status = RtmpOSNetDevOpsAlloc((void *) & pNetDevOps);
	if (status != NDIS_STATUS_SUCCESS) {
		/* error! no any available ra name can be used! */
		DBGPRINT(RT_DEBUG_TRACE, ("Allocate net device ops fail!\n"));
		RtmpOSNetDevFree(pNetDev);

		return NULL;
	} else {
		DBGPRINT(RT_DEBUG_TRACE, ("Allocate net device ops success!\n"));
		pNetDev->netdev_ops = pNetDevOps;
	}
	/* find a available interface name, max 32 interfaces */
	status = RtmpOSNetDevRequestName(MC_RowID, pIoctlIF, pNetDev, pNamePrefix, devNum);
	if (status != NDIS_STATUS_SUCCESS) {
		/* error! no any available ra name can be used! */
		DBGPRINT(RT_DEBUG_ERROR, ("Assign interface name (%s with suffix 0~32) failed...\n",
			  pNamePrefix));
		RtmpOSNetDevFree(pNetDev);

		return NULL;
	} else {
		DBGPRINT(RT_DEBUG_TRACE, ("The name of the new %s interface is %s...\n",
			  pNamePrefix, pNetDev->name));
	}

	return pNetDev;
}





/*
========================================================================
Routine Description:
    Allocate memory for adapter control block.

Arguments:
    pAd					Pointer to our adapter

Return Value:
	NDIS_STATUS_SUCCESS
	NDIS_STATUS_FAILURE
	NDIS_STATUS_RESOURCES

Note:
========================================================================
*/
NDIS_STATUS AdapterBlockAllocateMemory(void *handle, void **ppAd, UINT32 SizeOfpAd)
{

/*	*ppAd = (void *)vmalloc(sizeof(struct rtmp_adapter)); //pci_alloc_consistent(pci_dev, sizeof(struct rtmp_adapter), phy_addr); */
	*ppAd = (void *) vmalloc(SizeOfpAd);	/*pci_alloc_consistent(pci_dev, sizeof(struct rtmp_adapter), phy_addr); */
	if (*ppAd) {
		memset(*ppAd, 0, SizeOfpAd);
		return NDIS_STATUS_SUCCESS;
	} else
		return NDIS_STATUS_FAILURE;
}


/* ========================================================================== */

UINT RtmpOsWirelessExtVerGet(void)
{
	return WIRELESS_EXT;
}


void RtmpDrvAllMacPrint(
	IN void *pReserved,
	IN UINT32 *pBufMac,
	IN UINT32 AddrStart,
	IN UINT32 AddrEnd,
	IN UINT32 AddrStep)
{
	struct file *file_w;
	char *fileName = "MacDump.txt";
	mm_segment_t orig_fs;
	STRING *msg;
	UINT32 macAddr = 0, macValue = 0;

	os_alloc_mem(NULL, (UCHAR **)&msg, 1024);
	if (!msg)
		return;

	orig_fs = get_fs();
	set_fs(KERNEL_DS);

	/* open file */
	file_w = filp_open(fileName, O_WRONLY | O_CREAT, 0);
	if (IS_ERR(file_w)) {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("-->2) %s: Error %ld opening %s\n", __FUNCTION__,
			  -PTR_ERR(file_w), fileName));
	} else {
		if (file_w->f_op && file_w->f_op->write) {
			file_w->f_pos = 0;
			macAddr = AddrStart;

			while (macAddr <= AddrEnd) {
/*				RTMP_IO_READ32(pAd, macAddr, &macValue); // sample */
				macValue = *pBufMac++;
				sprintf(msg, "0x%04X = 0x%08X\n", macAddr, macValue);

				/* write data to file */
				file_w->f_op->write(file_w, msg, strlen(msg), &file_w->f_pos);

				printk("%s", msg);
				macAddr += AddrStep;
			}
			sprintf(msg, "\nDump all MAC values to %s\n", fileName);
		}
		filp_close(file_w, NULL);
	}
	set_fs(orig_fs);
	kfree(msg);
}


void RtmpDrvAllE2PPrint(
	IN void *pReserved,
	IN USHORT *pMacContent,
	IN UINT32 AddrEnd,
	IN UINT32 AddrStep)
{
	struct file *file_w;
	char *fileName = "EEPROMDump.txt";
	mm_segment_t orig_fs;
	STRING *msg;
	USHORT eepAddr = 0;
	USHORT eepValue;

	os_alloc_mem(NULL, (UCHAR **)&msg, 1024);
	if (!msg)
		return;

	orig_fs = get_fs();
	set_fs(KERNEL_DS);

	/* open file */
	file_w = filp_open(fileName, O_WRONLY | O_CREAT, 0);
	if (IS_ERR(file_w)) {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("-->2) %s: Error %ld opening %s\n", __FUNCTION__,
			  -PTR_ERR(file_w), fileName));
	} else {
		if (file_w->f_op && file_w->f_op->write) {
			file_w->f_pos = 0;
			eepAddr = 0x00;

			while (eepAddr <= AddrEnd) {
				eepValue = *pMacContent;
				sprintf(msg, "%08x = %04x\n", eepAddr, eepValue);

				/* write data to file */
				file_w->f_op->write(file_w, msg, strlen(msg), &file_w->f_pos);

				printk("%s", msg);
				eepAddr += AddrStep;
				pMacContent += (AddrStep >> 1);
			}
			sprintf(msg, "\nDump all EEPROM values to %s\n",
				fileName);
		}
		filp_close(file_w, NULL);
	}
	set_fs(orig_fs);
	kfree(msg);
}

/*
========================================================================
Routine Description:
	Check if the network interface is up.

Arguments:
	*pDev			- Network Interface

Return Value:
	None

Note:
========================================================================
*/
BOOLEAN RtmpOSNetDevIsUp(void *pDev)
{
	struct net_device *pNetDev = (struct net_device *)pDev;

	if ((pNetDev == NULL) || !(pNetDev->flags & IFF_UP))
		return FALSE;

	return TRUE;
}


/*
========================================================================
Routine Description:
	Wake up the command thread.

Arguments:
	pAd				- WLAN control block pointer

Return Value:
	None

Note:
========================================================================
*/
void RtmpOsCmdUp(RTMP_OS_TASK *pCmdQTask)
{
	OS_TASK *pTask = RTMP_OS_TASK_GET(pCmdQTask);
#ifdef KTHREAD_SUPPORT
	pTask->kthread_running = TRUE;
	wake_up(&pTask->kthread_q);
#else
	CHECK_PID_LEGALITY(pTask->taskPID) {
		RTMP_SEM_EVENT_UP(&(pTask->taskSema));
	}
#endif /* KTHREAD_SUPPORT */
}


/*
========================================================================
Routine Description:
	Wake up USB Mlme thread.

Arguments:
	pAd				- WLAN control block pointer

Return Value:
	None

Note:
========================================================================
*/
void RtmpOsMlmeUp(IN RTMP_OS_TASK *pMlmeQTask)
{
#ifdef RTMP_USB_SUPPORT
	OS_TASK *pTask = RTMP_OS_TASK_GET(pMlmeQTask);

#ifdef KTHREAD_SUPPORT
	if ((pTask != NULL) && (pTask->kthread_task)) {
		pTask->kthread_running = TRUE;
		wake_up(&pTask->kthread_q);
	}
#else
	if (pTask != NULL) {
		CHECK_PID_LEGALITY(pTask->taskPID) {
			RTMP_SEM_EVENT_UP(&(pTask->taskSema));
		}
	}
#endif /* KTHREAD_SUPPORT */
#endif /* RTMP_USB_SUPPORT */
}


/*
========================================================================
Routine Description:
	Check if the file is error.

Arguments:
	pFile			- the file

Return Value:
	OK or any error

Note:
	rt_linux.h, not rt_drv.h
========================================================================
*/
INT32 RtmpOsFileIsErr(IN void *pFile)
{
	return IS_FILE_OPEN_ERR(pFile);
}

int RtmpOSIRQRelease(
	IN struct net_device *pNetDev,
	IN UINT32 infType,
	IN PPCI_DEV pci_dev,
	IN BOOLEAN *pHaveMsi)
{
	struct net_device *net_dev = (struct net_device *)pNetDev;



	return 0;
}


/*
========================================================================
Routine Description:
	Enable or disable wireless event sent.

Arguments:
	pReserved		- Reserved
	FlgIsWEntSup	- TRUE or FALSE

Return Value:
	None

Note:
========================================================================
*/
void RtmpOsWlanEventSet(
	IN void *pReserved,
	IN BOOLEAN *pCfgWEnt,
	IN BOOLEAN FlgIsWEntSup)
{
/*	pAd->CommonCfg.bWirelessEvent = FlgIsWEntSup; */
	*pCfgWEnt = FlgIsWEntSup;
}

/*
========================================================================
Routine Description:
	vmalloc

Arguments:
	Size			- memory size

Return Value:
	the memory

Note:
========================================================================
*/
void *RtmpOsVmalloc(ULONG Size)
{
	return vmalloc(Size);
}

/*
========================================================================
Routine Description:
	vfree

Arguments:
	pMem			- the memory

Return Value:
	None

Note:
========================================================================
*/
void RtmpOsVfree(void *pMem)
{
	if (pMem != NULL)
		vfree(pMem);
}

/*
========================================================================
Routine Description:
	Get network interface name.

Arguments:
	pDev			- the device

Return Value:
	the name

Note:
========================================================================
*/
char *RtmpOsGetNetDevName(void *pDev)
{
	return ((struct net_device *) pDev)->name;
}

/*
========================================================================
Routine Description:
	Assign protocol to the packet.

Arguments:
	pPkt			- the packet

Return Value:
	None

Note:
========================================================================
*/
void RtmpOsPktProtocolAssign(PNDIS_PACKET pNetPkt)
{
	struct sk_buff *pRxPkt = RTPKT_TO_OSPKT(pNetPkt);
	pRxPkt->protocol = eth_type_trans(pRxPkt, pRxPkt->dev);
}


BOOLEAN RtmpOsStatsAlloc(
	IN void **ppStats,
	IN void **ppIwStats)
{
	os_alloc_mem(NULL, (UCHAR **) ppStats, sizeof (struct net_device_stats));
	if ((*ppStats) == NULL)
		return FALSE;
	memset((UCHAR *) *ppStats, 0, sizeof (struct net_device_stats));

	os_alloc_mem(NULL, (UCHAR **) ppIwStats, sizeof (struct iw_statistics));
	if ((*ppIwStats) == NULL) {
		kfree(*ppStats);
		return FALSE;
	}
	memset((UCHAR *)* ppIwStats, 0, sizeof (struct iw_statistics));

	return TRUE;
}

/*
========================================================================
Routine Description:
	Pass the received packet to OS.

Arguments:
	pPkt			- the packet

Return Value:
	None

Note:
========================================================================
*/
void RtmpOsPktRcvHandle(PNDIS_PACKET pNetPkt)
{
	struct sk_buff *pRxPkt = RTPKT_TO_OSPKT(pNetPkt);


	netif_rx(pRxPkt);
}


void RtmpOsTaskPidInit(RTMP_OS_PID *pPid)
{
	*pPid = THREAD_PID_INIT_VALUE;
}

/*
========================================================================
Routine Description:
	Get the network interface for the packet.

Arguments:
	pPkt			- the packet

Return Value:
	None

Note:
========================================================================
*/
struct net_device *RtmpOsPktNetDevGet(void *pPkt)
{
	return GET_OS_PKT_NETDEV(pPkt);
}


#ifdef IAPP_SUPPORT
/* Layer 2 Update frame to switch/bridge */
/* For any Layer2 devices, e.g., bridges, switches and other APs, the frame
   can update their forwarding tables with the correct port to reach the new
   location of the STA */
typedef struct GNU_PACKED _RT_IAPP_L2_UPDATE_FRAME {

	UCHAR DA[ETH_ALEN];	/* broadcast MAC address */
	UCHAR SA[ETH_ALEN];	/* the MAC address of the STA that has just associated
				   or reassociated */
	USHORT Len;		/* 8 octets */
	UCHAR DSAP;		/* null */
	UCHAR SSAP;		/* null */
	UCHAR Control;		/* reference to IEEE Std 802.2 */
	UCHAR XIDInfo[3];	/* reference to IEEE Std 802.2 */
} RT_IAPP_L2_UPDATE_FRAME, *PRT_IAPP_L2_UPDATE_FRAME;


PNDIS_PACKET RtmpOsPktIappMakeUp(
	IN struct net_device *pNetDev,
	IN u8 *pMac)
{
	RT_IAPP_L2_UPDATE_FRAME frame_body;
	INT size = sizeof (RT_IAPP_L2_UPDATE_FRAME);
	PNDIS_PACKET pNetBuf;

	if (pNetDev == NULL)
		return NULL;

	pNetBuf = RtmpOSNetPktAlloc(NULL, size);
	if (!pNetBuf) {
		DBGPRINT(RT_DEBUG_ERROR, ("Error! Can't allocate a skb.\n"));
		return NULL;
	}

	/* init the update frame body */
	memset(&frame_body, 0, size);

	memset(frame_body.DA, 0xFF, ETH_ALEN);
	memcpy(frame_body.SA, pMac, ETH_ALEN);

	frame_body.Len = OS_HTONS(ETH_ALEN);
	frame_body.DSAP = 0;
	frame_body.SSAP = 0x01;
	frame_body.Control = 0xAF;

	frame_body.XIDInfo[0] = 0x81;
	frame_body.XIDInfo[1] = 1;
	frame_body.XIDInfo[2] = 1 << 1;

	SET_OS_PKT_NETDEV(pNetBuf, pNetDev);
	skb_reserve(pNetBuf, 2);
	memcpy(skb_put(pNetBuf, size), &frame_body, size);
	return pNetBuf;
}
#endif /* IAPP_SUPPORT */


#ifdef RT_CFG80211_SUPPORT
/* all available channels */
UCHAR Cfg80211_Chan[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,

	/* 802.11 UNI / HyperLan 2 */
	36, 38, 40, 44, 46, 48, 52, 54, 56, 60, 62, 64,

	/* 802.11 HyperLan 2 */
	100, 104, 108, 112, 116, 118, 120, 124, 126, 128, 132, 134, 136,

	/* 802.11 UNII */
	140, 149, 151, 153, 157, 159, 161, 165, 167, 169, 171, 173,

	/* Japan */
	184, 188, 192, 196, 208, 212, 216,
};

/*
	Array of bitrates the hardware can operate with
	in this band. Must be sorted to give a valid "supported
	rates" IE, i.e. CCK rates first, then OFDM.

	For HT, assign MCS in another structure, ieee80211_sta_ht_cap.
*/
const struct ieee80211_rate Cfg80211_SupRate[12] = {
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 10,
		.hw_value = 0,
		.hw_value_short = 0,
	},
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 20,
		.hw_value = 1,
		.hw_value_short = 1,
	},
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 55,
		.hw_value = 2,
		.hw_value_short = 2,
	},
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 110,
		.hw_value = 3,
		.hw_value_short = 3,
	},
	{
		.flags = 0,
		.bitrate = 60,
		.hw_value = 4,
		.hw_value_short = 4,
	},
	{
		.flags = 0,
		.bitrate = 90,
		.hw_value = 5,
		.hw_value_short = 5,
	},
	{
		.flags = 0,
		.bitrate = 120,
		.hw_value = 6,
		.hw_value_short = 6,
	},
	{
		.flags = 0,
		.bitrate = 180,
		.hw_value = 7,
		.hw_value_short = 7,
	},
	{
		.flags = 0,
		.bitrate = 240,
		.hw_value = 8,
		.hw_value_short = 8,
	},
	{
		.flags = 0,
		.bitrate = 360,
		.hw_value = 9,
		.hw_value_short = 9,
	},
	{
		.flags = 0,
		.bitrate = 480,
		.hw_value = 10,
		.hw_value_short = 10,
	},
	{
		.flags = 0,
		.bitrate = 540,
		.hw_value = 11,
		.hw_value_short = 11,
	},
};

static const UINT32 CipherSuites[] = {
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
	WLAN_CIPHER_SUITE_CCMP,
};

/*
========================================================================
Routine Description:
	UnRegister MAC80211 Module.

Arguments:
	pCB				- CFG80211 control block pointer
	pNetDev			- Network device

Return Value:
	NONE

Note:
========================================================================
*/
void CFG80211OS_UnRegister(
	IN void *pCB,
	IN void *pNetDevOrg)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;
	struct net_device *pNetDev = (struct net_device *)pNetDevOrg;


	/* unregister */
	if (pCfg80211_CB->pCfg80211_Wdev != NULL)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("80211> unregister/free wireless device\n"));

		/*
			Must unregister, or you will suffer problem when you change
			regulatory domain by using iw.
		*/

#ifdef RFKILL_HW_SUPPORT
		wiphy_rfkill_stop_polling(pCfg80211_CB->pCfg80211_Wdev->wiphy);
#endif /* RFKILL_HW_SUPPORT */
		wiphy_unregister(pCfg80211_CB->pCfg80211_Wdev->wiphy);
		wiphy_free(pCfg80211_CB->pCfg80211_Wdev->wiphy);
		kfree(pCfg80211_CB->pCfg80211_Wdev);

		if (pCfg80211_CB->pCfg80211_Channels != NULL)
			kfree(pCfg80211_CB->pCfg80211_Channels);

		if (pCfg80211_CB->pCfg80211_Rates != NULL)
			kfree(pCfg80211_CB->pCfg80211_Rates);

		pCfg80211_CB->pCfg80211_Wdev = NULL;
		pCfg80211_CB->pCfg80211_Channels = NULL;
		pCfg80211_CB->pCfg80211_Rates = NULL;

		/* must reset to NULL; or kernel will panic in unregister_netdev */
		pNetDev->ieee80211_ptr = NULL;
		SET_NETDEV_DEV(pNetDev, NULL);
	}

	kfree(pCfg80211_CB);
}


/*
========================================================================
Routine Description:
	Initialize wireless channel in 2.4GHZ and 5GHZ.

Arguments:
	pAd				- WLAN control block pointer
	pWiphy			- WLAN PHY interface
	pChannels		- Current channel info
	pRates			- Current rate info

Return Value:
	TRUE			- init successfully
	FALSE			- init fail

Note:
	TX Power related:

	1. Suppose we can send power to 15dBm in the board.
	2. A value 0x0 ~ 0x1F for a channel. We will adjust it based on 15dBm/
		54Mbps. So if value == 0x07, the TX power of 54Mbps is 15dBm and
		the value is 0x07 in the EEPROM.
	3. Based on TX power value of 54Mbps/channel, adjust another value
		0x0 ~ 0xF for other data rate. (-6dBm ~ +6dBm)

	Other related factors:
	1. TX power percentage from UI/users;
	2. Maximum TX power limitation in the regulatory domain.
========================================================================
*/
BOOLEAN CFG80211_SupBandInit(
	IN void *pCB,
	IN CFG80211_BAND *pBandInfo,
	IN void *pWiphyOrg,
	IN void *pChannelsOrg,
	IN void *pRatesOrg)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;
	struct wiphy *pWiphy = (struct wiphy *)pWiphyOrg;
	struct ieee80211_channel *pChannels = (struct ieee80211_channel *)pChannelsOrg;
	struct ieee80211_rate *pRates = (struct ieee80211_rate *)pRatesOrg;
	struct ieee80211_supported_band *pBand;
	UINT32 NumOfChan, NumOfRate;
	UINT32 IdLoop;
	UINT32 CurTxPower;


	/* sanity check */
	if (pBandInfo->RFICType == 0)
		pBandInfo->RFICType = RFIC_24GHZ | RFIC_5GHZ;

	CFG80211DBG(RT_DEBUG_ERROR, ("80211> RFICType = %d\n",
				pBandInfo->RFICType));

	/* init */
	if (pBandInfo->RFICType & RFIC_5GHZ)
		NumOfChan = CFG80211_NUM_OF_CHAN_2GHZ + CFG80211_NUM_OF_CHAN_5GHZ;
	else
		NumOfChan = CFG80211_NUM_OF_CHAN_2GHZ;

	if (pBandInfo->FlgIsBMode == TRUE)
		NumOfRate = 4;
	else
		NumOfRate = 4 + 8;

	if (pChannels == NULL)
	{
		pChannels = kzalloc(sizeof(*pChannels) * NumOfChan, GFP_KERNEL);
		if (!pChannels)
		{
			DBGPRINT(RT_DEBUG_ERROR, ("80211> ieee80211_channel allocation fail!\n"));
			return FALSE;
		}
	}

	CFG80211DBG(RT_DEBUG_ERROR, ("80211> Number of channel = %d\n",
				CFG80211_NUM_OF_CHAN_5GHZ));

	if (pRates == NULL)
	{
		pRates = kzalloc(sizeof(*pRates) * NumOfRate, GFP_KERNEL);
		if (!pRates)
		{
			kfree(pChannels);
			DBGPRINT(RT_DEBUG_ERROR, ("80211> ieee80211_rate allocation fail!\n"));
			return FALSE;
		}
	}

	CFG80211DBG(RT_DEBUG_ERROR, ("80211> Number of rate = %d\n", NumOfRate));

	/* get TX power */
#ifdef SINGLE_SKU
	CurTxPower = pBandInfo->DefineMaxTxPwr; /* dBm */
#else
	CurTxPower = 0; /* unknown */
#endif /* SINGLE_SKU */

	CFG80211DBG(RT_DEBUG_ERROR, ("80211> CurTxPower = %d dBm\n", CurTxPower));

	/* init channel */
	for(IdLoop=0; IdLoop<NumOfChan; IdLoop++)
	{
		// @andy 2/24/2016
		// 2.6.39 is the first kernel with ieee80211_channel_to_frequency requiring two parameters
		// https://git.kernel.org/cgit/linux/kernel/git/stable/linux-stable.git/tree/include/net/cfg80211.h?h=linux-2.6.38.y
		// https://git.kernel.org/cgit/linux/kernel/git/stable/linux-stable.git/tree/include/net/cfg80211.h?h=linux-2.6.39.y
		// http://www.infty.nl/wordpress/2011/0/
		// https://github.com/coolshou/mt7610u/pull/1/files?diff=split
		pChannels[IdLoop].center_freq = \
					ieee80211_channel_to_frequency(Cfg80211_Chan[IdLoop],
						(IdLoop<CFG80211_NUM_OF_CHAN_2GHZ)?NL80211_BAND_2GHZ:NL80211_BAND_5GHZ);
		pChannels[IdLoop].hw_value = IdLoop;

		if (IdLoop < CFG80211_NUM_OF_CHAN_2GHZ)
			pChannels[IdLoop].max_power = CurTxPower;
		else
			pChannels[IdLoop].max_power = CurTxPower;

		pChannels[IdLoop].max_antenna_gain = 0xff;
	}

	/* init rate */
	for(IdLoop=0; IdLoop<NumOfRate; IdLoop++)
		memcpy(&pRates[IdLoop], &Cfg80211_SupRate[IdLoop], sizeof(*pRates));

	pBand = &pCfg80211_CB->Cfg80211_bands[NL80211_BAND_2GHZ];
	if (pBandInfo->RFICType & RFIC_24GHZ)
	{
		pBand->n_channels = CFG80211_NUM_OF_CHAN_2GHZ;
		pBand->n_bitrates = NumOfRate;
		pBand->channels = pChannels;
		pBand->bitrates = pRates;

#ifdef DOT11_N_SUPPORT
		/* for HT, assign pBand->ht_cap */
		pBand->ht_cap.ht_supported = true;
		pBand->ht_cap.cap = IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
					       IEEE80211_HT_CAP_SM_PS |
					       IEEE80211_HT_CAP_SGI_40 |
					       IEEE80211_HT_CAP_DSSSCCK40;
		pBand->ht_cap.ampdu_factor = 3; /* 2 ^ 16 */
		pBand->ht_cap.ampdu_density = pBandInfo->MpduDensity;

		memset(&pBand->ht_cap.mcs, 0, sizeof(pBand->ht_cap.mcs));
		CFG80211DBG(RT_DEBUG_ERROR,
					("80211> TxStream = %d\n", pBandInfo->TxStream));

		switch(pBandInfo->TxStream)
		{
			case 1:
			default:
				pBand->ht_cap.mcs.rx_mask[0] = 0xff;
				break;

			case 2:
				pBand->ht_cap.mcs.rx_mask[0] = 0xff;
				pBand->ht_cap.mcs.rx_mask[1] = 0xff;
				break;

			case 3:
				pBand->ht_cap.mcs.rx_mask[0] = 0xff;
				pBand->ht_cap.mcs.rx_mask[1] = 0xff;
				pBand->ht_cap.mcs.rx_mask[2] = 0xff;
				break;
		}

		pBand->ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;
#endif /* DOT11_N_SUPPORT */

		pWiphy->bands[NL80211_BAND_2GHZ] = pBand;
	}
	else
	{
		pWiphy->bands[NL80211_BAND_2GHZ] = NULL;
		pBand->channels = NULL;
		pBand->bitrates = NULL;
	}

	pBand = &pCfg80211_CB->Cfg80211_bands[NL80211_BAND_5GHZ];
	if (pBandInfo->RFICType & RFIC_5GHZ)
	{
		pBand->n_channels = CFG80211_NUM_OF_CHAN_5GHZ;
		pBand->n_bitrates = NumOfRate - 4;
		pBand->channels = &pChannels[CFG80211_NUM_OF_CHAN_2GHZ];
		pBand->bitrates = &pRates[4];

		/* for HT, assign pBand->ht_cap */
#ifdef DOT11_N_SUPPORT
		/* for HT, assign pBand->ht_cap */
		pBand->ht_cap.ht_supported = true;
		pBand->ht_cap.cap = IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
					       IEEE80211_HT_CAP_SM_PS |
					       IEEE80211_HT_CAP_SGI_40 |
					       IEEE80211_HT_CAP_DSSSCCK40;
		pBand->ht_cap.ampdu_factor = 3; /* 2 ^ 16 */
		pBand->ht_cap.ampdu_density = pBandInfo->MpduDensity;

		memset(&pBand->ht_cap.mcs, 0, sizeof(pBand->ht_cap.mcs));
		switch(pBandInfo->RxStream)
		{
			case 1:
			default:
				pBand->ht_cap.mcs.rx_mask[0] = 0xff;
				break;

			case 2:
				pBand->ht_cap.mcs.rx_mask[0] = 0xff;
				pBand->ht_cap.mcs.rx_mask[1] = 0xff;
				break;

			case 3:
				pBand->ht_cap.mcs.rx_mask[0] = 0xff;
				pBand->ht_cap.mcs.rx_mask[1] = 0xff;
				pBand->ht_cap.mcs.rx_mask[2] = 0xff;
				break;
		}

		pBand->ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;
#endif /* DOT11_N_SUPPORT */

		pWiphy->bands[NL80211_BAND_5GHZ] = pBand;
	}
	else
	{
		pWiphy->bands[NL80211_BAND_5GHZ] = NULL;
		pBand->channels = NULL;
		pBand->bitrates = NULL;
	}

	pCfg80211_CB->pCfg80211_Channels = pChannels;
	pCfg80211_CB->pCfg80211_Rates = pRates;

	return TRUE;
}


/*
========================================================================
Routine Description:
	Re-Initialize wireless channel/PHY in 2.4GHZ and 5GHZ.

Arguments:
	pCB				- CFG80211 control block pointer
	pBandInfo		- Band information

Return Value:
	TRUE			- re-init successfully
	FALSE			- re-init fail

Note:
	CFG80211_SupBandInit() is called in xx_probe().
	But we do not have complete chip information in xx_probe() so we
	need to re-init bands in xx_open().
========================================================================
*/
BOOLEAN CFG80211OS_SupBandReInit(
	IN void *pCB,
	IN CFG80211_BAND *pBandInfo)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;
	struct wiphy *pWiphy;


	if ((pCfg80211_CB == NULL) || (pCfg80211_CB->pCfg80211_Wdev == NULL))
		return FALSE;

	pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;

	if (pWiphy != NULL)
	{
		CFG80211DBG(RT_DEBUG_ERROR, ("80211> re-init bands...\n"));

		/* re-init bands */
		CFG80211_SupBandInit(pCfg80211_CB, pBandInfo, pWiphy,
							pCfg80211_CB->pCfg80211_Channels,
							pCfg80211_CB->pCfg80211_Rates);

		/* re-init PHY */
		pWiphy->rts_threshold = pBandInfo->RtsThreshold;
		pWiphy->frag_threshold = pBandInfo->FragmentThreshold;
		pWiphy->retry_short = pBandInfo->RetryMaxCnt & 0xff;
		pWiphy->retry_long = (pBandInfo->RetryMaxCnt & 0xff00)>>8;

		return TRUE;
	}

	return FALSE;
}


/*
========================================================================
Routine Description:
	Hint to the wireless core a regulatory domain from driver.

Arguments:
	pAd				- WLAN control block pointer
	pCountryIe		- pointer to the country IE
	CountryIeLen	- length of the country IE

Return Value:
	NONE

Note:
	Must call the function in kernel thread.
========================================================================
*/
void CFG80211OS_RegHint(
	IN void *pCB,
	IN UCHAR *pCountryIe,
	IN ULONG CountryIeLen)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;


	CFG80211DBG(RT_DEBUG_ERROR,
			("crda> regulatory domain hint: %c%c\n",
			pCountryIe[0], pCountryIe[1]));

	if ((pCfg80211_CB->pCfg80211_Wdev == NULL) || (pCountryIe == NULL))
	{
		CFG80211DBG(RT_DEBUG_ERROR, ("crda> regulatory domain hint not support!\n"));
		return;
	}

	/* hints a country IE as a regulatory domain "without" channel/power info. */
/*	regulatory_hint(pCfg80211_CB->pMac80211_Hw->wiphy, pCountryIe); */
	regulatory_hint(pCfg80211_CB->pCfg80211_Wdev->wiphy, (const char *)pCountryIe);
}


/*
========================================================================
Routine Description:
	Hint to the wireless core a regulatory domain from country element.

Arguments:
	pAdCB			- WLAN control block pointer
	pCountryIe		- pointer to the country IE
	CountryIeLen	- length of the country IE

Return Value:
	NONE

Note:
	Must call the function in kernel thread.
========================================================================
*/
void CFG80211OS_RegHint11D(
	IN void *pCB,
	IN UCHAR *pCountryIe,
	IN ULONG CountryIeLen)
{
	/* no regulatory_hint_11d() in 2.6.32 */
}


BOOLEAN CFG80211OS_BandInfoGet(
	IN void *pCB,
	IN void *pWiphyOrg,
	OUT void **ppBand24,
	OUT void **ppBand5)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;
	struct wiphy *pWiphy = (struct wiphy *)pWiphyOrg;


	if (pWiphy == NULL)
	{
		if ((pCfg80211_CB != NULL) && (pCfg80211_CB->pCfg80211_Wdev != NULL))
			pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;
	}

	if (pWiphy == NULL)
		return FALSE;

	*ppBand24 = pWiphy->bands[NL80211_BAND_2GHZ];
	*ppBand5 = pWiphy->bands[NL80211_BAND_5GHZ];
	return TRUE;
}


UINT32 CFG80211OS_ChanNumGet(
	IN void 					*pCB,
	IN void 					*pWiphyOrg,
	IN UINT32					IdBand)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;
	struct wiphy *pWiphy = (struct wiphy *)pWiphyOrg;


	if (pWiphy == NULL)
	{
		if ((pCfg80211_CB != NULL) && (pCfg80211_CB->pCfg80211_Wdev != NULL))
			pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;
	}

	if (pWiphy == NULL)
		return 0;

	if (pWiphy->bands[IdBand] != NULL)
		return pWiphy->bands[IdBand]->n_channels;

	return 0;
}


BOOLEAN CFG80211OS_ChanInfoGet(
	IN void 					*pCB,
	IN void 					*pWiphyOrg,
	IN UINT32					IdBand,
	IN UINT32					IdChan,
	OUT UINT32					*pChanId,
	OUT UINT32					*pPower,
	OUT BOOLEAN					*pFlgIsRadar)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;
	struct wiphy *pWiphy = (struct wiphy *)pWiphyOrg;
	struct ieee80211_supported_band *pSband;
	struct ieee80211_channel *pChan;


	if (pWiphy == NULL)
	{
		if ((pCfg80211_CB != NULL) && (pCfg80211_CB->pCfg80211_Wdev != NULL))
			pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;
	}

	if (pWiphy == NULL)
		return FALSE;

	pSband = pWiphy->bands[IdBand];
	pChan = &pSband->channels[IdChan];

	*pChanId = ieee80211_frequency_to_channel(pChan->center_freq);

	if (pChan->flags & IEEE80211_CHAN_DISABLED)
	{
		CFG80211DBG(RT_DEBUG_ERROR,
					("Chan %03d (frq %d):\tnot allowed!\n",
					(*pChanId), pChan->center_freq));
		return FALSE;
	}

	*pPower = pChan->max_power;

	if (pChan->flags & IEEE80211_CHAN_RADAR)
		*pFlgIsRadar = TRUE;
	else
		*pFlgIsRadar = FALSE;

	return TRUE;
}


/*
========================================================================
Routine Description:
	Initialize a channel information used in scan inform.

Arguments:

Return Value:
	TRUE		- Successful
	FALSE		- Fail

Note:
========================================================================
*/
BOOLEAN CFG80211OS_ChanInfoInit(
	IN void 					*pCB,
	IN UINT32					InfoIndex,
	IN UCHAR					ChanId,
	IN UCHAR					MaxTxPwr,
	IN BOOLEAN					FlgIsNMode,
	IN BOOLEAN					FlgIsBW20M)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;
	struct ieee80211_channel *pChan;


	if (InfoIndex >= MAX_NUM_OF_CHANNELS)
		return FALSE;

	pChan = (struct ieee80211_channel *)&(pCfg80211_CB->ChanInfo[InfoIndex]);
	memset(pChan, 0, sizeof(*pChan));

	if (ChanId > 14)
		pChan->band = NL80211_BAND_5GHZ;
	else
		pChan->band = NL80211_BAND_2GHZ;

	pChan->center_freq = ieee80211_channel_to_frequency(ChanId, pChan->band);


	/* no use currently in 2.6.30 */
/*	if (ieee80211_is_beacon(((struct ieee80211_mgmt *)pFrame)->frame_control)) */
/*		pChan->beacon_found = 1; */

	return TRUE;
}


/*
========================================================================
Routine Description:
	Inform us that a scan is got.

Arguments:
	pAdCB				- WLAN control block pointer

Return Value:
	NONE

Note:
	Call RT_CFG80211_SCANNING_INFORM, not CFG80211_Scaning
========================================================================
*/
void CFG80211OS_Scaning(
	IN void 					*pCB,
	IN UINT32					ChanId,
	IN UCHAR					*pFrame,
	IN UINT32					FrameLen,
	IN INT32					RSSI,
	IN BOOLEAN					FlgIsNMode,
	IN u8					BW)
{
#ifdef CONFIG_STA_SUPPORT
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;
	UINT32 IdChan;
	UINT32 CenFreq;

	/* get channel information */
	CenFreq = ieee80211_channel_to_frequency(ChanId,
		(ChanId<CFG80211_NUM_OF_CHAN_2GHZ)?NL80211_BAND_2GHZ:NL80211_BAND_5GHZ);

	for(IdChan=0; IdChan<MAX_NUM_OF_CHANNELS; IdChan++)
	{
		if (pCfg80211_CB->ChanInfo[IdChan].center_freq == CenFreq)
			break;
	}
	if (IdChan >= MAX_NUM_OF_CHANNELS)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("80211> Can not find any chan info!\n"));
		return;
	}

	/* inform 80211 a scan is got */
	/* we can use cfg80211_inform_bss in 2.6.31, it is easy more than the one */
	/* in cfg80211_inform_bss_frame(), it will memcpy pFrame but pChan */
	cfg80211_inform_bss_frame(pCfg80211_CB->pCfg80211_Wdev->wiphy,
								&pCfg80211_CB->ChanInfo[IdChan],
								(struct ieee80211_mgmt *)pFrame,
								FrameLen,
								RSSI,
								GFP_ATOMIC);

	CFG80211DBG(RT_DEBUG_ERROR, ("80211> cfg80211_inform_bss_frame\n"));
#endif /* CONFIG_STA_SUPPORT */
}


/*
========================================================================
Routine Description:
	Inform us that scan ends.

Arguments:
	pAdCB			- WLAN control block pointer
	FlgIsAborted	- 1: scan is aborted

Return Value:
	NONE

Note:
========================================================================
*/
void CFG80211OS_ScanEnd(
	IN void *pCB,
	IN BOOLEAN FlgIsAborted)
{
#ifdef CONFIG_STA_SUPPORT
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;


	CFG80211DBG(RT_DEBUG_ERROR, ("80211> cfg80211_scan_done\n"));
	cfg80211_scan_done(pCfg80211_CB->pCfg80211_ScanReq, FlgIsAborted);
#endif /* CONFIG_STA_SUPPORT */
}


/*
========================================================================
Routine Description:
	Inform CFG80211 about association status.

Arguments:
	pAdCB			- WLAN control block pointer
	pBSSID			- the BSSID of the AP
	pReqIe			- the element list in the association request frame
	ReqIeLen		- the request element length
	pRspIe			- the element list in the association response frame
	RspIeLen		- the response element length
	FlgIsSuccess	- 1: success; otherwise: fail

Return Value:
	None

Note:
========================================================================
*/
void CFG80211OS_ConnectResultInform(
	IN void *pCB,
	IN UCHAR *pBSSID,
	IN UCHAR *pReqIe,
	IN UINT32 ReqIeLen,
	IN UCHAR *pRspIe,
	IN UINT32 RspIeLen,
	IN UCHAR FlgIsSuccess)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;


	if ((pCfg80211_CB->pCfg80211_Wdev->netdev == NULL) || (pBSSID == NULL))
		return;

	if (FlgIsSuccess)
	{
		cfg80211_connect_result(pCfg80211_CB->pCfg80211_Wdev->netdev,
								pBSSID,
								pReqIe,
								ReqIeLen,
								pRspIe,
								RspIeLen,
								WLAN_STATUS_SUCCESS,
								GFP_KERNEL);
	}
	else
	{
		cfg80211_connect_result(pCfg80211_CB->pCfg80211_Wdev->netdev,
								pBSSID,
								NULL, 0, NULL, 0,
								WLAN_STATUS_UNSPECIFIED_FAILURE,
								GFP_KERNEL);
	}
}
#endif /* RT_CFG80211_SUPPORT */


/*
========================================================================
Routine Description:
	Flush a data cache line.

Arguments:
	AddrStart		- the start address
	Size			- memory size

Return Value:
	None

Note:
========================================================================
*/
void RtmpOsDCacheFlush(
	IN ULONG AddrStart,
	IN ULONG Size)
{
	RTMP_UTIL_DCACHE_FLUSH(AddrStart, Size);
}


/*
========================================================================
Routine Description:
	Assign private data pointer to the network interface.

Arguments:
	pDev			- the device
	pPriv			- the pointer

Return Value:
	None

Note:
========================================================================
*/
void RtmpOsSetNetDevPriv(struct net_device *pDev, struct rtmp_adapter *pPriv)
{
	struct dev_priv_info *pDevInfo = NULL;

	pDevInfo = (struct dev_priv_info *) netdev_priv(pDev);
	if (pDevInfo == NULL)
	{
		os_alloc_mem(NULL, (UCHAR **)&pDevInfo, sizeof(*pDevInfo));
		if (pDevInfo == NULL)
			return;
	}

	pDevInfo->pPriv = pPriv;
	pDevInfo->priv_flags = 0;

	pDev->ml_priv = pDevInfo;
}


/*
========================================================================
Routine Description:
	Get private data pointer from the network interface.

Arguments:
	pDev			- the device
	pPriv			- the pointer

Return Value:
	None

Note:
========================================================================
*/
struct rtmp_adapter *RtmpOsGetNetDevPriv(struct net_device *pDev)
{
	struct dev_priv_info *pDevInfo = NULL;

	pDevInfo = (struct dev_priv_info *) netdev_priv(pDev);
	if (pDevInfo != NULL)
		return pDevInfo->pPriv;
	return NULL;
}


/*
========================================================================
Routine Description:
	Get private flags from the network interface.

Arguments:
	pDev			- the device

Return Value:
	pPriv			- the pointer

Note:
========================================================================
*/
u32 RtmpDevPrivFlagsGet(struct net_device *pDev)
{
	struct dev_priv_info *pDevInfo = NULL;

	pDevInfo = (struct dev_priv_info *) netdev_priv(pDev);
	if (pDevInfo != NULL)
		return pDevInfo->priv_flags;
	return 0;
}


/*
========================================================================
Routine Description:
	Get private flags from the network interface.

Arguments:
	pDev			- the device

Return Value:
	pPriv			- the pointer

Note:
========================================================================
*/
void RtmpDevPrivFlagsSet(struct net_device *pDev, u32 PrivFlags)
{
	struct dev_priv_info *pDevInfo = NULL;


	pDevInfo = (struct dev_priv_info *) netdev_priv(pDev);
	if (pDevInfo != NULL)
		pDevInfo->priv_flags = PrivFlags;
}

void OS_SPIN_LOCK_IRQSAVE(NDIS_SPIN_LOCK *lock, unsigned long *flags)
{
	spin_lock_irqsave((spinlock_t *)(lock), *flags);
}

void OS_SPIN_UNLOCK_IRQRESTORE(NDIS_SPIN_LOCK *lock, unsigned long *flags)
{
	spin_unlock_irqrestore((spinlock_t *)(lock), *flags);
}

void OS_SPIN_LOCK_IRQ(NDIS_SPIN_LOCK *lock)
{
	spin_lock_irq((spinlock_t *)(lock));
}

void OS_SPIN_UNLOCK_IRQ(NDIS_SPIN_LOCK *lock)
{
	spin_unlock_irq((spinlock_t *)(lock));
}

int OS_TEST_BIT(int bit, unsigned long *flags)
{
	return test_bit(bit, flags);
}

void OS_SET_BIT(int bit, unsigned long *flags)
{
	set_bit(bit, flags);
}

void OS_CLEAR_BIT(int bit, unsigned long *flags)
{
	clear_bit(bit, flags);
}


void RtmpOSFSInfoChange(RTMP_OS_FS_INFO *pOSFSInfoOrg, BOOLEAN bSet)
{
	__RtmpOSFSInfoChange(pOSFSInfoOrg, bSet);
}


/* timeout -- ms */
void RTMP_SetPeriodicTimer(NDIS_MINIPORT_TIMER *pTimerOrg, unsigned long timeout)
{
	__RTMP_SetPeriodicTimer(pTimerOrg, timeout);
}


/* convert NdisMInitializeTimer --> RTMP_OS_Init_Timer */
void RTMP_OS_Init_Timer(
	void *pReserved,
	NDIS_MINIPORT_TIMER *pTimerOrg,
	TIMER_FUNCTION function,
	void *data,
	LIST_HEADER *pTimerList)
{
	__RTMP_OS_Init_Timer(pReserved, pTimerOrg, function, data);
}


void RTMP_OS_Add_Timer(NDIS_MINIPORT_TIMER *pTimerOrg, unsigned long timeout)
{
	__RTMP_OS_Add_Timer(pTimerOrg, timeout);
}


void RTMP_OS_Mod_Timer(NDIS_MINIPORT_TIMER *pTimerOrg, unsigned long timeout)
{
	__RTMP_OS_Mod_Timer(pTimerOrg, timeout);
}


void RTMP_OS_Del_Timer(NDIS_MINIPORT_TIMER *pTimerOrg, BOOLEAN *pCancelled)
{
	__RTMP_OS_Del_Timer(pTimerOrg, pCancelled);
}


void RTMP_OS_Release_Timer(NDIS_MINIPORT_TIMER *pTimerOrg)
{
	__RTMP_OS_Release_Timer(pTimerOrg);
}


NDIS_STATUS RtmpOSTaskKill(RTMP_OS_TASK *pTask)
{
	return __RtmpOSTaskKill(pTask);
}


INT RtmpOSTaskNotifyToExit(RTMP_OS_TASK *pTask)
{
	return __RtmpOSTaskNotifyToExit(pTask);
}


void RtmpOSTaskCustomize(RTMP_OS_TASK *pTask)
{
	__RtmpOSTaskCustomize(pTask);
}


NDIS_STATUS RtmpOSTaskAttach(
	RTMP_OS_TASK *pTask,
	RTMP_OS_TASK_CALLBACK fn,
	ULONG arg)
{
	return __RtmpOSTaskAttach(pTask, fn, arg);
}


NDIS_STATUS RtmpOSTaskInit(
	RTMP_OS_TASK *pTask,
	char *pTaskName,
	void *pPriv,
	LIST_HEADER *pTaskList,
	LIST_HEADER *pSemList)
{
	return __RtmpOSTaskInit(pTask, pTaskName, pPriv, pSemList);
}


BOOLEAN RtmpOSTaskWait(void *pReserved, RTMP_OS_TASK * pTask, INT32 *pStatus)
{
	return __RtmpOSTaskWait(pReserved, pTask, pStatus);
}


void RtmpOsTaskWakeUp(RTMP_OS_TASK *pTask)
{
#ifdef KTHREAD_SUPPORT
	WAKE_UP(pTask);
#else
	RTMP_SEM_EVENT_UP(&pTask->taskSema);
#endif
}

