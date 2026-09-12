#include <cstdint>
#include <string>
#include <iostream>
#include <memory>
#include <map>
#include <deque>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <stdexcept>
#include <functional>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

// MinGW.org's legacy GCC 6 distribution does not implement the C++ thread
// library.  Keep the exchange thread-safe on that toolchain by using the
// Windows synchronization/thread primitives directly.
#ifdef _WIN32
class ExchangeMutex {
private:
    CRITICAL_SECTION nativeMutex;

public:
    ExchangeMutex() { InitializeCriticalSection(&nativeMutex); }
    ~ExchangeMutex() { DeleteCriticalSection(&nativeMutex); }

    ExchangeMutex(const ExchangeMutex&) = delete;
    ExchangeMutex& operator=(const ExchangeMutex&) = delete;

    void lock() { EnterCriticalSection(&nativeMutex); }
    void unlock() { LeaveCriticalSection(&nativeMutex); }
};

template <typename MutexType>
class ExchangeLockGuard {
private:
    MutexType& mutex;

public:
    explicit ExchangeLockGuard(MutexType& mutex) : mutex(mutex) {
        mutex.lock();
    }

    ~ExchangeLockGuard() { mutex.unlock(); }

    ExchangeLockGuard(const ExchangeLockGuard&) = delete;
    ExchangeLockGuard& operator=(const ExchangeLockGuard&) = delete;
};

class ExchangeThread {
private:
    HANDLE handle;

    static DWORD WINAPI run(void* context) {
        unique_ptr<function<void()> > task(
            static_cast<function<void()>*>(context)
        );
        (*task)();
        return 0;
    }

public:
    template <typename Function>
    explicit ExchangeThread(Function task) : handle(NULL) {
        unique_ptr<function<void()> > context(
            new function<void()>(move(task))
        );
        handle = CreateThread(
            NULL,
            0,
            &ExchangeThread::run,
            context.get(),
            0,
            NULL
        );

        if (handle == NULL) {
            throw runtime_error("Could not create worker thread.");
        }

        context.release();
    }

    ~ExchangeThread() {
        if (handle != NULL) {
            CloseHandle(handle);
        }
    }

    ExchangeThread(const ExchangeThread&) = delete;
    ExchangeThread& operator=(const ExchangeThread&) = delete;

    ExchangeThread(ExchangeThread&& other) : handle(other.handle) {
        other.handle = NULL;
    }

    ExchangeThread& operator=(ExchangeThread&& other) {
        if (this != &other) {
            if (handle != NULL) CloseHandle(handle);
            handle = other.handle;
            other.handle = NULL;
        }
        return *this;
    }

    void join() {
        if (handle != NULL) {
            WaitForSingleObject(handle, INFINITE);
            CloseHandle(handle);
            handle = NULL;
        }
    }
};
#else
#include <mutex>
#include <thread>
typedef mutex ExchangeMutex;
template <typename MutexType>
using ExchangeLockGuard = lock_guard<MutexType>;
typedef thread ExchangeThread;
#endif

// ============================================================
// LOW-LATENCY TRADING & ORDER MATCHING ENGINE
// C++14 single-file capstone implementation
//
// Features:
//  - Price-time priority
//  - LIMIT and MARKET orders
//  - Partial fills
//  - Cancel / modify
//  - Validation + duplicate active-ID protection
//  - Multiple instruments
//  - Order lifecycle states
//  - Exchange-wide order/trade sequencing
//  - Thread-safe exchange gateway
//  - Trade/event history
//  - CSV persistence/export
//  - Deterministic test suite
//  - Simple latency benchmark
//
// Money is represented as integer ticks (e.g. 18050 = $180.50).
// ============================================================

enum class Side {
    BUY,
    SELL
};

enum class OrderType {
    MARKET,
    LIMIT
};

enum class OrderStatus {
    NEW,
    PARTIALLY_FILLED,
    FILLED,
    CANCELLED,
    REJECTED,
    EXPIRED
};

static string sideToString(Side side) {
    return side == Side::BUY ? "BUY" : "SELL";
}

static string orderTypeToString(OrderType type) {
    return type == OrderType::LIMIT ? "LIMIT" : "MARKET";
}

static string statusToString(OrderStatus status) {
    switch (status) {
        case OrderStatus::NEW: return "NEW";
        case OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
        case OrderStatus::FILLED: return "FILLED";
        case OrderStatus::CANCELLED: return "CANCELLED";
        case OrderStatus::REJECTED: return "REJECTED";
        case OrderStatus::EXPIRED: return "EXPIRED";
    }
    return "UNKNOWN";
}

// ============================================================
// ORDER
// ============================================================

