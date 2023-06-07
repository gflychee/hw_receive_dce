#include <winterfell/instab.h>
#include <winterfell/mdclient.h>
#include "efvi-udp.h"
#include "dce-lv2.h"
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/ethernet.h>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#include <math.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <set>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include <cfloat>
#include <map>
#include <onload/extensions.h>
#pragma pack(push)
#pragma pack(1)

struct DepthMarketDataField
{
    unsigned int type;
    ///交易所时间戳
    int64_t ExchTime;
    ///最新价
    double  LastPrice;
    ///数量
    int Volume;
    ///成交金额
    double  Turnover;
    ///持仓量
    int  OpenInterest;
    ///涨停板价
    double UpperLimitPrice;
    ///跌停板价
    double LowerLimitPrice;
    ///申买价一
    double  BidPrice1;
    ///申卖价一
    double  AskPrice1;
    ///申买量一
    int BidVolume1;
    ///申卖量一
    int AskVolume1;
    ///申买价二
    double  BidPrice2;
    ///申卖价二
    double  AskPrice2;
    ///申买量二
    int BidVolume2;
    ///申卖量二
    int AskVolume2;
    ///申买价三
    double  BidPrice3;
    ///申卖价三
    double  AskPrice3;
    ///申买量三
    int BidVolume3;
    ///申卖量三
    int AskVolume3;
    ///申买价四
    double  BidPrice4;
    ///申卖价四
    double  AskPrice4;
    ///申买量四
    int BidVolume4;
    ///申卖量四
    int AskVolume4;
    ///申买价五
    double  BidPrice5;
    ///申卖价五
    double  AskPrice5;
    ///申买量五
    int BidVolume5;
    ///申卖量五
    int AskVolume5;
    ///合约代码
    char    InstrumentID[31];
};

struct efvi_hw_czce_dce_mc_client {
    struct mdclient mdclient;
    char   ifname[64];
    char   udp_srvip[32];
    char   local_ip[32];
    int    udp_port;
    int    debug;
};

#pragma pack(pop)

using namespace std;

struct mdclient *pclient;
map<string, DepthMarketDataField> mdmap;

static inline long get_exchtime(const char *time, int msec)
{
    long t;

    t  = (time[0] - '0') * 10 + (time[1] - '0');
    t *= 60;
    t += (time[3] - '0') * 10 + (time[4] - '0');
    t *= 60;
    t += (time[6] - '0') * 10 + (time[7] - '0');
    if (t > 3600 * 18)
        t -= 3600 * 24;

    t *= 1000;
    t += msec;

    return t;
}

