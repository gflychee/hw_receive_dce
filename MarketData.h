#ifndef MARKETDATA_H
#define MARKETDATA_H

#include <stdint.h>

#include <sys/time.h>

enum MDType {
	MDLevel1 = 1,
	MDLevel2,
	MD10Entrust
};

///�������
struct DepthMarketDataField
{
	MDType type;
	///����ʱ���
	struct timeval LocalTime;
	///������ʱ���
	int64_t ExchTime;
	///���¼�
	double 	LastPrice;
	///����
	int	Volume;
	///�ɽ����
	double	Turnover;
	///�ֲ���
	double	OpenInterest;
	///��ͣ���
	double UpperLimitPrice;
	///��ͣ���
	double LowerLimitPrice;
	///�����һ
	double	BidPrice1;
	///������һ
	double	AskPrice1;
	///������һ
	int	BidVolume1;
	///������һ
	int	AskVolume1;
	///����۶�
	double	BidPrice2;
	///�����۶�
	double	AskPrice2;
	///��������
	int	BidVolume2;
	///��������
	int	AskVolume2;
	///�������
	double	BidPrice3;
	///��������
	double	AskPrice3;
	///��������
	int	BidVolume3;
	///��������
	int	AskVolume3;
	///�������
	double	BidPrice4;
	///��������
	double	AskPrice4;
	///��������
	int	BidVolume4;
	///��������
	int	AskVolume4;
	///�������
	double	BidPrice5;
	///��������
	double	AskPrice5;
	///��������
	int	BidVolume5;
	///��������
	int	AskVolume5;
	///�������
	double	BidPrice6;
	///��������
	double	AskPrice6;
	///��������
	int	BidVolume6;
	///��������
	int	AskVolume6;
	///�������
	double	BidPrice7;
	///��������
	double	AskPrice7;
	///��������
	int	BidVolume7;
	///��������
	int	AskVolume7;
	///����۰�
	double	BidPrice8;
	///�����۰�
	double	AskPrice8;
	///��������
	int	BidVolume8;
	///��������
	int	AskVolume8;
	///����۾�
	double	BidPrice9;
	///�����۾�
	double	AskPrice9;
	///��������
	int	BidVolume9;
	///��������
	int	AskVolume9;
	///�����ʮ
	double	BidPriceA;
	///������ʮ
	double	AskPriceA;
	///������ʮ
	int	BidVolumeA;
	///������ʮ
	int	AskVolumeA;
	///ƽ����
	double	AveragePrice;
	///��Լ����
	char	InstrumentID[31];
	///����������
	char    ExchangeID[9];
	///����ֵ
	double IOPV;
	///ί�С��ɽ����
	int Index;
	///��ί�����
	int BuyIndex;
	///����ί�����
	int SellIndex;
	///��������
	char OrderType;
	///��������
	char Direction;
};

#endif
