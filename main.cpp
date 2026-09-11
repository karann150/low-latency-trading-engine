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

    uint64_t getOrderId() const;
    uint64_t getTraderId() const;
    string getSymbol() const;
    Side getSide() const;
    OrderType getOrderType() const;
    int64_t getPrice() const;
    uint64_t getQuantity() const;
    uint64_t getTimestamp() const;

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

    Trade(
        uint64_t tradeId,
        uint64_t buyOrderId,
        uint64_t sellOrderId,
        string symbol,
        int64_t price,
        uint64_t quantity,
        uint64_t timestamp
    );

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

    // Successfully executed trades.
    vector<Trade> trades;

    // Next trade ID.
    uint64_t nextTradeId;

public:

    OrderBook();

    // Validate incoming order.
    bool validateOrder(const Order& order) const;

    // Add directly to order book.
    void addOrder(const Order& order);

    // Submit incoming order.
    void submitOrder(const Order& order);

    // Cancel order by ID.
    bool cancelOrder(uint64_t orderId);

    // Modify existing order.
    bool modifyOrder(
        uint64_t orderId,
        int64_t newPrice,
        uint64_t newQuantity
    );

    // Count orders.
    size_t getBuyOrderCount() const;
    size_t getSellOrderCount() const;

    // Display book.
    void printBuyOrders() const;
    void printSellOrders() const;

    // Matching.
    bool canMatch() const;
    void matchOrders();

    // Trade history.
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
// ORDER VALIDATION
// ============================================================

bool OrderBook::validateOrder(const Order& order) const {

    // Order ID must be valid.
    if (order.getOrderId() == 0) {

        cout << "\nOrder rejected: Order ID must be greater than zero."
             << endl;

        return false;
    }

    // Duplicate active order ID.
    if (orderIndex.find(order.getOrderId()) != orderIndex.end()) {

        cout << "\nOrder rejected: Duplicate active order ID."
             << endl;

        return false;
    }

    // Trader ID must be valid.
    if (order.getTraderId() == 0) {

        cout << "\nOrder rejected: Trader ID must be greater than zero."
             << endl;

        return false;
    }

    // Symbol cannot be empty.
    if (order.getSymbol().empty()) {

        cout << "\nOrder rejected: Symbol cannot be empty."
             << endl;

        return false;
    }

    // Quantity must be positive.
    if (order.getQuantity() == 0) {

        cout << "\nOrder rejected: Quantity must be greater than zero."
             << endl;

        return false;
    }

    // LIMIT orders must have a positive price.
    if (
        order.getOrderType() == OrderType::LIMIT &&
        order.getPrice() <= 0
    ) {

        cout << "\nOrder rejected: LIMIT order price must be greater than zero."
             << endl;

        return false;
    }

    return true;
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

    // Store order location for cancellation.
    orderIndex[order.getOrderId()] =
        make_pair(
            order.getSide(),
            order.getPrice()
        );
}

// ============================================================
// SUBMIT ORDER
// ============================================================

