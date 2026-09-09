#include <cstdint>
#include <string>
#include <iostream>
#include <map>
#include <deque>
#include <vector>
#include <algorithm>

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

    // Executed trades
    vector<Trade> trades;

    // Next trade ID
    uint64_t nextTradeId;

public:

    OrderBook();

    // Add an order directly to the book
    void addOrder(const Order& order);

    // New primary order-entry function
    void submitOrder(const Order& order);

    // Count orders
    size_t getBuyOrderCount() const;
    size_t getSellOrderCount() const;

    // Print book
    void printBuyOrders() const;
    void printSellOrders() const;

    // Matching helpers
    bool canMatch() const;
    void matchOrders();

    // Trade history
    void printTrades() const;
};

// ============================================================
// ORDER BOOK IMPLEMENTATION
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
}

// ============================================================
// SUBMIT ORDER
// ============================================================

void OrderBook::submitOrder(const Order& order) {

    // For now we only support LIMIT orders.
    // MARKET orders will be implemented later.

    if (order.getOrderType() != OrderType::LIMIT) {

        cout << "\nMARKET orders are not implemented yet." << endl;
        return;
    }

    // Make a local copy because the incoming order's
    // remaining quantity may change during matching.
    Order incomingOrder = order;

    // ========================================================
    // TRY TO MATCH THE INCOMING ORDER
    // ========================================================

    while (incomingOrder.getQuantity() > 0) {

        // ====================================================
        // INCOMING BUY
        // ====================================================

        if (incomingOrder.getSide() == Side::BUY) {

            // No SELL orders available
            if (sellOrders.empty()) {
                break;
            }

            // Best SELL = lowest sell price
            int64_t bestSellPrice =
                sellOrders.begin()->first;

            // BUY limit price must be >= SELL price
            if (incomingOrder.getPrice() < bestSellPrice) {
                break;
            }

            // Get oldest order at the best SELL price
            Order& restingOrder =
                sellOrders.begin()->second.front();

            // Quantity that can be traded
            uint64_t tradeQuantity =
                min(
                    incomingOrder.getQuantity(),
                    restingOrder.getQuantity()
                );

            // Execution price = resting order price
            int64_t tradePrice =
                restingOrder.getPrice();

            // Create trade
            Trade trade(
                nextTradeId++,
                incomingOrder.getOrderId(),
                restingOrder.getOrderId(),
                incomingOrder.getSymbol(),
                tradePrice,
                tradeQuantity,
                incomingOrder.getTimestamp()
            );

            trades.push_back(trade);

            // Display trade
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

            // Reduce both orders
            incomingOrder.reduceQuantity(tradeQuantity);
            restingOrder.reduceQuantity(tradeQuantity);

            // Was the resting SELL completely filled?
            if (restingOrder.getQuantity() == 0) {

                sellOrders.begin()->second.pop_front();

                if (sellOrders.begin()->second.empty()) {
                    sellOrders.erase(sellOrders.begin());
                }
            }
        }

        // ====================================================
        // INCOMING SELL
        // ====================================================

        else {

            // No BUY orders available
            if (buyOrders.empty()) {
                break;
            }

            // Best BUY = highest buy price
            int64_t bestBuyPrice =
                buyOrders.begin()->first;

            // SELL limit price must be <= BUY price
            if (incomingOrder.getPrice() > bestBuyPrice) {
                break;
            }

            // Get oldest order at the best BUY price
            Order& restingOrder =
                buyOrders.begin()->second.front();

            // Quantity that can be traded
            uint64_t tradeQuantity =
                min(
                    incomingOrder.getQuantity(),
                    restingOrder.getQuantity()
                );

            // Execution price = resting order price
            int64_t tradePrice =
                restingOrder.getPrice();

            // Create trade
            Trade trade(
                nextTradeId++,
                restingOrder.getOrderId(),
                incomingOrder.getOrderId(),
                incomingOrder.getSymbol(),
                tradePrice,
                tradeQuantity,
                incomingOrder.getTimestamp()
            );

            trades.push_back(trade);

            // Display trade
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

            // Reduce both orders
            incomingOrder.reduceQuantity(tradeQuantity);
            restingOrder.reduceQuantity(tradeQuantity);

            // Was the resting BUY completely filled?
            if (restingOrder.getQuantity() == 0) {

                buyOrders.begin()->second.pop_front();

                if (buyOrders.begin()->second.empty()) {
                    buyOrders.erase(buyOrders.begin());
                }
            }
        }
    }

    // ========================================================
    // ADD REMAINING INCOMING QUANTITY TO THE BOOK
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

    if (buyOrders.empty()) {
        cout << "    EMPTY" << endl;
    }
}

// ============================================================
// PRINT SELL ORDERS
// ============================================================

void OrderBook::printSellOrders() const {

    cout << "\nSELL ORDERS:\n";

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

    if (sellOrders.empty()) {
        cout << "    EMPTY" << endl;
    }
}

// ============================================================
// CHECK WHETHER MATCHING IS POSSIBLE
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

        buyOrder.reduceQuantity(tradeQuantity);
        sellOrder.reduceQuantity(tradeQuantity);

        bool buyFilled =
            (buyOrder.getQuantity() == 0);

        bool sellFilled =
            (sellOrder.getQuantity() == 0);

        if (buyFilled) {

            buyOrders.begin()->second.pop_front();

            if (buyOrders.begin()->second.empty()) {
                buyOrders.erase(buyOrders.begin());
            }
        }

        if (sellFilled) {

            sellOrders.begin()->second.pop_front();

            if (sellOrders.begin()->second.empty()) {
                sellOrders.erase(sellOrders.begin());
            }
        }
    }
}

// ============================================================
// PRINT TRADE HISTORY
// ============================================================

void OrderBook::printTrades() const {

    cout << "\n==================== TRADE HISTORY ====================\n";

    if (trades.empty()) {

        cout << "No trades executed." << endl;

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

    // --------------------------------------------------------
    // RESTING SELL ORDER
    // --------------------------------------------------------

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

    // Submit it first.
    // Nothing is available to match it,
    // so it rests in the SELL book.
    book.submitOrder(restingSell);

    cout << "\nBook after submitting SELL order:\n";

    book.printBuyOrders();
    book.printSellOrders();

    // --------------------------------------------------------
    // INCOMING BUY ORDER
    // --------------------------------------------------------

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

    // Submit BUY order.
    // It should automatically match the SELL order.
    book.submitOrder(incomingBuy);

    // --------------------------------------------------------
    // FINAL BOOK
    // --------------------------------------------------------

    cout << "\n================ FINAL BOOK ================\n";

    book.printBuyOrders();
    book.printSellOrders();

    // --------------------------------------------------------
    // TRADE HISTORY
    // --------------------------------------------------------

    book.printTrades();

    return 0;
}