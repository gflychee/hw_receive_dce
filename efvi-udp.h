#include "common.h"

#include "etherfabric/ef_vi.h"

CDECLS_BEGIN;

struct efd;

struct efd *efd_alloc(const char *ifname, unsigned flags);
void efd_destroy(struct efd *efd);

void efd_poll(struct efd **efd, int n);
void efd_set_callback(struct efd *efd, void (*rx_callback)(struct timespec *, void *, int),
		void (*tx_callback)(struct timespec *, void *, int));
int efd_add_udp_filter(struct efd *efd, const char *ip, int port);
int efd_add_unicast_all_filter(struct efd *efd);
int efd_add_multicast_all_filter(struct efd *efd);

CDECLS_END;
