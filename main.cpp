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

    // Constructor
    OrderBook();

    // Add order
    void addOrder(const Order& order);

    // Count orders
    size_t getBuyOrderCount() const;
    size_t getSellOrderCount() const;

    // Print order book
    void printBuyOrders() const;
    void printSellOrders() const;

    // Check whether matching is possible
    bool canMatch() const;

    // Execute matching
    void matchOrders();

    // Print trade history
    void printTrades() const;
};

// ============================================================
// ORDER BOOK IMPLEMENTATION
// ============================================================

OrderBook::OrderBook()
    : nextTradeId(1)
{
}

// Add order

void OrderBook::addOrder(const Order& order) {

    if (order.getSide() == Side::BUY) {

        buyOrders[order.getPrice()].push_back(order);

    }
    else {

        sellOrders[order.getPrice()].push_back(order);

    }
}

// Count BUY orders

size_t OrderBook::getBuyOrderCount() const {

    size_t count = 0;

    for (const auto& entry : buyOrders) {
        count += entry.second.size();
    }

    return count;
}

// Count SELL orders

size_t OrderBook::getSellOrderCount() const {

    size_t count = 0;

    for (const auto& entry : sellOrders) {
        count += entry.second.size();
    }

    return count;
}

// Print BUY orders

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
}

// Print SELL orders

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
}

// Check if matching is possible

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
// MATCHING ENGINE
// ============================================================

void OrderBook::matchOrders() {

    while (canMatch()) {

        // Best BUY order
        Order& buyOrder =
            buyOrders.begin()->second.front();

        // Best SELL order
        Order& sellOrder =
            sellOrders.begin()->second.front();

        // Trade quantity = smaller quantity
        uint64_t tradeQuantity =
            min(
                buyOrder.getQuantity(),
                sellOrder.getQuantity()
            );

        // Save information before modifying/removing orders
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

        // ====================================================
        // CREATE TRADE
        // ====================================================

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

        // ====================================================
        // DISPLAY TRADE
        // ====================================================

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

        // ====================================================
        // REDUCE QUANTITIES
        // ====================================================

        buyOrder.reduceQuantity(tradeQuantity);
        sellOrder.reduceQuantity(tradeQuantity);

        // Check whether completely filled

        bool buyFilled =
            (buyOrder.getQuantity() == 0);

        bool sellFilled =
            (sellOrder.getQuantity() == 0);

        // ====================================================
        // REMOVE FILLED BUY ORDER
        // ====================================================

        if (buyFilled) {

            buyOrders.begin()->second.pop_front();

            if (buyOrders.begin()->second.empty()) {
                buyOrders.erase(buyOrders.begin());
            }
        }

        // ====================================================
        // REMOVE FILLED SELL ORDER
        // ====================================================

        if (sellFilled) {

            sellOrders.begin()->second.pop_front();

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
    // NORMAL MATCHING TEST
    // ========================================================

    OrderBook book;

    // ---------------- BUY ORDERS ----------------

    Order order1(
        1001,
        25,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        18050,
        100,
        123456789
    );

    Order order2(
        1002,
        26,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        17900,
        200,
        123456790
    );

    Order order3(
        1003,
        27,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        18100,
        150,
        123456791
    );

    // ---------------- SELL ORDERS ----------------

    Order sellOrder1(
        2001,
        30,
        "AAPL",
        Side::SELL,
        OrderType::LIMIT,
        18100,
        50,
        123456792
    );

    Order sellOrder2(
        2002,
        31,
        "AAPL",
        Side::SELL,
        OrderType::LIMIT,
        18000,
        75,
        123456793
    );

    // ---------------- ADD ORDERS ----------------

    book.addOrder(order1);
    book.addOrder(order2);
    book.addOrder(order3);

    book.addOrder(sellOrder1);
    book.addOrder(sellOrder2);

    // ---------------- BEFORE MATCHING ----------------

    cout << "\n================ BEFORE MATCHING ================\n";

    book.printBuyOrders();
    book.printSellOrders();

    // ---------------- MATCH ----------------

    if (book.canMatch()) {

        cout << "\nA TRADE CAN HAPPEN!\n";

        book.matchOrders();

    }
    else {

        cout << "\nNO TRADE POSSIBLE.\n";
    }

    // ---------------- AFTER MATCHING ----------------

    cout << "\n================ AFTER MATCHING ================\n";

    book.printBuyOrders();
    book.printSellOrders();

    // ---------------- TRADE HISTORY ----------------

    book.printTrades();


    // ========================================================
    // FIFO / PRICE-TIME PRIORITY TEST
    // ========================================================

    cout << "\n\n================ FIFO TEST ================\n";

    OrderBook fifoBook;

    // Three BUY orders at exactly the same price

    Order fifoBuy1(
        3001,
        40,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        18000,
        100,
        100
    );

    Order fifoBuy2(
        3002,
        41,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        18000,
        200,
        101
    );

    Order fifoBuy3(
        3003,
        42,
        "AAPL",
        Side::BUY,
        OrderType::LIMIT,
        18000,
        150,
        102
    );

    // SELL 250 shares at the same price

    Order fifoSell(
        4001,
        50,
        "AAPL",
        Side::SELL,
        OrderType::LIMIT,
        18000,
        250,
        103
    );

    // Add BUY orders in arrival order

    fifoBook.addOrder(fifoBuy1);
    fifoBook.addOrder(fifoBuy2);
    fifoBook.addOrder(fifoBuy3);

    // Add SELL order

    fifoBook.addOrder(fifoSell);

    // Show book before FIFO matching

    cout << "\nBefore FIFO matching:" << endl;

    fifoBook.printBuyOrders();
    fifoBook.printSellOrders();

    // Execute matching

    fifoBook.matchOrders();

    // Show remaining orders

    cout << "\nAfter FIFO matching:" << endl;

    fifoBook.printBuyOrders();
    fifoBook.printSellOrders();

    // Show trade history

    fifoBook.printTrades();

    return 0;
}