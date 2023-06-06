#include <winterfell/instab.h>
#include <winterfell/mdclient.h>
#include "efvi-udp.h"
#include "MarketData.h"
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
typedef unsigned char uint8;   //8位无符号整数
typedef unsigned short uint16; //16位无符号整数
typedef unsigned int uint32;   //32位无符号整数

#define FIELD_VALUE_BIT 0x3FFFFFF

/************* 协议定义 开始 *************/
///<  消息类型定义
#define	PACKAGE_INSTRUMENT_IDX		    0x05    ///< 合约索引
#define	PACKAGE_INSTRUMENT_INIT		    0x06    ///< 初始行情
#define	PACKAGE_INSTRUMENT		        0x10    ///< 单腿行情
#define	PACKAGE_CMBTYPE	    		    0x11    ///< 组合行情
#define	PACKAGE_BULLETINE				0x12    ///< 交易所告示
#define	PACKAGE_QUOT_REQ		        0x13    ///< 报价请求
#define	PACKAGE_TRADE_STATUS			0x14    ///< 交易状态
#define	PACKAGE_INSTRUMENT_DEPTH		0x20    ///< 深度行情

#define PKG_HEAD_LEN 4   ///< 报文头长度
#define MSG_HEAD_LEN 2   ///< 消息头长度

#pragma pack(push, 1)

// 报文头
typedef struct
{
    uint8 msg_type;   ///< 报文的类型，0x01-0xff
    uint8 msg_num;    ///< 报文中消息个数，最大为255
    uint16 pkg_len;   ///< 报文的长度，不包含4字节报头，高字节在前
    char pkg_data[0]; ///< 数据正文
} pkg_head_t;         ///< 报文头

// 消息头
typedef struct
{
    uint16 msg_len;   ///< 消息长度，不包含2字节消息头，高字节在前
    char msg_data[0]; ///< 消息正文
} msg_head_t;         ///< 消息头

struct SingleLegInfo {
	///合约索引
	int instrumentIndex;
	///最新价
	double lastPrice;
	///成交量
	int volume;
	///持仓量
	int openInterest;
	///更新时间
	int updateTime;
	///更新时间（微妙部分）
	int updateTimeUsec;
	///总成交金额1
	long long tradeTurnover1;
	///总成交金额2
	long long tradeTurnover2;
	///总成交金额
	double turnover;
	///申买价一
	double	bidPrice1;
	///申卖价一
	double	askPrice1;
	///申买量一
	int	bidVolume1;
	///申卖量一
	int	askVolume1;
	///申买价二
	double	bidPrice2;
	///申卖价二
	double	askPrice2;
	///申买量二
	int	bidVolume2;
	///申卖量二
	int	askVolume2;
	///申买价三
	double	bidPrice3;
	///申卖价三
	double	askPrice3;
	///申买量三
	int	bidVolume3;
	///申卖量三
	int	askVolume3;
	///申买价四
	double	bidPrice4;
	///申卖价四
	double	askPrice4;
	///申买量四
	int	bidVolume4;
	///申卖量四
	int	askVolume4;
	///申买价五
	double	bidPrice5;
	///申卖价五
	double	askPrice5;
	///申买量五
	int	bidVolume5;
	///申卖量五
	int	askVolume5;
	///合约代码
	char InstrumentID[20];
	///涨停板价
	double upperLimitPrice;
	///跌停板价
	double lowerLimitPrice;
};

struct efvi_czce_mc_client {
    struct mdclient mdclient;
    char   ifname[64];
    char   udp_srvip[32];
    int    udp_port;
};
#pragma pack(pop)

using namespace std;


struct mdclient *pclient;
const int MAX_INS_NO = 4096;
string instruments[MAX_INS_NO];
int ins_inited[MAX_INS_NO];
int subscribed[MAX_INS_NO];
struct SingleLegInfo allmd[MAX_INS_NO];
long prev_exchtime[MAX_INSTRUMENT_NR];

long get_exchtime(int sec, int usec) {
    long rt = sec / 10000;
    rt *= 60;
    rt += (sec % 10000) / 100;
    rt *= 60;
    rt += sec % 100;
    if (rt > 3600 * 18)
        rt -= 3600 * 24;
    rt *= 1000;
    rt += usec / 1000;
    return rt;
}

