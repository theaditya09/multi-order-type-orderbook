#include<iostream>
#include<format>
#include<vector>
#include<stdexcept>
#include<map>
#include<set>
#include<unordered_map>
#include<unordered_set>
#include<algorithm>
#include<functional>
#include<chrono>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<atomic>

enum class OrderType{
    GoodTillCancel,
    FillOrKill
};

enum class OrderSide{
    Buy,
    Sell
};

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;

struct LevelInfo{
    Price price_;
    Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;

class OrderbookLevelInfos{
public:
    OrderbookLevelInfos(const LevelInfos& bids, const LevelInfos& asks) 
        : bids_{ bids }, 
        asks_{ asks }
    { }

    const LevelInfos& GetBids() const { return bids_; }
    const LevelInfos& GetAsks() const { return asks_; }

private:
    LevelInfos bids_;
    LevelInfos asks_;
};

class Order{
public:
    Order( OrderType orderType, OrderId orderId, OrderSide orderSide, Price price, Quantity quantity)
        : orderType_{ orderType }
        , orderId_{ orderId }
        , orderSide_{ orderSide }
        , price_{ price }
        , initialQuantity_{ quantity }
        , remainingQuantity_{ quantity }
        {}

    OrderType GetOrderType() const { return orderType_; }
    OrderId GetOrderId() const { return orderId_; }
    OrderSide GetOrderSide() const { return orderSide_; }
    Price GetPrice() const { return price_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return initialQuantity_ - remainingQuantity_; }

    void Fill(Quantity quantity) {
        if(quantity > remainingQuantity_) {
            throw std::logic_error(std::format("Order {} cannot fill more than remaining quantity", orderId_));
        }

        remainingQuantity_ -= quantity;
    }

private:
    OrderType orderType_;
    OrderId orderId_;
    OrderSide orderSide_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

class OrderModify{
public:
    OrderModify(OrderId orderId, OrderSide orderSide, Price newPrice, Quantity newQuantity)
        : orderId_{ orderId }
        , orderSide_{ orderSide }
        , newPrice_{ newPrice }
        , newQuantity_{ newQuantity }
    { }

    OrderId GetOrderId() const { return orderId_; }
    OrderSide GetOrderSide() const { return orderSide_; }
    Price GetNewPrice() const { return newPrice_; }
    Quantity GetNewQuantity() const { return newQuantity_; }

    OrderPointer ToOrderPointer(OrderType orderType) const {
        return std::shared_ptr<Order>(orderType, orderId_, orderSide_, newPrice_, newQuantity_);
    }

private:
    OrderId orderId_;
    OrderSide orderSide_;
    Price newPrice_;
    Quantity newQuantity_;
};

struct TradeInfo{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};

class Trade{
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
        : bidTrade_{ bidTrade }
        , askTrade_{ askTrade }
    { }

    const TradeInfo& GetBidTrade() const { return bidTrade_; }
    const TradeInfo& GetAskTrade() const { return askTrade_; }

private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;
};

using Trades = std::vector<Trade>;

class Orderbook{
private:
    struct OrderEntry{
        OrderPointer order_{ nullptr };
        OrderPointers::iterator location_;
    };

    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orderMap_;

    bool CanMatch(Side side, Price price) const {
        if(side == Side::Buy) {
            if(asks_.empty()) return false;
            const auto& [bestAsk, _] = *asks_.begin();
            return price >= bestAsk;
        } else {
            if(bids_.empty()) return false;
            const auto& [bestBid, _] = *bids_.begin();
            return price <= bestBid;
        }
    }
};

int main() {
    return 0;
}