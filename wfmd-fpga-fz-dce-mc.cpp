#include <winterfell/instab.h>
#include <winterfell/mdclient.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/if_ether.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <string.h>
#include <math.h>
#include "udp.h"
#include "dce-nano.h"
//#include "utils.h"

#include <exanic/exanic.h>
#include <exanic/fifo_rx.h>
#include <exanic/fifo_tx.h>
#include <exanic/util.h>
#include <signal.h>
#include <pthread.h>
#include <thread>
#include <limits.h>
#include <cfloat>
#include <sys/time.h>
#include <map>
#include <fstream>

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

#pragma pack(pop)

// 全局变量
constexpr double precision = 0.000001;
std::map<std::string, DepthMarketDataField> mdmap;
std::map<std::string, DepthMarketDataField> mds_lv1;
struct mdclient *pclient;
pthread_spinlock_t write_lock;

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

static long get_lv1_exchtime(uint64_t epoch_ns) {
    long rt = (long)epoch_ns;
    if (rt > 64800000000000)
        rt -= 86400000000000;
    return rt;
}


struct exanic_dce_mc_client {
	struct mdclient mdclient;
	char   lv1_exanic[64];
	char   lv2_exanic[64];
	int    debug;
	int    merge;
        int    lv1_cpu;


	static void *recv_udp_lv1(void *arg);

	void run_lv1_thread();
};

void exanic_dce_mc_client::run_lv1_thread() {
	pthread_t lv1_thread;
	pthread_create(&lv1_thread, NULL, recv_udp_lv1, this);
}

void *exanic_dce_mc_client::recv_udp_lv1(void *arg) {
    struct exanic_dce_mc_client *self = (struct exanic_dce_mc_client *)arg;
	printf("start receiving udp lv1...\n");
	printf("ifname = %s\n", self->lv1_exanic);
    printf("lv1_cpu = %d\n", self->lv1_cpu);
    fflush(stdout);
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(self->lv1_cpu, &cpuset);
    auto rc = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    if (rc)
    {
        printf("setting affinity failed!\n");
        fflush(stdout);
        return NULL;
    }

	exanic_t *nic = exanic_acquire_handle(self->lv1_exanic);

	if (!nic) {
        printf("failed to acquire NIC handle:%s\n", exanic_get_last_error());
        fflush(stdout);
        exit(-1);
    }

    exanic_rx_t *rx1 = exanic_acquire_rx_buffer(nic, 1, 0);

    char rxbuf[1024];

    memset(rxbuf, 0, sizeof(rxbuf));

    while(1) {
        ssize_t size = exanic_receive_frame(rx1, rxbuf, sizeof(rxbuf), 0);
		if (size > 0) {
	    	if (*(uint32_t *)rxbuf == 33554432) {
                long rece_time = currtime();
				dce_lv1_t *dce_lv1 = (dce_lv1_t *)rxbuf;
                //过滤期权，组合合约
                if (strlen((const char*)dce_lv1->contract_name) > 8) {
                    continue;
                }
                //过滤wf没订阅的合约
                const int insidx = ins2idx(pclient->instab, (const char*)dce_lv1->contract_name);
                if (insidx == -1) {
                    continue;
                }
                pthread_spin_lock(&write_lock);
                DepthMarketDataField *md = &mdmap[(const char*)dce_lv1->contract_name];
                //printf("time:%lu\n",dce_lv1->send_time);
                if ( dce_lv1->total_qty > md->Volume || 
                    (dce_lv1->total_qty == md->Volume && 
                    !(  fabs(md->BidPrice1 - dce_lv1->bid_px / 10000.0) < precision &&
                        fabs(md->AskPrice1 - dce_lv1->ask_px / 10000.0) < precision &&
                        md->BidVolume1 == dce_lv1->bid_qty &&
                        md->AskVolume1 == dce_lv1->ask_qty &&
                        fabs(md->LastPrice - dce_lv1->last_px / 10000.0) < precision && 
                        fabs(md->Turnover - dce_lv1->turnover / 10000.0) < precision && 
                        md->OpenInterest == dce_lv1->open_interest
                    )))
                {
                    // 写行情到memdb
                    uint32_t mdslot;
                    struct md_static *mdst = (struct md_static *)get_md_static(pclient->instab, insidx);
                    struct md_snapshot *mdsn = snapshottab_get_next_slot(pclient->sstab, insidx, &mdslot);
                    mdsn->type = MDT_Level1;
                    mdsn->exchange_time = get_lv1_exchtime(dce_lv1->send_time % 86400000000000 + 28800000000000); //to do
                    mdsn->recv_time = rece_time;
                    mdsn->last_price = dce_lv1->last_px != 0x7FFFFFFFFFFFFFFF ? dce_lv1->last_px / 10000.0: 0.0;
                    mdsn->volume = dce_lv1->total_qty;
                    mdsn->turnover = dce_lv1->turnover / 10000.0;
                    mdsn->open_interest = dce_lv1->open_interest;
                    mdsn->bid_price[0] = dce_lv1->bid_px != 0x7FFFFFFFFFFFFFFF ? dce_lv1->bid_px / 10000.0: 0.0;
                    mdsn->bid_size[0]  = dce_lv1->bid_qty;
                    mdsn->ask_price[0] = dce_lv1->ask_px != 0x7FFFFFFFFFFFFFFF ? dce_lv1->ask_px / 10000.0: 0.0;
                    mdsn->ask_size[0]  = dce_lv1->ask_qty;
                    mdsn->decode_time = currtime();
                    pclient->output(pclient, mdst, mdslot);


                    // 更新最新行情
                    md->type = 0;
                    //md->ExchTime = dce_lv1->send_time; //to do
                    md->LastPrice = mdsn->last_price;
                    md->Volume = mdsn->volume;
                    md->Turnover = mdsn->turnover;
                    md->OpenInterest = mdsn->open_interest;
                    md->BidPrice1  = mdsn->bid_price[0];
                    md->BidVolume1 = mdsn->bid_size[0];
                    md->AskPrice1  = mdsn->ask_price[0];
                    md->AskVolume1 = mdsn->ask_size[0];
                }
                pthread_spin_unlock(&write_lock);
	    	}
		}
    }
	return NULL;
}