bool illegal_data(struct SingleLegInfo *curmd)
{
    if (fabs(curmd->lastPrice) < 0.000001 || curmd->volume == 0 || curmd->openInterest == 0 || curmd->lowerLimitPrice == 0 || curmd->upperLimitPrice == 0)
        return true;
    if (curmd->askVolume5 != 0) {
        if (curmd->askVolume4 == 0 || curmd->askVolume3 == 0 || curmd->askVolume2 == 0 || curmd->askVolume1 == 0)
            return true;
    } else if (curmd->askVolume4 != 0) {
        if (curmd->askVolume3 == 0 || curmd->askVolume2 == 0 || curmd->askVolume1 == 0)
            return true;
    } else if (curmd->askVolume3 != 0) {
        if (curmd->askVolume2 == 0 || curmd->askVolume1 == 0)
            return true;
    }else if (curmd->askVolume2 != 0) {
        if (curmd->askVolume1 == 0)
            return true;
    }

    if (curmd->bidVolume5 != 0) {
        if (curmd->bidVolume4 == 0 || curmd->bidVolume3 == 0 || curmd->bidVolume2 == 0 || curmd->bidVolume1 == 0)
            return true;
    } else if (curmd->bidVolume4 != 0) {
        if (curmd->bidVolume3 == 0 || curmd->bidVolume2 == 0 || curmd->bidVolume1 == 0)
            return true;
    } else if (curmd->bidVolume3 != 0) {
        if (curmd->bidVolume2 == 0 || curmd->bidVolume1 == 0)
            return true;
    } else if (curmd->bidVolume2 != 0) {
        if (curmd->bidVolume1 == 0)
            return true;
    }
    return false;
}



uint16 read_uint16(const char *tbuf)
{
    unsigned char *buf = (unsigned char *)tbuf;
    return (buf[0] << 8) + buf[1];
}