class Order {
private:
    uint64_t orderId;
    uint64_t traderId;
    string symbol;
    Side side;
    OrderType orderType;
    int64_t price;
    uint64_t originalQuantity;
    uint64_t quantity;
    uint64_t timestamp;
    OrderStatus status;

public:
    Order(
        uint64_t orderId,
        uint64_t traderId,
        const string& symbol,
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
          originalQuantity(quantity),
          quantity(quantity),
          timestamp(timestamp),
          status(OrderStatus::NEW) {}

    uint64_t getOrderId() const { return orderId; }
    uint64_t getTraderId() const { return traderId; }
    const string& getSymbol() const { return symbol; }
    Side getSide() const { return side; }
    OrderType getOrderType() const { return orderType; }
    int64_t getPrice() const { return price; }
    uint64_t getOriginalQuantity() const { return originalQuantity; }
    uint64_t getQuantity() const { return quantity; }
    uint64_t getFilledQuantity() const { return originalQuantity - quantity; }
    uint64_t getTimestamp() const { return timestamp; }
    OrderStatus getStatus() const { return status; }

    void reduceQuantity(uint64_t amount) {
        if (amount > quantity) {
            throw logic_error("Cannot reduce order by more than remaining quantity.");
        }

        quantity -= amount;

        if (quantity == 0) {
            status = OrderStatus::FILLED;
        } else if (quantity < originalQuantity) {
            status = OrderStatus::PARTIALLY_FILLED;
        }
    }

    void setStatus(OrderStatus newStatus) {
        status = newStatus;
    }
};

// ============================================================
// TRADE
// ============================================================

class Trade {
private:
    uint64_t tradeId;
    uint64_t sequence;
    uint64_t buyOrderId;
    uint64_t sellOrderId;
    string symbol;
    int64_t price;
    uint64_t quantity;
    uint64_t timestamp;

public:
    Trade(
        uint64_t tradeId,
        uint64_t sequence,
        uint64_t buyOrderId,
        uint64_t sellOrderId,
        const string& symbol,
        int64_t price,
        uint64_t quantity,
        uint64_t timestamp
    )
        : tradeId(tradeId),
          sequence(sequence),
          buyOrderId(buyOrderId),
          sellOrderId(sellOrderId),
          symbol(symbol),
          price(price),
          quantity(quantity),
          timestamp(timestamp) {}

    uint64_t getTradeId() const { return tradeId; }
    uint64_t getSequence() const { return sequence; }
    uint64_t getBuyOrderId() const { return buyOrderId; }
    uint64_t getSellOrderId() const { return sellOrderId; }
    const string& getSymbol() const { return symbol; }
    int64_t getPrice() const { return price; }
    uint64_t getQuantity() const { return quantity; }
    uint64_t getTimestamp() const { return timestamp; }
};

// ============================================================
// EVENT
// ============================================================

enum class EventType {
    ACCEPTED,
    TRADE,
    CANCELLED,
    MODIFIED,
    REJECTED,
    EXPIRED
};

static string eventTypeToString(EventType type) {
    switch (type) {
        case EventType::ACCEPTED: return "ACCEPTED";
        case EventType::TRADE: return "TRADE";
        case EventType::CANCELLED: return "CANCELLED";
        case EventType::MODIFIED: return "MODIFIED";
        case EventType::REJECTED: return "REJECTED";
        case EventType::EXPIRED: return "EXPIRED";
    }
    return "UNKNOWN";
}

struct EngineEvent {
    uint64_t sequence;
    EventType type;
    uint64_t orderId;
    uint64_t tradeId;
    string symbol;
    uint64_t timestamp;
    string message;
};

// ============================================================
// ORDER BOOK
// One OrderBook represents exactly ONE instrument.
// ============================================================

class OrderBook {
private:
    string symbol;

    // Highest BUY first.
    map<int64_t, deque<Order>, greater<int64_t>> buyOrders;

    // Lowest SELL first.
    map<int64_t, deque<Order>> sellOrders;

    // Active order ID -> side + price.
    unordered_map<uint64_t, pair<Side, int64_t>> orderIndex;

    vector<Trade> trades;

    uint64_t nextLocalTradeId;
    uint64_t* globalSequence;

    vector<EngineEvent>* events;

    bool validateOrder(const Order& order) const {
        if (order.getOrderId() == 0) return false;
        if (order.getTraderId() == 0) return false;
        if (order.getSymbol().empty()) return false;
        if (order.getSymbol() != symbol) return false;
        if (order.getQuantity() == 0) return false;

        if (order.getOrderType() == OrderType::LIMIT &&
            order.getPrice() <= 0) {
            return false;
        }

        if (orderIndex.find(order.getOrderId()) != orderIndex.end()) {
            return false;
        }

        return true;
    }

    uint64_t nextSequence() {
        return ++(*globalSequence);
    }

    void addEvent(
        EventType type,
        uint64_t orderId,
        uint64_t tradeId,
        uint64_t timestamp,
        const string& message
    ) {
        if (events == nullptr) return;

        EngineEvent event;
        event.sequence = nextSequence();
        event.type = type;
        event.orderId = orderId;
        event.tradeId = tradeId;
        event.symbol = symbol;
        event.timestamp = timestamp;
        event.message = message;

        events->push_back(event);
    }

    void executeTrade(
        Order& aggressiveOrder,
        Order& restingOrder,
        uint64_t tradeQuantity,
        int64_t tradePrice
    ) {
        uint64_t buyOrderId;
        uint64_t sellOrderId;

        if (aggressiveOrder.getSide() == Side::BUY) {
            buyOrderId = aggressiveOrder.getOrderId();
            sellOrderId = restingOrder.getOrderId();
        } else {
            buyOrderId = restingOrder.getOrderId();
            sellOrderId = aggressiveOrder.getOrderId();
        }

        uint64_t sequence = nextSequence();

        Trade trade(
            nextLocalTradeId++,
            sequence,
            buyOrderId,
            sellOrderId,
            symbol,
            tradePrice,
            tradeQuantity,
            sequence
        );

        trades.push_back(trade);

        aggressiveOrder.reduceQuantity(tradeQuantity);
        restingOrder.reduceQuantity(tradeQuantity);

        addEvent(
            EventType::TRADE,
            aggressiveOrder.getOrderId(),
            trade.getTradeId(),
            trade.getTimestamp(),
            "Trade executed"
        );
    }

