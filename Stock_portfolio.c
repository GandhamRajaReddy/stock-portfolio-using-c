#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define TABLE_SIZE      101
#define MAX_SYMBOL_LEN  16
#define MAX_SECTOR_LEN  20
#define MAX_DATE_LEN    32
#define MAX_TRANSACTIONS 1000  // For transaction history

#define MARKET_FILE "market_data.txt"     // Market data: symbol, sector, current price
#define USER_FILE   "user_portfolio.txt"  // User: holdings
#define TRANSACTION_FILE "transactions.txt"  // Transaction history

typedef enum {
    EMPTY,
    OCCUPIED,
    DELETED
} EntryStatus;

// -------- Market Entry --------
typedef struct {
    char symbol[MAX_SYMBOL_LEN];
    char sector[MAX_SECTOR_LEN];
    double price;
    EntryStatus status;
} MarketEntry;

// -------- User Holding Entry --------
typedef struct {
    char symbol[MAX_SYMBOL_LEN];
    char sector[MAX_SECTOR_LEN];
    int  quantity;
    double avgBuyPrice;
    char lastBuyDate[MAX_DATE_LEN];
    EntryStatus status;
} HoldingEntry;

// -------- Transaction Entry --------
typedef struct {
    char symbol[MAX_SYMBOL_LEN];
    int quantity;
    double pricePerShare;
    char date[MAX_DATE_LEN];
    int type;  // 0 = buy, 1 = sell
} TransactionEntry;

// Global tables
MarketEntry  marketTable[TABLE_SIZE];
HoldingEntry holdingTable[TABLE_SIZE];
TransactionEntry transactionHistory[MAX_TRANSACTIONS];
int transactionCount = 0;

// ---------- Utility Prototypes ----------
void clearInputBuffer();
void toUpperStr(char *s);
int equalsIgnoreCase(const char *a, const char *b);
int startsWithIgnoreCase(const char *text, const char *prefix);
void getCurrentDateTime(char *buffer);

// Hash & common
unsigned int hash(const char *symbol);

// Market functions
void initMarketTable();
int findMarketSlot(const char *symbol, int *found);
int searchMarketStockExact(const char *symbolRaw, double *priceOut, char *sectorOut);
void searchMarketStocksInteractive();
void filterMarketByPriceInteractive();
void filterMarketBySectorInteractive();
void displayAllMarketStocksInteractive();
int insertMarketStockInteractive();  // NEW: Add market stock
int saveMarketToFile(const char *filename);
int loadMarketFromFile(const char *filename);
void showMarketStatistics();

// Holding (user) functions
void initHoldingTable();
int findHoldingSlot(const char *symbol, int *found);
int buyStockInteractive();
int sellStockInteractive();
void displayUserPortfolioInteractive();
int saveHoldingsToFile(const char *filename);
int loadHoldingsFromFile(const char *filename);
void showPortfolioStatistics();

// Transaction functions
void addTransaction(const char *symbol, int quantity, double price, const char *date, int type);
void saveTransactionsToFile(const char *filename);
void loadTransactionsFromFile(const char *filename);
void viewTransactionHistory();

// Menus
void userMenu();

// ================= Utility =================

void clearInputBuffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

void toUpperStr(char *s) {
    while (*s) {
        *s = toupper((unsigned char)*s);
        s++;
    }
}

int equalsIgnoreCase(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

int startsWithIgnoreCase(const char *text, const char *prefix) {
    // Check if prefix is longer than text
    size_t textLen = strlen(text);
    size_t prefixLen = strlen(prefix);
    if (prefixLen > textLen) return 0;
    
    for (size_t i = 0; i < prefixLen; i++) {
        if (tolower((unsigned char)text[i]) != tolower((unsigned char)prefix[i]))
            return 0;
    }
    return 1;
}

void getCurrentDateTime(char *buffer) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, MAX_DATE_LEN, "%Y-%m-%d_%H:%M", t);
}

// ---------- Hash ----------
unsigned int hash(const char *symbol) {
    unsigned long hashValue = 0;
    const unsigned int p = 31;

    for (int i = 0; symbol[i] != '\0'; i++) {
        hashValue = hashValue * p + (unsigned char)symbol[i];
    }
    return (unsigned int)(hashValue % TABLE_SIZE);
}

// ================= MARKET TABLE =================

void initMarketTable() 
{
    for (int i = 0; i < TABLE_SIZE; i++) {
        marketTable[i].status = EMPTY;
        marketTable[i].symbol[0] = '\0';
        marketTable[i].sector[0] = '\0';
        marketTable[i].price = 0.0;
    }
}

