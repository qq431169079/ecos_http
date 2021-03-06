/*
 * Dongle BUS interface for USB, SDIO, SPI, etc.
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: dbus.c,v 1.66.2.65 2011-01-27 11:27:46 Exp $
 */

#include "osl.h"
#include "dbus.h"
#include "bcmdevs.h"
#ifdef BCMEMBEDIMAGE
#include BCMEMBEDIMAGE
#elif defined(BCM_DNGL_EMBEDIMAGE)
#ifdef EMBED_IMAGE_4322
#include "rtecdc_4322.h"
#endif /* EMBED_IMAGE_4322 */
#if defined(EMBED_IMAGE_43236b1)
#include "rtecdc_43236b1.h"
#endif /* EMBED_IMAGE_43236b1 */
#ifdef EMBED_IMAGE_4319usb
#include "rtecdc_4319usb.h"
#endif /* EMBED_IMAGE_4319usb */
#ifdef EMBED_IMAGE_4325sd
#include "rtecdc_4325sd.h"
#endif /* EMBED_IMAGE_4325sd */
#ifdef EMBED_IMAGE_4319sd
#include "rtecdc_4319sd.h"
#endif /* EMBED_IMAGE_4319sd */
#ifdef EMBED_IMAGE_43237a0
#include "rtecdc_43237a0.h"
#endif /* EMBED_IMAGE_43237a0 */
#ifdef EMBED_IMAGE_GENERIC
#include "rtecdc.h"
#endif
#endif /* BCMEMBEDIMAGE */

#include <bcmutils.h>
#if (defined(BCM_DNGL_EMBEDIMAGE) || defined(BCMEMBEDIMAGE))
#include <bcmsrom_fmt.h>
#if (defined(NDIS) || defined(USBAP) || defined(WL_NVRAM_FILE))
#include <trxhdr.h>
#include <usbrdl.h>
#include <bcmendian.h>
#include <sbpcmcia.h>
#endif /* (defined(NDIS) || defined(USBAP) || defined (WL_NVRAM_FILE)) */

#ifdef WL_FW_DECOMP
#include <trxhdr.h>
#include <usbrdl.h>
#include <zutil.h>
#include <bcmendian.h>
/* zlib file format field ids etc from gzio.c */
#define Z_DEFLATED   8
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */
#endif /* WL_FW_DECOMP */

#endif /* defined(BCM_DNGL_EMBEDIMAGE) || defined(BCMEMBEDIMAGE) */

/* General info for all BUS */
typedef struct dbus_irbq {
	dbus_irb_t *head;
	dbus_irb_t *tail;
	int cnt;
} dbus_irbq_t;

/* This private structure dbus_info_t is also declared in dbus_usb_linux.c.
 * All the fields must be consistent in both declarations.
 */
typedef struct {
	dbus_pub_t pub; /* MUST BE FIRST */

	void *cbarg;
	dbus_callbacks_t *cbs;
	void *bus_info;
	dbus_intf_t *drvintf;
	uint8 *fw;
	int fwlen;
	uint32 errmask;
	int rx_low_watermark;
	int tx_low_watermark;
	bool txoff;
	bool txoverride;
	bool rxoff;
	bool tx_timer_ticking;

	dbus_irbq_t *rx_q;
	dbus_irbq_t *tx_q;

#ifdef BCMDBG
	int *txpend_q_hist;
	int *rxpend_q_hist;
#endif /* BCMDBG */
#ifdef EHCI_FASTPATH_RX
	atomic_t rx_outstanding;
#endif
	uint8 *nvram;
	int	nvram_len;
	uint8 *image;	/* buffer for combine fw and nvram */
	int image_len;
	uint8 *orig_fw;
	int origfw_len;
	int decomp_memsize;
} dbus_info_t;

struct exec_parms {
union {
	/* Can consolidate same params, if need be, but this shows
	 * group of parameters per function
	 */
	struct {
		dbus_irbq_t *q;
		dbus_irb_t *b;
	} qenq;

	struct {
		dbus_irbq_t *q;
	} qdeq;
};
};

#define DBUS_NTXQ	1024
#define DBUS_NRXQ	1024

#define EXEC_RXLOCK(info, fn, a) \
	info->drvintf->exec_rxlock(dbus_info->bus_info, ((exec_cb_t)fn), ((struct exec_parms *) a))

#define EXEC_TXLOCK(info, fn, a) \
	info->drvintf->exec_txlock(dbus_info->bus_info, ((exec_cb_t)fn), ((struct exec_parms *) a))

#if defined(EHCI_FASTPATH_TX) || defined(EHCI_FASTPATH_RX)
#include <linux/usb.h>
#endif /* EHCI_FASTPATH_TX || EHCI_FASTPATH_RX */

/*
 * Callbacks common for all BUS
 */
static void dbus_if_send_irb_timeout(void *handle, dbus_irb_tx_t *txirb);
static void dbus_if_send_irb_complete(void *handle, dbus_irb_tx_t *txirb, int status);
static void dbus_if_recv_irb_complete(void *handle, dbus_irb_rx_t *rxirb, int status);
static void dbus_if_errhandler(void *handle, int err);
static void dbus_if_ctl_complete(void *handle, int type, int status);
static void dbus_if_state_change(void *handle, int state);
static void *dbus_if_pktget(void *handle, uint len, bool send);
static void dbus_if_pktfree(void *handle, void *p, bool send);
static struct dbus_irb *dbus_if_getirb(void *cbarg, bool send);
static void dbus_if_rxerr_indicate(void *handle, bool on);

static dbus_intf_callbacks_t dbus_intf_cbs = {
	dbus_if_send_irb_timeout,
	dbus_if_send_irb_complete,
	dbus_if_recv_irb_complete,
	dbus_if_errhandler,
	dbus_if_ctl_complete,
	dbus_if_state_change,
	NULL,			/* isr */
	NULL,			/* dpc */
	NULL,			/* watchdog */
	dbus_if_pktget,
	dbus_if_pktfree,
	dbus_if_getirb,
	dbus_if_rxerr_indicate
};

/*
 * Need global for probe() and disconnect() since
 * attach() is not called at probe and detach()
 * can be called inside disconnect()
 */
static dbus_intf_t *g_busintf = NULL;
static probe_cb_t probe_cb = NULL;
static disconnect_cb_t disconnect_cb = NULL;
static void *probe_arg = NULL;
static void *disc_arg = NULL;

#if (defined(BCM_DNGL_EMBEDIMAGE) || defined(BCMEMBEDIMAGE)) && (defined(WLC_HIGH) && \
	!defined(WLC_LOW))
char BCMATTACHDATA(mfgsromvars)[VARS_MAX];
int defvarslen = 0;
int8 *jumbonvram = NULL;
#endif /* (defined(BCM_DNGL_EMBEDIMAGE) || defined(BCMEMBEDIMAGE)) && defined(NDIS) */

#if defined(EHCI_FASTPATH_TX) || defined(EHCI_FASTPATH_RX)
void  dbus_flowctrl_tx(dbus_info_t *dbus_info, bool on);
#else
static void  dbus_flowctrl_tx(dbus_info_t *dbus_info, bool on);
#endif
static void* q_enq(dbus_irbq_t *q, dbus_irb_t *b);
static void* q_enq_exec(struct exec_parms *args);
static dbus_irb_t*q_deq(dbus_irbq_t *q);
static void* q_deq_exec(struct exec_parms *args);
static int   dbus_tx_timer_init(dbus_info_t *dbus_info);
static int   dbus_tx_timer_start(dbus_info_t *dbus_info, uint timeout);
static int   dbus_tx_timer_stop(dbus_info_t *dbus_info);
static int   dbus_irbq_init(dbus_info_t *dbus_info, dbus_irbq_t *q, int nq, int size_irb);
static int   dbus_irbq_deinit(dbus_info_t *dbus_info, dbus_irbq_t *q, int size_irb);
static int   dbus_rxirbs_fill(dbus_info_t *dbus_info);
static int   dbus_send_irb(const dbus_pub_t *pub, uint8 *buf, int len, void *pkt, void *info);
static void  dbus_disconnect(void *handle);
static void *dbus_probe(void *arg, const char *desc, uint32 bustype, uint32 hdrlen);

/* zlib support - store the global osh handle */
static osl_t *osl_handle = NULL;
void *dbus_zlib_calloc(int num, int size);
void dbus_zlib_free(void *ptr);

#if (defined(BCM_DNGL_EMBEDIMAGE) || defined(BCMEMBEDIMAGE)) && (defined(NDIS) || \
	defined(USBAP)|| defined(WL_NVRAM_FILE)) && (defined(WLC_HIGH) && !defined(WLC_LOW))
extern char * dngl_firmware;
extern unsigned int dngl_fwlen;
static int dbus_get_nvram(dbus_info_t *dbus_info);
#endif /* (defined(BCM_DNGL_EMBEDIMAGE) || defined(BCMEMBEDIMAGE)) && defined(NDIS) */

#if (defined(BCM_DNGL_EMBEDIMAGE) || defined(BCMEMBEDIMAGE)) && defined(WL_FW_DECOMP)
static int dbus_zlib_decomp(dbus_info_t *dbus_info);
#endif

/* function */
#if defined(EHCI_FASTPATH_TX) || defined(EHCI_FASTPATH_RX)
void
#else
static void
#endif
dbus_flowctrl_tx(dbus_info_t *dbus_info, bool on)
{
	if (dbus_info == NULL)
		return;

	DBUSTRACE(("%s on %d\n", __FUNCTION__, on));

	if (dbus_info->txoff == on)
		return;

	dbus_info->txoff = on;

	if (dbus_info->cbs && dbus_info->cbs->txflowcontrol)
		dbus_info->cbs->txflowcontrol(dbus_info->cbarg, on);
}

static void
dbus_if_rxerr_indicate(void *handle, bool on)
{
	dbus_info_t *dbus_info = (dbus_info_t *) handle;

	DBUSTRACE(("%s, on %d\n", __FUNCTION__, on));

	if (dbus_info == NULL)
		return;

	if (dbus_info->txoverride == on)
		return;

	dbus_info->txoverride = on;

	if (!on)
		dbus_rxirbs_fill(dbus_info);

}