    void matchLimitOrder(Order incomingOrder) {
        while (incomingOrder.getQuantity() > 0) {

            if (incomingOrder.getSide() == Side::BUY) {

                if (sellOrders.empty()) break;

                auto levelIt = sellOrders.begin();
                int64_t bestSellPrice = levelIt->first;

                if (incomingOrder.getPrice() < bestSellPrice) break;

                Order& restingOrder = levelIt->second.front();

                uint64_t tradeQuantity = min(
                    incomingOrder.getQuantity(),
                    restingOrder.getQuantity()
                );

                executeTrade(
                    incomingOrder,
                    restingOrder,
                    tradeQuantity,
                    restingOrder.getPrice()
                );

                if (restingOrder.getQuantity() == 0) {
                    uint64_t filledId = restingOrder.getOrderId();
                    levelIt->second.pop_front();
                    orderIndex.erase(filledId);

                    if (levelIt->second.empty()) {
                        sellOrders.erase(levelIt);
                    }
                }

            } else {

                if (buyOrders.empty()) break;

                auto levelIt = buyOrders.begin();
                int64_t bestBuyPrice = levelIt->first;

                if (incomingOrder.getPrice() > bestBuyPrice) break;

                Order& restingOrder = levelIt->second.front();

                uint64_t tradeQuantity = min(
                    incomingOrder.getQuantity(),
                    restingOrder.getQuantity()
                );

                executeTrade(
                    incomingOrder,
                    restingOrder,
                    tradeQuantity,
                    restingOrder.getPrice()
                );

                if (restingOrder.getQuantity() == 0) {
                    uint64_t filledId = restingOrder.getOrderId();
                    levelIt->second.pop_front();
                    orderIndex.erase(filledId);

                    if (levelIt->second.empty()) {
                        buyOrders.erase(levelIt);
                    }
                }
            }
        }

        if (incomingOrder.getQuantity() > 0) {
            incomingOrder.setStatus(
                incomingOrder.getFilledQuantity() > 0
                    ? OrderStatus::PARTIALLY_FILLED
                    : OrderStatus::NEW
            );

            if (incomingOrder.getOrderType() == OrderType::LIMIT) {
                addOrder(incomingOrder);

                addEvent(
                    EventType::ACCEPTED,
                    incomingOrder.getOrderId(),
                    0,
                    incomingOrder.getTimestamp(),
                    "Remaining LIMIT quantity resting in book"
                );
            }
        }
    }

    void matchMarketOrder(Order incomingOrder) {

        while (incomingOrder.getQuantity() > 0) {

            if (incomingOrder.getSide() == Side::BUY) {

                if (sellOrders.empty()) break;

                auto levelIt = sellOrders.begin();
                Order& restingOrder = levelIt->second.front();

                uint64_t tradeQuantity = min(
                    incomingOrder.getQuantity(),
                    restingOrder.getQuantity()
                );

                executeTrade(
                    incomingOrder,
                    restingOrder,
                    tradeQuantity,
                    restingOrder.getPrice()
                );

                if (restingOrder.getQuantity() == 0) {
                    uint64_t filledId = restingOrder.getOrderId();
                    levelIt->second.pop_front();
                    orderIndex.erase(filledId);

                    if (levelIt->second.empty()) {
                        sellOrders.erase(levelIt);
                    }
                }

            } else {

                if (buyOrders.empty()) break;

                auto levelIt = buyOrders.begin();
                Order& restingOrder = levelIt->second.front();

                uint64_t tradeQuantity = min(
                    incomingOrder.getQuantity(),
                    restingOrder.getQuantity()
                );

                executeTrade(
                    incomingOrder,
                    restingOrder,
                    tradeQuantity,
                    restingOrder.getPrice()
                );

                if (restingOrder.getQuantity() == 0) {
                    uint64_t filledId = restingOrder.getOrderId();
                    levelIt->second.pop_front();
                    orderIndex.erase(filledId);

                    if (levelIt->second.empty()) {
                        buyOrders.erase(levelIt);
                    }
                }
            }
        }

        if (incomingOrder.getQuantity() > 0) {
            incomingOrder.setStatus(
                incomingOrder.getFilledQuantity() > 0
                    ? OrderStatus::EXPIRED
                    : OrderStatus::EXPIRED
            );

            addEvent(
                EventType::EXPIRED,
                incomingOrder.getOrderId(),
                0,
                incomingOrder.getTimestamp(),
                "MARKET order expired with unfilled quantity"
            );
        }
    }

public:
    OrderBook(
        const string& symbol,
        uint64_t* globalSequence,
        vector<EngineEvent>* events
    )
        : symbol(symbol),
          nextLocalTradeId(1),
          globalSequence(globalSequence),
          events(events) {}

    const string& getSymbol() const {
        return symbol;
    }