uint32 read_uint32(const char *tbuf)
{
    unsigned char*buf = (unsigned char*)tbuf;
    return (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
}

int get_int_value(const char* p_buf, int &fld_idx, int &value)
{
    uint32 temp = 0;
    memcpy(&temp, p_buf, 4);
    temp = ntohl(temp);

    int sign = temp >> 31;           // 符号位(B31)
    fld_idx = (temp >> 26) & 0x1F;   // item索引(B30-B26)

    value = temp & FIELD_VALUE_BIT;  // 数值部分(B25-B0)
    value *= (0 == sign ? 1 : -1);

    return 4;
}

int get_dep_orderbook(const char* p_buf, int &fld_idx, int &price, int &qty, int &ord_cnt)
{
    // 获取价格(低位4字节的B0~B31)
    get_int_value(p_buf, fld_idx, price);

    // 获取委托量和订单个数（从高位4字节获取）
    uint32 temp = 0;
    memcpy(&temp, &p_buf[4], 4);
    temp = ntohl(temp);

    qty = temp >> 12;         // 委托量(B31-B12)
    ord_cnt = temp & 0x0FFF;  // 订单个数(B11-B0)

    return 8;
}

void write_memdb(long rece_time, struct SingleLegInfo *curmd)
{
    const int insidx = ins2idx(pclient->instab, curmd->InstrumentID);
    if (insidx == -1)
        return;
    if (curmd->instrumentIndex < 0 || curmd->instrumentIndex >= MAX_INS_NO)
        return;
    long exchtime = get_exchtime(curmd->updateTime, curmd->updateTimeUsec);
    if (prev_exchtime[insidx] != 0 && prev_exchtime[insidx] >= exchtime) {
        return;
    }
    prev_exchtime[insidx] = exchtime;
    
    
    uint32_t mdslot;
    struct md_static *ms = (struct md_static *)get_md_static(pclient->instab, insidx);
    struct md_snapshot *md = snapshottab_get_next_slot(pclient->sstab, insidx, &mdslot);

    if (unlikely(ms->upper_limit == 0.0)) {
        ms->upper_limit = curmd->upperLimitPrice;
        ms->lower_limit = curmd->lowerLimitPrice;
    }

    md->type = MDT_Level5;
    md->exchange_time = exchtime * 1000000;
    md->recv_time = rece_time;
    md->last_price = curmd->lastPrice != DBL_MAX ? curmd->lastPrice : 0.0;
    md->volume = curmd->volume;
    md->turnover = curmd->turnover;
    md->open_interest = curmd->openInterest;

    md->bid_price[0] = curmd->bidPrice1 != DBL_MAX ? curmd->bidPrice1 : 0.0;
    md->bid_size[0]  = curmd->bidVolume1;
    md->ask_price[0] = curmd->askPrice1 != DBL_MAX ? curmd->askPrice1 : 0.0;
    md->ask_size[0]  = curmd->askVolume1;

    md->bid_price[1] = curmd->bidPrice2 != DBL_MAX ? curmd->bidPrice2 : 0.0;
    md->bid_size[1]  = curmd->bidVolume2;
    md->ask_price[1] = curmd->askPrice2 != DBL_MAX ? curmd->askPrice2 : 0.0;
    md->ask_size[1]  = curmd->askVolume2;

    md->bid_price[2] = curmd->bidPrice3 != DBL_MAX ? curmd->bidPrice3 : 0.0;
    md->bid_size[2]  = curmd->bidVolume3;
    md->ask_price[2] = curmd->askPrice3 != DBL_MAX ? curmd->askPrice3 : 0.0;
    md->ask_size[2]  = curmd->askVolume3;

    md->bid_price[3] = curmd->bidPrice4 != DBL_MAX ? curmd->bidPrice4 : 0.0;
    md->bid_size[3]  = curmd->bidVolume4;
    md->ask_price[3] = curmd->askPrice4 != DBL_MAX ? curmd->askPrice4 : 0.0;
    md->ask_size[3]  = curmd->askVolume4;

    md->bid_price[4] = curmd->bidPrice5 != DBL_MAX ? curmd->bidPrice5 : 0.0;
    md->bid_size[4]  = curmd->bidVolume5;
    md->ask_price[4] = curmd->askPrice5 != DBL_MAX ? curmd->askPrice5 : 0.0;
    md->ask_size[4]  = curmd->askVolume5;

    md->decode_time = currtime();

    pclient->output(pclient, ms, mdslot);
    // printf("label md_tick:%d,%d,%d,%ld,%ld,%ld,%f,%ld,%f,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%f\n", insidx, md->type, md->open_interest, md->recv_time, md->decode_time, md->exchange_time, md->last_price, md->volume, md->turnover, 
    //         md->bid_price[0],md->bid_size[0],md->ask_price[0],md->ask_size[0],md->bid_price[1],md->bid_size[1],md->ask_price[1],md->ask_size[1],
    //         md->bid_price[2],md->bid_size[2],md->ask_price[2],md->ask_size[2],md->bid_price[3],md->bid_size[3],md->ask_price[3],md->ask_size[3],
    //         md->bid_price[4],md->bid_size[4],md->ask_price[4],md->ask_size[4],ms->upper_limit, ms->lower_limit);
    // fflush(stdout);
}


void UDPPush(uint8_t *pkt, int pkt_len)
{
    long rece_time = currtime();
    int offset = 0;
    int first_pkt = 1;
    int update_time = 0;
    int update_time_usec = 0;
    while(offset < pkt_len) { //一个UDP报文中可能含有多个数据包
        pkg_head_t *p_head = (pkg_head_t*)(pkt + offset);

        int length = 0; // 记录当前PKG中已处理的数据长度
        int pkg_len = read_uint16((char*)&p_head->pkg_len);
        for (size_t i = 0; i < p_head->msg_num && length < pkg_len; ++i) { // 一个PKG中可能包含多个MSG
            msg_head_t *p_msg_head = (msg_head_t *)(p_head->pkg_data + length);
    
            uint16 msg_len = read_uint16((char*)&p_msg_head->msg_len) - MSG_HEAD_LEN;  //减去消息头长度
            char *p_data = p_msg_head->msg_data;

            int data_pos = 0;
            
            switch (p_head->msg_type) {
            case PACKAGE_INSTRUMENT_IDX: { //合约索引信息消息
                // 交易日（仅在第一个msg中存在）
                if (i == 0) {
                    //uint32 trade_date = read_uint32(&p_data[data_pos]);
                    data_pos += 4;
                }

                // 合约类型
                uint8 type = p_data[data_pos];
                // 只处理单腿合约，所以只建立单腿合约的索引
                if (type != 0)
                    break;
                data_pos += 1;

                // 合约索引
                uint16 ins_idx = read_uint16(&p_data[data_pos]);
                if (ins_idx >= MAX_INS_NO || ins_inited[ins_idx])
                    break;
                data_pos += 2;

                // 合约编码
                char instrument_id[20] = {0};
                memcpy(instrument_id, &p_data[data_pos], i == 0 ? (msg_len - 7) : (msg_len - 3));
                
                ins_inited[ins_idx] = 1;
                instruments[ins_idx] = instrument_id;

                if (strlen(instrument_id) < 7) {
                    subscribed[ins_idx] = 1;
                    snprintf(allmd[ins_idx].InstrumentID, sizeof(allmd[ins_idx].InstrumentID), "%s", instrument_id);
                    allmd[ins_idx].instrumentIndex = ins_idx;
                }
                break;
            }
                
            case PACKAGE_INSTRUMENT_INIT: { //初始行情消息
                // 价格精度
                uint16 price_size = read_uint16(&p_data[data_pos]);
                data_pos += 2;

                // 合约索引
                uint16 ins_idx = read_uint16(&p_data[data_pos]);
                data_pos += 2;

                if (!ins_inited[ins_idx])
                    break;
                if (!subscribed[ins_idx])
                    break;
                while (data_pos < msg_len) { // 一个MSG中可能含有多个ITEM
                    int fld_idx = 0;
                    int value = 0;
                    data_pos += get_int_value(&p_data[data_pos], fld_idx, value);

                    switch (fld_idx)
                    {
                    case 4:  // 涨停价
                        allmd[ins_idx].upperLimitPrice = (double)value / price_size;
                        break;
                    case 5:  // 跌停价
                        allmd[ins_idx].lowerLimitPrice = (double)value / price_size;
                        break;
                    }
                }
                break;
            }
                
            case PACKAGE_INSTRUMENT: { //单腿行情消息
                // 价格精度
                uint16 price_size = read_uint16(&p_data[data_pos]);
                data_pos += 2;
            
                // 合约索引
                uint16 ins_idx = read_uint16(&p_data[data_pos]);
                if (!subscribed[ins_idx] && !first_pkt)
                    break;
                data_pos += 2;
                
                struct SingleLegInfo *curmd = &allmd[ins_idx];
            
                while (data_pos < msg_len) { // 一个MSG中可能含有多个ITEM
                    int fld_idx = 0;
                    int value = 0;
                    data_pos += get_int_value(&p_data[data_pos], fld_idx, value);
            
                    switch (fld_idx) {
                    case 4:  // 最新价
                        curmd->lastPrice = (double)value / price_size;
                        break;
                    case 9:  // 成交量
                        curmd->volume = value;
                        break;
                    case 10:  // 持仓量
                        curmd->openInterest = value;
                        break;
                    case 16:  // 秒级时间戳
                        curmd->updateTime = value;
                        break;
                    case 18:  // 新增：微秒级时间戳
                        curmd->updateTimeUsec = value;
                        break;
                    case 19:  // 新增：总成交金额(part1)
                        curmd->tradeTurnover1 = value;
                        break;
                    case 20:  // 新增：总成交金额(part2)
                        curmd->tradeTurnover2 = value;
                        break;
                    default:
                        break;
                    }
                }

                if (first_pkt) {
                    first_pkt = 0;
                    update_time = curmd->updateTime;
                    update_time_usec = curmd->updateTimeUsec;
                } else {
                    curmd->updateTime = update_time;
                    curmd->updateTimeUsec = update_time_usec;
                }
            
                // 总成交金额
                if (curmd->tradeTurnover1 != 0 || curmd->tradeTurnover2 != 0) {
                    curmd->turnover = (double)((curmd->tradeTurnover1 << 26) | curmd->tradeTurnover2) / price_size;
                }

                break;
            }
                
            case PACKAGE_INSTRUMENT_DEPTH: { //深度行情消息
                // 价格精度
                uint16 price_size = read_uint16(&p_data[data_pos]);
                data_pos += 2;

                // 合约索引
                uint16 ins_idx = read_uint16(&p_data[data_pos]);
                if (!subscribed[ins_idx])
                    break;
                data_pos += 2;
                struct SingleLegInfo *curmd = &allmd[ins_idx];

                while (data_pos < msg_len) { // 一个MSG中可能含有多个ITEM
                    int fld_idx = 0;
                    int price = 0;
                    int qty = 0;
                    int ord_cnt = 0;

                    data_pos += get_dep_orderbook(&p_data[data_pos], fld_idx, price, qty, ord_cnt);
                    double d_price = (double)price / price_size;

                    switch (fld_idx) {
                    case 1:
                        curmd->bidPrice1 = d_price;
                        curmd->bidVolume1 = qty;
                        break;
                    case 2:
                        curmd->askPrice1 = d_price;
                        curmd->askVolume1 = qty;
                        break;
                    case 3:
                        curmd->bidPrice2 = d_price;
                        curmd->bidVolume2 = qty;
                        break;
                    case 4:
                        curmd->askPrice2 = d_price;
                        curmd->askVolume2 = qty;
                        break;
                    case 5:
                        curmd->bidPrice3 = d_price;
                        curmd->bidVolume3 = qty;
                        break;
                    case 6:
                        curmd->askPrice3 = d_price;
                        curmd->askVolume3 = qty;
                        break;
                    case 7:
                        curmd->bidPrice4 = d_price;
                        curmd->bidVolume4 = qty;
                        break;
                    case 8:
                        curmd->askPrice4 = d_price;
                        curmd->askVolume4 = qty;
                        break;
                    case 9:
                        curmd->bidPrice5 = d_price;
                        curmd->bidVolume5 = qty;
                        break;
                    case 10:
                        curmd->askPrice5 = d_price;
                        curmd->askVolume5 = qty;
                        break;
                    default:
                        break;
                    }
                }
                curmd->updateTime = update_time;
				curmd->updateTimeUsec = update_time_usec;
				if (illegal_data(curmd))
					break;
                write_memdb(rece_time, curmd);
                break;
            }
            default:
                break;
            }

            length += msg_len + MSG_HEAD_LEN;  //完成一个msg的解析
        }

        offset += pkg_len + PKG_HEAD_LEN;  //完成一个pkg的解析
    }
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
    struct efvi_czce_mc_client *efvi_mc = (struct efvi_czce_mc_client *)pclient->container;
    if (dst_port == efvi_mc->udp_port)
        UDPPush((uint8_t*)((uint8_t*)packet + poff), ntohs(udp_hdr->len) - sizeof(struct udphdr));
}


static void run(struct mdclient *client) {
    struct efvi_czce_mc_client *efvi_mc = (struct efvi_czce_mc_client *)client->container;
    pclient = client;
    memset(ins_inited, 0, sizeof(ins_inited));
    memset(subscribed, 0, sizeof(subscribed));
    memset(allmd, 0, sizeof(allmd));
    memset(prev_exchtime, 0, sizeof(prev_exchtime));
    struct efd *efd = efd_alloc(efvi_mc->ifname, EF_VI_FLAGS_DEFAULT);
    efd_set_callback(efd, parse_ether, NULL);
    efd_add_udp_filter(efd, efvi_mc->udp_srvip, efvi_mc->udp_port);
    efd_poll(&efd, 1);
}


static struct mdclient *efvi_czce_mc_create(cfg_t *cfg, struct memdb *memdb) {
    struct efvi_czce_mc_client *efvi_mc = new struct efvi_czce_mc_client;
    struct mdclient *client = &efvi_mc->mdclient;

    mdclient_init(client, cfg, memdb);

    const char *ifname;
    cfg_get_string(cfg, "ifname", &ifname);
    snprintf(efvi_mc->ifname, sizeof(efvi_mc->ifname), "%s", ifname);

    const char *udp_srvip;
    cfg_get_string(cfg, "udp_srvip", &udp_srvip);
    snprintf(efvi_mc->udp_srvip, sizeof(efvi_mc->udp_srvip), "%s", udp_srvip);

    cfg_get_int(cfg, "udp_port", &efvi_mc->udp_port);

    printf("label efvi_mc->ifname: %s\n", efvi_mc->ifname);
    printf("label efvi_mc->udp_srvip: %s\n", efvi_mc->udp_srvip);
    printf("label efvi_mc->udp_port: %d\n", efvi_mc->udp_port);
    fflush(stdout);
    client->run = run;
    client->decoder = NULL;
    client->flags = 0;
    client->container = efvi_mc;
    return client;
}


static struct mdsrc_module mdsrc_efvi_czce_mc = {
    .create = efvi_czce_mc_create,
    .api = "efvi-czce-mc"
};

mdsrc_module_register(&mdsrc_efvi_czce_mc);