/*
 * q_enq()/q_deq() are executed with protection
 * via exec_rxlock()/exec_txlock()
 */
static void*
q_enq(dbus_irbq_t *q, dbus_irb_t *b)
{
	ASSERT(q->tail != b);
	ASSERT(b->next == NULL);
	b->next = NULL;
	if (q->tail) {
		q->tail->next = b;
		q->tail = b;
	} else
		q->head = q->tail = b;

	q->cnt++;

	return b;
}

static void*
q_enq_exec(struct exec_parms *args)
{
	return q_enq(args->qenq.q, args->qenq.b);
}

static dbus_irb_t*
q_deq(dbus_irbq_t *q)
{
	dbus_irb_t *b;

	b = q->head;
	if (b) {
		q->head = q->head->next;
		b->next = NULL;

		if (q->head == NULL)
			q->tail = q->head;

		q->cnt--;
	}
	return b;
}

static void*
q_deq_exec(struct exec_parms *args)
{
	return q_deq(args->qdeq.q);
}

static int
dbus_tx_timer_init(dbus_info_t *dbus_info)
{
	if (dbus_info && dbus_info->drvintf && dbus_info->drvintf->tx_timer_init)
		return dbus_info->drvintf->tx_timer_init(dbus_info->bus_info);
	else
		return DBUS_ERR;
}

static int
dbus_tx_timer_start(dbus_info_t *dbus_info, uint timeout)
{
	if (dbus_info == NULL)
		return DBUS_ERR;

	if (dbus_info->tx_timer_ticking)
		return DBUS_OK;

	if (dbus_info->drvintf && dbus_info->drvintf->tx_timer_start) {
		if (dbus_info->drvintf->tx_timer_start(dbus_info->bus_info, timeout) == DBUS_OK) {
			dbus_info->tx_timer_ticking = TRUE;
			return DBUS_OK;
		}
	}

	return DBUS_ERR;
}

static int
dbus_tx_timer_stop(dbus_info_t *dbus_info)
{
	if (dbus_info == NULL)
		return DBUS_ERR;

	if (!dbus_info->tx_timer_ticking)
		return DBUS_OK;

	if (dbus_info->drvintf && dbus_info->drvintf->tx_timer_stop) {
		if (dbus_info->drvintf->tx_timer_stop(dbus_info->bus_info) == DBUS_OK) {
			dbus_info->tx_timer_ticking = FALSE;
			return DBUS_OK;
		}
	}

	return DBUS_ERR;
}

static int
dbus_irbq_init(dbus_info_t *dbus_info, dbus_irbq_t *q, int nq, int size_irb)
{
	int i;
	dbus_irb_t *irb;

	ASSERT(q);
	ASSERT(dbus_info);

	for (i = 0; i < nq; i++) {
		/* MALLOC dbus_irb_tx or dbus_irb_rx, but cast to simple dbus_irb_t linkedlist */
		irb = (dbus_irb_t *) MALLOC(dbus_info->pub.osh, size_irb);
		if (irb == NULL) {
			ASSERT(irb);
			return DBUS_ERR;
		}
		bzero(irb, size_irb);

		/* q_enq() does not need to go through EXEC_xxLOCK() during init() */
		q_enq(q, irb);
	}

	return DBUS_OK;
}

static int
dbus_irbq_deinit(dbus_info_t *dbus_info, dbus_irbq_t *q, int size_irb)
{
	dbus_irb_t *irb;

	ASSERT(q);
	ASSERT(dbus_info);

	/* q_deq() does not need to go through EXEC_xxLOCK()
	 * during deinit(); all callbacks are stopped by this time
	 */
	while ((irb = q_deq(q)) != NULL) {
		MFREE(dbus_info->pub.osh, irb, size_irb);
	}

	if (q->cnt)
		DBUSERR(("deinit: q->cnt=%d > 0\n", q->cnt));
	return DBUS_OK;
}

static int
dbus_rxirbs_fill(dbus_info_t *dbus_info)
{
	int err = DBUS_OK;

#ifdef EHCI_FASTPATH_RX
	while (atomic_read(&dbus_info->rx_outstanding) < 100) /* TODO: Improve constant */
	{
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
		/* NOCOPY force new packet allocation */
		optimize_submit_rx_request(&dbus_info->pub, 1, NULL, NULL);
#else
		/* Copy mode - allocate own buffer to be reused */
		void *buf = MALLOC(dbus_info->pub.osh, DBUS_BUFFER_SIZE_RX);
		optimize_submit_rx_request(&dbus_info->pub, 1, NULL, buf);
#endif /* BCM_RPC_NOCOPY || BCM_RPC_RXNOCOPY */
		atomic_inc(&dbus_info->rx_outstanding);
	}
#else /* EHCI_FASTPATH_RX */

	dbus_irb_rx_t *rxirb;
	struct exec_parms args;

	ASSERT(dbus_info);
	if (dbus_info->pub.busstate != DBUS_STATE_UP) {
		DBUSERR(("dbus_rxirbs_fill: DBUS not up \n"));
		return DBUS_ERR;
	} else if (!dbus_info->drvintf || (dbus_info->drvintf->recv_irb == NULL)) {
		/* Lower edge bus interface does not support recv_irb().
		 * No need to pre-submit IRBs in this case.
		 */
		return DBUS_ERR;
	}

	/* The dongle recv callback is freerunning without lock. So multiple callbacks(and this
	 *  refill) can run in parallel. While the rxoff condition is triggered outside,
	 *  below while loop has to check and abort posting more to avoid RPC rxq overflow.
	 */
	args.qdeq.q = dbus_info->rx_q;
	while ((!dbus_info->rxoff) &&
	       (rxirb = (EXEC_RXLOCK(dbus_info, q_deq_exec, &args))) != NULL) {
		err = dbus_info->drvintf->recv_irb(dbus_info->bus_info, rxirb);
		if (err == DBUS_ERR_RXDROP) {
			/* Add the the free rxirb back to the queue
			 * and wait till later
			 */
			bzero(rxirb, sizeof(dbus_irb_rx_t));
			args.qenq.q = dbus_info->rx_q;
			args.qenq.b = (dbus_irb_t *) rxirb;
			EXEC_RXLOCK(dbus_info, q_enq_exec, &args);
			break;
		}
	}
#endif /* EHCI_FASTPATH_RX */
	return err;
}

void
dbus_flowctrl_rx(const dbus_pub_t *pub, bool on)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;

	if (dbus_info == NULL)
		return;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dbus_info->rxoff == on)
		return;

	dbus_info->rxoff = on;

	if (dbus_info->pub.busstate == DBUS_STATE_UP) {
		if (!on) {
			/* post more irbs, resume rx if necessary */
			dbus_rxirbs_fill(dbus_info);
			if (dbus_info && dbus_info->drvintf->recv_resume) {
				dbus_info->drvintf->recv_resume(dbus_info->bus_info);
			}
		} else {
			/* ??? cancell posted irbs first */

			if (dbus_info && dbus_info->drvintf->recv_stop) {
				dbus_info->drvintf->recv_stop(dbus_info->bus_info);
			}
		}
	}
}

/* Handles both sending of a buffer or a pkt */
static int
dbus_send_irb(const dbus_pub_t *pub, uint8 *buf, int len, void *pkt, void *info)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;
	int err = DBUS_OK;
#ifndef EHCI_FASTPATH_TX
	dbus_irb_tx_t *txirb = NULL;
	int txirb_pending;
	struct exec_parms args;
#endif /* EHCI_FASTPATH_TX */

	if (dbus_info == NULL)
		return DBUS_ERR;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dbus_info->pub.busstate == DBUS_STATE_UP) {
#ifdef EHCI_FASTPATH_TX
		struct ehci_qtd *qtd;
		int token = EHCI_QTD_SET_CERR(3);
		int len;

		ASSERT(buf == NULL); /* Not handled */
		ASSERT(pkt != NULL);

		qtd = optimize_ehci_qtd_alloc(GFP_KERNEL);

		if (qtd == NULL)
			return DBUS_ERR;

		len = PKTLEN(pub->osh, pkt);

		len = ROUNDUP(len, sizeof(uint32));

#ifdef BCMDBG
		/* The packet length is already padded to not to be multiple of 512 bytes
		 * in bcm_rpc_tp_buf_send_internal(), so it should not be 512*N bytes here.
		 */
		if (len % EHCI_BULK_PACKET_SIZE == 0) {
			DBUSERR(("%s: len = %d (multiple of 512 bytes)\n", __FUNCTION__, len));
			return DBUS_ERR_TXDROP;
		}
#endif /* BCMDBG */

		optimize_qtd_fill_with_rpc(pub, 0, qtd, pkt, token, len);
		err = optimize_submit_async(qtd, 0);

		if (err) {
			optimize_ehci_qtd_free(qtd);
			err = DBUS_ERR_TXDROP;
		}

#else
		args.qdeq.q = dbus_info->tx_q;
		if (dbus_info->drvintf)
		txirb = EXEC_TXLOCK(dbus_info, q_deq_exec, &args);

		if (txirb == NULL) {
			DBUSERR(("Out of tx dbus_bufs\n"));
			return DBUS_ERR;
		}

		if (pkt != NULL) {
			txirb->pkt = pkt;
			txirb->buf = NULL;
			txirb->len = 0;
		} else if (buf != NULL) {
			txirb->pkt = NULL;
			txirb->buf = buf;
			txirb->len = len;
		} else {
			ASSERT(0); /* Should not happen */
		}
		txirb->info = info;
		txirb->arg = NULL;
		txirb->retry_count = 0;

		if (dbus_info->drvintf && dbus_info->drvintf->send_irb) {
			err = dbus_info->drvintf->send_irb(dbus_info->bus_info, txirb);
			if (err == DBUS_ERR_TXDROP) {
				/* tx fail and no completion routine to clean up, reclaim irb NOW */
				DBUSERR(("%s: send_irb failed, status = %d\n", __FUNCTION__, err));
				bzero(txirb, sizeof(dbus_irb_tx_t));
				args.qenq.q = dbus_info->tx_q;
				args.qenq.b = (dbus_irb_t *) txirb;
				EXEC_TXLOCK(dbus_info, q_enq_exec, &args);
			} else {
				dbus_tx_timer_start(dbus_info, DBUS_TX_TIMEOUT_INTERVAL);
				txirb_pending = dbus_info->pub.ntxq - dbus_info->tx_q->cnt;
#ifdef BCMDBG
				dbus_info->txpend_q_hist[txirb_pending]++;
#endif /* BCMDBG */
				if (txirb_pending > (dbus_info->tx_low_watermark * 3)) {
					dbus_flowctrl_tx(dbus_info, TRUE);
				}
			}
		}
#endif /* EHCI_FASTPATH_TX */
	} else {
		err = DBUS_ERR_TXFAIL;
		DBUSTRACE(("%s: bus down, send_irb failed\n", __FUNCTION__));
	}

	return err;
}

