#ifndef COCAINE_DRIVER_BASE_HPP
#define COCAINE_DRIVER_BASE_HPP

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/count.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/sum.hpp>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"

namespace cocaine { namespace engine { namespace driver {

using namespace boost::accumulators;

enum timing_type {
    in_queue,
    on_slave
};

class driver_t:
    public boost::noncopyable
{
    public:
        driver_t(engine_t* engine, const std::string& method);
        virtual ~driver_t();

        virtual void stop() = 0;
        virtual Json::Value info() const = 0;
        
        void audit(timing_type type, ev::tstamp timing);
       
    public: 
        inline engine_t* engine() { 
            return m_engine; 
        }

        inline std::string method() const { 
            return m_method; 
        }
        
    protected:
        Json::Value stats() const;

    protected:
        engine_t* m_engine;
        const std::string m_method;

    private:
        accumulator_set<
            float, 
            features<
                tag::sum, 
                tag::median
            >
        > m_spent_in_queues;

        accumulator_set<
            float,
            features<
                tag::count,
                tag::sum,
                tag::median
            > 
        > m_spent_on_slaves;
};

}}}

#endif