void packetHandler(char * packet, ssize_t size)
{
    int merge = ((struct exanic_dce_mc_client *)pclient->container)->merge;
    dce_dmdp_t *dmdp = (dce_dmdp_t *)packet;
    if (size >= 0x19 && dmdp->pkg_size <= size)
    {
        do {
            switch(dmdp->pkg_type)
            {
            case 1792:
            {
                long rece_time = currtime();
                int8_t * pd = (int8_t *)dmdp + sizeof(dce_dmdp_t);
                dce_dmdp_field_t *field = (dce_dmdp_field_t *)pd;
                DepthMarketDataField *md;
                DepthMarketDataField *mds;
                int insidx = -1;
                long exchtime = 0;
                if (field->field_id == 1792)
                {
                    fld_quot_t *quote = (fld_quot_t *)pd;
                    insidx = ins2idx(pclient->instab, (const char*)quote->contract_id);
                    if (strlen((const char*)quote->contract_id) > 8 || insidx == -1) { //过滤期权，组合合约, 过滤wf没订阅的合约
                    	dmdp = (dce_dmdp_t *)((int8_t *)dmdp + dmdp->pkg_size);
            			break;
        			} else {
                        if (merge == 1) {
                            pthread_spin_lock(&write_lock);
                            md = &mdmap[(const char*)quote->contract_id];
                        }
                        mds = &mds_lv1[(const char*)quote->contract_id];
                        exchtime = get_exchtime((const char*)quote->gen_time, strtol((char*)quote->gen_time + 9, NULL, 10));
                        mds->ExchTime = exchtime * 1000000l;
        			}
                }
                pd += field->field_size;
                field = (dce_dmdp_field_t *)pd;
                
                if (field->field_id == 1794)
                {
                    fld_snap_best_quot_t *best = (fld_snap_best_quot_t *)field;
                    
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

                    if (merge == 0) {
                        // 写行情到memdb
                        uint32_t mdslot;
                        struct md_static *mdst = (struct md_static *)get_md_static(pclient->instab, insidx);
                        struct md_snapshot *mdsn = snapshottab_get_next_slot(pclient->sstab, insidx, &mdslot);
                        
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
                    } else if (
                        mds->Volume > md->Volume || 
                        (mds->Volume == md->Volume && 
                        !(  fabs(md->BidPrice1 - mds->BidPrice1) < precision &&
                        fabs(md->AskPrice1 - mds->AskPrice1) < precision &&
                        md->BidVolume1 == mds->BidVolume1 &&
                        md->AskVolume1 == mds->AskVolume1 &&
                        fabs(md->LastPrice - mds->LastPrice) < precision && 
                        fabs(md->Turnover - mds->Turnover) < precision && 
                        md->OpenInterest == mds->OpenInterest     
                    ))) 
                    {
                        // 写行情到memdb
                        uint32_t mdslot;
                        struct md_static *mdst = (struct md_static *)get_md_static(pclient->instab, insidx);
                        struct md_snapshot *mdsn = snapshottab_get_next_slot(pclient->sstab, insidx, &mdslot);
                        
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
    
    
                        // 更新最新行情
                        md->type = 0;
                        //md->ExchTime = mds->ExchTime;
                        md->LastPrice = mds->LastPrice;
                        md->Volume = mds->Volume;
                        md->Turnover = mds->Turnover;
                        md->OpenInterest = mds->OpenInterest;
                        md->BidPrice1  = mds->BidPrice1;
                        md->BidVolume1 = mds->BidVolume1;
                        md->AskPrice1  = mds->AskPrice1;
                        md->AskVolume1 = mds->AskVolume1;
                    }
                }

                if (merge == 1) {
                    pthread_spin_unlock(&write_lock);
                }
                
                dmdp = (dce_dmdp_t *)((int8_t *)dmdp + dmdp->pkg_size);
                break;
            }
            case 1798:
            {
                long rece_time = currtime();
                int8_t * pd = (int8_t *)dmdp + sizeof(dce_dmdp_t);
                dce_dmdp_field_t *field = (dce_dmdp_field_t *)pd;
                DepthMarketDataField *md;
                DepthMarketDataField *mds;
                int insidx = -1;
		long exchtime = 0;
                if (field->field_id == 1792)
                {
                    fld_quot_t *quote = (fld_quot_t *)pd;
                    insidx = ins2idx(pclient->instab, (const char*)quote->contract_id);
                    if (strlen((const char*)quote->contract_id) > 8 || insidx == -1) { //过滤期权，组合合约, 过滤wf没订阅的合约
                    	dmdp = (dce_dmdp_t *)((int8_t *)dmdp + dmdp->pkg_size);
            			break;
        			} else {
                        if (merge == 1) {
                            pthread_spin_lock(&write_lock);
                            md = &mdmap[(const char*)quote->contract_id];
                        }
                        mds = &mds_lv1[(const char*)quote->contract_id];
			exchtime = get_exchtime((const char*)quote->gen_time, strtol((char*)quote->gen_time + 9, NULL, 10));
                        mds->ExchTime = exchtime * 1000000l;
        			}
                }
                pd += field->field_size;
                field = (dce_dmdp_field_t *)pd;

                if (field->field_id == 1798)
                {
                    fld_snap_mbl_t *deep = (fld_snap_mbl_t *)field;
                    if (merge == 1 && mds->Volume < md->Volume) {
                        dmdp = (dce_dmdp_t *)((int8_t *)dmdp + dmdp->pkg_size);
                        break;
                    }

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
                }

                if (merge == 1) {
                    pthread_spin_unlock(&write_lock);
                }

                dmdp = (dce_dmdp_t *)((int8_t *)dmdp + dmdp->pkg_size);
                break;
            }
            default:
            {
                dmdp = (dce_dmdp_t *)((int8_t *)dmdp + dmdp->pkg_size);
                break;
            }
            }
        } while ((char *)dmdp + 4 < packet + size);
    }
}

