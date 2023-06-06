#ifndef MARKETDATA_H
#define MARKETDATA_H

#include <stdint.h>

#include <sys/time.h>

enum MDType {
	MDLevel1 = 1,
	MDLevel2,
	MD10Entrust
};

///深度行情
struct DepthMarketDataField
{
	MDType type;
	///本地时间戳
	struct timeval LocalTime;
	///交易所时间戳
	int64_t ExchTime;
	///最新价
	double 	LastPrice;
	///数量
	int	Volume;
	///成交金额
	double	Turnover;
	///持仓量
	double	OpenInterest;
	///涨停板价
	double UpperLimitPrice;
	///跌停板价
	double LowerLimitPrice;
	///申买价一
	double	BidPrice1;
	///申卖价一
	double	AskPrice1;
	///申买量一
	int	BidVolume1;
	///申卖量一
	int	AskVolume1;
	///申买价二
	double	BidPrice2;
	///申卖价二
	double	AskPrice2;
	///申买量二
	int	BidVolume2;
	///申卖量二
	int	AskVolume2;
	///申买价三
	double	BidPrice3;
	///申卖价三
	double	AskPrice3;
	///申买量三
	int	BidVolume3;
	///申卖量三
	int	AskVolume3;
	///申买价四
	double	BidPrice4;
	///申卖价四
	double	AskPrice4;
	///申买量四
	int	BidVolume4;
	///申卖量四
	int	AskVolume4;
	///申买价五
	double	BidPrice5;
	///申卖价五
	double	AskPrice5;
	///申买量五
	int	BidVolume5;
	///申卖量五
	int	AskVolume5;
	///申买价六
	double	BidPrice6;
	///申卖价六
	double	AskPrice6;
	///申买量六
	int	BidVolume6;
	///申卖量六
	int	AskVolume6;
	///申买价七
	double	BidPrice7;
	///申卖价七
	double	AskPrice7;
	///申买量七
	int	BidVolume7;
	///申卖量七
	int	AskVolume7;
	///申买价八
	double	BidPrice8;
	///申卖价八
	double	AskPrice8;
	///申买量八
	int	BidVolume8;
	///申卖量八
	int	AskVolume8;
	///申买价九
	double	BidPrice9;
	///申卖价九
	double	AskPrice9;
	///申买量九
	int	BidVolume9;
	///申卖量九
	int	AskVolume9;
	///申买价十
	double	BidPriceA;
	///申卖价十
	double	AskPriceA;
	///申买量十
	int	BidVolumeA;
	///申卖量十
	int	AskVolumeA;
	///平均价
	double	AveragePrice;
	///合约代码
	char	InstrumentID[31];
	///交易所代码
	char    ExchangeID[9];
	///基金净值
	double IOPV;
	///委托、成交序号
	int Index;
	///买方委托序号
	int BuyIndex;
	///卖方委托序号
	int SellIndex;
	///报单类型
	char OrderType;
	///买卖方向
	char Direction;
};

#endif
