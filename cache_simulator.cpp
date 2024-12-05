#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>

//Các hằng số
#define CACHE_LINE_SIZE 64              //Kích thước dòng cache (64 byte).
#define NUM_SETS 16384                 // 16K sets //Số tập hợp (sets) trong cache (16K = 16384).

// Số đường (ways) trong bộ đệm tập hợp liên kết (2 và 4 tương ứng).
#define INSTRUCTION_WAYS 2            // 2-way set associative
#define DATA_WAYS 4                   // 4-way set associative
 

//Các thao tác (Operation)
enum Operation {
    READ_DATA = 0,                    //Đọc dữ liệu từ cache.
    WRITE_DATA = 1,                  //Ghi dữ liệu vào cache.
    INSTRUCTION_FETCH = 2,          //Tải lệnh từ cache.
    EVICT_L2 = 3,                   //(không sử dụng trong đoạn mã này).
    CLEAR_CACHE = 8,                //Xóa bộ nhớ đệm.
    PRINT_STATE = 9                 //Hiển thị trạng thái bộ nhớ đệm.
};


//Cấu trúc dữ liệu CacheLine
struct CacheLine {

    //Đại diện cho một dòng trong cache với các thuộc tính:
    bool valid;                //Dòng có hợp lệ không.
    bool dirty;                //Dòng có bị thay đổi không.
    uint32_t tag;              // Tag của dòng (phần định danh).
    int lru;                   //Giá trị LRU (Least Recently Used) để quản lý thay thế.

    CacheLine() : valid(false), dirty(false), tag(0), lru(0) {}
};

//Lớp Cache
class Cache {

    //Quản lý các tập hợp và thao tác với các dòng trong cache:
    //Thuộc tính chính:

public:
    Cache(int num_sets, int ways)
        : ways(ways), hits(0), misses(0), reads(0), writes(0) {
        lines.resize(num_sets, std::vector<CacheLine>(ways));         // //lines: Ma trận 2D đại diện cho các tập hợp và các dòng trong từng tập hợp.
    }

    void reset() {                                                   //reset(): Khởi tạo lại trạng thái của cache.
        hits = misses = reads = writes = 0;                          //hits, misses, reads, writes: Thống kê các sự kiện cache hit/miss, đọc/ghi.
        for (size_t i = 0; i < lines.size(); ++i) {
            for (size_t j = 0; j < lines[i].size(); ++j) {
                lines[i][j].valid = false;
                lines[i][j].dirty = false;
                lines[i][j].lru = 0;
            }
        }
    }

    void printState() const {                                         //printState(): In trạng thái hiện tại của cache.
        for (size_t i = 0; i < lines.size(); ++i) {
            std::cout << "Set " << i << ": ";
            for (size_t j = 0; j < lines[i].size(); ++j) {
                if (lines[i][j].valid) {
                    std::cout << "[Tag: " << std::hex << lines[i][j].tag
                              << ", LRU: " << lines[i][j].lru
                              << ", Dirty: " << lines[i][j].dirty << "] ";
                }
            }
            std::cout << std::endl;
        }
    }

    int hits, misses, reads, writes;


     //accessCache(address, isWrite, displayL2Messages): Thực hiện truy cập vào cache và xử lý LRU.
     //Nếu hit, cập nhật LRU và tăng đếm hits.
     //Nếu miss, thực hiện thay thế dòng sử dụng chiến lược LRU.

    bool accessCache(uint32_t address, bool isWrite, bool displayL2Messages) {
        int index = (address / CACHE_LINE_SIZE) % NUM_SETS;
        uint32_t tag = address / CACHE_LINE_SIZE;

        // Search for hit
        for (int way = 0; way < ways; ++way) {
            if (lines[index][way].valid && lines[index][way].tag == tag) {
                // Cache hit
                hits++;
                if (isWrite) {
                    writes++;
                    lines[index][way].dirty = true; // Write-back policy
                } else {
                    reads++;
                }
                updateLRU(lines[index], way);
                return true;
            }
        }

        // Cache miss
        misses++;
        if (isWrite) {
            writes++;
        } else {
            reads++;
            if (displayL2Messages) {
                std::cout << "Read from L2 " << std::hex << address << std::endl;
            }
        }

        // Handle replacement with LRU
        int lruWay = getLRUWay(lines[index]);
        if (lines[index][lruWay].valid && lines[index][lruWay].dirty) {
            if (displayL2Messages) {
                std::cout << "Write to L2 " << std::hex
                          << (lines[index][lruWay].tag * CACHE_LINE_SIZE) << std::endl;
            }
        }

        // Replace the LRU line
        lines[index][lruWay].valid = true;
        lines[index][lruWay].dirty = isWrite;
        lines[index][lruWay].tag = tag;
        updateLRU(lines[index], lruWay);

        return false;
    }

private:
    int ways;
    std::vector<std::vector<CacheLine> > lines;

