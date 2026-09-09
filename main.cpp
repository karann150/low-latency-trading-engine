#include <cstdint>
#include <string>
#include <iostream>
#include <map>
#include <deque>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <utility>

using namespace std;

// ============================================================
// ORDER TYPES
// ============================================================

enum class Side {
    BUY,
    SELL
};

enum class OrderType {
    MARKET,
    LIMIT
};

// ============================================================
// ORDER
// ============================================================

class Order {

    uint64_t orderId;
    uint64_t traderId;
    string symbol;

    Side side;
    OrderType orderType;

    int64_t price;
    uint64_t quantity;
    uint64_t timestamp;

public:

    // Constructor
    Order(
        uint64_t orderId,
        uint64_t traderId,
        string symbol,
        Side side,
        OrderType orderType,
        int64_t price,
        uint64_t quantity,
        uint64_t timestamp
    );

    // Getters
    uint64_t getOrderId() const;
    uint64_t getTraderId() const;
    string getSymbol() const;
    Side getSide() const;
    OrderType getOrderType() const;
    int64_t getPrice() const;
    uint64_t getQuantity() const;
    uint64_t getTimestamp() const;

    // Reduce remaining quantity
    void reduceQuantity(uint64_t amount);
};

// ============================================================
// ORDER IMPLEMENTATION
// ============================================================

Order::Order(
    uint64_t orderId,
    uint64_t traderId,
    string symbol,
    Side side,
    OrderType orderType,
    int64_t price,
    uint64_t quantity,
    uint64_t timestamp
)
    : orderId(orderId),
      traderId(traderId),
      symbol(symbol),
      side(side),
      orderType(orderType),
      price(price),
      quantity(quantity),
      timestamp(timestamp)
{
}

uint64_t Order::getOrderId() const {
    return orderId;
}

uint64_t Order::getTraderId() const {
    return traderId;
}

string Order::getSymbol() const {
    return symbol;
}

Side Order::getSide() const {
    return side;
}

OrderType Order::getOrderType() const {
    return orderType;
}

int64_t Order::getPrice() const {
    return price;
}

uint64_t Order::getQuantity() const {
    return quantity;
}

uint64_t Order::getTimestamp() const {
    return timestamp;
}

void Order::reduceQuantity(uint64_t amount) {

    if (amount <= quantity) {
        quantity -= amount;
    }
}

// ============================================================
// TRADE
// ============================================================

class Trade {

    uint64_t tradeId;
    uint64_t buyOrderId;
    uint64_t sellOrderId;

    string symbol;

    int64_t price;
    uint64_t quantity;
    uint64_t timestamp;

public:

    // Constructor
    Trade(
        uint64_t tradeId,
        uint64_t buyOrderId,
        uint64_t sellOrderId,
        string symbol,
        int64_t price,
        uint64_t quantity,
        uint64_t timestamp
    );

    // Getters
    uint64_t getTradeId() const;
    uint64_t getBuyOrderId() const;
    uint64_t getSellOrderId() const;
    string getSymbol() const;
    int64_t getPrice() const;
    uint64_t getQuantity() const;
    uint64_t getTimestamp() const;
};

// ============================================================
// TRADE IMPLEMENTATION
// ============================================================

Trade::Trade(
    uint64_t tradeId,
    uint64_t buyOrderId,
    uint64_t sellOrderId,
    string symbol,
    int64_t price,
    uint64_t quantity,
    uint64_t timestamp
)
    : tradeId(tradeId),
      buyOrderId(buyOrderId),
      sellOrderId(sellOrderId),
      symbol(symbol),
      price(price),
      quantity(quantity),
      timestamp(timestamp)
{
}

uint64_t Trade::getTradeId() const {
    return tradeId;
}

uint64_t Trade::getBuyOrderId() const {
    return buyOrderId;
}

uint64_t Trade::getSellOrderId() const {
    return sellOrderId;
}

string Trade::getSymbol() const {
    return symbol;
}

int64_t Trade::getPrice() const {
    return price;
}

uint64_t Trade::getQuantity() const {
    return quantity;
}

uint64_t Trade::getTimestamp() const {
    return timestamp;
}

// ============================================================
// ORDER BOOK
// ============================================================

