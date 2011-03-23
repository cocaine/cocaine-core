#include <string>
#include <map>

typedef std::map<std::string, std::string> dict_t;

struct source_t {
    virtual dict_t fetch() = 0;
};