    int getLRUWay(const std::vector<CacheLine>& set) const {                 //getLRUWay(set): Lấy dòng ít được sử dụng nhất trong một tập hợp.
        int maxLRU = -1, lruWay = -1;
        for (int i = 0; i < set.size(); ++i) {
            if (!set[i].valid) return i; // Use invalid line if available
            if (set[i].lru > maxLRU) {
                maxLRU = set[i].lru;
                lruWay = i;
            }
        }
        return lruWay;
    }

    void updateLRU(std::vector<CacheLine>& set, int accessedWay) {             //updateLRU(set, accessedWay): Cập nhật trạng thái LRU.
        for (int i = 0; i < set.size(); ++i) {
            if (i == accessedWay) {
                set[i].lru = 0;
            } else {
                set[i].lru++;
            }
        }
    }
};

void processTrace(Cache& dataCache, Cache& instructionCache, const std::vector<std::pair<int, uint32_t> >& trace, bool verbose) {
    for (size_t i = 0; i < trace.size(); ++i) {
        int operation = trace[i].first;
        uint32_t address = trace[i].second;

        switch (operation) {
            case READ_DATA:
            case WRITE_DATA:
                dataCache.accessCache(address, operation == WRITE_DATA, verbose);
                break;
            case INSTRUCTION_FETCH:
                instructionCache.accessCache(address, false, verbose);
                break;
            case CLEAR_CACHE:
                dataCache.reset();
                instructionCache.reset();
                break;
            case PRINT_STATE:
                dataCache.printState();
                instructionCache.printState();
                break;
            default:
                break;
        }
    }

    std::cout << "Data Cache: Hits = " << dataCache.hits
              << ", Misses = " << dataCache.misses
              << ", Hit Ratio = "
              << static_cast<float>(dataCache.hits) /
                     (dataCache.hits + dataCache.misses)
              << std::endl;

    std::cout << "Instruction Cache: Hits = " << instructionCache.hits
              << ", Misses = " << instructionCache.misses
              << ", Hit Ratio = "
              << static_cast<float>(instructionCache.hits) /
                     (instructionCache.hits + instructionCache.misses)
              << std::endl;
}

int main() {

    //Dữ liệu truy cập (trace):
    //Lưu danh sách các cặp thao tác và địa chỉ để mô phỏng truy cập bộ nhớ.
    //Các thao tác bao gồm: tải lệnh, đọc dữ liệu, ghi dữ liệu và in trạng thái.

    // Define memory trace
    std::vector<std::pair<int, uint32_t> > trace;
    trace.push_back(std::make_pair(INSTRUCTION_FETCH, 0x408ED4));  // Fetch instruction from Instruction Cache
    trace.push_back(std::make_pair(READ_DATA, 0x10019D94));        // Read data from Data Cache
    trace.push_back(std::make_pair(WRITE_DATA, 0x10019D88));       // Write data to Data Cache
    trace.push_back(std::make_pair(INSTRUCTION_FETCH, 0x408ED8));  // Fetch instruction from Instruction Cache
    trace.push_back(std::make_pair(INSTRUCTION_FETCH, 0x408EDC));  // Fetch instruction from Instruction Cache
    trace.push_back(std::make_pair(PRINT_STATE, 0));                // Print Cache state

    bool verbose = true;  // Enable verbose mode


    //Tạo cache dữ liệu (dataCache) và cache lệnh (instructionCache) với các thông số cụ thể.

    Cache dataCache(NUM_SETS, DATA_WAYS);         // L1 Data Cache
    Cache instructionCache(NUM_SETS, INSTRUCTION_WAYS); // L1 Instruction Cache

   

    //Với lệnh đọc/ghi: Truy cập cache dữ liệu.
    //Với lệnh tải: Truy cập cache lệnh.
    //Với lệnh xóa: Đặt lại trạng thái của cả hai cache.
    //Với lệnh in: Hiển thị trạng thái của cache.
    processTrace(dataCache, instructionCache, trace, verbose);         //Xử lý từng thao tác trong trace

    return 0;
}
