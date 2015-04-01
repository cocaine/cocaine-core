#pragma once

#include "cocaine/rpc/dispatch.hpp"

#include "cocaine/idl/node.hpp"

namespace cocaine { namespace service {

///// From client to worker.
//class streaming_service_t:
//    public dispatch<event_traits<app::enqueue>::dispatch_type>
//{
//    const api::stream_ptr_t downstream;

//public:
//    streaming_service_t(const std::string& name, const api::stream_ptr_t& downstream_):
//        dispatch<event_traits<app::enqueue>::dispatch_type>(name),
//        downstream(downstream_)
//    {
//        typedef io::protocol<event_traits<app::enqueue>::dispatch_type>::scope protocol;

//        on<protocol::chunk>(std::bind(&streaming_service_t::write, this, ph::_1));
//        on<protocol::error>(std::bind(&streaming_service_t::error, this, ph::_1, ph::_2));
//        on<protocol::choke>(std::bind(&streaming_service_t::close, this));
//    }

//    // The client has disconnected.
//    virtual
//    void
//    discard(const std::error_code&) const override {}

//private:
//    void
//    write(const std::string& chunk) {
//        downstream->write(chunk.data(), chunk.size());
//    }

//    void
//    error(int code, const std::string& reason) {
//        downstream->error(code, reason);
//        downstream->close();
//    }

//    void
//    close() {
//        downstream->close();
//    }
//};

//class enqueue_slot_t:
//    public io::basic_slot<io::app::enqueue>
//{
//    app_service_t *const parent;

//public:
//    enqueue_slot_t(app_service_t *const parent):
//        parent(parent)
//    { }

//    typedef basic_slot<io::app::enqueue>::dispatch_type dispatch_type;
//    typedef basic_slot<io::app::enqueue>::upstream_type upstream_type;
//    typedef basic_slot<io::app::enqueue>::tuple_type tuple_type;

//    virtual
//    boost::optional<std::shared_ptr<const dispatch_type>>
//    operator()(tuple_type&& args, upstream_type&& upstream) override {
//        return tuple::invoke(
//            std::bind(&app_service_t::enqueue, parent, std::move(upstream), ph::_1, ph::_2),
//            std::move(args)
//        );
//    }
//};

/// From worker to client.
//struct engine_stream_adapter_t:
//    public api::stream_t
//{
//    engine_stream_adapter_t(enqueue_slot_t::upstream_type&& upstream):
//        upstream(std::move(upstream))
//    { }

//    typedef io::protocol<event_traits<app::enqueue>::upstream_type>::scope protocol;

//    virtual
//    void
//    write(const char* chunk, size_t size) {
//        upstream = upstream.send<protocol::chunk>(literal_t { chunk, size });
//    }

//    virtual
//    void
//    error(int code, const std::string& reason) {
//        upstream.send<protocol::error>(code, reason);
//    }

//    virtual
//    void
//    close() {
//        upstream.send<protocol::choke>();
//    }

//private:
//    enqueue_slot_t::upstream_type upstream;
//};

class app_service_t:
    public dispatch<io::app_tag>
{
    app_t *const parent;

private:
    std::shared_ptr<const enqueue_slot_t::dispatch_type>
    enqueue(enqueue_slot_t::upstream_type& upstream, const std::string& event, const std::string& tag) {
        api::stream_ptr_t downstream;

        if(tag.empty()) {
            downstream = parent->enqueue(api::event_t(event), std::make_shared<engine_stream_adapter_t>(upstream));
        } else {
            downstream = parent->enqueue(api::event_t(event), std::make_shared<engine_stream_adapter_t>(upstream), tag);
        }

        return std::make_shared<const streaming_service_t>(name(), downstream);
    }

public:
    app_service_t(const std::string& name, app_t *const parent):
        dispatch<io::app_tag>(name),
        parent(parent)
    {
        on<app::enqueue>(std::make_shared<enqueue_slot_t>(this));
        on<app::info>(std::bind(&app_t::info, parent));
    }
};

}}