int findMarketSlot(const char *symbol, int *found) 
{
    unsigned int index = hash(symbol);
    int firstDeletedIndex = -1;

    if (found) *found = 0;

    for (int i = 0; i < TABLE_SIZE; i++) {
        int current = (index + i) % TABLE_SIZE;

        if (marketTable[current].status == EMPTY) {
            // Return first deleted slot if found, otherwise empty slot
            if (firstDeletedIndex != -1) {
                // Reuse deleted slot
                return firstDeletedIndex;
            }
            return current;
        }

        if (marketTable[current].status == DELETED) {
            if (firstDeletedIndex == -1)
                firstDeletedIndex = current;
            // Continue probing to find existing entry
        } else if (marketTable[current].status == OCCUPIED &&
                   equalsIgnoreCase(marketTable[current].symbol, symbol)) {
            if (found) *found = 1;
            return current;
        }
    }
    // Table is full, return first deleted slot or -1
    return (firstDeletedIndex != -1) ? firstDeletedIndex : -1;
}

int searchMarketStockExact(const char *symbolRaw, double *priceOut, char *sectorOut) {
    char symbol[MAX_SYMBOL_LEN];
    strncpy(symbol, symbolRaw, MAX_SYMBOL_LEN - 1);
    symbol[MAX_SYMBOL_LEN - 1] = '\0';
    toUpperStr(symbol);

    unsigned int index = hash(symbol);

    for (int i = 0; i < TABLE_SIZE; i++) {
        int current = (index + i) % TABLE_SIZE;

        if (marketTable[current].status == EMPTY)
            return 0;

        if (marketTable[current].status == OCCUPIED &&
            equalsIgnoreCase(marketTable[current].symbol, symbol)) {
            if (priceOut) *priceOut = marketTable[current].price;
            if (sectorOut) strcpy(sectorOut, marketTable[current].sector);
            return 1;
        }
    }
    return 0;
}

//Insert market stock
int insertMarketStockInteractive() {
    char symbol[MAX_SYMBOL_LEN];
    char sector[MAX_SECTOR_LEN];
    double price;
    
    printf("Enter stock symbol: ");
    if (scanf("%15s", symbol) != 1) {
        printf("Invalid input.\n");
        clearInputBuffer();
        return 0;
    }
    clearInputBuffer();
    
    
    toUpperStr(symbol);
    
    // Check if already exists
    double existingPrice;
    char existingSector[MAX_SECTOR_LEN];
    if (searchMarketStockExact(symbol, &existingPrice, existingSector)) {
        printf("Stock %s already exists. Update price and sector? (y/n): ", symbol);
        char choice;
        scanf("%c", &choice);
        clearInputBuffer();
        
        if (choice != 'y' && choice != 'Y') {
            return 0;
        }
    }
    
    printf("Enter sector: ");
    fgets(sector, MAX_SECTOR_LEN, stdin);
    toUpperStr(sector);
    sector[strcspn(sector, "\n")] = '\0';  
    
    printf("Enter current price: ");
    if (scanf("%lf", &price) != 1 || price <= 0) {
        printf("Invalid price.\n");
        clearInputBuffer();
        return 0;
    }
    clearInputBuffer();
    
    // Find slot and insert/update
    int found = 0;
    int slot = findMarketSlot(symbol, &found);
    if (slot == -1) {
        printf("Error: Market table is full.\n");
        return 0;
    }
    
    strcpy(marketTable[slot].symbol, symbol);
    strcpy(marketTable[slot].sector, sector);
    marketTable[slot].price = price;
    marketTable[slot].status = OCCUPIED;
    
    printf("Stock %s %s at price %.2f\n", 
           found ? "updated" : "added", symbol, price);
    saveMarketToFile(MARKET_FILE);       
    return 1;
}

