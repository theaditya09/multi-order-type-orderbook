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
#include<list>
#include <numeric>

enum class OrderType{
    GoodTillCancel,
    FillAndKill
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
    bool isFilled() const { return remainingQuantity_ == 0; }

    void Fill(Quantity quantity) {
        //check if >= is applicable here or just >
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
        return std::make_shared<Order>(orderType, orderId_, orderSide_, newPrice_, newQuantity_);
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
    std::unordered_map<OrderId, OrderEntry> orders_;

    bool CanMatch(OrderSide side, Price price) const {
        if(side == OrderSide::Buy) {
            if(asks_.empty()) return false;
            const auto& [bestAsk, _] = *asks_.begin();
            return price >= bestAsk;
        } else {
            if(bids_.empty()) return false;
            const auto& [bestBid, _] = *bids_.begin();
            return price <= bestBid;
        }
    }

    Trades MatchOrders(){
        Trades trades;
        trades.reserve(orders_.size());

        while(true) {
            if(bids_.empty() || asks_.empty()) break;
            auto& [bidPrice, bids] = *bids_.begin();
            auto& [askPrice, asks] = *asks_.begin();
            
            if(bidPrice < askPrice) break;
            while(bids.size() && asks.size()) {
                auto& bid = bids.front();
                auto& ask = asks.front();
                Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

                bid -> Fill(quantity);
                ask -> Fill(quantity);

                if(bid -> isFilled()) {
                    bids.pop_front();
                    orders_.erase(bid->GetOrderId());
                }

                if(ask -> isFilled()) {
                    asks.pop_front();
                    orders_.erase(ask->GetOrderId());
                }

                if(bids.empty()) bids_.erase(bidPrice);
                if(asks.empty()) asks_.erase(askPrice);

                trades.push_back(Trade{
                    TradeInfo{bid->GetOrderId(), bid->GetPrice(), quantity},
                    TradeInfo{ask->GetOrderId(), ask->GetPrice(), quantity}
                });
            }
        }

        if(!bids_.empty()){
            auto& [_, bids] = *bids_.begin();
            auto& bid = bids.front();
            if(bid -> GetOrderType() == OrderType::FillAndKill) {
                CancelOrder(bid->GetOrderId());
            }
        }

        if(!asks_.empty()){
            auto& [_, asks] = *asks_.begin();
            auto& ask = asks.front();
            if(ask -> GetOrderType() == OrderType::FillAndKill) {
                CancelOrder(ask->GetOrderId());
            }
        }

        return trades;
    }

public:
    Trades AddOrder(OrderPointer order){
        if(orders_.contains(order->GetOrderId())) return {};

        if(order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetOrderSide(), order->GetPrice())) return {};

        OrderPointers::iterator iterator;
        if(order->GetOrderSide() == OrderSide::Buy) {
            auto& orders = bids_[order->GetPrice()];
            orders.push_back(order);
            iterator = std::next(orders.begin(), orders.size() - 1);
        } else {
            auto& orders = asks_[order->GetPrice()];
            orders.push_back(order);
            iterator = std::next(orders.begin(), orders.size() - 1);
        }

        orders_.insert({order->GetOrderId(), OrderEntry{order, iterator}});
        return MatchOrders();
    }

    void CancelOrder(OrderId orderId){
        if(!orders_.contains(orderId)) return;
        
        const auto& [order, iterator] = orders_.at(orderId);
        orders_.erase(orderId);

        if(order->GetOrderSide() == OrderSide::Buy) {
            auto price = order->GetPrice();
            auto& orders = bids_.at(price);
            orders.erase(iterator);
            if(orders.empty()) bids_.erase(price);
        }
        else {
            auto price = order->GetPrice();
            auto& orders = asks_.at(price);
            orders.erase(iterator);
            if(orders.empty()) asks_.erase(price);
        }
    }

    Trades MatchOrder(OrderModify order){
        if(!orders_.contains(order.GetOrderId())) return {};

        const auto& [existingOrder, iterator] = orders_.at(order.GetOrderId());
        CancelOrder(order.GetOrderId());
        return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
    }

    std::size_t Size() const { return orders_.size(); }

    OrderbookLevelInfos GetOrderInfos() const {
        LevelInfos bidInfos, askInfos;
        bidInfos.reserve(orders_.size());
        askInfos.reserve(orders_.size());

        auto CreateLevelInfos = [](Price price, const OrderPointers& orders) {
            return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), Quantity(0),
            [](std::size_t runningSum, const OrderPointer& order)
            { return runningSum + order->GetRemainingQuantity(); }) };
        };

        for(const auto& [price, orders] : bids_) bidInfos.push_back(CreateLevelInfos(price, orders));
        for(const auto& [price, orders] : asks_) askInfos.push_back(CreateLevelInfos(price, orders));

        return OrderbookLevelInfos{ bidInfos, askInfos };
    }
};

int main() {
    Orderbook orderbook;
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, OrderSide::Buy, 100, 100));
    std::cout << orderbook.Size() << std::endl;
    orderbook.CancelOrder(1);
    std::cout << orderbook.Size() << std::endl;
    return 0;
}