void UDPPush(uint8_t *pkt, int pkt_len)
{
    long rece_time = currtime();
    // 解析第一个uint16_t字段判断数据类型
    uint16_t type = *(uint16_t*)pkt;
    long exchtime = 0;
    switch(type) {
        case 1792:
        {
            new_fld_snap_best_quot_t *best = (new_fld_snap_best_quot_t *)pkt;
            DepthMarketDataField *mds;
            int insidx = ins2idx(pclient->instab, (const char*)best->contract_id);
            if (strlen((const char*)best->contract_id) > 8 || insidx == -1) { //过滤期权，组合合约, 过滤wf没订阅的合约
                break;
            } else {
                mds = &mdmap[(const char*)best->contract_id];
                strncpy(mds->InstrumentID, (const char*)best->contract_id, sizeof(mds->InstrumentID));
                exchtime = get_exchtime((const char*)best->gen_time, strtol((char*)best->gen_time + 9, NULL, 10));
                mds->ExchTime = exchtime * 1000000l;
                mds->LastPrice = best->last_price != DBL_MAX ? best->last_price : 0.0;
                mds->Volume = best->match_tot_qty;
                mds->Turnover = best->turnover;
                mds->OpenInterest = best->open_interest;
                mds->UpperLimitPrice = best->rise_limit;
                mds->LowerLimitPrice = best->fall_limit;
                mds->BidPrice1 = best->bid_price != DBL_MAX ? best->bid_price : 0.0;
                mds->BidVolume1 = best->bid_qty;
                mds->AskPrice1 = best->ask_price != DBL_MAX ? best->ask_price : 0.0;
                mds->AskVolume1 = best->ask_qty;

                // 写行情到memdb
                uint32_t mdslot;
                struct md_static *mdst = (struct md_static *)get_md_static(pclient->instab, insidx);
                struct md_snapshot *mdsn = snapshottab_get_next_slot(pclient->sstab, insidx, &mdslot);

                if (unlikely(mdst->upper_limit == 0.0)) {
                    mdst->upper_limit = mds->UpperLimitPrice;
                    mdst->lower_limit = mds->LowerLimitPrice;
                }
                        
                mdsn->type = MDT_Level1;
                mdsn->exchange_time = mds->ExchTime;
                mdsn->recv_time = rece_time;
                mdsn->last_price = mds->LastPrice;
                mdsn->volume = mds->Volume;
                mdsn->turnover = mds->Turnover;
                mdsn->open_interest = mds->OpenInterest;
                mdsn->bid_price[0] = mds->BidPrice1;
                mdsn->bid_size[0]  = mds->BidVolume1;
                mdsn->ask_price[0] = mds->AskPrice1;
                mdsn->ask_size[0]  = mds->AskVolume1;
                mdsn->decode_time = currtime();
                pclient->output(pclient, mdst, mdslot);
                struct efvi_hw_czce_dce_mc_client *efvi_mc = (struct efvi_hw_czce_dce_mc_client *)pclient->container;
                if (efvi_mc->debug) {
                    printf("label md_tick:%s,%d,%d,%ld,%ld,%ld,%f,%ld,%f,%f,%d,%f,%d,%f,%f\n",\
                    best->contract_id, mdsn->type, mdsn->open_interest, mdsn->recv_time, mdsn->decode_time, mdsn->exchange_time, mdsn->last_price,\
                    mdsn->volume, mdsn->turnover,mdsn->bid_price[0],mdsn->bid_size[0],mdsn->ask_price[0],mdsn->ask_size[0],mdst->upper_limit, mdst->lower_limit);
                    fflush(stdout);
                }
            }
            break;
        }
        case 1798:
        {
            new_fld_snap_mbl_t *deep = (new_fld_snap_mbl_t *)pkt;
            DepthMarketDataField *mds;
            int insidx = ins2idx(pclient->instab, (const char*)deep->contract_id);
            if (strlen((const char*)deep->contract_id) > 8 || insidx == -1) { //过滤期权，组合合约, 过滤wf没订阅的合约
                break;
            } else {
                mds = &mdmap[(const char*)deep->contract_id];
                if (strlen(mds->InstrumentID) == 0) {
                    break;
                }
                exchtime = get_exchtime((const char*)deep->gen_time, strtol((char*)deep->gen_time + 9, NULL, 10));
                mds->ExchTime = exchtime * 1000000l;
                
                // 写行情到memdb
                uint32_t mdslot;
                struct md_static *mdst = (struct md_static *)get_md_static(pclient->instab, insidx);
                struct md_snapshot *mdsn = snapshottab_get_next_slot(pclient->sstab, insidx, &mdslot);

                mdsn->type = MDT_Level5;
                mdsn->exchange_time = mds->ExchTime;
                mdsn->recv_time = rece_time;
                mdsn->last_price = mds->LastPrice;
                mdsn->volume = mds->Volume;
                mdsn->turnover = mds->Turnover;
                mdsn->open_interest = mds->OpenInterest;
                mdsn->bid_price[0] = deep->bid_1 != DBL_MAX ? deep->bid_1 : 0.0;
                mdsn->bid_size[0]  = deep->bid_1_qty;
                mdsn->ask_price[0] = deep->ask_1 != DBL_MAX ? deep->ask_1 : 0.0;
                mdsn->ask_size[0]  = deep->ask_1_qty;
                mdsn->bid_price[1] = deep->bid_2 != DBL_MAX ? deep->bid_2 : 0.0;
                mdsn->bid_size[1]  = deep->bid_2_qty;
                mdsn->ask_price[1] = deep->ask_2 != DBL_MAX ? deep->ask_2 : 0.0;
                mdsn->ask_size[1]  = deep->ask_2_qty;
                mdsn->bid_price[2] = deep->bid_3 != DBL_MAX ? deep->bid_3 : 0.0;
                mdsn->bid_size[2]  = deep->bid_3_qty;
                mdsn->ask_price[2] = deep->ask_3 != DBL_MAX ? deep->ask_3 : 0.0;
                mdsn->ask_size[2]  = deep->ask_3_qty;
                mdsn->bid_price[3] = deep->bid_4 != DBL_MAX ? deep->bid_4 : 0.0;
                mdsn->bid_size[3]  = deep->bid_4_qty;
                mdsn->ask_price[3] = deep->ask_4 != DBL_MAX ? deep->ask_4 : 0.0;
                mdsn->ask_size[3]  = deep->ask_4_qty;
                mdsn->bid_price[4] = deep->bid_5 != DBL_MAX ? deep->bid_5 : 0.0;
                mdsn->bid_size[4]  = deep->bid_5_qty;
                mdsn->ask_price[4] = deep->ask_5 != DBL_MAX ? deep->ask_5 : 0.0;
                mdsn->ask_size[4]  = deep->ask_5_qty;

                mdsn->decode_time = currtime();
                pclient->output(pclient, mdst, mdslot);

                struct efvi_hw_czce_dce_mc_client *efvi_mc = (struct efvi_hw_czce_dce_mc_client *)pclient->container;
                if (efvi_mc->debug) {
                    printf("label md_tick:%s,%d,%d,%ld,%ld,%ld,%f,%ld,%f,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%f\n",\
                    deep->contract_id, mdsn->type, mdsn->open_interest, mdsn->recv_time, mdsn->decode_time, mdsn->exchange_time, mdsn->last_price, mdsn->volume, mdsn->turnover,\
                    mdsn->bid_price[0],mdsn->bid_size[0],mdsn->ask_price[0],mdsn->ask_size[0],mdsn->bid_price[1],mdsn->bid_size[1],mdsn->ask_price[1],mdsn->ask_size[1],\
                    mdsn->bid_price[2],mdsn->bid_size[2],mdsn->ask_price[2],mdsn->ask_size[2],mdsn->bid_price[3],mdsn->bid_size[3],mdsn->ask_price[3],mdsn->ask_size[3],\
                    mdsn->bid_price[4],mdsn->bid_size[4],mdsn->ask_price[4],mdsn->ask_size[4],mdst->upper_limit, mdst->lower_limit);
                    fflush(stdout);
                }
        	}
            break;
        }
        default:
        {
            break;
        }
    }
    return;
}