class OrderBook {

    // BUY:
    // Highest price first.
    map<int64_t, deque<Order>, greater<int64_t>> buyOrders;

    // SELL:
    // Lowest price first.
    map<int64_t, deque<Order>> sellOrders;

    // Order ID -> Side + Price
    // Used for faster cancellation lookup.
    unordered_map<uint64_t, pair<Side, int64_t>> orderIndex;

    // Successfully executed trades
    vector<Trade> trades;

    // Next trade ID
    uint64_t nextTradeId;

public:

    // Constructor
    OrderBook();

    // Add directly to order book
    void addOrder(const Order& order);

    // Submit an incoming order
    void submitOrder(const Order& order);

    // Cancel order by ID
    bool cancelOrder(uint64_t orderId);

    // Count orders
    size_t getBuyOrderCount() const;
    size_t getSellOrderCount() const;

    // Display book
    void printBuyOrders() const;
    void printSellOrders() const;

    // Matching
    bool canMatch() const;
    void matchOrders();

    // Trade history
    void printTrades() const;
};

// ============================================================
// ORDER BOOK CONSTRUCTOR
// ============================================================

OrderBook::OrderBook()
    : nextTradeId(1)
{
}

// ============================================================
// ADD ORDER
// ============================================================

void OrderBook::addOrder(const Order& order) {

    if (order.getSide() == Side::BUY) {

        buyOrders[order.getPrice()].push_back(order);

    }
    else {

        sellOrders[order.getPrice()].push_back(order);

    }

    // Store order location for cancellation
    orderIndex[order.getOrderId()] =
        make_pair(order.getSide(), order.getPrice());
}

// ============================================================
// SUBMIT ORDER
// ============================================================