void OrderBook::submitOrder(const Order& order) {

    // Validate first.
    if (!validateOrder(order)) {
        return;
    }

    // ========================================================
    // MARKET ORDERS
    // ========================================================

    if (order.getOrderType() == OrderType::MARKET) {

        // ----------------------------------------------------
        // MARKET BUY
        // ----------------------------------------------------
        // A MARKET BUY takes liquidity from the cheapest SELL
        // orders available in the book.
        //
        // Important:
        // A MARKET order NEVER rests in the order book.
        // Any unfilled quantity is discarded.
        // ----------------------------------------------------

        if (order.getSide() == Side::BUY) {

            Order incomingOrder = order;

            while (
                incomingOrder.getQuantity() > 0 &&
                !sellOrders.empty()
            ) {

                // Best SELL = lowest available price.
                int64_t bestSellPrice =
                    sellOrders.begin()->first;

                // Oldest SELL at the best price.
                Order& restingOrder =
                    sellOrders.begin()->second.front();

                // Determine trade quantity.
                uint64_t tradeQuantity =
                    min(
                        incomingOrder.getQuantity(),
                        restingOrder.getQuantity()
                    );

                // Market BUY executes at the resting SELL price.
                int64_t tradePrice =
                    bestSellPrice;

                uint64_t buyOrderId =
                    incomingOrder.getOrderId();

                uint64_t sellOrderId =
                    restingOrder.getOrderId();

                string symbol =
                    incomingOrder.getSymbol();

                uint64_t tradeTimestamp =
                    incomingOrder.getTimestamp();

                // Create trade.
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

                // Reduce both orders.
                incomingOrder.reduceQuantity(tradeQuantity);
                restingOrder.reduceQuantity(tradeQuantity);

                // If resting SELL is completely filled,
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

            // MARKET BUY never rests in the book.
            if (incomingOrder.getQuantity() > 0) {

                cout << "\nMARKET BUY Order "
                     << incomingOrder.getOrderId()
                     << " expired with "
                     << incomingOrder.getQuantity()
                     << " shares unfilled."
                     << endl;
            }
            else {

                cout << "\nMARKET BUY Order "
                     << incomingOrder.getOrderId()
                     << " completely filled."
                     << endl;
            }

            return;
        }

        // ----------------------------------------------------
        // MARKET SELL
        // ----------------------------------------------------
        // A MARKET SELL takes liquidity from the highest BUY
        // orders available in the book.
        //
        // Important:
        // A MARKET order NEVER rests in the order book.
        // Any unfilled quantity is discarded.
        // ----------------------------------------------------

        if (order.getSide() == Side::SELL) {

            Order incomingOrder = order;

            while (
                incomingOrder.getQuantity() > 0 &&
                !buyOrders.empty()
            ) {

                // Best BUY = highest available price.
                int64_t bestBuyPrice =
                    buyOrders.begin()->first;

                // Oldest BUY at the best price.
                Order& restingOrder =
                    buyOrders.begin()->second.front();

                // Determine trade quantity.
                uint64_t tradeQuantity =
                    min(
                        incomingOrder.getQuantity(),
                        restingOrder.getQuantity()
                    );

                // Market SELL executes at the resting BUY price.
                int64_t tradePrice =
                    bestBuyPrice;

                uint64_t buyOrderId =
                    restingOrder.getOrderId();

                uint64_t sellOrderId =
                    incomingOrder.getOrderId();

                string symbol =
                    incomingOrder.getSymbol();

                uint64_t tradeTimestamp =
                    incomingOrder.getTimestamp();

                // Create trade.
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

                // Reduce both orders.
                incomingOrder.reduceQuantity(tradeQuantity);
                restingOrder.reduceQuantity(tradeQuantity);

                // If resting BUY is completely filled,
                // remove it from the book and index.
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

            // MARKET SELL never rests in the book.
            if (incomingOrder.getQuantity() > 0) {

                cout << "\nMARKET SELL Order "
                     << incomingOrder.getOrderId()
                     << " expired with "
                     << incomingOrder.getQuantity()
                     << " shares unfilled."
                     << endl;
            }
            else {

                cout << "\nMARKET SELL Order "
                     << incomingOrder.getOrderId()
                     << " completely filled."
                     << endl;
            }

            return;
        }

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

            // No SELL orders.
            if (sellOrders.empty()) {
                break;
            }

            // Best SELL = lowest price.
            int64_t bestSellPrice =
                sellOrders.begin()->first;

            // BUY price must be >= SELL price.
            if (incomingOrder.getPrice() < bestSellPrice) {
                break;
            }

            // Oldest SELL order at best price.
            Order& restingOrder =
                sellOrders.begin()->second.front();

            // Determine trade quantity.
            uint64_t tradeQuantity =
                min(
                    incomingOrder.getQuantity(),
                    restingOrder.getQuantity()
                );

            // Resting order determines execution price.
            int64_t tradePrice =
                restingOrder.getPrice();

            // Save IDs before any removal.
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

            // No BUY orders.
            if (buyOrders.empty()) {
                break;
            }

            // Best BUY = highest price.
            int64_t bestBuyPrice =
                buyOrders.begin()->first;

            // SELL price must be <= BUY price.
            if (incomingOrder.getPrice() > bestBuyPrice) {
                break;
            }

            // Oldest BUY order at best price.
            Order& restingOrder =
                buyOrders.begin()->second.front();

            // Determine trade quantity.
            uint64_t tradeQuantity =
                min(
                    incomingOrder.getQuantity(),
                    restingOrder.getQuantity()
                );

            // Resting BUY determines execution price.
            int64_t tradePrice =
                restingOrder.getPrice();

            // Save IDs before removal.
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

    // Find order in index.
    auto indexIt =
        orderIndex.find(orderId);

    // Order does not exist.
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

                // Remove empty price level.
                if (orders.empty()) {
                    buyOrders.erase(priceIt);
                }

                // Remove from index.
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

                // Remove empty price level.
                if (orders.empty()) {
                    sellOrders.erase(priceIt);
                }

                // Remove from index.
                orderIndex.erase(indexIt);

                return true;
            }
        }
    }

    return false;
}

