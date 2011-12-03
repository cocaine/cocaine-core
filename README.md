What the hell is it?
====================

Cocained is a fast and lightweight multi-language event-driven task-based distributed hybrid application server built on top of ZeroMQ transport. Yeah, it _is_ cool.

Notable features:

* Apps are defined as a set of tasks, which trigger events in the app engine, which are then processed on a slave pool. Tasks can be servers, time-based jobs, filesystem monitors, etc.
* Dynamic self-managing slave pools (threads or processes) for each app with a rich configuration to suit the application needs in the best way.
* A single maintainance pubsub-based interface for each application for easy access to monitoring and runtime data.
* Secure communications using RSA encryption.
* Simple modular design to add new languages, task types and slave backends easily.
* Support for chunked responses and, soon, requests.
* Automatic node discovery and smart peer-to-peer balancing using the [LSD](https://github.com/tinybit/lsd) library.

At the moment, Cocaine supports the following languages and specifications:

* C++
* Python
* Perl
* JavaScript (limited, as the language itself lacks a standard library)

The application tasks can be driven by any of the following drivers:

* Recurring Timer
* Cron (under development)
* Filesystem Monitor
* ZeroMQ Server
* [LSD](https://github.com/tinybit/lsd) (Native) Server

Application configuration example
=================================

```python
manifest = {
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

The JSON above is an application manifest, a description of the application you feed into Cocaine for it to be able to host it. In a distributed setup, this manifest will be sent to all the other nodes of the cluster automatically. Apart from this manifest, there is no other configuration needed to start serving the application.
