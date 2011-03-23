#include <string>
#include <map>

typedef std::map<std::string, std::string> dict_t;

class source_t {
    public:
        virtual dict_t fetch() = 0;
};