// ============================================================
// MODIFY ORDER
// ============================================================

bool OrderBook::modifyOrder(
    uint64_t orderId,
    int64_t newPrice,
    uint64_t newQuantity
) {

    // ========================================================
    // VALIDATE NEW VALUES
    // ========================================================

    if (newQuantity == 0) {

        cout << "\nModification rejected: "
             << "Quantity must be greater than zero."
             << endl;

        return false;
    }

    if (newPrice <= 0) {

        cout << "\nModification rejected: "
             << "LIMIT order price must be greater than zero."
             << endl;

        return false;
    }

    // ========================================================
    // FIND ORDER
    // ========================================================

    auto indexIt =
        orderIndex.find(orderId);

    if (indexIt == orderIndex.end()) {

        cout << "\nModification failed: "
             << "Order not found."
             << endl;

        return false;
    }

    Side side =
        indexIt->second.first;

    int64_t oldPrice =
        indexIt->second.second;

    // ========================================================
    // FIND ACTUAL ORDER
    // ========================================================

    Order oldOrder(
        0,
        0,
        "",
        Side::BUY,
        OrderType::LIMIT,
        0,
        0,
        0
    );

    bool found = false;

    if (side == Side::BUY) {

        auto priceIt =
            buyOrders.find(oldPrice);

        if (priceIt != buyOrders.end()) {

            for (const auto& order : priceIt->second) {

                if (order.getOrderId() == orderId) {

                    oldOrder = order;
                    found = true;
                    break;
                }
            }
        }
    }
    else {

        auto priceIt =
            sellOrders.find(oldPrice);

        if (priceIt != sellOrders.end()) {

            for (const auto& order : priceIt->second) {

                if (order.getOrderId() == orderId) {

                    oldOrder = order;
                    found = true;
                    break;
                }
            }
        }
    }

    if (!found) {

        cout << "\nModification failed: "
             << "Order not found in book."
             << endl;

        return false;
    }

    // ========================================================
    // CANCEL OLD ORDER
    // ========================================================

    if (!cancelOrder(orderId)) {

        cout << "\nModification failed: "
             << "Could not remove old order."
             << endl;

        return false;
    }

    // ========================================================
    // CREATE MODIFIED ORDER
    // ========================================================

    Order modifiedOrder(
        oldOrder.getOrderId(),
        oldOrder.getTraderId(),
        oldOrder.getSymbol(),
        oldOrder.getSide(),
        oldOrder.getOrderType(),
        newPrice,
        newQuantity,
        oldOrder.getTimestamp() + 1
    );

    cout << "\n==================== ORDER MODIFIED ====================\n";

    cout << "Order ID: "
         << modifiedOrder.getOrderId()
         << endl;

    cout << "Old Price: "
         << oldOrder.getPrice()
         << endl;

    cout << "New Price: "
         << modifiedOrder.getPrice()
         << endl;

    cout << "Old Quantity: "
         << oldOrder.getQuantity()
         << endl;

    cout << "New Quantity: "
         << modifiedOrder.getQuantity()
         << endl;

    cout << "Old Timestamp: "
         << oldOrder.getTimestamp()
         << endl;

    cout << "New Timestamp: "
         << modifiedOrder.getTimestamp()
         << endl;

    // ========================================================
    // SUBMIT MODIFIED ORDER
    // ========================================================

    submitOrder(modifiedOrder);

    return true;
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
// MATCH ORDERS
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

        // Create trade.
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

        // Reduce quantities.
        buyOrder.reduceQuantity(tradeQuantity);
        sellOrder.reduceQuantity(tradeQuantity);

        bool buyFilled =
            (buyOrder.getQuantity() == 0);

        bool sellFilled =
            (sellOrder.getQuantity() == 0);

        // Remove filled BUY.
        if (buyFilled) {

            uint64_t filledOrderId =
                buyOrder.getOrderId();

            buyOrders.begin()->second.pop_front();

            orderIndex.erase(filledOrderId);

            if (buyOrders.begin()->second.empty()) {
                buyOrders.erase(buyOrders.begin());
            }
        }

        // Remove filled SELL.
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
    // ORDER VALIDATION TEST
    // ========================================================

    cout << "\n================ ORDER VALIDATION TEST ================\n";

    OrderBook validationBook;

    // Invalid Order ID.
    Order invalidOrderId(
        0,
        101,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        18000,
        100,
        1
    );

    validationBook.submitOrder(invalidOrderId);

    // Invalid Trader ID.
    Order invalidTraderId(
        1001,
        0,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        18000,
        100,
        2
    );

    validationBook.submitOrder(invalidTraderId);

    // Empty symbol.
    Order invalidSymbol(
        1002,
        101,
        "",
        Side::BUY,
        OrderType::LIMIT,
        18000,
        100,
        3
    );

    validationBook.submitOrder(invalidSymbol);

    // Zero quantity.
    Order invalidQuantity(
        1003,
        101,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        18000,
        0,
        4
    );

    validationBook.submitOrder(invalidQuantity);

    // LIMIT order with zero price.
    Order invalidPrice(
        1004,
        101,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        0,
        100,
        5
    );

    validationBook.submitOrder(invalidPrice);

    cout << "\nBook after invalid orders:" << endl;

    validationBook.printBuyOrders();
    validationBook.printSellOrders();

    // ========================================================
    // DUPLICATE ORDER ID TEST
    // ========================================================

    cout << "\n\n================ DUPLICATE ORDER ID TEST ================\n";

    OrderBook duplicateBook;

    Order duplicateTest1(
        1100,
        101,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        18000,
        100,
        5000
    );

    Order duplicateTest2(
        1100,
        102,
        "AAPL",
        Side::SELL,
        OrderType::LIMIT,
        18100,
        50,
        5001
    );

    duplicateBook.submitOrder(duplicateTest1);
    duplicateBook.submitOrder(duplicateTest2);

    cout << "\nBook after duplicate ID test:" << endl;

    duplicateBook.printBuyOrders();
    duplicateBook.printSellOrders();

    // ========================================================
    // SUBMIT ORDER TEST
    // ========================================================

    cout << "\n\n================ SUBMIT ORDER TEST ================\n";

    OrderBook book;

    // Resting SELL.
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

    // Incoming BUY.
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
    // MARKET BUY TEST
    // ========================================================

    cout << "\n\n================ MARKET BUY TEST ================\n";

    OrderBook marketBuyBook;

    // Cheapest SELL.
    Order marketSell1(
        10001,
        100,
        "AAPL",
        Side::SELL,
        OrderType::LIMIT,
        18000,
        120,
        6000
    );

    // More expensive SELL.
    Order marketSell2(
        10002,
        101,
        "AAPL",
        Side::SELL,
        OrderType::LIMIT,
        18100,
        50,
        6001
    );

    marketBuyBook.submitOrder(marketSell1);
    marketBuyBook.submitOrder(marketSell2);

    cout << "\nBook before MARKET BUY:" << endl;

    marketBuyBook.printBuyOrders();
    marketBuyBook.printSellOrders();

    // MARKET BUY for 150 shares.
    //
    // Expected:
    // 120 shares @ 18000
    // 30 shares  @ 18100
    // Remaining SELL:
    // 20 shares  @ 18100
    //
    // MARKET BUY itself must NOT remain in the book.
    Order marketBuy(
        10003,
        102,
        "AAPL",
        Side::BUY,
        OrderType::MARKET,
        0,
        150,
        6002
    );

    marketBuyBook.submitOrder(marketBuy);

    cout << "\nBook after MARKET BUY:" << endl;

    marketBuyBook.printBuyOrders();
    marketBuyBook.printSellOrders();

    marketBuyBook.printTrades();

    // ========================================================
    // MARKET SELL TEST
    // ========================================================

    cout << "\n\n================ MARKET SELL TEST ================\n";

    OrderBook marketSellBook;

    // Highest BUY.
    Order marketBuy1(
        10101,
        101,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        18100,
        80,
        7000
    );

    // Lower BUY.
    Order marketBuy2(
        10102,
        102,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        18000,
        100,
        7001
    );

    marketSellBook.submitOrder(marketBuy1);
    marketSellBook.submitOrder(marketBuy2);

    cout << "\nBook before MARKET SELL:" << endl;

    marketSellBook.printBuyOrders();
    marketSellBook.printSellOrders();

    // MARKET SELL for 150 shares.
    //
    // Expected:
    // 80 shares  @ 18100
    // 70 shares  @ 18000
    // Remaining BUY:
    // 30 shares  @ 18000
    //
    // MARKET SELL itself must NOT remain in the book.
    Order marketSell(
        10103,
        103,
        "AAPL",
        Side::SELL,
        OrderType::MARKET,
        0,
        150,
        7002
    );

    marketSellBook.submitOrder(marketSell);

    cout << "\nBook after MARKET SELL:" << endl;

    marketSellBook.printBuyOrders();
    marketSellBook.printSellOrders();

    marketSellBook.printTrades();

    // ========================================================
    // MARKET ORDER INSUFFICIENT LIQUIDITY TEST
    // ========================================================

    cout << "\n\n================ MARKET ORDER INSUFFICIENT LIQUIDITY TEST ================\n";

    // --------------------------------------------------------
    // MARKET BUY: requested quantity > available SELL quantity
    // --------------------------------------------------------

    OrderBook insufficientBuyBook;

    Order limitedSell(
        10201,
        201,
        "AAPL",
        Side::SELL,
        OrderType::LIMIT,
        18000,
        50,
        8000
    );

    insufficientBuyBook.submitOrder(limitedSell);

    Order largeMarketBuy(
        10202,
        202,
        "AAPL",
        Side::BUY,
        OrderType::MARKET,
        0,
        100,
        8001
    );

    insufficientBuyBook.submitOrder(largeMarketBuy);

    cout << "\nBook after insufficient MARKET BUY:" << endl;

    insufficientBuyBook.printBuyOrders();
    insufficientBuyBook.printSellOrders();

    insufficientBuyBook.printTrades();

    // --------------------------------------------------------
    // MARKET SELL: requested quantity > available BUY quantity
    // --------------------------------------------------------

    OrderBook insufficientSellBook;

    Order limitedBuy(
        10301,
        301,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        18000,
        40,
        9000
    );

    insufficientSellBook.submitOrder(limitedBuy);

    Order largeMarketSell(
        10302,
        302,
        "AAPL",
        Side::SELL,
        OrderType::MARKET,
        0,
        100,
        9001
    );

    insufficientSellBook.submitOrder(largeMarketSell);

    cout << "\nBook after insufficient MARKET SELL:" << endl;

    insufficientSellBook.printBuyOrders();
    insufficientSellBook.printSellOrders();

    insufficientSellBook.printTrades();

    // --------------------------------------------------------
    // MARKET BUY with completely empty SELL book
    // --------------------------------------------------------

    OrderBook emptyMarketBook;

    Order emptyBookMarketBuy(
        10401,
        401,
        "AAPL",
        Side::BUY,
        OrderType::MARKET,
        0,
        75,
        10000
    );

    emptyMarketBook.submitOrder(emptyBookMarketBuy);

    cout << "\nBook after MARKET BUY with empty SELL book:" << endl;

    emptyMarketBook.printBuyOrders();
    emptyMarketBook.printSellOrders();

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

    // Cancel order.
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

    // Try to cancel same order again.
    if (book.cancelOrder(7001)) {

        cout << "\nOrder 7001 cancelled again."
             << endl;
    }
    else {

        cout << "\nOrder 7001 no longer exists."
             << endl;
    }

    // ========================================================
    // ORDER MODIFICATION TEST
    // ========================================================

    cout << "\n\n================ ORDER MODIFICATION TEST ================\n";

    Order modifyTest1(
        8001,
        80,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        17500,
        100,
        3000
    );

    Order modifyTest2(
        8002,
        81,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        17500,
        100,
        3001
    );

    book.submitOrder(modifyTest1);
    book.submitOrder(modifyTest2);

    cout << "\nBefore modification:" << endl;

    book.printBuyOrders();

    // Modify 8001.
    book.modifyOrder(
        8001,
        17600,
        200
    );

    cout << "\nAfter modifying Order 8001:" << endl;

    book.printBuyOrders();

    // ========================================================
    // MODIFICATION + MATCH TEST
    // ========================================================

    cout << "\n\n================ MODIFICATION + MATCH TEST ================\n";

    Order restingSell9001(
        9001,
        90,
        "AAPL",
        Side::SELL,
        OrderType::LIMIT,
        17700,
        50,
        4000
    );

    book.submitOrder(restingSell9001);

    cout << "\nBefore modifying BUY into matching price:" << endl;

    book.printBuyOrders();
    book.printSellOrders();

    // Modify BUY 8001 from 17600 to 17700.
    // It should immediately match SELL 9001.
    book.modifyOrder(
        8001,
        17700,
        150
    );

    cout << "\nAfter modifying Order 8001 to 17700:" << endl;

    book.printBuyOrders();
    book.printSellOrders();

    // ========================================================
    // FINAL TRADE HISTORY
    // ========================================================

    book.printTrades();

    return 0;
}