#if defined(BCM_DNGL_EMBEDIMAGE) || defined(BCMEMBEDIMAGE)
#if (defined(NDIS) || defined(USBAP)|| defined(WL_NVRAM_FILE)) && (defined(WLC_HIGH) && \
	!defined(WLC_LOW))

static int check_file(osl_t *osh, unsigned char *headers);
static int dbus_otp(dbus_info_t *dbus_info, uint16 *boardtype, uint16 *boardrev);
static int dbus_select_nvram(dbus_info_t *dbus_info, int8 *jumbonvram, int jumbolen,
uint16 boardtype, uint16 boardrev, int8 **nvram, int *nvram_len);

static int
dbus_get_nvram(dbus_info_t *dbus_info)
{
	int len, i;
	struct trx_header *hdr;
	int	actual_fwlen;

	dbus_info->nvram_len = 0;
	if (defvarslen) {

		if (jumbonvram) {

			uint16 boardrev = 0, boardtype = 0;
			int8 *nvram = NULL;
			int nvram_len = 0;
			int ret = DBUS_ERR;

			ret = dbus_otp(dbus_info, &boardtype, &boardrev);

			if (ret == DBUS_OK) {

				ret = dbus_select_nvram(dbus_info, jumbonvram, defvarslen,
					boardtype, boardrev, &nvram, &nvram_len);

				if (ret != DBUS_OK) {
					DBUSERR(("NO matching boardtype and boardrev\n"));
					return DBUS_ERR_NVRAM;
				}
				if (nvram_len > VARS_MAX) {
					DBUSERR(("NVRAM len %d is larger than %d \n",
					nvram_len, VARS_MAX));
					return DBUS_ERR_NVRAM;
				}
				bcopy(nvram, mfgsromvars, nvram_len);
				dbus_info->nvram = mfgsromvars;
				dbus_info->nvram_len = nvram_len;

			} else {
				/* otp might not have been programmed
				* program any nvram record  returned   
				*/
				boardrev = 0xFFFF, boardtype = 0xFFFF;

				ret = dbus_select_nvram(dbus_info, jumbonvram, defvarslen,
				boardtype, boardrev, &nvram, &nvram_len);

				if (nvram_len > VARS_MAX) {
					DBUSERR(("NVRAM len %d is larger than %d \n",
						defvarslen, VARS_MAX));
					return DBUS_ERR_NVRAM;
				}
				bcopy(nvram, mfgsromvars, nvram_len);
				dbus_info->nvram = mfgsromvars;
				dbus_info->nvram_len = nvram_len;
				DBUSTRACE(("OTP with no boardtype/rev nvramlen = %d\n", nvram_len));
			}
		} else {

			dbus_info->nvram = mfgsromvars;
			dbus_info->nvram_len = defvarslen;
		}

		DBUSERR(("NVRAM %d bytes downloaded\n", dbus_info->nvram_len));
	}

	if (dbus_info->nvram) {
		uint8 nvram_words_pad = 0;

		/* Validate the format/length etc of the file */
		if ((actual_fwlen = check_file(dbus_info->pub.osh, dbus_info->fw)) <= 0) {
			DBUSERR(("%s: bad firmware format!\n", __FUNCTION__));
			return DBUS_ERR_NVRAM;
		}
		/* dongle bootloader expects to round to word boundry */
		if (dbus_info->nvram_len % 4) {
			nvram_words_pad = 4 - (dbus_info->nvram_len % 4);
		}

		len = actual_fwlen + dbus_info->nvram_len +
			nvram_words_pad;
#if defined(USBAP) || defined(WL_VMEM_NVRAM_DECOMP)
		/* Allocate virtual memory otherwise it might fail on embedded systems */
		dbus_info->image = VMALLOC(dbus_info->pub.osh, len);
#else
		dbus_info->image = MALLOC(dbus_info->pub.osh, len);
#endif /* USBAP */
		dbus_info->image_len = len;
		if (dbus_info->image == NULL) {
			DBUSERR(("%s: malloc failed!\n", __FUNCTION__));
			return DBUS_ERR_NVRAM;
		}
		bcopy(dbus_info->fw, dbus_info->image, actual_fwlen);
		bcopy(dbus_info->nvram, (uint8 *)(dbus_info->image + actual_fwlen),
			dbus_info->nvram_len);
		if (nvram_words_pad) {
			bzero(&dbus_info->image[actual_fwlen + dbus_info->nvram_len],
				nvram_words_pad);
		}
		/* update TRX header for nvram size */
		hdr = (struct trx_header *)dbus_info->image;
		hdr->len = htol32(len);
		/* Pass the actual fw len */
		hdr->offsets[TRX_OFFSETS_NVM_LEN_IDX] =
			htol32(dbus_info->nvram_len + nvram_words_pad);
		/* Calculate CRC over header */
		hdr->crc32 = hndcrc32((uint8 *)&hdr->flag_version,
			sizeof(struct trx_header) - OFFSETOF(struct trx_header, flag_version),
			CRC32_INIT_VALUE);

		/* Calculate CRC over data */
		for (i = sizeof(struct trx_header); i < len; ++i)
				hdr->crc32 = hndcrc32((uint8 *)&dbus_info->image[i], 1, hdr->crc32);
		hdr->crc32 = htol32(hdr->crc32);
	} else {
		dbus_info->image = dbus_info->fw;
		dbus_info->image_len = (uint32)dbus_info->fwlen;
	}
	return DBUS_OK;
}
#endif /* (defined(NDIS) || defined(USBAP)) && (defined(WLC_HIGH) && !defined(WLC_LOW)) */

static int
dbus_do_download(dbus_info_t *dbus_info)
{
	unsigned int devid = 0;
	int err = DBUS_OK;
#ifdef WL_FW_DECOMP
	int decomp_override = 0;
#endif

	if (dbus_info == NULL)
		return DBUS_ERR;

	(void)devid; /* avoid unused variable warning */

#if defined(BCM_DNGL_EMBEDIMAGE)
	dbus_info->fw = NULL;
	dbus_info->fwlen = 0;
	devid = dbus_info->pub.attrib.devid;
	if ((devid == 0x4323) ||(devid == 0x43231) || (devid == 0x4322)) {
#ifdef EMBED_IMAGE_4322
		dbus_info->fw = (uint8 *) dlarray_4322;
		dbus_info->fwlen = sizeof(dlarray_4322);
#ifdef WL_FW_DECOMP
		decomp_override = 1;
#endif
#endif /* EMBED_IMAGE_4322 */
	} else if ((devid == 43236) || (devid == 43235) || (devid == 43238)) {
		ASSERT(dbus_info->pub.attrib.chiprev <= 3);
		if ((dbus_info->pub.attrib.chiprev == 2) || (dbus_info->pub.attrib.chiprev == 3)) {
#ifdef EMBED_IMAGE_43236b1
			dbus_info->fw = (uint8 *)dlarray_43236b1;
			dbus_info->fwlen = sizeof(dlarray_43236b1);
#endif
		}
	} else if (devid == 43237) {
#ifdef EMBED_IMAGE_43237a0
		dbus_info->fw = (uint8 *)dlarray_43237a0;
		dbus_info->fwlen = sizeof(dlarray_43237a0);
#endif
	} else if (devid == 43234) {
#ifdef EMBED_IMAGE_43236b1
		/* 43234 uses same image as 43236b1 */
		dbus_info->fw = (uint8 *)dlarray_43236b1;
		dbus_info->fwlen = sizeof(dlarray_43236b1);
#endif
	} else if ((devid == BCM4319_CHIP_ID) || (devid == BCM4319_D11N2G_ID)) {
		if (dbus_info->pub.attrib.bustype == DBUS_USB) {
#ifdef EMBED_IMAGE_4319usb
			dbus_info->fw = (uint8 *)dlarray_4319usb;
			dbus_info->fwlen = sizeof(dlarray_4319usb);
#endif
		}
		if (dbus_info->pub.attrib.bustype == DBUS_SDIO) {
#ifdef EMBED_IMAGE_4319sd
			dbus_info->fw = (uint8 *)dlarray_4319sd;
			dbus_info->fwlen = sizeof(dlarray_4319sd);
#endif
		}
	} else if (devid == 0x4325) {
#ifdef EMBED_IMAGE_4325sd
		dbus_info->fw = (uint8 *)dlarray_4325sd;
		dbus_info->fwlen = sizeof(dlarray_4325sd);
#endif
	} else {
#ifdef EMBED_IMAGE_GENERIC
		dbus_info->fw = (uint8 *)dlarray;
		dbus_info->fwlen = sizeof(dlarray);
#endif
	}
#if defined(NDIS) && (defined(WLC_HIGH) && !defined(WLC_LOW))
	if ((dngl_firmware) && (dngl_fwlen)) {
		dbus_info->fw = (uint8 *)dngl_firmware;
		dbus_info->fwlen = dngl_fwlen;
		DBUSERR(("using override firmmware %d bytes\n", dbus_info->fwlen));
	}
#endif /* defined(NDIS) && (defined(WLC_HIGH) && !defined(WLC_LOW)) */			
	if (!dbus_info->fw) {
		DBUSERR(("dbus_attach: devid 0x%x / %d not supported\n",
			devid, devid));
		return DBUS_ERR;
	}
	dbus_info->image = dbus_info->fw;
	dbus_info->image_len = (uint32)dbus_info->fwlen;
#ifdef WL_FW_DECOMP
	if (!decomp_override)
		err = dbus_zlib_decomp(dbus_info);
	if (err) {
		DBUSERR(("dbus_attach: fw decompress fail %d\n", err));
		return err;
	}
#endif
#if (defined(NDIS) || defined(USBAP)|| defined(WL_NVRAM_FILE)) && (defined(WLC_HIGH) && \
	!defined(WLC_LOW)) && !defined(SDIO_BMAC)
	if ((devid == 0x4323) ||(devid == 0x43231) || (devid == 0x4322)) {
		DBUSTRACE(("dbus_attach: devid 0x%x / %d nvram file not supported\n",
			devid, devid));
	} else {
		err = dbus_get_nvram(dbus_info);
		if (err) {
			 DBUSERR(("dbus_attach: fail to get nvram %d\n", err));
			 return err;
		}
	}
