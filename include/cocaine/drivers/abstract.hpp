#ifndef COCAINE_DRIVERS_ABSTRACT_HPP
#define COCAINE_DRIVERS_ABSTRACT_HPP

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/count.hpp>
#include <boost/accumulators/statistics/sum.hpp>

#include "cocaine/common.hpp"
#include "cocaine/engine.hpp"

using namespace boost::accumulators;

namespace cocaine { namespace engine {

class driver_t:
    public boost::noncopyable
{
    public:
        driver_t(engine_t* engine, const std::string& method);
        virtual ~driver_t();

        void audit(ev::tstamp spent);
        void expire(boost::shared_ptr<job_t> job);
        
        virtual Json::Value info() const = 0;
        virtual void stop() = 0;
        
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
        job_policy m_policy;

    private:
        accumulator_set<
            float, 
            features<
                tag::count,
                tag::sum
            >
        > m_stats;
};

}}

#endif