static void run(struct mdclient *client) {
	pthread_spin_init(&write_lock, 0);
	struct exanic_dce_mc_client *exanic_mc = (struct exanic_dce_mc_client *)client->container;
	pclient = client;

    if (exanic_mc->merge) {
        exanic_mc->run_lv1_thread();
    }

	/* acquire exanic device handle */
    exanic_t *nic = exanic_acquire_handle(exanic_mc->lv2_exanic);
    if (!nic)
    {
        printf("exanic_acquire_handle: %s\n", exanic_get_last_error());
        fflush(stdout);
        exit(-1);
    }

    /* fpga upload data to port1, acquire rx buffer to receive data */
    exanic_rx_t *rx = exanic_acquire_rx_buffer(nic, 1, 0);
    if (!rx)
    {
        printf("exanic_acquire_rx_buffer: %s\n", exanic_get_last_error());
        fflush(stdout);
        exit(-1);
    }

    ssize_t size = 0;
    /* uploaded data will be copied to buf */
    char buf[2048];
    memset(buf, 0, sizeof(buf));

    while (1)
    {
        size = exanic_receive_frame(rx, buf, sizeof(buf), 0);
        if (size > 0)
        {
            packetHandler(buf, size);
        }
    }
}

static struct mdclient *exanic_dce_mc_create(cfg_t *cfg, struct memdb *memdb) {
	struct exanic_dce_mc_client *exanic_mc = new struct exanic_dce_mc_client();
	struct mdclient *client = &exanic_mc->mdclient;

	mdclient_init(client, cfg, memdb);

	const char *lv1_exanic;
	cfg_get_string(cfg, "lv1_exanic", &lv1_exanic);
	snprintf(exanic_mc->lv1_exanic, sizeof(exanic_mc->lv1_exanic), "%s", lv1_exanic);
	const char *lv2_exanic;
	cfg_get_string(cfg, "lv2_exanic", &lv2_exanic);
	snprintf(exanic_mc->lv2_exanic, sizeof(exanic_mc->lv2_exanic), "%s", lv2_exanic);
	cfg_get_int(cfg, "debug", &exanic_mc->debug);
	cfg_get_int(cfg, "merge", &exanic_mc->merge);
        cfg_get_int(cfg, "lv1_cpu", &exanic_mc->lv1_cpu);

	client->run = run;
	client->decoder = NULL;
	client->flags = 0;
	client->container = exanic_mc;

	return client;
}

static struct mdsrc_module mdsrc_exanic_dce_mc = {
	.create = exanic_dce_mc_create,
	.api = "fpga-fz-dce-mc"
};

mdsrc_module_register(&mdsrc_exanic_dce_mc);