    void submitOrder(const Order& order) {
        if (!validateOrder(order)) {
            addEvent(
                EventType::REJECTED,
                order.getOrderId(),
                0,
                order.getTimestamp(),
                "Order validation failed"
            );
            return;
        }

        if (order.getOrderType() == OrderType::MARKET) {
            matchMarketOrder(order);
        } else {
            matchLimitOrder(order);
        }
    }

    void addOrder(const Order& order) {
        if (order.getSide() == Side::BUY) {
            buyOrders[order.getPrice()].push_back(order);
        } else {
            sellOrders[order.getPrice()].push_back(order);
        }

        orderIndex[order.getOrderId()] =
            make_pair(order.getSide(), order.getPrice());
    }

    bool cancelOrder(uint64_t orderId) {

        auto indexIt = orderIndex.find(orderId);

        if (indexIt == orderIndex.end()) {
            return false;
        }

        Side side = indexIt->second.first;
        int64_t price = indexIt->second.second;

        if (side == Side::BUY) {

            auto priceIt = buyOrders.find(price);
            if (priceIt == buyOrders.end()) return false;

            deque<Order>& orders = priceIt->second;

            for (auto it = orders.begin(); it != orders.end(); ++it) {
                if (it->getOrderId() == orderId) {
                    it->setStatus(OrderStatus::CANCELLED);
                    orders.erase(it);

                    if (orders.empty()) {
                        buyOrders.erase(priceIt);
                    }

                    orderIndex.erase(indexIt);

                    addEvent(
                        EventType::CANCELLED,
                        orderId,
                        0,
                        nextSequence(),
                        "Order cancelled"
                    );

                    return true;
                }
            }

        } else {

            auto priceIt = sellOrders.find(price);
            if (priceIt == sellOrders.end()) return false;

            deque<Order>& orders = priceIt->second;

            for (auto it = orders.begin(); it != orders.end(); ++it) {
                if (it->getOrderId() == orderId) {
                    it->setStatus(OrderStatus::CANCELLED);
                    orders.erase(it);

                    if (orders.empty()) {
                        sellOrders.erase(priceIt);
                    }

                    orderIndex.erase(indexIt);

                    addEvent(
                        EventType::CANCELLED,
                        orderId,
                        0,
                        nextSequence(),
                        "Order cancelled"
                    );

                    return true;
                }
            }
        }

        return false;
    }

    bool modifyOrder(
        uint64_t orderId,
        int64_t newPrice,
        uint64_t newQuantity,
        uint64_t newTimestamp
    ) {
        if (newPrice <= 0 || newQuantity == 0) {
            return false;
        }

        auto indexIt = orderIndex.find(orderId);

        if (indexIt == orderIndex.end()) {
            return false;
        }

        Side side = indexIt->second.first;
        int64_t oldPrice = indexIt->second.second;

        Order oldOrder(
            0, 0, "", Side::BUY, OrderType::LIMIT, 0, 0, 0
        );

        bool found = false;

        if (side == Side::BUY) {
            auto priceIt = buyOrders.find(oldPrice);

            if (priceIt != buyOrders.end()) {
                for (const auto& order : priceIt->second) {
                    if (order.getOrderId() == orderId) {
                        oldOrder = order;
                        found = true;
                        break;
                    }
                }
            }
        } else {
            auto priceIt = sellOrders.find(oldPrice);

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

        if (!found) return false;

        if (!cancelOrder(orderId)) {
            return false;
        }

        Order modified(
            oldOrder.getOrderId(),
            oldOrder.getTraderId(),
            oldOrder.getSymbol(),
            oldOrder.getSide(),
            oldOrder.getOrderType(),
            newPrice,
            newQuantity,
            newTimestamp
        );

        addEvent(
            EventType::MODIFIED,
            orderId,
            0,
            newTimestamp,
            "Order modified and re-entered with new priority"
        );

        submitOrder(modified);
        return true;
    }

    size_t getBuyOrderCount() const {
        size_t count = 0;
        for (const auto& level : buyOrders) {
            count += level.second.size();
        }
        return count;
    }

    size_t getSellOrderCount() const {
        size_t count = 0;
        for (const auto& level : sellOrders) {
            count += level.second.size();
        }
        return count;
    }

    bool empty() const {
        return buyOrders.empty() && sellOrders.empty();
    }

    bool getBestBid(int64_t& price, uint64_t& quantity) const {
        if (buyOrders.empty()) return false;

        price = buyOrders.begin()->first;
        quantity = 0;

        for (const auto& order : buyOrders.begin()->second) {
            quantity += order.getQuantity();
        }

        return true;
    }

    bool getBestAsk(int64_t& price, uint64_t& quantity) const {
        if (sellOrders.empty()) return false;

        price = sellOrders.begin()->first;
        quantity = 0;

        for (const auto& order : sellOrders.begin()->second) {
            quantity += order.getQuantity();
        }

        return true;
    }

    const vector<Trade>& getTrades() const {
        return trades;
    }

    void printBook() const {
        cout << "\n========== " << symbol << " ORDER BOOK ==========\n";

        cout << "SELL:\n";

        if (sellOrders.empty()) {
            cout << "  EMPTY\n";
        } else {
            for (const auto& level : sellOrders) {
                uint64_t total = 0;

                for (const auto& order : level.second) {
                    total += order.getQuantity();
                }

                cout << "  " << level.first
                     << " | Orders: " << level.second.size()
                     << " | Qty: " << total << "\n";
            }
        }

        cout << "BUY:\n";

        if (buyOrders.empty()) {
            cout << "  EMPTY\n";
        } else {
            for (const auto& level : buyOrders) {
                uint64_t total = 0;

                for (const auto& order : level.second) {
                    total += order.getQuantity();
                }

                cout << "  " << level.first
                     << " | Orders: " << level.second.size()
                     << " | Qty: " << total << "\n";
            }
        }
    }
};

// ============================================================
// EXCHANGE
// ============================================================

class Exchange {
private:
    map<string, unique_ptr<OrderBook>> books;