#endif /* (defined(NDIS) || defined(USBAP)) && (defined(WLC_HIGH) && !defined(WLC_LOW)) */
#endif /* defined(BCM_DNGL_EMBEDIMAGE) */

	if (dbus_info->drvintf->dlstart && dbus_info->drvintf->dlrun) {
		err = dbus_info->drvintf->dlstart(dbus_info->bus_info,
			dbus_info->image, dbus_info->image_len);

		if (err == DBUS_OK)
			err = dbus_info->drvintf->dlrun(dbus_info->bus_info);
	} else
		err = DBUS_ERR;
#if defined(NDIS) || defined(USBAP) || defined(WL_NVRAM_FILE)
	if (dbus_info->nvram) {

#if defined(USBAP) || defined(WL_VMEM_NVRAM_DECOMP)
		VFREE(dbus_info->pub.osh, dbus_info->image, dbus_info->image_len);
#else
		MFREE(dbus_info->pub.osh, dbus_info->image, dbus_info->image_len);
#endif /* USBAP */
		dbus_info->image = dbus_info->fw;
		dbus_info->image_len = (uint32)dbus_info->fwlen;
	}
#endif /* NDIS || USBAP */

#ifdef WL_FW_DECOMP
	if ((!decomp_override) && dbus_info->orig_fw)
	{
#if defined(WL_VMEM_NVRAM_DECOMP)
		VFREE(dbus_info->pub.osh, dbus_info->fw, dbus_info->decomp_memsize);
#else
		MFREE(dbus_info->pub.osh, dbus_info->fw, dbus_info->decomp_memsize);
#endif
		dbus_info->image = dbus_info->fw = dbus_info->orig_fw;
		dbus_info->image_len = dbus_info->fwlen = dbus_info->origfw_len;
	}
#endif /* WL_VMEM_NVRAM_DECOMP */
	return err;
}
#endif /* BCM_DNGL_EMBEDIMAGE */

static void
dbus_disconnect(void *handle)
{
	DBUSTRACE(("%s\n", __FUNCTION__));

	if (disconnect_cb)
		disconnect_cb(disc_arg);
}

/*
 * This function is called when the sent irb timesout without a tx response status.
 * DBUS adds reliability by resending timedout irbs DBUS_TX_RETRY_LIMIT times.
 */
static void
dbus_if_send_irb_timeout(void *handle, dbus_irb_tx_t *txirb)
{
	dbus_info_t *dbus_info = (dbus_info_t *) handle;

	if ((dbus_info == NULL) || (dbus_info->drvintf == NULL) || (txirb == NULL)) {
		return;
	}

	DBUSTRACE(("%s\n", __FUNCTION__));

	return;

}

static void BCMFASTPATH
dbus_if_send_irb_complete(void *handle, dbus_irb_tx_t *txirb, int status)
{
	dbus_info_t *dbus_info = (dbus_info_t *) handle;
	int txirb_pending;
	struct exec_parms args;
	void *pktinfo;

	if ((dbus_info == NULL) || (txirb == NULL)) {
		return;
	}

	DBUSTRACE(("%s: status = %d\n", __FUNCTION__, status));

	dbus_tx_timer_stop(dbus_info);

	/* re-queue BEFORE calling send_complete which will assume that this irb 
	   is now available.
	 */
	pktinfo = txirb->info;
	bzero(txirb, sizeof(dbus_irb_tx_t));
	args.qenq.q = dbus_info->tx_q;
	args.qenq.b = (dbus_irb_t *) txirb;
	EXEC_TXLOCK(dbus_info, q_enq_exec, &args);

	if (dbus_info->pub.busstate != DBUS_STATE_DOWN) {
		if ((status == DBUS_OK) || (status == DBUS_ERR_NODEVICE)) {
			if (dbus_info->cbs && dbus_info->cbs->send_complete)
				dbus_info->cbs->send_complete(dbus_info->cbarg, pktinfo,
					status);

			if (status == DBUS_OK) {
				txirb_pending = dbus_info->pub.ntxq - dbus_info->tx_q->cnt;
				if (txirb_pending)
					dbus_tx_timer_start(dbus_info, DBUS_TX_TIMEOUT_INTERVAL);
				if ((txirb_pending < dbus_info->tx_low_watermark) &&
					dbus_info->txoff && !dbus_info->txoverride) {
					dbus_flowctrl_tx(dbus_info, OFF);
				}
			}
		} else {
			DBUSERR(("%s: %d WARNING freeing orphan pkts\n", __FUNCTION__, __LINE__));
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY) || defined(BCM_RPC_TOC)
			if (dbus_info->cbs && dbus_info->cbs->send_complete)
				dbus_info->cbs->send_complete(dbus_info->cbarg, pktinfo,
					status);
#else
			dbus_if_pktfree(dbus_info, (void*)pktinfo, TRUE);
#endif /* defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY) || defined(BCM_RPC_TOC) */
		}
	} else {
		DBUSERR(("%s: %d WARNING freeing orphan pkts\n", __FUNCTION__, __LINE__));

#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY) || defined(BCM_RPC_TOC)
			if (dbus_info->cbs && dbus_info->cbs->send_complete)
				dbus_info->cbs->send_complete(dbus_info->cbarg, pktinfo,
					status);
#else
		dbus_if_pktfree(dbus_info, (void*)pktinfo, TRUE);
#endif /* defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY) defined(BCM_RPC_TOC) */
	}
}

static void BCMFASTPATH
dbus_if_recv_irb_complete(void *handle, dbus_irb_rx_t *rxirb, int status)
{
	dbus_info_t *dbus_info = (dbus_info_t *) handle;
	int rxirb_pending;
	struct exec_parms args;

	if ((dbus_info == NULL) || (rxirb == NULL)) {
		return;
	}

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dbus_info->pub.busstate != DBUS_STATE_DOWN) {
		if (status == DBUS_OK) {
			if ((rxirb->buf != NULL) && (rxirb->actual_len > 0)) {
				if (dbus_info->cbs && dbus_info->cbs->recv_buf)
					dbus_info->cbs->recv_buf(dbus_info->cbarg, rxirb->buf,
					rxirb->actual_len);
			} else if (rxirb->pkt != NULL) {
				if (dbus_info->cbs && dbus_info->cbs->recv_pkt)
					dbus_info->cbs->recv_pkt(dbus_info->cbarg, rxirb->pkt);
			} else {
				ASSERT(0); /* Should not happen */
			}

			rxirb_pending = dbus_info->pub.nrxq - dbus_info->rx_q->cnt - 1;
#ifdef BCMDBG
			dbus_info->rxpend_q_hist[rxirb_pending]++;
#endif /* BCMDBG */
			if ((rxirb_pending <= dbus_info->rx_low_watermark) &&
				!dbus_info->rxoff) {
				DBUSTRACE(("Low watermark so submit more %d <= %d \n",
					dbus_info->rx_low_watermark, rxirb_pending));

				dbus_rxirbs_fill(dbus_info);
			} else if (dbus_info->rxoff)
				DBUSTRACE(("rx flow controlled. not filling more. cut_rxq=%d\n",
					dbus_info->rx_q->cnt));
		} else if (status == DBUS_ERR_NODEVICE) {
			DBUSTRACE(("%s: status = %d\n", __FUNCTION__, status));
		} else {
			DBUSERR(("%s: status = %d\n", __FUNCTION__, status));
		}
	} else {
		DBUSTRACE(("%s: DBUS down, ignoring recv callback\n", __FUNCTION__));
	}

	bzero(rxirb, sizeof(dbus_irb_rx_t));
	args.qenq.q = dbus_info->rx_q;
	args.qenq.b = (dbus_irb_t *) rxirb;
	EXEC_RXLOCK(dbus_info, q_enq_exec, &args);
}

static void
dbus_if_errhandler(void *handle, int err)
{
	dbus_info_t *dbus_info = handle;
	uint32 mask = 0;

	if (dbus_info == NULL)
		return;

	switch (err) {
		case DBUS_ERR_TXFAIL:
			dbus_info->pub.stats.tx_errors++;
			mask |= ERR_CBMASK_TXFAIL;
			break;
		case DBUS_ERR_TXDROP:
			dbus_info->pub.stats.tx_dropped++;
			mask |= ERR_CBMASK_TXFAIL;
			break;
		case DBUS_ERR_RXFAIL:
			dbus_info->pub.stats.rx_errors++;
			mask |= ERR_CBMASK_RXFAIL;
			break;
		case DBUS_ERR_RXDROP:
			dbus_info->pub.stats.rx_dropped++;
			mask |= ERR_CBMASK_RXFAIL;
			break;
		default:
			break;
	}

	if (dbus_info->cbs && dbus_info->cbs->errhandler && (dbus_info->errmask & mask))
		dbus_info->cbs->errhandler(dbus_info->cbarg, err);
}