void OrderBook::submitOrder(const Order& order) {

    // MARKET orders will be implemented later.
    if (order.getOrderType() != OrderType::LIMIT) {

        cout << "\nMARKET orders are not implemented yet."
             << endl;

        return;
    }

    // Local copy because remaining quantity can change.
    Order incomingOrder = order;

    // ========================================================
    // MATCH INCOMING ORDER
    // ========================================================

    while (incomingOrder.getQuantity() > 0) {

        // ====================================================
        // INCOMING BUY
        // ====================================================

        if (incomingOrder.getSide() == Side::BUY) {

            // No SELL orders
            if (sellOrders.empty()) {
                break;
            }

            // Best SELL = lowest price
            int64_t bestSellPrice =
                sellOrders.begin()->first;

            // BUY price must be >= SELL price
            if (incomingOrder.getPrice() < bestSellPrice) {
                break;
            }

            // Oldest SELL order at best price
            Order& restingOrder =
                sellOrders.begin()->second.front();

            // Determine trade quantity
            uint64_t tradeQuantity =
                min(
                    incomingOrder.getQuantity(),
                    restingOrder.getQuantity()
                );

            // Resting order determines execution price
            int64_t tradePrice =
                restingOrder.getPrice();

            // Save IDs before any removal
            uint64_t buyOrderId =
                incomingOrder.getOrderId();

            uint64_t sellOrderId =
                restingOrder.getOrderId();

            string symbol =
                incomingOrder.getSymbol();

            uint64_t tradeTimestamp =
                incomingOrder.getTimestamp();

            // =================================================
            // CREATE TRADE
            // =================================================

            Trade trade(
                nextTradeId++,
                buyOrderId,
                sellOrderId,
                symbol,
                tradePrice,
                tradeQuantity,
                tradeTimestamp
            );

            trades.push_back(trade);

            cout << "\n==================== TRADE EXECUTED ====================\n";

            cout << "Trade ID: "
                 << trade.getTradeId()
                 << endl;

            cout << "Buy Order ID: "
                 << trade.getBuyOrderId()
                 << endl;

            cout << "Sell Order ID: "
                 << trade.getSellOrderId()
                 << endl;

            cout << "Symbol: "
                 << trade.getSymbol()
                 << endl;

            cout << "Trade Price: "
                 << trade.getPrice()
                 << endl;

            cout << "Trade Quantity: "
                 << trade.getQuantity()
                 << endl;

            // =================================================
            // REDUCE QUANTITIES
            // =================================================

            incomingOrder.reduceQuantity(tradeQuantity);
            restingOrder.reduceQuantity(tradeQuantity);

            // If resting order is fully filled,
            // remove it from the book and index.
            if (restingOrder.getQuantity() == 0) {

                uint64_t filledOrderId =
                    restingOrder.getOrderId();

                sellOrders.begin()->second.pop_front();

                orderIndex.erase(filledOrderId);

                if (sellOrders.begin()->second.empty()) {
                    sellOrders.erase(sellOrders.begin());
                }
            }
        }

        // ====================================================
        // INCOMING SELL
        // ====================================================

        else {

            // No BUY orders
            if (buyOrders.empty()) {
                break;
            }

            // Best BUY = highest price
            int64_t bestBuyPrice =
                buyOrders.begin()->first;

            // SELL price must be <= BUY price
            if (incomingOrder.getPrice() > bestBuyPrice) {
                break;
            }

            // Oldest BUY order at best price
            Order& restingOrder =
                buyOrders.begin()->second.front();

            // Determine trade quantity
            uint64_t tradeQuantity =
                min(
                    incomingOrder.getQuantity(),
                    restingOrder.getQuantity()
                );

            // Resting BUY determines execution price
            int64_t tradePrice =
                restingOrder.getPrice();

            // Save IDs before removal
            uint64_t buyOrderId =
                restingOrder.getOrderId();

            uint64_t sellOrderId =
                incomingOrder.getOrderId();

            string symbol =
                incomingOrder.getSymbol();

            uint64_t tradeTimestamp =
                incomingOrder.getTimestamp();

            // =================================================
            // CREATE TRADE
            // =================================================

            Trade trade(
                nextTradeId++,
                buyOrderId,
                sellOrderId,
                symbol,
                tradePrice,
                tradeQuantity,
                tradeTimestamp
            );

            trades.push_back(trade);

            cout << "\n==================== TRADE EXECUTED ====================\n";

            cout << "Trade ID: "
                 << trade.getTradeId()
                 << endl;

            cout << "Buy Order ID: "
                 << trade.getBuyOrderId()
                 << endl;

            cout << "Sell Order ID: "
                 << trade.getSellOrderId()
                 << endl;

            cout << "Symbol: "
                 << trade.getSymbol()
                 << endl;

            cout << "Trade Price: "
                 << trade.getPrice()
                 << endl;

            cout << "Trade Quantity: "
                 << trade.getQuantity()
                 << endl;

            // =================================================
            // REDUCE QUANTITIES
            // =================================================

            incomingOrder.reduceQuantity(tradeQuantity);
            restingOrder.reduceQuantity(tradeQuantity);

            // If resting order is fully filled,
            // remove it from book and index.
            if (restingOrder.getQuantity() == 0) {

                uint64_t filledOrderId =
                    restingOrder.getOrderId();

                buyOrders.begin()->second.pop_front();

                orderIndex.erase(filledOrderId);

                if (buyOrders.begin()->second.empty()) {
                    buyOrders.erase(buyOrders.begin());
                }
            }
        }
    }

    // ========================================================
    // REMAINING INCOMING QUANTITY
    // ========================================================

    if (incomingOrder.getQuantity() > 0) {

        addOrder(incomingOrder);

        cout << "\nOrder "
             << incomingOrder.getOrderId()
             << " has "
             << incomingOrder.getQuantity()
             << " shares remaining in the book."
             << endl;
    }
    else {

        cout << "\nOrder "
             << incomingOrder.getOrderId()
             << " completely filled."
             << endl;
    }
}

// ============================================================
// CANCEL ORDER
// ============================================================