    unordered_map<uint64_t, string> orderToSymbol;

    vector<EngineEvent> events;

    mutable ExchangeMutex exchangeMutex;

    uint64_t globalSequence;
    uint64_t nextOrderTimestamp;

    OrderBook* getOrCreateBookUnlocked(const string& symbol) {

        auto it = books.find(symbol);

        if (it != books.end()) {
            return it->second.get();
        }

        unique_ptr<OrderBook> newBook(
            new OrderBook(
                symbol,
                &globalSequence,
                &events
            )
        );

        OrderBook* ptr = newBook.get();

        books[symbol] = move(newBook);

        return ptr;
    }

    uint64_t nextTimestampUnlocked() {
        return ++nextOrderTimestamp;
    }

public:
    Exchange()
        : globalSequence(0),
          nextOrderTimestamp(0) {}

    // Exchange-level order submission.
    void submitOrder(const Order& order) {

        ExchangeLockGuard<ExchangeMutex> lock(exchangeMutex);

        if (orderToSymbol.find(order.getOrderId()) != orderToSymbol.end()) {
            return;
        }

        OrderBook* book =
            getOrCreateBookUnlocked(order.getSymbol());

        size_t beforeBuy = book->getBuyOrderCount();
        size_t beforeSell = book->getSellOrderCount();

        book->submitOrder(order);

        size_t afterBuy = book->getBuyOrderCount();
        size_t afterSell = book->getSellOrderCount();

        // Keep an exchange-level mapping only while the ID can
        // potentially remain active. A precise active-ID lookup
        // is maintained by the book itself.
        if (afterBuy > beforeBuy || afterSell > beforeSell) {
            orderToSymbol[order.getOrderId()] = order.getSymbol();
        }
    }

    bool cancelOrder(uint64_t orderId) {

        ExchangeLockGuard<ExchangeMutex> lock(exchangeMutex);

        auto it = orderToSymbol.find(orderId);

        if (it == orderToSymbol.end()) {
            // Search books as a fallback. This also handles IDs
            // inserted before the exchange-level index was created.
            for (auto& entry : books) {
                if (entry.second->cancelOrder(orderId)) {
                    return true;
                }
            }

            return false;
        }

        auto bookIt = books.find(it->second);

        if (bookIt == books.end()) {
            orderToSymbol.erase(it);
            return false;
        }

        bool result = bookIt->second->cancelOrder(orderId);

        if (result) {
            orderToSymbol.erase(it);
        }

        return result;
    }

    bool modifyOrder(
        uint64_t orderId,
        int64_t newPrice,
        uint64_t newQuantity
    ) {
        ExchangeLockGuard<ExchangeMutex> lock(exchangeMutex);

        auto it = orderToSymbol.find(orderId);

        if (it == orderToSymbol.end()) {
            return false;
        }

        auto bookIt = books.find(it->second);

        if (bookIt == books.end()) {
            return false;
        }

        uint64_t timestamp = nextTimestampUnlocked();

        return bookIt->second->modifyOrder(
            orderId,
            newPrice,
            newQuantity,
            timestamp
        );
    }

    OrderBook* getOrderBook(const string& symbol) {

        ExchangeLockGuard<ExchangeMutex> lock(exchangeMutex);

        auto it = books.find(symbol);

        if (it == books.end()) {
            return nullptr;
        }

        return it->second.get();
    }

    const map<string, unique_ptr<OrderBook>>& getBooks() const {
        return books;
    }

    const vector<EngineEvent>& getEvents() const {
        return events;
    }

    void printAllBooks() const {

        ExchangeLockGuard<ExchangeMutex> lock(exchangeMutex);

        cout << "\n\n================ EXCHANGE MARKET DATA ================\n";

        if (books.empty()) {
            cout << "No instruments.\n";
            return;
        }

        for (const auto& entry : books) {
            entry.second->printBook();
        }
    }

    void printTrades() const {

        ExchangeLockGuard<ExchangeMutex> lock(exchangeMutex);

        cout << "\n\n================ TRADE HISTORY ================\n";

        for (const auto& entry : books) {

            const vector<Trade>& trades =
                entry.second->getTrades();

            for (const auto& trade : trades) {
                cout << "Seq: " << trade.getSequence()
                     << " | Trade: " << trade.getTradeId()
                     << " | " << trade.getSymbol()
                     << " | BUY " << trade.getBuyOrderId()
                     << " | SELL " << trade.getSellOrderId()
                     << " | Price " << trade.getPrice()
                     << " | Qty " << trade.getQuantity()
                     << "\n";
            }
        }
    }