static void
dbus_if_ctl_complete(void *handle, int type, int status)
{
	dbus_info_t *dbus_info = (dbus_info_t *) handle;

	if (dbus_info == NULL)
		return;

	if (dbus_info->pub.busstate != DBUS_STATE_DOWN) {
		if (dbus_info->cbs && dbus_info->cbs->ctl_complete)
			dbus_info->cbs->ctl_complete(dbus_info->cbarg, type, status);
	}
}

static void
dbus_if_state_change(void *handle, int state)
{
	dbus_info_t *dbus_info = (dbus_info_t *) handle;
	int old_state;

	if (dbus_info == NULL)
		return;

	if (dbus_info->pub.busstate == state)
		return;
	old_state = dbus_info->pub.busstate;
	if (state == DBUS_STATE_DISCONNECT) {
		DBUSERR(("DBUS disconnected\n"));
	}

	DBUSTRACE(("dbus state change from %d to to %d\n", old_state, state));

	/* Don't update state if it's PnP firmware re-download */
	if (state != DBUS_STATE_PNP_FWDL)
		dbus_info->pub.busstate = state;
	if (state == DBUS_STATE_SLEEP)
		dbus_flowctrl_rx(handle, TRUE);
	if ((old_state  == DBUS_STATE_SLEEP) && (state == DBUS_STATE_UP)) {
		dbus_rxirbs_fill(dbus_info);
		dbus_flowctrl_rx(handle, FALSE);
	}

	if (dbus_info->cbs && dbus_info->cbs->state_change)
		dbus_info->cbs->state_change(dbus_info->cbarg, state);
}

static void *
dbus_if_pktget(void *handle, uint len, bool send)
{
	dbus_info_t *dbus_info = (dbus_info_t *) handle;
	void *p = NULL;

	if (dbus_info == NULL)
		return NULL;

	if (dbus_info->cbs && dbus_info->cbs->pktget)
		p = dbus_info->cbs->pktget(dbus_info->cbarg, len, send);
	else
		ASSERT(0);

	return p;
}

static void
dbus_if_pktfree(void *handle, void *p, bool send)
{
	dbus_info_t *dbus_info = (dbus_info_t *) handle;

	if (dbus_info == NULL)
		return;

	if (dbus_info->cbs && dbus_info->cbs->pktfree)
		dbus_info->cbs->pktfree(dbus_info->cbarg, p, send);
	else
		ASSERT(0);
}

static struct dbus_irb*
dbus_if_getirb(void *cbarg, bool send)
{
	dbus_info_t *dbus_info = (dbus_info_t *) cbarg;
	struct exec_parms args;
	struct dbus_irb *irb;

	if ((dbus_info == NULL) || (dbus_info->pub.busstate != DBUS_STATE_UP))
		return NULL;

	if (send == TRUE) {
		args.qdeq.q = dbus_info->tx_q;
		irb = EXEC_TXLOCK(dbus_info, q_deq_exec, &args);
	} else {
		args.qdeq.q = dbus_info->rx_q;
		irb = EXEC_RXLOCK(dbus_info, q_deq_exec, &args);
	}

	return irb;
}

static void *
dbus_probe(void *arg, const char *desc, uint32 bustype, uint32 hdrlen)
{
	if (probe_cb) {
		disc_arg = probe_cb(probe_arg, desc, bustype, hdrlen);
		return disc_arg;
	}

	return (void *)DBUS_ERR;
}

int
dbus_register(int vid, int pid, probe_cb_t prcb,
	disconnect_cb_t discb, void *prarg, void *param1, void *param2)
{
	int err;

	DBUSTRACE(("%s\n", __FUNCTION__));

	probe_cb = prcb;
	disconnect_cb = discb;
	probe_arg = prarg;

	err = dbus_bus_register(vid, pid, dbus_probe,
		dbus_disconnect, NULL, &g_busintf, param1, param2);

	return err;
}

int
dbus_deregister()
{
	int ret;

	DBUSTRACE(("%s\n", __FUNCTION__));

	ret = dbus_bus_deregister();
	probe_cb = NULL;
	disconnect_cb = NULL;
	probe_arg = NULL;

	return ret;

}


const dbus_pub_t *
dbus_attach(osl_t *osh, int rxsize, int nrxq, int ntxq, void *cbarg,
	dbus_callbacks_t *cbs, struct shared_info *sh)
{
	dbus_info_t *dbus_info;
	int err;

	if ((g_busintf == NULL) || (g_busintf->attach == NULL) || (cbs == NULL))
		return NULL;

	DBUSTRACE(("%s\n", __FUNCTION__));

	dbus_info = MALLOC(osh, sizeof(dbus_info_t));
	if (dbus_info == NULL)
		return NULL;

	bzero(dbus_info, sizeof(dbus_info_t));

	/* BUS-specific driver interface */
	dbus_info->drvintf = g_busintf;
	dbus_info->cbarg = cbarg;
	dbus_info->cbs = cbs;

	dbus_info->pub.sh = sh;
	dbus_info->pub.osh = osh;
	dbus_info->pub.rxsize = rxsize;

#ifdef EHCI_FASTPATH_RX
	atomic_set(&dbus_info->rx_outstanding, 0);
#endif

	if (nrxq < DBUS_NRXQ)
		nrxq = DBUS_NRXQ;
	if (ntxq < DBUS_NTXQ)
		ntxq = DBUS_NTXQ;

	dbus_info->pub.nrxq = nrxq;
	dbus_info->rx_low_watermark = nrxq / 2;	/* keep enough posted rx urbs */
	dbus_info->pub.ntxq = ntxq;
	dbus_info->tx_low_watermark = ntxq / 4;	/* flow control when too many tx urbs posted */

	dbus_info->tx_q = MALLOC(osh, sizeof(dbus_irbq_t));
	if (dbus_info->tx_q == NULL)
		goto error;
	else {
		bzero(dbus_info->tx_q, sizeof(dbus_irbq_t));
		err = dbus_irbq_init(dbus_info, dbus_info->tx_q, ntxq, sizeof(dbus_irb_tx_t));
		if (err != DBUS_OK)
			goto error;
	}

	dbus_info->rx_q = MALLOC(osh, sizeof(dbus_irbq_t));
	if (dbus_info->rx_q == NULL)
		goto error;
	else {
		bzero(dbus_info->rx_q, sizeof(dbus_irbq_t));
		err = dbus_irbq_init(dbus_info, dbus_info->rx_q, nrxq, sizeof(dbus_irb_rx_t));
		if (err != DBUS_OK)
			goto error;
	}

#ifdef BCMDBG
	dbus_info->txpend_q_hist = MALLOC(osh, dbus_info->pub.ntxq * sizeof(int));
	if (dbus_info->txpend_q_hist == NULL)
		goto error;
	bzero(dbus_info->txpend_q_hist, dbus_info->pub.ntxq * sizeof(int));
	dbus_info->rxpend_q_hist = MALLOC(osh, dbus_info->pub.nrxq * sizeof(int));
	if (dbus_info->rxpend_q_hist == NULL)
		goto error;
	bzero(dbus_info->rxpend_q_hist, dbus_info->pub.nrxq * sizeof(int));
#endif /* BCMDBG */

	dbus_info->bus_info = (void *)g_busintf->attach(&dbus_info->pub,
		dbus_info, &dbus_intf_cbs);
	if (dbus_info->bus_info == NULL)
		goto error;

	dbus_tx_timer_init(dbus_info);

	dbus_get_attrib(&dbus_info->pub, &dbus_info->pub.attrib);

	/* Use default firmware */
#if defined(BCMEMBEDIMAGE)
	dbus_info->fw = (uint8 *) dlarray;
	dbus_info->fwlen = sizeof(dlarray);
#endif /* BCMEMBEDIMAGE */	

#if defined(BCM_DNGL_EMBEDIMAGE) || defined(BCMEMBEDIMAGE)
	if (dbus_info->drvintf->dlneeded) {
		if (dbus_info->drvintf->dlneeded(dbus_info->bus_info)) {
			err = dbus_do_download(dbus_info);
			if (err == DBUS_ERR) {
				DBUSERR(("attach: download failed=%d\n", err));
				goto error;
			}
		}
	}
#endif /* defined(BCM_DNGL_EMBEDIMAGE) || defined(BCMEMBEDIMAGE) */


	return (dbus_pub_t *)dbus_info;

error:
	dbus_detach((dbus_pub_t *)dbus_info);
	return NULL;
}

void
dbus_detach(const dbus_pub_t *pub)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;
	osl_t *osh;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dbus_info == NULL)
		return;

	dbus_tx_timer_stop(dbus_info);

	osh = pub->osh;

	if (dbus_info->drvintf && dbus_info->drvintf->detach)
		 dbus_info->drvintf->detach((dbus_pub_t *)dbus_info, dbus_info->bus_info);

	if (dbus_info->tx_q) {
		dbus_irbq_deinit(dbus_info, dbus_info->tx_q, sizeof(dbus_irb_tx_t));
		MFREE(osh, dbus_info->tx_q, sizeof(dbus_irbq_t));
		dbus_info->tx_q = NULL;
	}

	if (dbus_info->rx_q) {
		dbus_irbq_deinit(dbus_info, dbus_info->rx_q, sizeof(dbus_irb_rx_t));
		MFREE(osh, dbus_info->rx_q, sizeof(dbus_irbq_t));
		dbus_info->rx_q = NULL;
	}

#ifdef BCMDBG
	MFREE(osh, dbus_info->txpend_q_hist, dbus_info->pub.ntxq * sizeof(int));
	MFREE(osh, dbus_info->rxpend_q_hist, dbus_info->pub.nrxq * sizeof(int));
#endif /* BCMDBG */

	MFREE(osh, dbus_info, sizeof(dbus_info_t));
}

