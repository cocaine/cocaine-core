What the hell is it?
====================

Your personal application engine.

WAT?
====

Cocaine is a fast and lightweight multi-language (you can easily write your own language binding) event-driven (also, you can easily write your own event drivers) task-based distributed application engine. Yeah, it __is__ cool.

Notable features:

* Apps are defined as a set of tasks, which trigger events in the app engine, which are then processed on a slave pool. Tasks can be servers, time-based jobs, filesystem monitors, etc.
* Dynamic self-managing slave pools for each app with a rich configuration to suit the application needs in the best way.
* Optional resource control via Linux cgroups.
* Optional secure communications using RSA encryption.
* Support for chunked responses and, soon, requests.
* Automatic node discovery and smart peer-to-peer balancing. Note that ZeroMQ already offers built-in fair balancing features which you can use, although they do not consider real node load.
* Simple modular design to add new languages, task types, storages and slave backends easily.

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
    "args": {
        "source": "/path/to/application",
    },
    "engine": {
        "heartbeat-timeout": 60,
        "pool-limit": 20,
        "queue-limit": 5,
        "resource-limits": {
            "cpuset": {
                "cpuset.cpus": "0-3",
                "cpuset.mems": "1",
            }
        }
    },
    "tasks": {
        "aggregate": {
            "type": "recurring-timer",
            "interval": 60000
        },
        "event": {
            "type" : "zeromq-server",
            "endpoint" : "tcp://lo:9100"
        },
        "spool": {
            "type": "filesystem-monitor",
            "path": "/var/spool/my-app-data"
        }
    }
}
```

The JSON above is an application manifest, a description of the application you feed into Cocaine for it to be able to host it. In a distributed setup, this manifest will be sent to all the other nodes of the cluster automatically. Apart from this manifest, there is __no other configuration needed to start serving the application__.

Okay, I want to try it!
=======================

Then it's time to read our [Wiki](https://github.com/cocaine/cocaine-core/wiki) for installation instructions, reference manuals and cookies!
