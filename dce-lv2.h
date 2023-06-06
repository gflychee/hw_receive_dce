#include <stdint.h>
#include <stdio.h>

#pragma pack(1)

typedef struct 
{
    uint16_t    field_type;             /* 1792 */
    uint8_t     contract_id[20];        // 合约名称
    uint32_t    contract_no;            // 合约编号(未使用)
    uint32_t    trade_date;             // 交易日
    uint8_t     gen_time[13];           // 该行情生成时间
	double	    last_price;             // 最新成交价
	double	    high_price;             // 最高价
	double	    low_price;              // 最低价
	uint32_t    last_match_qty;         // 最新成交数量
	uint32_t    match_tot_qty;          // 成交总量
	double	    turnover;               // 成交金额
	uint32_t    open_interest;          // 持仓量
	int32_t     interest_chg;           // 持仓量变化
	double	    rise_limit;             // 最高报价
	double	    fall_limit;             // 最低报价
	double	    bid_price;              // 一档买价格
	uint32_t    bid_qty;                // 一档买数量
	uint32_t    bid_imply_qty;          // 一档买推导量
	double	    ask_price;              // 一档卖价格
	uint32_t    ask_qty;                // 一档卖数量
	uint32_t    ask_imply_qty;          // 一档卖推导量
	double	    avg_price;              // 成交均价
} new_fld_snap_best_quot_t;

typedef struct  {
    uint16_t    field_type;             /* 1793 */
    uint8_t     contract_id[20];        // 合约名称
    uint32_t    contract_no;            // 合约编号(未使用)
    uint32_t    trade_date;             // 交易日
    uint8_t     gen_time[13];           // 该行情生成时间
	double	    last_price;             // 最新成交价
	uint32_t    last_match_qty;         // 最新成交数量
	double	    rise_limit;             // 最高报价
	double	    fall_limit;             // 最低报价
	double	    bid_price;              // 一档买价格
	uint32_t    bid_qty;                // 一档买数量
	double	    ask_price;              // 一档卖价格
	uint32_t    ask_qty;                // 一档卖数量
} new_fld_snap_arbi_best_quot_t;

typedef struct  {
    uint16_t    field_type;             /* 1795 */
    uint8_t     contract_id[20];        // 合约名称
    uint32_t    contract_no;            // 合约编号(未使用)
    uint32_t    trade_date;             // 交易日
    uint8_t     gen_time[13];           // 该行情生成时间
	double      price_1;                // 价格1
	uint32_t    price_1bo_qty;          // 价格1的买开数量
	uint32_t    price_1be_qty;          // 价格1的买平数量
	uint32_t    price_1so_qty;          // 价格1的卖开数量
	uint32_t    price_1se_qty;          // 价格1的卖平数量
	double	    price_2;                // 价格2
	uint32_t    price_2bo_qty;          // 价格2的买开数量
	uint32_t    price_2be_qty;          // 价格2的买平数量
	uint32_t    price_2so_qty;          // 价格2的卖开数量
	uint32_t    price_2se_qty;          // 价格2的卖平数量
	double	    price_3;                // 价格3
	uint32_t    price_3bo_qty;          // 价格3的买开数量
	uint32_t    price_3be_qty;          // 价格3的买平数量
	uint32_t    price_3so_qty;          // 价格3的卖开数量
	uint32_t    price_3se_qty;          // 价格3的卖平数量
	double	    price_4;                // 价格4
	uint32_t    price_4bo_qty;          // 价格4的买开数量
	uint32_t    price_4be_qty;          // 价格4的买平数量
	uint32_t    price_4so_qty;          // 价格4的卖开数量
	uint32_t    price_4se_qty;          // 价格4的卖平数量
	double	    price_5;                // 价格5
	uint32_t    price_5bo_qty;          // 价格5的买开数量
	uint32_t    price_5be_qty;          // 价格5的买平数量
	uint32_t    price_5so_qty;          // 价格5的卖开数量
	uint32_t    price_5se_qty;          // 价格5的卖平数量
} new_fld_snap_segment_price_qty_t;

