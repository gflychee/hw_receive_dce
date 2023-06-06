#include "efvi-udp.h"

#include "etherfabric/base.h"
#include "etherfabric/pd.h"
#include "etherfabric/vi.h"
#include "etherfabric/capabilities.h"
#include "etherfabric/memreg.h"

#include <arpa/inet.h>
#include <net/if.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PKT_BUF_SIZE 2048
#define NUM_POLL_EVENTS 4
#define REFILL_BATCH_SIZE  16
#define RX_BUFFER_NR (256 - REFILL_BATCH_SIZE)
#define TX_BUFFER_NR 256
#define BUFFER_NR (RX_BUFFER_NR + TX_BUFFER_NR)

#define PAGE_SIZE 4096

#define offsetof __builtin_offsetof
#define __cacheline_aligned __attribute__((aligned(EF_VI_DMA_ALIGN)))

struct pkt_buf {
	int id;
	ef_addr dma_addr;
	struct pkt_buf *next;
	struct timespec hw_ts, sw_ts;
	unsigned char *pkt;
	uint8_t __cacheline_aligned buf[1];
};

struct efd {
	char *ifname;
	ef_driver_handle drv;
	ef_pd pd;
	ef_vi vi;

	enum ef_vi_flags flags;
	struct ef_memreg memreg;
	void *mem;
	int refill_level;
	int refill_min;
	int rx_prefix_len;
	void (*rx_callback)(struct timespec *, void *pkt, int len);
	void (*tx_callback)(struct timespec *, void *pkt, int len);

	struct pkt_buf *free_pkt_bufs;
	int free_pkt_bufs_nr;
	struct pkt_buf *bufs[BUFFER_NR];
};

static inline void
pkt_buf_free(struct efd *efd, struct pkt_buf *pkt_buf)
{
	pkt_buf->next = efd->free_pkt_bufs;
	efd->free_pkt_bufs = pkt_buf;
	++efd->free_pkt_bufs_nr;
}

static void
efd_refill_rx_ring(struct efd *efd)
{
	struct pkt_buf *pb;

	if (ef_vi_receive_fill_level(&efd->vi) > efd->refill_level
	    || efd->free_pkt_bufs_nr < REFILL_BATCH_SIZE)
		return;

	do {
		int i;

		for (i = 0; i < REFILL_BATCH_SIZE; ++i) {
			pb = efd->free_pkt_bufs;
			efd->free_pkt_bufs = efd->free_pkt_bufs->next;
			--efd->free_pkt_bufs_nr;
			ef_vi_receive_init(&efd->vi, pb->dma_addr, pb->id);
		}
	} while (ef_vi_receive_fill_level(&efd->vi) < efd->refill_min
		 && efd->free_pkt_bufs_nr >= REFILL_BATCH_SIZE);

	ef_vi_receive_push(&efd->vi);
}

static inline void
efd_handle_rx_packet(struct efd *efd, int id, int len)
{
	unsigned ts_flags;
	struct timespec *ts = NULL;
	struct pkt_buf *pb = efd->bufs[id];

	if (efd->flags & EF_VI_RX_TIMESTAMPS) {
		ef_vi_receive_get_timestamp_with_sync_flags(&efd->vi, pb->buf, &pb->hw_ts, &ts_flags);
		ts = &pb->hw_ts;
	}

	if (efd->rx_callback)
		efd->rx_callback(ts, pb->pkt, len);

	pkt_buf_free(efd, pb);
}

static void
efd_poll_evq(struct efd *efd)
{
	int i, n_ev, id;
	ef_event evs[NUM_POLL_EVENTS];

	n_ev = ef_eventq_poll(&efd->vi, evs, NUM_POLL_EVENTS);

	for (i = 0; i < n_ev; ++i) {
		switch (EF_EVENT_TYPE(evs[i])) {
		case EF_EVENT_TYPE_RX: {
			efd_handle_rx_packet(efd, EF_EVENT_RX_RQ_ID(evs[i]),
					     EF_EVENT_RX_BYTES(evs[i]));
			break;
		}
		case EF_EVENT_TYPE_RX_DISCARD: {
			fprintf(stderr, "ERROR: RX_DISCARD type=%d", 
				EF_EVENT_RX_DISCARD_TYPE(evs[i]));
			efd_handle_rx_packet(efd, EF_EVENT_RX_RQ_ID(evs[i]),
					     EF_EVENT_RX_BYTES(evs[i]));
			break;
		}
		case EF_EVENT_TYPE_TX_WITH_TIMESTAMP: {
			id = EF_EVENT_TX_WITH_TIMESTAMP_RQ_ID(evs[i]);
			struct pkt_buf *pb = efd->bufs[id];
			struct timespec hw_ts, sw_ts;

			hw_ts.tv_sec  = EF_EVENT_TX_WITH_TIMESTAMP_SEC(evs[i]);
			hw_ts.tv_nsec = EF_EVENT_TX_WITH_TIMESTAMP_NSEC(evs[i]);

			clock_gettime(CLOCK_REALTIME, &sw_ts);
			unsigned long hw_tdiff = (hw_ts.tv_sec - pb->hw_ts.tv_sec) * 1000000000UL + (hw_ts.tv_nsec - pb->hw_ts.tv_nsec);
			unsigned long sw_tdiff = (sw_ts.tv_sec - pb->sw_ts.tv_sec) * 1000000000UL + (sw_ts.tv_nsec - pb->sw_ts.tv_nsec);
			printf("%lu %lu\n", hw_tdiff, sw_tdiff);
			pkt_buf_free(efd, pb);
			break;
		}
		default: {
			fprintf(stderr, "ERROR: unexpected event type=%d\n", (int)EF_EVENT_TYPE(evs[i]));
			break;
		}
		}
	}
}

