class loadavg_t: public source_t {
    public:
        loadavg_t(const std::string& uri) {
            // Dance!
        }
    
        virtual dict_t fetch() {
            dict_t dict;

            dict["5"] = "1";
            dict["10"] = "0.663";
            dict["15"] = "0.433";

            return dict;
        }
};
