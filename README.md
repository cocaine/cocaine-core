What the hell is it?
====================

Cocaine is a fast and lightweight multi-language (you can easily write your own language binding) event-driven (also, you can easily write your own event drivers) task-based distributed application server built on top of ZeroMQ transport and MessagePack serialization library. Yeah, it __is__ cool.

Notable features:

* Apps are defined as a set of tasks, which trigger events in the app engine, which are then processed on a slave pool. Tasks can be servers, time-based jobs, filesystem monitors, etc.
* Dynamic self-managing slave pools (threads or processes) for each app with a rich configuration to suit the application needs in the best way.
* A single maintainance pubsub-based interface for each application for easy access to monitoring and runtime data.
* Optional secure communications using RSA encryption.
* Support for chunked responses and, soon, requests.
* Automatic node discovery and smart peer-to-peer balancing. Note that ZeroMQ already offers built-in fair balancing features which you can use, although they do not consider real node load.
* Simple modular design to add new languages, task types and slave backends easily.

At the moment, Cocaine supports the following languages and specifications:

* C++
* Python
* [In Development] Perl
* [In Development] JavaScript

The application tasks can be driven by any of the following drivers:

* Recurring Timer (multiple jobs can be run if they are not finished in the timer intervals)
* Drifting Timer (only one job can be run, hence the drift)
* [In Development] Cron
* [In Development] Manual Scheduler
* Filesystem Monitor
* ZeroMQ Server (Request-Response)
* [In Development] ZeroMQ Subscriber (Publishing Chain)
* Native Server
* ZeroMQ Sink (Request-Publish)
* Native Sink (Request-Publish)
* [Planned] Raw Socket Server

An example
==========

```python
{
    "type": "python",
    "args": "local:///path/to/application/__init__.py",
    "version": 1,
    "engine": {
        "backend": "process",
        "heartbeat-timeout": 60,
        "pool-limit": 20,
        "queue-limit": 5
    },
    "pubsub": {
        "endpoint": "tcp://lo:9200"
    },
    "tasks": {
        "aggregate": {
            "interval": 60000,
            "type": "recurring-timer"
        },
        "event": {
            "endpoint" : "tcp://lo:9100",
            "type" : "native-server"
        },
        "spool": {
            "path": "/var/spool/my-app-data",
            "type": "filesystem-monitor"
        }
    }
}
```

The JSON above is an application manifest, a description of the application you feed into Cocaine for it to be able to host it. In a distributed setup, this manifest will be sent to all the other nodes of the cluster automatically. Apart from this manifest, there is __no other configuration needed to start serving the application__.

Okay, I want to try it!
=======================

Then it's time to read our [Wiki](https://github.com/kobolog/cocaine/wiki) for installation instructions, reference manuals and cookies!