int
dbus_up(const dbus_pub_t *pub)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;
	int err = DBUS_OK;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dbus_info == NULL)
		return DBUS_ERR;

	if ((dbus_info->pub.busstate == DBUS_STATE_DL_DONE) ||
		(dbus_info->pub.busstate == DBUS_STATE_DOWN) ||
		(dbus_info->pub.busstate == DBUS_STATE_SLEEP)) {
		if (dbus_info->drvintf && dbus_info->drvintf->up) {
			err = dbus_info->drvintf->up(dbus_info->bus_info);

			if (err == DBUS_OK) {
				dbus_rxirbs_fill(dbus_info);
			}
		}
	} else
		err = DBUS_ERR;

	return err;
}

int
dbus_down(const dbus_pub_t *pub)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dbus_info == NULL)
		return DBUS_ERR;

	dbus_tx_timer_stop(dbus_info);

	if (dbus_info->pub.busstate == DBUS_STATE_UP) {
		if (dbus_info->drvintf && dbus_info->drvintf->down)
			return dbus_info->drvintf->down(dbus_info->bus_info);
	}

	return DBUS_ERR;
}

int
dbus_shutdown(const dbus_pub_t *pub)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dbus_info == NULL)
		return DBUS_ERR;

	if (dbus_info->drvintf && dbus_info->drvintf->shutdown)
		return dbus_info->drvintf->shutdown(dbus_info->bus_info);

	return DBUS_OK;
}

int
dbus_stop(const dbus_pub_t *pub)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dbus_info == NULL)
		return DBUS_ERR;

	if (dbus_info->pub.busstate == DBUS_STATE_UP) {
		if (dbus_info->drvintf && dbus_info->drvintf->stop)
			return dbus_info->drvintf->stop(dbus_info->bus_info);
	}

	return DBUS_ERR;
}

int
dbus_send_buf(const dbus_pub_t *pub, uint8 *buf, int len, void *info)
{
	return dbus_send_irb(pub, buf, len, NULL, info);
}

int
dbus_send_pkt(const dbus_pub_t *pub, void *pkt, void *info)
{
	return dbus_send_irb(pub, NULL, 0, pkt, info);
}

int
dbus_send_ctl(const dbus_pub_t *pub, uint8 *buf, int len)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;

	if (dbus_info == NULL)
		return DBUS_ERR;

	if (dbus_info->pub.busstate == DBUS_STATE_UP) {
		if (dbus_info->drvintf && dbus_info->drvintf->send_ctl)
			return dbus_info->drvintf->send_ctl(dbus_info->bus_info, buf, len);
	}

	return DBUS_ERR;
}

int
dbus_recv_ctl(const dbus_pub_t *pub, uint8 *buf, int len)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;

	if ((dbus_info == NULL) || (buf == NULL))
		return DBUS_ERR;

	if (dbus_info->pub.busstate == DBUS_STATE_UP) {
		if (dbus_info->drvintf && dbus_info->drvintf->recv_ctl)
			return dbus_info->drvintf->recv_ctl(dbus_info->bus_info, buf, len);
	}

	return DBUS_ERR;
}

int
dbus_recv_bulk(const dbus_pub_t *pub, uint32 ep_idx)
{
#ifdef EHCI_FASTPATH_RX
	/* 2nd bulk in not supported for EHCI_FASTPATH_RX */
	ASSERT(0);
#else
	dbus_info_t *dbus_info = (dbus_info_t *) pub;

	dbus_irb_rx_t *rxirb;
	struct exec_parms args;
	int status;


	if (dbus_info == NULL)
		return DBUS_ERR;

	args.qdeq.q = dbus_info->rx_q;
	if (dbus_info->pub.busstate == DBUS_STATE_UP) {
		if (dbus_info->drvintf && dbus_info->drvintf->recv_irb_from_ep) {
			if ((rxirb = (EXEC_RXLOCK(dbus_info, q_deq_exec, &args))) != NULL) {
				status = dbus_info->drvintf->recv_irb_from_ep(dbus_info->bus_info,
					rxirb, ep_idx);
				if (status == DBUS_ERR_RXDROP) {
					bzero(rxirb, sizeof(dbus_irb_rx_t));
					args.qenq.q = dbus_info->rx_q;
					args.qenq.b = (dbus_irb_t *) rxirb;
					EXEC_RXLOCK(dbus_info, q_enq_exec, &args);
				}
			}
		}
	}
#endif /* EHCI_FASTPATH_RX */
	return DBUS_ERR;
}

void *
dbus_pktget(const dbus_pub_t *pub, int len)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;

	if ((dbus_info == NULL) || (len < 0))
		return NULL;

	return PKTGET(dbus_info->pub.osh, len, TRUE);
}

void
dbus_pktfree(const dbus_pub_t *pub, void* pkt)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;

	if ((dbus_info == NULL) || (pkt == NULL))
		return;

	PKTFREE(dbus_info->pub.osh, pkt, TRUE);
}

int
dbus_get_stats(const dbus_pub_t *pub, dbus_stats_t *stats)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;

	if ((dbus_info == NULL) || (stats == NULL))
		return DBUS_ERR;

	bcopy(&dbus_info->pub.stats, stats, sizeof(dbus_stats_t));

	return DBUS_OK;
}

int
dbus_get_attrib(const dbus_pub_t *pub, dbus_attrib_t *attrib)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;
	int err = DBUS_ERR;

	if ((dbus_info == NULL) || (attrib == NULL))
		return DBUS_ERR;

	if (dbus_info->drvintf && dbus_info->drvintf->get_attrib) {
		err = dbus_info->drvintf->get_attrib(dbus_info->bus_info,
		&dbus_info->pub.attrib);
	}

	bcopy(&dbus_info->pub.attrib, attrib, sizeof(dbus_attrib_t));
	return err;
}

int
dbus_get_device_speed(const dbus_pub_t *pub)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;

	if (dbus_info == NULL)
		return INVALID_SPEED;

	return (dbus_info->pub.device_speed);
}

int
dbus_set_config(const dbus_pub_t *pub, dbus_config_t *config)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;
	int err = DBUS_ERR;

	if ((dbus_info == NULL) || (config == NULL))
		return DBUS_ERR;

	if (dbus_info->drvintf && dbus_info->drvintf->set_config) {
		err = dbus_info->drvintf->set_config(dbus_info->bus_info,
		config);
	}

	return err;
}

int
dbus_get_config(const dbus_pub_t *pub, dbus_config_t *config)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;
	int err = DBUS_ERR;

	if ((dbus_info == NULL) || (config == NULL))
		return DBUS_ERR;

	if (dbus_info->drvintf && dbus_info->drvintf->get_config) {
		err = dbus_info->drvintf->get_config(dbus_info->bus_info,
		config);
	}

	return err;
}

int
dbus_set_errmask(const dbus_pub_t *pub, uint32 mask)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;
	int err = DBUS_OK;

	if (dbus_info == NULL)
		return DBUS_ERR;

	dbus_info->errmask = mask;
	return err;
}

int
dbus_pnp_resume(const dbus_pub_t *pub, int *fw_reload)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;
	int err = DBUS_ERR;
	bool fwdl = FALSE;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dbus_info == NULL)
		return DBUS_ERR;

	if (dbus_info->pub.busstate == DBUS_STATE_UP) {
		return DBUS_OK;
	}

	if (dbus_info->drvintf->pnp) {
		err = dbus_info->drvintf->pnp(dbus_info->bus_info,
			DBUS_PNP_RESUME);
	}
#if defined(BCM_DNGL_EMBEDIMAGE) || defined(BCMEMBEDIMAGE)
	if (dbus_info->drvintf->device_exists &&
		dbus_info->drvintf->device_exists(dbus_info->bus_info)) {
		if (dbus_info->drvintf->dlneeded) {
			if (dbus_info->drvintf->dlneeded(dbus_info->bus_info)) {
				err = dbus_do_download(dbus_info);
				if (err == DBUS_OK) {
					fwdl = TRUE;
				}
				if (dbus_info->pub.busstate == DBUS_STATE_DL_DONE)
					dbus_up(pub);
			}
		}
	} else {
		return DBUS_ERR;
	}
#endif /* BCM_DNGL_EMBEDIMAGE */

	if (dbus_info->drvintf->recv_needed) {
		if (dbus_info->drvintf->recv_needed(dbus_info->bus_info)) {
			/* Refill after sleep/hibernate */
			dbus_rxirbs_fill(dbus_info);
		}
	}

	if (fwdl == TRUE) {
		dbus_if_state_change(dbus_info, DBUS_STATE_PNP_FWDL);
	}

	if (fw_reload)
		*fw_reload = fwdl;

	return err;
}

int
dbus_pnp_sleep(const dbus_pub_t *pub)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;
	int err = DBUS_ERR;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dbus_info == NULL)
		return DBUS_ERR;

	dbus_tx_timer_stop(dbus_info);

	if (dbus_info->drvintf && dbus_info->drvintf->pnp) {
		err = dbus_info->drvintf->pnp(dbus_info->bus_info,
			DBUS_PNP_SLEEP);
	}

	return err;
}

int
dbus_pnp_disconnect(const dbus_pub_t *pub)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;
	int err = DBUS_ERR;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dbus_info == NULL)
		return DBUS_ERR;

	dbus_tx_timer_stop(dbus_info);

	if (dbus_info->drvintf && dbus_info->drvintf->pnp) {
		err = dbus_info->drvintf->pnp(dbus_info->bus_info,
			DBUS_PNP_DISCONNECT);
	}

	return err;
}

int
dbus_iovar_op(const dbus_pub_t *pub, const char *name,
	void *params, int plen, void *arg, int len, bool set)
{
	dbus_info_t *dbus_info = (dbus_info_t *) pub;
	int err = DBUS_ERR;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dbus_info == NULL)
		return DBUS_ERR;

	if (dbus_info->drvintf && dbus_info->drvintf->iovar_op) {
		err = dbus_info->drvintf->iovar_op(dbus_info->bus_info,
			name, params, plen, arg, len, set);
	}

	return err;
}