bool OrderBook::cancelOrder(uint64_t orderId) {

    // Find order in index
    auto indexIt = orderIndex.find(orderId);

    // Order does not exist
    if (indexIt == orderIndex.end()) {

        return false;
    }

    Side side =
        indexIt->second.first;

    int64_t price =
        indexIt->second.second;

    // ========================================================
    // CANCEL BUY
    // ========================================================

    if (side == Side::BUY) {

        auto priceIt =
            buyOrders.find(price);

        if (priceIt == buyOrders.end()) {
            return false;
        }

        deque<Order>& orders =
            priceIt->second;

        for (
            auto orderIt = orders.begin();
            orderIt != orders.end();
            ++orderIt
        ) {

            if (orderIt->getOrderId() == orderId) {

                orders.erase(orderIt);

                // Remove empty price level
                if (orders.empty()) {
                    buyOrders.erase(priceIt);
                }

                // Remove from index
                orderIndex.erase(indexIt);

                return true;
            }
        }
    }

    // ========================================================
    // CANCEL SELL
    // ========================================================

    else {

        auto priceIt =
            sellOrders.find(price);

        if (priceIt == sellOrders.end()) {
            return false;
        }

        deque<Order>& orders =
            priceIt->second;

        for (
            auto orderIt = orders.begin();
            orderIt != orders.end();
            ++orderIt
        ) {

            if (orderIt->getOrderId() == orderId) {

                orders.erase(orderIt);

                // Remove empty price level
                if (orders.empty()) {
                    sellOrders.erase(priceIt);
                }

                // Remove from index
                orderIndex.erase(indexIt);

                return true;
            }
        }
    }

    return false;
}

// ============================================================
// COUNT BUY ORDERS
// ============================================================

size_t OrderBook::getBuyOrderCount() const {

    size_t count = 0;

    for (const auto& entry : buyOrders) {
        count += entry.second.size();
    }

    return count;
}

// ============================================================
// COUNT SELL ORDERS
// ============================================================

size_t OrderBook::getSellOrderCount() const {

    size_t count = 0;

    for (const auto& entry : sellOrders) {
        count += entry.second.size();
    }

    return count;
}

// ============================================================
// PRINT BUY ORDERS
// ============================================================

void OrderBook::printBuyOrders() const {

    cout << "\nBUY ORDERS:\n";

    if (buyOrders.empty()) {

        cout << "    EMPTY" << endl;

        return;
    }

    for (const auto& entry : buyOrders) {

        cout << "Price: "
             << entry.first
             << " | Orders: "
             << entry.second.size()
             << endl;

        for (const auto& order : entry.second) {

            cout << "    Order ID: "
                 << order.getOrderId()
                 << " | Quantity: "
                 << order.getQuantity()
                 << " | Timestamp: "
                 << order.getTimestamp()
                 << endl;
        }
    }
}

// ============================================================
// PRINT SELL ORDERS
// ============================================================

void OrderBook::printSellOrders() const {

    cout << "\nSELL ORDERS:\n";

    if (sellOrders.empty()) {

        cout << "    EMPTY" << endl;

        return;
    }

    for (const auto& entry : sellOrders) {

        cout << "Price: "
             << entry.first
             << " | Orders: "
             << entry.second.size()
             << endl;

        for (const auto& order : entry.second) {

            cout << "    Order ID: "
                 << order.getOrderId()
                 << " | Quantity: "
                 << order.getQuantity()
                 << " | Timestamp: "
                 << order.getTimestamp()
                 << endl;
        }
    }
}

// ============================================================
// CHECK MATCH
// ============================================================

bool OrderBook::canMatch() const {

    if (buyOrders.empty() || sellOrders.empty()) {
        return false;
    }

    int64_t bestBuyPrice =
        buyOrders.begin()->first;

    int64_t bestSellPrice =
        sellOrders.begin()->first;

    return bestBuyPrice >= bestSellPrice;
}

// ============================================================
// OLD MATCHING FUNCTION
// ============================================================