typedef struct  {
    uint16_t    field_type;             /* 1796 */
    uint8_t     contract_id[20];        // 合约名称
    uint32_t    contract_no;            // 合约编号(未使用)
    uint32_t    trade_date;             // 交易日
    uint8_t     gen_time[13];           // 该行情生成时间
    uint32_t    total_buy_qty;          // 买委托总量
    uint32_t    total_sell_qty;         // 卖委托总量
    double      weighted_avg_buy_px;    // 加权平均委托买价格
    double      weighted_avg_sell_px;   // 加权平均委托卖价格
} new_fld_snap_order_statics_t;

typedef struct
{
    uint16_t    field_type;             /* 1797 */
    uint8_t     contract_id[20];        // 合约名称
    uint32_t    contract_no;            // 合约编号(未使用)
    uint32_t    trade_date;             // 交易日
    uint8_t     gen_time[13];           // 该行情生成时间
    double      bid;                    // 最优买价格
    double      ask;                    // 最优卖价格
    int32_t     bid_qty_1;              // 买委托量1
    int32_t     bid_qty_2;              // 买委托量2
    int32_t     bid_qty_3;              // 买委托量3
    int32_t     bid_qty_4;              // 买委托量4
    int32_t     bid_qty_5;              // 买委托量5
    int32_t     bid_qty_6;              // 买委托量6
    int32_t     bid_qty_7;              // 买委托量7
    int32_t     bid_qty_8;              // 买委托量8
    int32_t     bid_qty_9;              // 买委托量9
    int32_t     bid_qty_10;             // 买委托量10
    int32_t     ask_qty_1;              // 卖委托量1
    int32_t     ask_qty_2;              // 卖委托量2
    int32_t     ask_qty_3;              // 卖委托量3
    int32_t     ask_qty_4;              // 卖委托量4
    int32_t     ask_qty_5;              // 卖委托量5
    int32_t     ask_qty_6;              // 卖委托量6
    int32_t     ask_qty_7;              // 卖委托量7
    int32_t     ask_qty_8;              // 卖委托量8
    int32_t     ask_qty_9;              // 卖委托量9
    int32_t     ask_qty_10;             // 卖委托量10
} new_fld_snap_best_orders_t;

typedef struct
{
    uint16_t    field_type;             /* 1798 */
    uint8_t     contract_id[20];        // 合约名称
    uint32_t    contract_no;            // 合约编号(未使用)
    uint32_t    trade_date;             // 交易日
    uint8_t     gen_time[13];           // 该行情生成时间
    double      bid_1;                  // 买1价格
    uint32_t    bid_1_qty;              // 买1数量
    uint32_t    bid_1_imp_qty;          // 买1推导量
    double      bid_2;                  // 买2价格
	uint32_t    bid_2_qty;              // 买2数量
	uint32_t    bid_2_imp_qty;          // 买2推导量
	double	    bid_3;                  // 买3价格
	uint32_t    bid_3_qty;              // 买3数量
	uint32_t    bid_3_imp_qty;          // 买3推导量
	double	    bid_4;                  // 买4价格
	uint32_t    bid_4_qty;              // 买4数量
	uint32_t    bid_4_imp_qty;          // 买4推导量
	double	    bid_5;                  // 买5价格
	uint32_t    bid_5_qty;              // 买5数量
	uint32_t    bid_5_imp_qty;          // 买5推导量
	double	    ask_1;                  // 卖1价格
	uint32_t    ask_1_qty;              // 卖1数量
	uint32_t    ask_1_imp_qty;          // 卖1推导量
	double	    ask_2;                  // 卖2价格
	uint32_t    ask_2_qty;              // 卖2数量
	uint32_t    ask_2_imp_qty;          // 卖2推导量
	double	    ask_3;                  // 卖3价格
	uint32_t    ask_3_qty;              // 卖3数量
	uint32_t    ask_3_imp_qty;          // 卖3推到量
	double	    ask_4;                  // 卖4价格
	uint32_t    ask_4_qty;              // 卖4数量
	uint32_t    ask_4_imp_qty;          // 卖4推导量
	double	    ask_5;                  // 卖5价格
	uint32_t    ask_5_qty;              // 卖5数量
	uint32_t    ask_5_imp_qty;          // 卖5推导量
} new_fld_snap_mbl_t;
#pragma pack()