#ifdef BCMDBG
void
dbus_hist_dump(const dbus_pub_t *pub, struct bcmstrbuf *b)
{
	int i = 0, j = 0;
	dbus_info_t *dbus_info = (dbus_info_t *) pub;

	bcm_bprintf(b, "\nDBUS histogram\n");
	bcm_bprintf(b, "txq\n");
	for (i = 0; i < dbus_info->pub.ntxq; i++) {
		if (dbus_info->txpend_q_hist[i]) {
			bcm_bprintf(b, "%d: %d ", i, dbus_info->txpend_q_hist[i]);
			j++;
			if (j % 10 == 0) {
				bcm_bprintf(b, "\n");
			}
		}
	}

	j = 0;
	bcm_bprintf(b, "\nrxq\n");
	for (i = 0; i < dbus_info->pub.nrxq; i++) {
		if (dbus_info->rxpend_q_hist[i]) {
			bcm_bprintf(b, "%d: %d ", i, dbus_info->rxpend_q_hist[i]);
			j++;
			if (j % 10 == 0) {
				bcm_bprintf(b, "\n");
			}
		}
	}
	bcm_bprintf(b, "\n");

	if (dbus_info->drvintf && dbus_info->drvintf->dump)
		dbus_info->drvintf->dump(dbus_info->bus_info, b);
}
#endif /* BCMDBG */

#if defined(BCM_DNGL_EMBEDIMAGE) || defined(BCMEMBEDIMAGE)
#if defined(NDIS) || defined(USBAP) || defined(WL_NVRAM_FILE)
static int
check_file(osl_t *osh, unsigned char *headers)
{
	struct trx_header *trx;
	int actual_len = -1;

	/* Extract trx header */
	trx = (struct trx_header *)headers;
	if (ltoh32(trx->magic) != TRX_MAGIC) {
		printf("Error: trx bad hdr %x\n", ltoh32(trx->magic));
		return -1;
	}

	headers += sizeof(struct trx_header);

	if (ltoh32(trx->flag_version) & TRX_UNCOMP_IMAGE) {
		actual_len = ltoh32(trx->offsets[TRX_OFFSETS_DLFWLEN_IDX]) +
		                     sizeof(struct trx_header);
		return actual_len;
	}  else {
		printf("compressed image\n");
	}
	return -1;

}

static int
dbus_otp(dbus_info_t *dbus_info, uint16 *boardtype, uint16 *boardrev)
{
	uint32 value = 0;
	uint8 *cis;
	uint16 *otpinfo;
	uint32 i;
	bool standard_cis = TRUE;
	uint8 tup, tlen;
	bool btype_present = FALSE;
	bool brev_present = FALSE;
	int ret;
	int devid;
	uint32 otp_size = 0, otp_addr = 0, otp_sw_rgn = 0;

	if (dbus_info == NULL || dbus_info->drvintf == NULL ||
		dbus_info->drvintf->readreg == NULL)
		return DBUS_ERR;

	devid = dbus_info->pub.attrib.devid;

	if ((devid == BCM43234_CHIP_ID) || (devid == BCM43235_CHIP_ID) ||
		(devid == BCM43236_CHIP_ID)) {

		otp_size = BCM_OTP_SIZE_43236;
		otp_sw_rgn = BCM_OTP_SW_RGN_43236;
		otp_addr = BCM_OTP_ADDR_43236;

	} else {
		return DBUS_ERR_NVRAM;
	}

	cis = MALLOC(dbus_info->pub.osh, otp_size * 2);

	if (!cis)
		return DBUS_ERR;

	otpinfo = (uint16 *) cis;

	for (i = 0; i < otp_size; i++) {

		ret = dbus_info->drvintf->readreg(dbus_info->bus_info,
			otp_addr + ((otp_sw_rgn + i) << 1), 2, &value);

		if (ret != DBUS_OK) {
			MFREE(dbus_info->pub.osh, cis, otp_size * 2);
			return ret;
		}
		otpinfo[i] = (uint16) value;
	}

	for (i = 0; i < (otp_size << 1); ) {

		if (standard_cis) {
			tup = cis[i++];
			if (tup == CISTPL_NULL || tup == CISTPL_END)
				tlen = 0;
			else
				tlen = cis[i++];
		} else {
			if (cis[i] == CISTPL_NULL || cis[i] == CISTPL_END) {
				tlen = 0;
				tup = cis[i];
			} else {
				tlen = cis[i];
				tup = CISTPL_BRCM_HNBU;
			}
			++i;
		}

		if (tup == CISTPL_END || (i + tlen) >= (otp_size << 1)) {
			break;
		}

		switch (tup) {

		case CISTPL_BRCM_HNBU:

			switch (cis[i]) {

			case HNBU_BOARDTYPE:

				*boardtype = (uint16) ((cis[i + 2] << 8) + cis[i + 1]);
				btype_present = TRUE;
				DBUSTRACE(("%s: HNBU_BOARDTYPE = 0x%2x\n", __FUNCTION__,
					(uint32)*boardtype));
				break;

			case HNBU_BOARDREV:

				if (tlen == 2)
					*boardrev = (uint16) cis[i + 1];
				else
					*boardrev = (uint16) ((cis[i + 2] << 8) + cis[i + 1]);
				brev_present = TRUE;
				DBUSTRACE(("%s: HNBU_BOARDREV =  0x%2x\n", __FUNCTION__,
					(uint32)*boardrev));
				break;

			case HNBU_HNBUCIS:
				DBUSTRACE(("%s: HNBU_HNBUCIS\n", __FUNCTION__));
				tlen++;
				standard_cis = FALSE;
				break;
			}
			break;
		}

		i += tlen;
	}

	MFREE(dbus_info->pub.osh, cis, otp_size * 2);

	if (btype_present == TRUE && brev_present == TRUE) {
		DBUSERR(("otp boardtype = 0x%2x boardrev = 0x%2x\n",
			*boardtype, *boardrev));

		return DBUS_OK;
	}
	else
		return DBUS_ERR;
}

#define BCM_JUMBO_NVRAM_DELIMIT '\n'
#define BCM_JUMBO_START "Broadcom Jumbo Nvram file"
static int
dbus_select_nvram(dbus_info_t *dbus_info, int8 *jumbonvram, int jumbolen,
uint16 boardtype, uint16 boardrev, int8 **nvram, int *nvram_len)
{
	/* Multi board nvram file format is contenation of nvram info with \r 
	*  The file format for two contatenated set is  
	*  \nBroadcom Jumbo Nvram file\nfirst_set\nsecond_set\nthird_set\n	
	*/
	uint8 *nvram_start = NULL, *nvram_end = NULL;
	uint8 *nvram_start_prev = NULL, *nvram_end_prev = NULL;
	uint16 btype = 0, brev = 0;
	int len  = 0;
	char *field;

	*nvram = NULL;
	*nvram_len = 0;

	if (strncmp(BCM_JUMBO_START, jumbonvram, strlen(BCM_JUMBO_START))) {
		/* single nvram file in the native format */
		DBUSTRACE(("%s: Non-Jumbo NVRAM File \n", __FUNCTION__));
		*nvram = jumbonvram;
		*nvram_len = jumbolen;
		return DBUS_OK;
	} else {
		DBUSTRACE(("%s: Jumbo NVRAM File \n", __FUNCTION__));
	}

	/* sanity test the end of the config sets for proper ending */
	if (jumbonvram[jumbolen - 1] != BCM_JUMBO_NVRAM_DELIMIT ||
		jumbonvram[jumbolen - 2] != '\0') {
		DBUSERR(("%s: Bad NVRAM file format\n", __FUNCTION__));
		return DBUS_JUMBO_BAD_FORMAT;
	}

	nvram_start = jumbonvram;

	while (*nvram_start != BCM_JUMBO_NVRAM_DELIMIT && len < jumbolen) {

		/* consume the  first file info line 
		* \nBroadcom Jumbo Nvram file\nfile1\n ...
		*/
		len ++;
		nvram_start ++;
	}

	nvram_end = nvram_start;

	/* search for "boardrev=0xabcd" and "boardtype=0x1234" information in 
	* the concatenated nvram config files /sets 
	*/

	while (len < jumbolen) {

		if (*nvram_end == '\0') {
			/* end of a config set is marked by multiple null characters */
			len ++;
			nvram_end ++;
			DBUSTRACE(("%s: NULL chr len = %d char = 0x%x\n", __FUNCTION__,
				len, *nvram_end));
			continue;

		} else if (*nvram_end == BCM_JUMBO_NVRAM_DELIMIT) {

			/* config set delimiter is reached */
			/* check if next config set is present or not
			*  return  if next config is not present
			*/	

			/* start search the next config set */
			nvram_start_prev = nvram_start;
			nvram_end_prev = nvram_end;

			nvram_end ++;
			nvram_start = nvram_end;
			btype = brev = 0;
			DBUSTRACE(("%s: going to next record len = %d "
					"char = 0x%x \n", __FUNCTION__, len, *nvram_end));
			len ++;
			if (len >= jumbolen) {

				*nvram = nvram_start_prev;
				*nvram_len = (int)(nvram_end_prev - nvram_start_prev);

				DBUSTRACE(("%s: no more len = %d nvram_end = 0x%p",
					__FUNCTION__, len, nvram_end));

				return DBUS_JUMBO_NOMATCH;

			} else {
				continue;
			}

		} else {

			DBUSTRACE(("%s: config str = %s\n", __FUNCTION__, nvram_end));

			if (bcmp(nvram_end, "boardtype", strlen("boardtype")) == 0) {

				field = strchr(nvram_end, '=');
				field++;
				btype = (uint16)bcm_strtoul(field, NULL, 0);

				DBUSTRACE(("%s: btype = 0x%x boardtype = 0x%x \n", __FUNCTION__,
					btype, boardtype));
			}

			if (bcmp(nvram_end, "boardrev", strlen("boardrev")) == 0) {

				field = strchr(nvram_end, '=');
				field++;
				brev = (uint16)bcm_strtoul(field, NULL, 0);

				DBUSTRACE(("%s: brev = 0x%x boardrev = 0x%x \n", __FUNCTION__,
					brev, boardrev));
			}
			if (btype == boardtype && brev == boardrev) {
				/* locate nvram config set end - ie.find '\r' char */
				while (*nvram_end != BCM_JUMBO_NVRAM_DELIMIT)
					nvram_end ++;
				*nvram = nvram_start;
				*nvram_len = (int) (nvram_end - nvram_start);
				DBUSTRACE(("found len = %d nvram_start = 0x%p "
					"nvram_end = 0x%p\n", *nvram_len, nvram_start, nvram_end));
				return DBUS_OK;
			}

			len += (strlen(nvram_end) + 1);
			nvram_end += (strlen(nvram_end) + 1);
		}
	}
	return DBUS_JUMBO_NOMATCH;
}