void OrderBook::matchOrders() {

    while (canMatch()) {

        Order& buyOrder =
            buyOrders.begin()->second.front();

        Order& sellOrder =
            sellOrders.begin()->second.front();

        uint64_t tradeQuantity =
            min(
                buyOrder.getQuantity(),
                sellOrder.getQuantity()
            );

        uint64_t buyOrderId =
            buyOrder.getOrderId();

        uint64_t sellOrderId =
            sellOrder.getOrderId();

        string symbol =
            buyOrder.getSymbol();

        int64_t tradePrice =
            sellOrder.getPrice();

        uint64_t tradeTimestamp =
            sellOrder.getTimestamp();

        // Create trade
        Trade trade(
            nextTradeId++,
            buyOrderId,
            sellOrderId,
            symbol,
            tradePrice,
            tradeQuantity,
            tradeTimestamp
        );

        trades.push_back(trade);

        cout << "\n==================== TRADE EXECUTED ====================\n";

        cout << "Trade ID: "
             << trade.getTradeId()
             << endl;

        cout << "Buy Order ID: "
             << trade.getBuyOrderId()
             << endl;

        cout << "Sell Order ID: "
             << trade.getSellOrderId()
             << endl;

        cout << "Trade Price: "
             << trade.getPrice()
             << endl;

        cout << "Trade Quantity: "
             << trade.getQuantity()
             << endl;

        // Reduce quantities
        buyOrder.reduceQuantity(tradeQuantity);
        sellOrder.reduceQuantity(tradeQuantity);

        bool buyFilled =
            (buyOrder.getQuantity() == 0);

        bool sellFilled =
            (sellOrder.getQuantity() == 0);

        // Remove filled BUY
        if (buyFilled) {

            uint64_t filledOrderId =
                buyOrder.getOrderId();

            buyOrders.begin()->second.pop_front();

            orderIndex.erase(filledOrderId);

            if (buyOrders.begin()->second.empty()) {
                buyOrders.erase(buyOrders.begin());
            }
        }

        // Remove filled SELL
        if (sellFilled) {

            uint64_t filledOrderId =
                sellOrder.getOrderId();

            sellOrders.begin()->second.pop_front();

            orderIndex.erase(filledOrderId);

            if (sellOrders.begin()->second.empty()) {
                sellOrders.erase(sellOrders.begin());
            }
        }
    }
}

// ============================================================
// TRADE HISTORY
// ============================================================

void OrderBook::printTrades() const {

    cout << "\n==================== TRADE HISTORY ====================\n";

    if (trades.empty()) {

        cout << "No trades executed."
             << endl;

        return;
    }

    for (const auto& trade : trades) {

        cout << "Trade ID: "
             << trade.getTradeId()

             << " | Buy Order: "
             << trade.getBuyOrderId()

             << " | Sell Order: "
             << trade.getSellOrderId()

             << " | Price: "
             << trade.getPrice()

             << " | Quantity: "
             << trade.getQuantity()

             << endl;
    }
}

// ============================================================
// MAIN
// ============================================================

int main() {

    // ========================================================
    // SUBMIT ORDER TEST
    // ========================================================

    cout << "\n================ SUBMIT ORDER TEST ================\n";

    OrderBook book;

    // Resting SELL
    Order restingSell(
        5001,
        50,
        "AAPL",
        Side::SELL,
        OrderType::LIMIT,
        18000,
        100,
        1000
    );

    book.submitOrder(restingSell);

    cout << "\nBook after SELL submission:" << endl;

    book.printBuyOrders();
    book.printSellOrders();

    // Incoming BUY
    Order incomingBuy(
        6001,
        60,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        18000,
        150,
        1001
    );

    book.submitOrder(incomingBuy);

    cout << "\nBook after BUY submission:" << endl;

    book.printBuyOrders();
    book.printSellOrders();

    book.printTrades();

    // ========================================================
    // CANCELLATION TEST
    // ========================================================

    cout << "\n\n================ CANCELLATION TEST ================\n";

    // This order cannot match because
    // there are no SELL orders at this stage.
    Order cancelTest(
        7001,
        70,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        17000,
        100,
        2000
    );

    book.submitOrder(cancelTest);

    cout << "\nBefore cancellation:" << endl;

    book.printBuyOrders();

    // Cancel order
    if (book.cancelOrder(7001)) {

        cout << "\nOrder 7001 cancelled successfully."
             << endl;

    }
    else {

        cout << "\nOrder 7001 was not found."
             << endl;
    }

    cout << "\nAfter cancellation:" << endl;

    book.printBuyOrders();

    // ========================================================
    // TRY TO CANCEL SAME ORDER AGAIN
    // ========================================================

    if (book.cancelOrder(7001)) {

        cout << "\nOrder 7001 cancelled again."
             << endl;

    }
    else {

        cout << "\nOrder 7001 no longer exists."
             << endl;
    }

    return 0;
}