    void exportTradesCSV(const string& filename) const {

        ExchangeLockGuard<ExchangeMutex> lock(exchangeMutex);

        ofstream file(filename.c_str());

        if (!file) {
            throw runtime_error("Could not open trade CSV file.");
        }

        file << "trade_id,sequence,symbol,buy_order_id,"
             << "sell_order_id,price,quantity,timestamp\n";

        for (const auto& entry : books) {

            const vector<Trade>& trades =
                entry.second->getTrades();

            for (const auto& trade : trades) {

                file << trade.getTradeId() << ","
                     << trade.getSequence() << ","
                     << trade.getSymbol() << ","
                     << trade.getBuyOrderId() << ","
                     << trade.getSellOrderId() << ","
                     << trade.getPrice() << ","
                     << trade.getQuantity() << ","
                     << trade.getTimestamp() << "\n";
            }
        }
    }

    uint64_t getGlobalSequence() const {
        ExchangeLockGuard<ExchangeMutex> lock(exchangeMutex);
        return globalSequence;
    }
};

// ============================================================
// ASSERTIONS / TEST HELPERS
// ============================================================

static void require(bool condition, const string& message) {
    if (!condition) {
        throw runtime_error("TEST FAILED: " + message);
    }
}

static Order limitOrder(
    uint64_t id,
    uint64_t trader,
    const string& symbol,
    Side side,
    int64_t price,
    uint64_t quantity,
    uint64_t timestamp
) {
    return Order(
        id,
        trader,
        symbol,
        side,
        OrderType::LIMIT,
        price,
        quantity,
        timestamp
    );
}

static Order marketOrder(
    uint64_t id,
    uint64_t trader,
    const string& symbol,
    Side side,
    uint64_t quantity,
    uint64_t timestamp
) {
    return Order(
        id,
        trader,
        symbol,
        side,
        OrderType::MARKET,
        0,
        quantity,
        timestamp
    );
}

// ============================================================
// TESTS
// ============================================================

static void testLimitMatching() {

    cout << "\n[TEST] Limit matching + partial fill\n";

    Exchange exchange;

    exchange.submitOrder(
        limitOrder(1001, 1, "AAPL", Side::SELL, 18000, 100, 1)
    );

    exchange.submitOrder(
        limitOrder(1002, 2, "AAPL", Side::BUY, 18000, 150, 2)
    );

    OrderBook* book = exchange.getOrderBook("AAPL");

    require(book != nullptr, "AAPL book should exist.");
    require(book->getBuyOrderCount() == 1,
            "The unfilled BUY remainder should rest in the book.");
    require(book->getSellOrderCount() == 0,
            "SELL should be fully filled.");
    require(book->getTrades().size() == 1,
            "Exactly one trade expected.");
    require(book->getTrades()[0].getQuantity() == 100,
            "Trade quantity should be 100.");

    int64_t bidPrice = 0;
    uint64_t bidQuantity = 0;
    require(book->getBestBid(bidPrice, bidQuantity),
            "The remaining BUY should be the best bid.");
    require(bidPrice == 18000 && bidQuantity == 50,
            "The resting BUY quantity should be 50.");

    cout << "PASS\n";
}

static void testPriceTimePriority() {

    cout << "\n[TEST] Price-time priority\n";

    Exchange exchange;

    exchange.submitOrder(
        limitOrder(2001, 1, "MSFT", Side::SELL, 40000, 100, 10)
    );

    exchange.submitOrder(
        limitOrder(2002, 2, "MSFT", Side::SELL, 40000, 100, 11)
    );

    exchange.submitOrder(
        limitOrder(2003, 3, "MSFT", Side::BUY, 40000, 150, 12)
    );

    OrderBook* book = exchange.getOrderBook("MSFT");

    require(book->getTrades().size() == 2,
            "Two FIFO trades expected.");
    require(book->getTrades()[0].getSellOrderId() == 2001,
            "Oldest SELL must execute first.");
    require(book->getTrades()[1].getSellOrderId() == 2002,
            "Second SELL must execute second.");

    cout << "PASS\n";
}

static void testMarketOrders() {

    cout << "\n[TEST] Market orders\n";

    Exchange exchange;

    exchange.submitOrder(
        limitOrder(3001, 1, "TSLA", Side::SELL, 25000, 100, 20)
    );

    exchange.submitOrder(
        limitOrder(3002, 2, "TSLA", Side::SELL, 25100, 100, 21)
    );

    exchange.submitOrder(
        marketOrder(3003, 3, "TSLA", Side::BUY, 150, 22)
    );

    OrderBook* book = exchange.getOrderBook("TSLA");

    require(book->getTrades().size() == 2,
            "Market BUY should consume two levels.");

    require(book->getTrades()[0].getPrice() == 25000,
            "Market BUY must hit cheapest ask.");

    require(book->getTrades()[1].getPrice() == 25100,
            "Market BUY must continue to next ask.");

    require(book->getSellOrderCount() == 1,
            "One SELL order should remain.");

    cout << "PASS\n";
}

static void testMultiInstrumentIsolation() {

    cout << "\n[TEST] Multi-instrument isolation\n";

    Exchange exchange;

    exchange.submitOrder(
        limitOrder(4001, 1, "AAPL", Side::SELL, 18000, 100, 30)
    );

    exchange.submitOrder(
        limitOrder(4002, 2, "MSFT", Side::SELL, 40000, 100, 31)
    );

    exchange.submitOrder(
        limitOrder(4003, 3, "AAPL", Side::BUY, 18000, 50, 32)
    );

    exchange.submitOrder(
        limitOrder(4004, 4, "MSFT", Side::BUY, 40000, 70, 33)
    );

    OrderBook* aapl = exchange.getOrderBook("AAPL");
    OrderBook* msft = exchange.getOrderBook("MSFT");

    require(aapl != nullptr && msft != nullptr,
            "Both instrument books should exist.");

    require(aapl->getTrades().size() == 1,
            "AAPL must have exactly one trade.");

    require(msft->getTrades().size() == 1,
            "MSFT must have exactly one trade.");

    require(aapl->getTrades()[0].getSymbol() == "AAPL",
            "AAPL trade must remain isolated.");

    require(msft->getTrades()[0].getSymbol() == "MSFT",
            "MSFT trade must remain isolated.");

    cout << "PASS\n";
}

static void testCancellation() {

    cout << "\n[TEST] Cancellation\n";

    Exchange exchange;

    exchange.submitOrder(
        limitOrder(5001, 1, "AAPL", Side::BUY, 17900, 100, 40)
    );

    require(exchange.cancelOrder(5001),
            "Existing order should cancel.");

    require(!exchange.cancelOrder(5001),
            "Cancelled order should not cancel twice.");

    OrderBook* book = exchange.getOrderBook("AAPL");

    require(book->getBuyOrderCount() == 0,
            "Cancelled order must leave the book.");

    cout << "PASS\n";
}

static void testModification() {

    cout << "\n[TEST] Modification\n";

    Exchange exchange;

    exchange.submitOrder(
        limitOrder(6001, 1, "AAPL", Side::BUY, 17500, 100, 50)
    );

    exchange.submitOrder(
        limitOrder(6002, 2, "AAPL", Side::SELL, 18000, 100, 51)
    );

    require(
        exchange.modifyOrder(6001, 18000, 150),
        "Modification should succeed."
    );

    OrderBook* book = exchange.getOrderBook("AAPL");

    require(book->getTrades().size() == 1,
            "Modified order should immediately match.");

    require(book->getTrades()[0].getQuantity() == 100,
            "Modified order should consume resting liquidity.");

    cout << "PASS\n";
}

static void testValidation() {

    cout << "\n[TEST] Validation\n";

    Exchange exchange;

    exchange.submitOrder(
        limitOrder(0, 1, "AAPL", Side::BUY, 18000, 100, 60)
    );

    exchange.submitOrder(
        limitOrder(7001, 0, "AAPL", Side::BUY, 18000, 100, 61)
    );

    exchange.submitOrder(
        limitOrder(7002, 1, "", Side::BUY, 18000, 100, 62)
    );

    exchange.submitOrder(
        limitOrder(7003, 1, "AAPL", Side::BUY, 0, 100, 63)
    );

    exchange.submitOrder(
        limitOrder(7004, 1, "AAPL", Side::BUY, 18000, 0, 64)
    );

    OrderBook* book = exchange.getOrderBook("AAPL");

    require(book->getBuyOrderCount() == 0,
            "Invalid orders must not enter the book.");

    cout << "PASS\n";
}

static void testDuplicateActiveId() {

    cout << "\n[TEST] Duplicate active ID\n";

    Exchange exchange;

    exchange.submitOrder(
        limitOrder(8001, 1, "AAPL", Side::BUY, 18000, 100, 70)
    );

    exchange.submitOrder(
        limitOrder(8001, 2, "AAPL", Side::SELL, 18000, 100, 71)
    );

    OrderBook* book = exchange.getOrderBook("AAPL");

    require(book->getBuyOrderCount() == 1,
            "First active ID should remain.");
    require(book->getSellOrderCount() == 0,
            "Duplicate active ID must be rejected.");

    cout << "PASS\n";
}

static void testBestBidAsk() {

    cout << "\n[TEST] Best bid / ask\n";

    Exchange exchange;

    exchange.submitOrder(
        limitOrder(9001, 1, "AAPL", Side::BUY, 17900, 100, 80)
    );

    exchange.submitOrder(
        limitOrder(9002, 2, "AAPL", Side::BUY, 18000, 200, 81)
    );

    exchange.submitOrder(
        limitOrder(9003, 3, "AAPL", Side::SELL, 18100, 50, 82)
    );

    int64_t bidPrice = 0;
    uint64_t bidQty = 0;
    int64_t askPrice = 0;
    uint64_t askQty = 0;

    OrderBook* book = exchange.getOrderBook("AAPL");

    require(book->getBestBid(bidPrice, bidQty),
            "Best bid should exist.");

    require(book->getBestAsk(askPrice, askQty),
            "Best ask should exist.");

    require(bidPrice == 18000,
            "Best bid should be highest BUY price.");

    require(bidQty == 200,
            "Best bid quantity should be 200.");

    require(askPrice == 18100,
            "Best ask should be lowest SELL price.");

    require(askQty == 50,
            "Best ask quantity should be 50.");

    cout << "PASS\n";
}

static void testGlobalSequence() {

    cout << "\n[TEST] Global sequencing\n";

    Exchange exchange;

    exchange.submitOrder(
        limitOrder(10001, 1, "AAPL", Side::SELL, 18000, 100, 90)
    );

    exchange.submitOrder(
        limitOrder(10002, 2, "AAPL", Side::BUY, 18000, 100, 91)
    );

    exchange.submitOrder(
        limitOrder(10003, 3, "MSFT", Side::SELL, 40000, 100, 92)
    );

    exchange.submitOrder(
        limitOrder(10004, 4, "MSFT", Side::BUY, 40000, 100, 93)
    );

    require(exchange.getGlobalSequence() > 0,
            "Global sequence should advance.");

    cout << "PASS\n";
}

static void testConcurrentSubmission() {

    cout << "\n[TEST] Thread-safe concurrent submission\n";

    Exchange exchange;

    const int threadCount = 4;
    const int ordersPerThread = 100;

    vector<ExchangeThread> workers;

    for (int t = 0; t < threadCount; ++t) {

        workers.push_back(
            ExchangeThread(
                [&exchange, t]() {

                    for (int i = 0; i < ordersPerThread; ++i) {

                        uint64_t id =
                            11000 +
                            static_cast<uint64_t>(t) * 1000 +
                            static_cast<uint64_t>(i);

                        exchange.submitOrder(
                            limitOrder(
                                id,
                                static_cast<uint64_t>(t + 1),
                                "GOOG",
                                Side::BUY,
                                10000 + t,
                                1,
                                id
                            )
                        );
                    }
                }
            )
        );
    }

    for (auto& worker : workers) {
        worker.join();
    }

    OrderBook* book = exchange.getOrderBook("GOOG");

    require(book != nullptr,
            "GOOG book should exist after concurrent submissions.");

    require(
        book->getBuyOrderCount() ==
        static_cast<size_t>(threadCount * ordersPerThread),
        "All concurrent orders should be accepted."
    );

    cout << "PASS\n";
}

static void benchmarkSubmission() {

    cout << "\n[BENCHMARK] Sequential order submission\n";

    Exchange exchange;

    const int orderCount = 10000;

    auto start = chrono::high_resolution_clock::now();

    for (int i = 0; i < orderCount; ++i) {

        exchange.submitOrder(
            limitOrder(
                20000 + i,
                1,
                "BENCH",
                Side::BUY,
                10000,
                1,
                static_cast<uint64_t>(i)
            )
        );
    }

    auto end = chrono::high_resolution_clock::now();

    double elapsedMs =
        chrono::duration<double, milli>(end - start).count();

    double ordersPerSecond =
        (elapsedMs > 0.0)
            ? (static_cast<double>(orderCount) / elapsedMs) * 1000.0
            : 0.0;

    cout << fixed << setprecision(2);
    cout << "Orders: " << orderCount << "\n";
    cout << "Elapsed: " << elapsedMs << " ms\n";
    cout << "Throughput: " << ordersPerSecond
         << " orders/sec\n";

    cout << "NOTE: This is a local functional benchmark, "
         << "not a production latency claim.\n";
}

// ============================================================
// DEMO
// ============================================================

static void runDemo() {

    cout << "\n\n============================================================\n";
    cout << "        LOW-LATENCY TRADING & ORDER MATCHING ENGINE\n";
    cout << "============================================================\n";

    Exchange exchange;

    // AAPL
    exchange.submitOrder(
        limitOrder(
            50001, 101, "AAPL", Side::SELL,
            18050, 100, 1001
        )
    );

    exchange.submitOrder(
        limitOrder(
            50002, 102, "AAPL", Side::SELL,
            18100, 100, 1002
        )
    );

    exchange.submitOrder(
        limitOrder(
            50003, 103, "AAPL", Side::BUY,
            18050, 150, 1003
        )
    );

    // MSFT
    exchange.submitOrder(
        limitOrder(
            51001, 201, "MSFT", Side::SELL,
            40000, 200, 1010
        )
    );

    exchange.submitOrder(
        marketOrder(
            51002, 202, "MSFT", Side::BUY,
            75, 1011
        )
    );

    exchange.printAllBooks();
    exchange.printTrades();

    cout << "\nExporting trade history to trades.csv...\n";

    try {
        exchange.exportTradesCSV("trades.csv");
        cout << "CSV export complete.\n";
    } catch (const exception& e) {
        cout << "CSV export failed: " << e.what() << "\n";
    }
}

// ============================================================
// MAIN
// ============================================================

int main() {

    try {

        cout << "\nRunning engine test suite...\n";

        testLimitMatching();
        testPriceTimePriority();
        testMarketOrders();
        testMultiInstrumentIsolation();
        testCancellation();
        testModification();
        testValidation();
        testDuplicateActiveId();
        testBestBidAsk();
        testGlobalSequence();
        testConcurrentSubmission();

        cout << "\n============================================================\n";
        cout << "ALL TESTS PASSED\n";
        cout << "============================================================\n";

        benchmarkSubmission();

        runDemo();

        cout << "\nEngine execution completed successfully.\n";
        return 0;

    } catch (const exception& e) {

        cerr << "\nFATAL ERROR: " << e.what() << "\n";
        return 1;
    }
}