static int
efd_add_filter(struct efd *efd, const char *ip, int port, int protocol)
{
	struct sockaddr_in sa_local = {};

	sa_local.sin_family = AF_INET;
	sa_local.sin_port = htons(port);
	inet_pton(AF_INET, ip, &sa_local.sin_addr);

	ef_filter_spec filter_spec;

	ef_filter_spec_init(&filter_spec, EF_FILTER_FLAG_NONE);
	ef_filter_spec_set_ip4_local(&filter_spec, protocol, sa_local.sin_addr.s_addr, sa_local.sin_port);

	return ef_vi_filter_add(&efd->vi, efd->drv, &filter_spec, NULL);
}

int
efd_add_udp_filter(struct efd *efd, const char *ip, int port)
{
	return efd_add_filter(efd, ip, port, IPPROTO_UDP);
}

int
efd_add_unicast_all_filter(struct efd *efd)
{
	ef_filter_spec filter_spec;

	ef_filter_spec_init(&filter_spec, EF_FILTER_FLAG_NONE);
	ef_filter_spec_set_unicast_all(&filter_spec);

	return ef_vi_filter_add(&efd->vi, efd->drv, &filter_spec, NULL);
}

int
efd_add_multicast_all_filter(struct efd *efd)
{
	ef_filter_spec filter_spec;

	ef_filter_spec_init(&filter_spec, EF_FILTER_FLAG_NONE);
	ef_filter_spec_set_multicast_all(&filter_spec);

	return ef_vi_filter_add(&efd->vi, efd->drv, &filter_spec, NULL);
}

static int
efd_pkt_buffer_init(struct efd *efd)
{
	const int bytes = BUFFER_NR * PKT_BUF_SIZE;

	efd->mem = NULL;

	if (posix_memalign(&efd->mem, PAGE_SIZE, bytes))
		return -1;

	ef_memreg_alloc(&efd->memreg, efd->drv, &efd->pd, efd->drv, efd->mem, bytes);

	efd->free_pkt_bufs_nr = 0;
	efd->free_pkt_bufs = NULL;

	int i;

	for (i = 0; i < BUFFER_NR; ++i) {
		struct pkt_buf *pb = (struct pkt_buf *)((char*)efd->mem + i * PKT_BUF_SIZE);
		efd->bufs[i] = pb;
		pb->id = i;
		pb->pkt = pb->buf + efd->rx_prefix_len;
		pb->dma_addr = ef_memreg_dma_addr(&efd->memreg, i * PKT_BUF_SIZE);
		pb->dma_addr += offsetof(struct pkt_buf, buf);
		pb->next = efd->free_pkt_bufs;
		efd->free_pkt_bufs = pb;
		++efd->free_pkt_bufs_nr;
	}

	return 0;
}

struct efd *
efd_alloc(const char *ifname, unsigned flags)
{
	const unsigned ifindex = if_nametoindex(ifname);
	struct efd *efd;

	if ( (efd = malloc(sizeof(*efd))) == NULL)
		return NULL;

	efd->flags = flags;
	efd->ifname = strdup(ifname);
	ef_driver_open(&efd->drv);
	ef_pd_alloc(&efd->pd, efd->drv, ifindex, EF_PD_DEFAULT);
	ef_vi_alloc_from_pd(&efd->vi, efd->drv, &efd->pd, efd->drv, -1, -1, -1, NULL, -1, flags);
	efd->rx_prefix_len = ef_vi_receive_prefix_len(&efd->vi);

	if (efd_pkt_buffer_init(efd))
		return NULL;

	efd->refill_level = RX_BUFFER_NR - REFILL_BATCH_SIZE;
	efd->refill_min = RX_BUFFER_NR / 2;

	while (ef_vi_receive_fill_level(&efd->vi) <= efd->refill_level)
		efd_refill_rx_ring(efd);

	return efd;
}

void
efd_destroy(struct efd *efd)
{
	free(efd->mem);
	free(efd->ifname);
	ef_memreg_free(&efd->memreg, efd->drv);
	ef_vi_free(&efd->vi, efd->drv);
	ef_pd_free(&efd->pd, efd->drv);
	ef_driver_close(efd->drv);
	free(efd);
}

void
efd_poll(struct efd **efd, int n)
{
	int i;
	while (1) {
		for (i = 0; i < n; ++i)
			efd_poll_evq(efd[i]);

		for (i = 0; i < n; ++i)
			efd_refill_rx_ring(efd[i]);
	}
}

void
efd_set_callback(struct efd *efd, void (*rx_callback)(struct timespec *, void *, int), void (*tx_callback)(struct timespec *, void *, int))
{
	efd->rx_callback = rx_callback;
	efd->tx_callback = tx_callback;
}