void parse_ether(struct timespec *ts, void *packet, int len) {
    struct ether_header *ethhdr = (struct ether_header *)packet;
    if (ntohs(ethhdr->ether_type) != ETHERTYPE_IP)
        return;

    int poff = sizeof(struct ether_header);

    struct iphdr *ip = (struct iphdr *)((uint8_t*)packet + poff);

    poff += 4 * ip->ihl;
    if (ip->protocol != IPPROTO_UDP)
        return;

    struct udphdr *udp_hdr = (struct udphdr *)((uint8_t*)packet + poff);

    poff += sizeof(struct udphdr);

    //int src_port = ntohs(udp_hdr->source);

    int dst_port = ntohs(udp_hdr->dest);
    // filter
    struct efvi_hw_czce_dce_mc_client *efvi_mc = (struct efvi_hw_czce_dce_mc_client *)pclient->container;
    if (dst_port == efvi_mc->udp_port)
        UDPPush((uint8_t*)((uint8_t*)packet + poff), ntohs(udp_hdr->len) - sizeof(struct udphdr));
}

static void mcast_add_group(int sock, const char *mcast_ip, const char *local_ip) {
    struct ip_mreq group;
    group.imr_multiaddr.s_addr = inet_addr(mcast_ip);

    if (!strcmp(local_ip, "any"))
        group.imr_interface.s_addr = INADDR_ANY;
    else
        group.imr_interface.s_addr = inet_addr(local_ip);
    setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group));
}

static void run(struct mdclient *client) {
    struct efvi_hw_czce_dce_mc_client *efvi_mc = (struct efvi_hw_czce_dce_mc_client *)client->container;
    pclient = client;
    struct efd *efd = efd_alloc(efvi_mc->ifname, EF_VI_FLAGS_DEFAULT);
    efd_set_callback(efd, parse_ether, NULL);
    efd_add_udp_filter(efd, efvi_mc->udp_srvip, efvi_mc->udp_port);
    int sock = onload_socket_nonaccel(AF_INET, SOCK_DGRAM, 0);
    mcast_add_group(sock, efvi_mc->udp_srvip, efvi_mc->local_ip);

    efd_poll(&efd, 1);
}


static struct mdclient *efvi_hw_czce_dce_mc_create(cfg_t *cfg, struct memdb *memdb) {
    struct efvi_hw_czce_dce_mc_client *efvi_mc = new struct efvi_hw_czce_dce_mc_client;
    struct mdclient *client = &efvi_mc->mdclient;

    mdclient_init(client, cfg, memdb);

    const char *ifname;
    cfg_get_string(cfg, "ifname", &ifname);
    snprintf(efvi_mc->ifname, sizeof(efvi_mc->ifname), "%s", ifname);

    const char *udp_srvip;
    cfg_get_string(cfg, "udp_srvip", &udp_srvip);
    snprintf(efvi_mc->udp_srvip, sizeof(efvi_mc->udp_srvip), "%s", udp_srvip);

    const char *local_ip;
    cfg_get_string(cfg, "local_ip", &local_ip);
    snprintf(efvi_mc->local_ip, sizeof(efvi_mc->local_ip), "%s", local_ip);

    cfg_get_int(cfg, "udp_port", &efvi_mc->udp_port);
    cfg_get_int(cfg, "debug", &efvi_mc->debug);

    printf("label efvi_mc->ifname: %s\n", efvi_mc->ifname);
    printf("label efvi_mc->udp_srvip: %s\n", efvi_mc->udp_srvip);
    printf("label efvi_mc->udp_port: %d\n", efvi_mc->udp_port);
    printf("label efvi_mc->local_ip: %s\n", efvi_mc->local_ip);
    printf("label efvi_mc->debug: %d\n", efvi_mc->debug);
    fflush(stdout);
    client->run = run;
    client->decoder = NULL;
    client->flags = 0;
    client->container = efvi_mc;
    return client;
}

static struct mdsrc_module mdsrc_efvi_hw_czce_receive_dce_mc = {
    .create = efvi_hw_czce_dce_mc_create,
    .api = "efvi-hw-czce-receive-dce-mc"
};

mdsrc_module_register(&mdsrc_efvi_hw_czce_receive_dce_mc);