void searchMarketStocksInteractive() {
    int choice;
    char input[MAX_SYMBOL_LEN];
    
    printf("\n----- Search Options -----\n");
    printf("1. Exact symbol match\n");
    printf("2. Prefix search (starts with)\n");
    printf("Enter choice: ");
    
    if (scanf("%d", &choice) != 1) {
        printf("Invalid input.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();
    
    if (choice == 1) {
        double price;
        char sector[MAX_SECTOR_LEN];
        
        printf("Enter exact symbol to search: ");
        if (scanf("%15s", input) != 1) {
            printf("Invalid input.\n");
            clearInputBuffer();
            return;
        }
        clearInputBuffer();
        
        if (searchMarketStockExact(input, &price, sector)) {
            printf("Found: %s | Sector: %s | Price: %.2f\n", input, sector, price);
        } else {
            printf("Stock %s not found in market.\n", input);
        }
        
    } else if (choice == 2) {
        printf("Enter symbol prefix (case-insensitive, no spaces): ");
        if (scanf("%15s", input) != 1) {
            printf("Invalid input.\n");
            clearInputBuffer();
            return;
        }
        clearInputBuffer();

        int foundAny = 0;
        printf("\n--- Stocks starting with \"%s\" ---\n", input);
        for (int i = 0; i < TABLE_SIZE; i++) {
            if (marketTable[i].status == OCCUPIED &&
                startsWithIgnoreCase(marketTable[i].symbol, input)) {
                printf("%-12s | %-10s | Price: %.2f\n",
                       marketTable[i].symbol,
                       marketTable[i].sector,
                       marketTable[i].price);
                foundAny = 1;
            }
        }
        if (!foundAny) {
            printf("No stocks found with prefix: %s\n", input);
        }
        
    } else {
        printf("Invalid choice.\n");
    }
}

void filterMarketByPriceInteractive() {
    double target;
    int choice;

    printf("Enter target price: ");
    if (scanf("%lf", &target) != 1) {
        printf("Invalid price.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();

    printf("1. Price >= target\n");
    printf("2. Price <= target\n");
    printf("Enter choice: ");
    if (scanf("%d", &choice) != 1) {
        printf("Invalid input.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();

    int foundAny = 0;
    printf("\n--- Market stocks by price filter ---\n");
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (marketTable[i].status == OCCUPIED) {
            int cond = 0;
            if (choice == 1) cond = (marketTable[i].price >= target);
            else if (choice == 2) cond = (marketTable[i].price <= target);

            if (cond) {
                printf("%-12s | %-10s | Price: %.2f\n",
                       marketTable[i].symbol,
                       marketTable[i].sector,
                       marketTable[i].price);
                foundAny = 1;
            }
        }
    }
    if (!foundAny) {
        printf("No stocks match the given price filter.\n");
    }
}

void filterMarketBySectorInteractive() {
    char sector[MAX_SECTOR_LEN];
    printf("Enter sector name (e.g., IT, BANKING, AUTO): ");
    if (scanf("%19s", sector) != 1) {
        printf("Invalid input.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();

    int foundAny = 0;
    printf("\n--- Market stocks in sector \"%s\" ---\n", sector);
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (marketTable[i].status == OCCUPIED &&
            equalsIgnoreCase(marketTable[i].sector, sector)) {
            printf("%-12s | %-10s | Price: %.2f\n",
                   marketTable[i].symbol,
                   marketTable[i].sector,
                   marketTable[i].price);
            foundAny = 1;
        }
    }
    if (!foundAny) {
        printf("No stocks found in this sector.\n");
    }
}

typedef struct {
    char symbol[MAX_SYMBOL_LEN];
    char sector[MAX_SECTOR_LEN];
    double price;
} MarketView;

int cmpMarketByPrice(const void *a, const void *b) {
    const MarketView *x = (const MarketView *)a;
    const MarketView *y = (const MarketView *)b;
    if (x->price < y->price) return -1;
    if (x->price > y->price) return  1;
    return strcmp(x->symbol, y->symbol);
}

int cmpMarketBySector(const void *a, const void *b) {
    const MarketView *x = (const MarketView *)a;
    const MarketView *y = (const MarketView *)b;
    int cmp = strcasecmp(x->sector, y->sector);
    if (cmp != 0) return cmp;
    return strcmp(x->symbol, y->symbol);
}

void displayAllMarketStocksInteractive() {
    int count = 0;
    MarketView temp[TABLE_SIZE];

    for (int i = 0; i < TABLE_SIZE; i++) {
        if (marketTable[i].status == OCCUPIED) {
            strcpy(temp[count].symbol, marketTable[i].symbol);
            strcpy(temp[count].sector, marketTable[i].sector);
            temp[count].price = marketTable[i].price;
            count++;
        }
    }

    if (count == 0) {
        printf("No market stocks available.\n");
        return;
    }

    int sortChoice;
    printf("\nSort market stocks by:\n");
    printf("1. No sorting\n");
    printf("2. By price\n");
    printf("3. By sector\n");
    printf("Enter choice: ");
    if (scanf("%d", &sortChoice) != 1) {
        printf("Invalid input.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();

    if (sortChoice == 2) {
        qsort(temp, count, sizeof(MarketView), cmpMarketByPrice);
    } else if (sortChoice == 3) {
        qsort(temp, count, sizeof(MarketView), cmpMarketBySector);
    }

    printf("\n----- Market Stocks -----\n");
    for (int i = 0; i < count; i++) {
        printf("%-12s | %-10s | Price: %.2f\n",
               temp[i].symbol, temp[i].sector, temp[i].price);
    }
    printf("---------------------------------\n");
}

int saveMarketToFile(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Error opening market file for saving");
        return 0;
    }

    for (int i = 0; i < TABLE_SIZE; i++) {
        if (marketTable[i].status == OCCUPIED) {
            fprintf(fp, "%s %s %.10f\n",
                    marketTable[i].symbol,
                    marketTable[i].sector,
                    marketTable[i].price);
        }
    }

    fclose(fp);
    return 1;
}

int loadMarketFromFile(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return 0;
    }

    initMarketTable();

    char symbol[MAX_SYMBOL_LEN];
    char sector[MAX_SECTOR_LEN];
    double price;

    while (fscanf(fp, "%15s %19s %lf", symbol, sector, &price) == 3) {
        // Convert symbol to uppercase
        toUpperStr(symbol);
        toUpperStr(sector);

        
        int found = 0;
        int slot = findMarketSlot(symbol, &found);
        if (slot != -1) {
            strcpy(marketTable[slot].symbol, symbol);
            strcpy(marketTable[slot].sector, sector);
            marketTable[slot].price = price;
            marketTable[slot].status = OCCUPIED;
        }
    }

    fclose(fp);
    return 1;
}

// ================= HOLDINGS TABLE (User) =================

void initHoldingTable() {
    for (int i = 0; i < TABLE_SIZE; i++) {
        holdingTable[i].status = EMPTY;
        holdingTable[i].symbol[0] = '\0';
        holdingTable[i].sector[0] = '\0';
        holdingTable[i].quantity = 0;
        holdingTable[i].avgBuyPrice = 0.0;
        holdingTable[i].lastBuyDate[0] = '\0';
    }
}

int findHoldingSlot(const char *symbol, int *found) {
    unsigned int index = hash(symbol);
    int firstDeletedIndex = -1;

    if (found) *found = 0;

    for (int i = 0; i < TABLE_SIZE; i++) {
        int current = (index + i) % TABLE_SIZE;

        if (holdingTable[current].status == EMPTY) {
            return (firstDeletedIndex != -1) ? firstDeletedIndex : current;
        }

        if (holdingTable[current].status == DELETED) {
            if (firstDeletedIndex == -1)
                firstDeletedIndex = current;
        } else if (holdingTable[current].status == OCCUPIED &&
                   equalsIgnoreCase(holdingTable[current].symbol, symbol)) {
            if (found) *found = 1;
            return current;
        }
    }
    return firstDeletedIndex;
}

// ================= TRANSACTION FUNCTIONS =================

void addTransaction(const char *symbol, int quantity, double price, const char *date, int type) {
    if (transactionCount >= MAX_TRANSACTIONS) {
        // Shift old transactions to make room
        for (int i = 0; i < MAX_TRANSACTIONS - 1; i++) {
            transactionHistory[i] = transactionHistory[i + 1];
        }
        transactionCount = MAX_TRANSACTIONS - 1;
    }
    
    strcpy(transactionHistory[transactionCount].symbol, symbol);
    transactionHistory[transactionCount].quantity = quantity;
    transactionHistory[transactionCount].pricePerShare = price;
    strcpy(transactionHistory[transactionCount].date, date);
    transactionHistory[transactionCount].type = type;
    transactionCount++;
}

void saveTransactionsToFile(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Error opening transaction file for saving");
        return;
    }

    for (int i = 0; i < transactionCount; i++) {
        fprintf(fp, "%s %d %.10f %s %d\n",
                transactionHistory[i].symbol,
                transactionHistory[i].quantity,
                transactionHistory[i].pricePerShare,
                transactionHistory[i].date,
                transactionHistory[i].type);
    }

    fclose(fp);
}

void loadTransactionsFromFile(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return;
    }

    transactionCount = 0;
    char symbol[MAX_SYMBOL_LEN];
    int quantity;
    double price;
    char date[MAX_DATE_LEN];
    int type;

    while (fscanf(fp, "%15s %d %lf %31s %d", 
                  symbol, &quantity, &price, date, &type) == 5) {
        if (transactionCount < MAX_TRANSACTIONS) {
            strcpy(transactionHistory[transactionCount].symbol, symbol);
            transactionHistory[transactionCount].quantity = quantity;
            transactionHistory[transactionCount].pricePerShare = price;
            strcpy(transactionHistory[transactionCount].date, date);
            transactionHistory[transactionCount].type = type;
            transactionCount++;
        }
    }

    fclose(fp);
}

void viewTransactionHistory() {
    printf("\n----- Transaction History -----\n");
    
    if (transactionCount == 0) {
        printf("No transaction history found.\n");
        return;
    }
    
    printf("%-12s | Type  | Qty | Price/Share | Date/Time\n", "Symbol");
    printf("---------------------------------------------------------\n");
    
    for (int i = 0; i < transactionCount; i++) {
        printf("%-12s | %-5s | %3d | %11.2f | %s\n",
               transactionHistory[i].symbol,
               transactionHistory[i].type == 0 ? "BUY" : "SELL",
               transactionHistory[i].quantity,
               transactionHistory[i].pricePerShare,
               transactionHistory[i].date);
    }
}

// ================= BUY/SELL FUNCTIONS =================

int buyStockInteractive() {
    char symbolRaw[MAX_SYMBOL_LEN];
    int qty;
    double buyPrice;
    char dateStr[MAX_DATE_LEN];

    printf("Enter stock symbol to BUY (no spaces): ");
    if (scanf("%15s", symbolRaw) != 1) {
        printf("Invalid input.\n");
        clearInputBuffer();
        return 0;
    }

    double currentPrice;
    char sector[MAX_SECTOR_LEN];
    if (!searchMarketStockExact(symbolRaw, &currentPrice, sector)) {
        printf("Stock not found in MARKET data.\n");
        clearInputBuffer();
        return 0;
    }

    printf("Market price for %s (sector %s) is: %.2f\n",
           symbolRaw, sector, currentPrice);

    printf("Enter quantity to buy: ");
    if (scanf("%d", &qty) != 1 || qty <= 0) {
        printf("Invalid quantity.\n");
        clearInputBuffer();
        return 0;
    }

    printf("Enter buy price (per share) (you can use current %.2f): ", currentPrice);
    if (scanf("%lf", &buyPrice) != 1 || buyPrice <= 0) {
        printf("Invalid price.\n");
        clearInputBuffer();
        return 0;
    }
    clearInputBuffer();

    printf("Enter purchase date/time (no spaces, e.g. 2025-11-27_14:05) or 'now' for current: ");
    if (scanf("%31s", dateStr) != 1) {
        printf("Invalid date/time.\n");
        clearInputBuffer();
        return 0;
    }
    clearInputBuffer();

    if (strcmp(dateStr, "now") == 0) {
        getCurrentDateTime(dateStr);
    }

    char symbol[MAX_SYMBOL_LEN];
    strncpy(symbol, symbolRaw, MAX_SYMBOL_LEN - 1);
    symbol[MAX_SYMBOL_LEN - 1] = '\0';
    toUpperStr(symbol);

    int found = 0;
    int slot = findHoldingSlot(symbol, &found);
    if (slot == -1) {
        printf("Error: Holdings table is full.\n");
        return 0;
    }

    // Add transaction BEFORE modifying holdings
    addTransaction(symbol, qty, buyPrice, dateStr, 0);  // 0 = buy

    if (found) {
        // Update quantity & average price
        int oldQty = holdingTable[slot].quantity;
        double oldAvg = holdingTable[slot].avgBuyPrice;
        int newQty = oldQty + qty;
        double newAvg = ((oldAvg * oldQty) + (buyPrice * qty)) / newQty;

        holdingTable[slot].quantity = newQty;
        holdingTable[slot].avgBuyPrice = newAvg;
        strncpy(holdingTable[slot].lastBuyDate, dateStr, MAX_DATE_LEN - 1);
        holdingTable[slot].lastBuyDate[MAX_DATE_LEN - 1] = '\0';

        printf("Bought more of %s. New quantity: %d, New avg price: %.2f\n",
               symbol, newQty, newAvg);
    } else {
        strcpy(holdingTable[slot].symbol, symbol);
        strcpy(holdingTable[slot].sector, sector);
        holdingTable[slot].quantity = qty;
        holdingTable[slot].avgBuyPrice = buyPrice;
        strncpy(holdingTable[slot].lastBuyDate, dateStr, MAX_DATE_LEN - 1);
        holdingTable[slot].lastBuyDate[MAX_DATE_LEN - 1] = '\0';
        holdingTable[slot].status = OCCUPIED;

        printf("Bought %d of %s at %.2f. Holding created.\n", qty, symbol, buyPrice);
    }
    saveHoldingsToFile(USER_FILE);
    saveTransactionsToFile(TRANSACTION_FILE);


    return 1;
}

int sellStockInteractive() {
    char symbolRaw[MAX_SYMBOL_LEN];
    int qty;

    printf("Enter stock symbol to SELL: ");
    if (scanf("%15s", symbolRaw) != 1) {
        printf("Invalid input.\n");
        clearInputBuffer();
        return 0;
    }
    clearInputBuffer();

    char symbol[MAX_SYMBOL_LEN];
    strncpy(symbol, symbolRaw, MAX_SYMBOL_LEN - 1);
    symbol[MAX_SYMBOL_LEN - 1] = '\0';
    toUpperStr(symbol);

    int found = 0;
    int slot = findHoldingSlot(symbol, &found);
    if (!found || holdingTable[slot].status != OCCUPIED) {
        printf("You do not hold any %s.\n", symbol);
        return 0;
    }

    printf("You currently hold %d shares of %s at avg price %.2f\n",
           holdingTable[slot].quantity,
           holdingTable[slot].symbol,
           holdingTable[slot].avgBuyPrice);

    printf("Enter quantity to sell: ");
    if (scanf("%d", &qty) != 1 || qty <= 0) {
        printf("Invalid quantity.\n");
        clearInputBuffer();
        return 0;
    }
    clearInputBuffer();

    if (qty > holdingTable[slot].quantity) {
        printf("You cannot sell more than you hold.\n");
        return 0;
    }

    double currentPrice;
    char sectorDummy[MAX_SECTOR_LEN];
    if (!searchMarketStockExact(symbol, &currentPrice, sectorDummy)) {
        printf("Current market price not found for %s.\n", symbol);
        return 0;
    }

    // Add sell transaction
    char dateStr[MAX_DATE_LEN];
    getCurrentDateTime(dateStr);
    addTransaction(symbol, qty, currentPrice, dateStr, 1);  // 1 = sell

    double avg = holdingTable[slot].avgBuyPrice;
    double profitPerShare = currentPrice - avg;
    double totalProfit = profitPerShare * qty;

    printf("Current market price: %.2f\n", currentPrice);
    if (totalProfit > 0)
        printf("If you sell %d now: PROFIT = %.2f\n", qty, totalProfit);
    else if (totalProfit < 0)
        printf("If you sell %d now: LOSS = %.2f\n", qty, -totalProfit);
    else
        printf("If you sell %d now: NO PROFIT / NO LOSS (break-even)\n", qty);

    // Update holdings
    holdingTable[slot].quantity -= qty;
    if (holdingTable[slot].quantity == 0) {
        holdingTable[slot].status = DELETED;
        printf("You sold all holdings of %s.\n", symbol);
    } else {
        printf("Remaining quantity of %s: %d\n",
               symbol, holdingTable[slot].quantity);
    }
    saveHoldingsToFile(USER_FILE);
    saveTransactionsToFile(TRANSACTION_FILE);


    return 1;
}

typedef struct {
    char symbol[MAX_SYMBOL_LEN];
    char sector[MAX_SECTOR_LEN];
    int quantity;
    double avgBuyPrice;
    char lastBuyDate[MAX_DATE_LEN];
    double currentPrice;
    double profitPerShare;
    double totalProfit;
} HoldingView;

int cmpHoldByPrice(const void *a, const void *b) {
    const HoldingView *x = (const HoldingView *)a;
    const HoldingView *y = (const HoldingView *)b;
    if (x->currentPrice < y->currentPrice) return -1;
    if (x->currentPrice > y->currentPrice) return  1;
    return strcmp(x->symbol, y->symbol);
}

int cmpHoldBySector(const void *a, const void *b) {
    const HoldingView *x = (const HoldingView *)a;
    const HoldingView *y = (const HoldingView *)b;
    int cmp = strcasecmp(x->sector, y->sector);
    if (cmp != 0) return cmp;
    return strcmp(x->symbol, y->symbol);
}

int cmpHoldByProfit(const void *a, const void *b) {
    const HoldingView *x = (const HoldingView *)a;
    const HoldingView *y = (const HoldingView *)b;
    if (x->totalProfit < y->totalProfit) return -1;
    if (x->totalProfit > y->totalProfit) return  1;
    return strcmp(x->symbol, y->symbol);
}

void displayUserPortfolioInteractive() {
    int count = 0;
    HoldingView temp[TABLE_SIZE];
    double totalInvestment = 0, totalCurrentValue = 0, totalProfit = 0;

    // Gather holdings with current prices and profit calculations
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (holdingTable[i].status == OCCUPIED) {
            strcpy(temp[count].symbol, holdingTable[i].symbol);
            strcpy(temp[count].sector, holdingTable[i].sector);
            temp[count].quantity = holdingTable[i].quantity;
            temp[count].avgBuyPrice = holdingTable[i].avgBuyPrice;
            strcpy(temp[count].lastBuyDate, holdingTable[i].lastBuyDate);

            // Get current market price
            double currentPrice;
            char sectorDummy[MAX_SECTOR_LEN];
            if (searchMarketStockExact(holdingTable[i].symbol, &currentPrice, sectorDummy)) {
                temp[count].currentPrice = currentPrice;
                temp[count].profitPerShare = currentPrice - holdingTable[i].avgBuyPrice;
                temp[count].totalProfit = temp[count].profitPerShare * holdingTable[i].quantity;
            } else {
                temp[count].currentPrice = 0;
                temp[count].profitPerShare = 0;
                temp[count].totalProfit = 0;
            }

            totalInvestment += holdingTable[i].avgBuyPrice * holdingTable[i].quantity;
            totalCurrentValue += temp[count].currentPrice * holdingTable[i].quantity;
            totalProfit += temp[count].totalProfit;

            count++;
        }
    }

    if (count == 0) {
        printf("No holdings in your portfolio.\n");
        return;
    }

    int sortChoice;
    printf("\nSort portfolio by:\n");
    printf("1. No sorting\n");
    printf("2. By current price\n");
    printf("3. By sector\n");
    printf("4. By total profit\n");
    printf("Enter choice: ");
    if (scanf("%d", &sortChoice) != 1) {
        printf("Invalid input.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();

    switch (sortChoice) {
        case 2: qsort(temp, count, sizeof(HoldingView), cmpHoldByPrice); break;
        case 3: qsort(temp, count, sizeof(HoldingView), cmpHoldBySector); break;
        case 4: qsort(temp, count, sizeof(HoldingView), cmpHoldByProfit); break;
    }

    printf("\n----- Your Portfolio -----\n");
    printf("%-12s | %-10s | Qty | AvgBuy | CurPrice | Profit/Sh | TotalProfit\n", 
           "Symbol", "Sector");
    printf("------------------------------------------------------------------------\n");
    
    for (int i = 0; i < count; i++) {
        printf("%-12s | %-10s | %3d | %6.2f | %8.2f | %9.2f | %11.2f\n",
               temp[i].symbol,
               temp[i].sector,
               temp[i].quantity,
               temp[i].avgBuyPrice,
               temp[i].currentPrice,
               temp[i].profitPerShare,
               temp[i].totalProfit);
    }
    
    printf("------------------------------------------------------------------------\n");
    printf("TOTALS: Investment: %.2f | Current Value: %.2f | Net Profit/Loss: %.2f\n",
           totalInvestment, totalCurrentValue, totalProfit);
}

int saveHoldingsToFile(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Error opening holdings file for saving");
        return 0;
    }

    for (int i = 0; i < TABLE_SIZE; i++) {
        if (holdingTable[i].status == OCCUPIED) {
            fprintf(fp, "%s %s %d %.10f %s\n",
                    holdingTable[i].symbol,
                    holdingTable[i].sector,
                    holdingTable[i].quantity,
                    holdingTable[i].avgBuyPrice,
                    holdingTable[i].lastBuyDate);
        }
    }

    fclose(fp);
    return 1;
}

int loadHoldingsFromFile(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return 0;
    }

    initHoldingTable();

    char symbol[MAX_SYMBOL_LEN];
    char sector[MAX_SECTOR_LEN];
    int quantity;
    double avgBuyPrice;
    char lastBuyDate[MAX_DATE_LEN];

    while (fscanf(fp, "%15s %19s %d %lf %31s", 
                  symbol, sector, &quantity, &avgBuyPrice, lastBuyDate) == 5) {
        
        char symbolUpper[MAX_SYMBOL_LEN];
        strcpy(symbolUpper, symbol);
        toUpperStr(symbolUpper);

        int found = 0;
        int slot = findHoldingSlot(symbolUpper, &found);
        if (slot != -1) {
            strcpy(holdingTable[slot].symbol, symbolUpper);
            strcpy(holdingTable[slot].sector, sector);
            holdingTable[slot].quantity = quantity;
            holdingTable[slot].avgBuyPrice = avgBuyPrice;
            strcpy(holdingTable[slot].lastBuyDate, lastBuyDate);
            holdingTable[slot].status = OCCUPIED;
        }
    }

    fclose(fp);
    return 1;
}

// ================= STATISTICS =================

void showMarketStatistics() {
    int count = 0;
    double totalValue = 0, minPrice = 1e9, maxPrice = 0;
    char sectors[TABLE_SIZE][MAX_SECTOR_LEN];
    int sectorCount = 0;

    for (int i = 0; i < TABLE_SIZE; i++) {
        if (marketTable[i].status == OCCUPIED) {
            count++;
            totalValue += marketTable[i].price;
            
            if (marketTable[i].price < minPrice) minPrice = marketTable[i].price;
            if (marketTable[i].price > maxPrice) maxPrice = marketTable[i].price;

            // Count unique sectors
            int found = 0;
            for (int j = 0; j < sectorCount; j++) {
                if (equalsIgnoreCase(sectors[j], marketTable[i].sector)) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                strcpy(sectors[sectorCount], marketTable[i].sector);
                sectorCount++;
            }
        }
    }

    printf("\n----- Market Statistics -----\n");
    printf("Total Stocks: %d\n", count);
    printf("Unique Sectors: %d\n", sectorCount);
    if (count > 0) {
        printf("Average Price: %.2f\n", totalValue / count);
        printf("Price Range: %.2f - %.2f\n", minPrice, maxPrice);
        printf("Sectors: ");
        for (int i = 0; i < sectorCount; i++) {
            printf("%s", sectors[i]);
            if (i < sectorCount - 1) printf(", ");
        }
        printf("\n");
    }
}

void showPortfolioStatistics() {
    int count = 0;
    double totalInvestment = 0, totalCurrentValue = 0;
    double bestProfit = -1e9, worstProfit = 1e9;
    char bestStock[MAX_SYMBOL_LEN] = "", worstStock[MAX_SYMBOL_LEN] = "";

    for (int i = 0; i < TABLE_SIZE; i++) {
        if (holdingTable[i].status == OCCUPIED) {
            count++;
            double investment = holdingTable[i].avgBuyPrice * holdingTable[i].quantity;
            totalInvestment += investment;

            double currentPrice;
            char sectorDummy[MAX_SECTOR_LEN];
            if (searchMarketStockExact(holdingTable[i].symbol, &currentPrice, sectorDummy)) {
                double currentValue = currentPrice * holdingTable[i].quantity;
                double profit = currentValue - investment;
                totalCurrentValue += currentValue;

                if (profit > bestProfit) {
                    bestProfit = profit;
                    strcpy(bestStock, holdingTable[i].symbol);
                }
                if (profit < worstProfit) {
                    worstProfit = profit;
                    strcpy(worstStock, holdingTable[i].symbol);
                }
            }
        }
    }

    printf("\n----- Portfolio Statistics -----\n");
    printf("Total Holdings: %d\n", count);
    printf("Total Investment: %.2f\n", totalInvestment);
    printf("Current Portfolio Value: %.2f\n", totalCurrentValue);
    printf("Net Profit/Loss: %.2f\n", totalCurrentValue - totalInvestment);
    if (totalInvestment > 0) {
        double roi = ((totalCurrentValue - totalInvestment) / totalInvestment) * 100;
        printf("ROI: %.2f%%\n", roi);
    }
    if (count > 0) {
        printf("Best Performing: %s (%.2f)\n", bestStock, bestProfit);
        printf("Worst Performing: %s (%.2f)\n", worstStock, worstProfit);
    }
}

// ================= USER MENU =================

void userMenu() {
    int choice;
    do {
        printf("\n===== STOCK PORTFOLIO MANAGER =====\n");
        printf("1. Buy Stock\n");
        printf("2. Sell Stock\n");
        printf("3. View Portfolio\n");
        printf("4. View Transaction History\n");
        printf("5. Show Portfolio Statistics\n");
        printf("6. Search Stocks (Exact or Prefix)\n");
        printf("7. Filter by Price Range\n");
        printf("8. Filter by Sector\n");
        printf("9. Display All Stocks\n");
        printf("10. Insert/Update Market Stock\n");  // NEW
        printf("11. Show Market Statistics\n");
        printf("0. Exit\n");
        printf("Enter choice: ");
        
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input.\n");
            clearInputBuffer();
            continue;
        }
        clearInputBuffer();

        switch (choice) {
            case 1:
                buyStockInteractive();
                break;
            case 2:
                sellStockInteractive();
                break;
            case 3:
                displayUserPortfolioInteractive();
                break;
            case 4:
                viewTransactionHistory();
                break;
            case 5:
                showPortfolioStatistics();
                break;
            case 6:
                searchMarketStocksInteractive();
                break;
            case 7:
                filterMarketByPriceInteractive();
                break;
            case 8:
                filterMarketBySectorInteractive();
                break;
            case 9:
                displayAllMarketStocksInteractive();
                break;
            case 10:  // NEW
                insertMarketStockInteractive();
                break;
            case 11:
                showMarketStatistics();
                break;
            case 0:
                printf("Saving data and exiting...\n");
                saveMarketToFile(MARKET_FILE);
                saveHoldingsToFile(USER_FILE);
                saveTransactionsToFile(TRANSACTION_FILE);
                printf("Goodbye!\n");
                break;
            default:
                printf("Invalid choice. Try again.\n");
        }
    } while (choice != 0);
}

// ================= MAIN FUNCTION =================

int main() {
    printf("Initializing Stock Portfolio Manager...\n");
    
    // Initialize tables
    initMarketTable();
    initHoldingTable();
    
    // Load existing data
    if (loadMarketFromFile(MARKET_FILE)) {
        printf("Market data loaded successfully.\n");
    } else {
        printf("No existing market data found. Starting fresh.\n");
    }
    
    if (loadHoldingsFromFile(USER_FILE)) {
        printf("Portfolio data loaded successfully.\n");
    } else {
        printf("No existing portfolio data found. Starting fresh.\n");
    }
    
    loadTransactionsFromFile(TRANSACTION_FILE);
    printf("Transaction history loaded.\n");
    
    // Start application
    userMenu();
    
    return 0;
}