#endif /* (defined(NDIS) || defined(USBAP)) && (defined(WLC_HIGH) && !defined(WLC_LOW)) */


#ifdef WL_FW_DECOMP


/* this function is a combination of trx.c and bcmdl.c plus dbus adaptation */ 
static int
dbus_zlib_decomp(dbus_info_t *dbus_info)
{

	int method, flags, len, status;
	unsigned int uncmp_len, uncmp_crc, dec_crc, crc_init;
	struct trx_header *trx, *newtrx;
	unsigned char *file = NULL;
	unsigned char gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */
	z_stream d_stream;
	unsigned char unused;
	int actual_len = -1;
	unsigned char *headers;
	unsigned int trxhdrsize, nvramsize, decomp_memsize, i;

	osl_handle = dbus_info->pub.osh;
	dbus_info->orig_fw = NULL;

	headers = dbus_info->fw;
	/* Extract trx header */
	trx = (struct trx_header *)headers;
	trxhdrsize = sizeof(struct trx_header);

	if (ltoh32(trx->magic) != TRX_MAGIC) {
		DBUSERR(("%s: Error: trx bad hdr %x\n", __FUNCTION__,
			ltoh32(trx->magic)));
		return -1;
	}

	headers += sizeof(struct trx_header);

	if (ltoh32(trx->flag_version) & TRX_UNCOMP_IMAGE) {
		actual_len = ltoh32(trx->offsets[TRX_OFFSETS_DLFWLEN_IDX]) +
		                     sizeof(struct trx_header);
		DBUSERR(("%s: not a compressed image\n", __FUNCTION__));
		return 0;
	} else {
		/* Extract the gzip header info */
		if ((*headers++ != gz_magic[0]) || (*headers++ != gz_magic[1])) {
			DBUSERR(("%s: Error: gzip bad hdr\n", __FUNCTION__));
			return -1;
		}

		method = (int) *headers++;
		flags = (int) *headers++;

		if (method != Z_DEFLATED || (flags & RESERVED) != 0) {
			DBUSERR(("%s: Error: gzip bad hdr not a Z_DEFLATED file\n", __FUNCTION__));
			return -1;
		}
	}

	/* Discard time, xflags and OS code: */
	for (len = 0; len < 6; len++)
		unused = *headers++;

	if ((flags & EXTRA_FIELD) != 0) { /* skip the extra field */
		len = (uint32) *headers++;
		len += ((uint32)*headers++)<<8;
		/* len is garbage if EOF but the loop below will quit anyway */
		while (len-- != 0) unused = *headers++;
	}

	if ((flags & ORIG_NAME) != 0) { /* skip the original file name */
		while (*headers++ && (*headers != 0));
	}

	if ((flags & COMMENT) != 0) {   /* skip the .gz file comment */
		while (*headers++ && (*headers != 0));
	}

	if ((flags & HEAD_CRC) != 0) {  /* skip the header crc */
		for (len = 0; len < 2; len++) unused = *headers++;
	}

	headers++;


	/* create space for the uncompressed file */
	/* the space is for trx header, uncompressed image  and nvram file */
	/* with typical compression of 0.6, space double of firmware should be ok */

	decomp_memsize = dbus_info->fwlen * 2;
	dbus_info->decomp_memsize = decomp_memsize;
#if defined(WL_VMEM_NVRAM_DECOMP)
	if (!(file = VMALLOC(osl_handle, decomp_memsize))) {
#else
	if (!(file = MALLOC(osl_handle, decomp_memsize))) {
#endif
		DBUSERR(("%s: check_file : failed malloc\n", __FUNCTION__));
		goto err;
	}

	bzero(file, decomp_memsize);

	/* Initialise the decompression struct */
	d_stream.next_in = NULL;
	d_stream.avail_in = 0;
	d_stream.next_out = NULL;
	d_stream.avail_out = decomp_memsize - trxhdrsize;
	d_stream.zalloc = (alloc_func)0;
	d_stream.zfree = (free_func)0;
	if (inflateInit2(&d_stream, -15) != Z_OK) {
		DBUSERR(("%s: Err: inflateInit2\n", __FUNCTION__));
		goto err;
	}

	/* Inflate the code */
	d_stream.next_in = headers;
	d_stream.avail_in = ltoh32(trx->len);
	d_stream.next_out = (unsigned char*)(file + trxhdrsize);

	status = inflate(&d_stream, Z_SYNC_FLUSH);

	if (status != Z_STREAM_END)	{
		DBUSERR(("%s: Error: decompression failed\n", __FUNCTION__));
		goto err;
	}

	uncmp_crc = *d_stream.next_in++;
	uncmp_crc |= *d_stream.next_in++<<8;
	uncmp_crc |= *d_stream.next_in++<<16;
	uncmp_crc |= *d_stream.next_in++<<24;

	uncmp_len = *d_stream.next_in++;
	uncmp_len |= *d_stream.next_in++<<8;
	uncmp_len |= *d_stream.next_in++<<16;
	uncmp_len |= *d_stream.next_in++<<24;

	actual_len = (int) (d_stream.next_in - (unsigned char *)trx);

	inflateEnd(&d_stream);

	/* Do a CRC32 on the uncompressed data */
	crc_init = crc32(0L, Z_NULL, 0);
	dec_crc = crc32(crc_init, file + trxhdrsize, uncmp_len);

	if (dec_crc != uncmp_crc) {
		DBUSERR(("%s: decompression: bad crc check \n", __FUNCTION__));
		goto err;
	}
	else {
		DBUSTRACE(("%s: decompression: good crc check \n", __FUNCTION__));
	}

	/* rebuild the new trx header and calculate crc */
	newtrx = (struct trx_header *)file;
	newtrx->magic = trx->magic;
	/* add the uncompressed image flag */
	newtrx->flag_version = trx->flag_version;
	newtrx->flag_version  |= htol32(TRX_UNCOMP_IMAGE);
	newtrx->offsets[TRX_OFFSETS_DLFWLEN_IDX] = htol32(uncmp_len);
	newtrx->offsets[TRX_OFFSETS_JUMPTO_IDX] = trx->offsets[TRX_OFFSETS_JUMPTO_IDX];
	newtrx->offsets[TRX_OFFSETS_NVM_LEN_IDX] = trx->offsets[TRX_OFFSETS_NVM_LEN_IDX];

	nvramsize = ltoh32(trx->offsets[TRX_OFFSETS_NVM_LEN_IDX]);

	/* the original firmware has nvram file appended */
	/* copy the nvram file to uncompressed firmware */

	if (nvramsize) {
		if (nvramsize + uncmp_len > decomp_memsize) {
			DBUSERR(("%s: nvram cannot be accomodated\n", __FUNCTION__));
			goto err;
		}
		bcopy(d_stream.next_in, &file[uncmp_len], nvramsize);
		uncmp_len += nvramsize;
	}

	/* add trx header size to uncmp_len */
	uncmp_len += trxhdrsize;
	uncmp_len = ROUNDUP(uncmp_len, 4096);
	newtrx->len	= htol32(uncmp_len);

	/* Calculate CRC over header */
	newtrx->crc32 = hndcrc32((uint8 *)&newtrx->flag_version,
	sizeof(struct trx_header) - OFFSETOF(struct trx_header, flag_version),
	CRC32_INIT_VALUE);

	/* Calculate CRC over data */
	for (i = trxhdrsize; i < (uncmp_len); ++i)
				newtrx->crc32 = hndcrc32((uint8 *)&file[i], 1, newtrx->crc32);
	newtrx->crc32 = htol32(newtrx->crc32);

	dbus_info->orig_fw = dbus_info->fw;
	dbus_info->origfw_len = dbus_info->fwlen;
	dbus_info->image = dbus_info->fw = file;
	dbus_info->image_len = dbus_info->fwlen = uncmp_len;

	DBUSERR(("decompression: done len = %d \n", uncmp_len));
	return 0;

err:
	if (file) {
#if defined(WL_VMEM_NVRAM_DECOMP)
	VFREE(osl_handle, file, decomp_memsize);
#else
	MFREE(osl_handle, file, decomp_memsize);
#endif
	}
	return -1;
}

#endif /*  WL_FW_DECOMP */
#endif /* defined(BCM_DNGL_EMBEDIMAGE) || defined(BCMEMBEDIMAGE) */
void *
dbus_zlib_calloc(int num, int size)
{
	uint *ptr;
	uint totalsize;

	if (osl_handle == NULL)
		return NULL;

	totalsize = (num * size) + sizeof(uint);

#if defined(WL_VMEM_NVRAM_DECOMP)
	ptr  = VMALLOC(osl_handle, totalsize);
#else
	ptr  = MALLOC(osl_handle, totalsize);
#endif

	if (ptr == NULL)
		return NULL;
	bzero(ptr, totalsize);

		/* store the size in the first integer space */

		ptr[0] = totalsize;

		return ((void *) &ptr[1]);
}

void
dbus_zlib_free(void *ptr)
{
	uint totalsize;
	uchar *memptr = (uchar *)ptr;

	if (ptr && osl_handle) {
		memptr -= sizeof(uint);
		totalsize = *(uint *) memptr;
#if defined(WL_VMEM_NVRAM_DECOMP)
		VFREE(osl_handle, memptr, totalsize);
#else
		MFREE(osl_handle, memptr, totalsize);
#endif
	}
}

void *
dbus_get_devinfo(const dbus_pub_t *pub)
{
	return pub->dev_info;
}
