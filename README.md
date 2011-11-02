What the hell is it?
====================

Cocained is a fast and lightweight multi-language event-driven task-based distributed
hybrid application server built on top of ZeroMQ transport. Yeah, it _is_ cool.

Notable features:

* Apps are defined as a set of tasks, which trigger events in the app engine, which are then processed on a worker pool. Tasks can be servers, time-based jobs, filesystem monitors, etc.
* Dynamic self-managing worker pools (threads or processes) for each app with a rich configuration to suit the application needs in the best way.
* Distributed operations coordination via a MongoDB storage.
* A single maintainance pubsub-based interface for each application for easy access to monitoring and runtime data.
* Secure remote administration using RSA encryption.
* Simple modular design to add new languages, task types and backends easily.
* Support for chunked responses and, soon, requests.

At the moment, Cocaine supports the following languages and specifications:

* C++
* Python
* Python + JSON
* Python + WSGI (almost ready)
* Perl (under active development)
* JavaScript (very limited, as the language itself lacks a standard library)

The applications can be driven by the following drivers:

* Timer
* Cron (under development)
* Filesystem monitor
* ZeroMQ server
* LSD server
* HTTP server (under development)

Also, there are a couple of abstract drivers to build upon:

* Timed job
* Raw socket server
* Raw ZeroMQ server

Application configuration example
=================================

```python
manifest = {
    "type": "python+wsgi",
    "args": "local:///path/to/application/__init__.py",
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
            "type": "auto"
        },
        "event": {
            "endpoint" : "tcp://lo:9100",
            "type" : "server+zmq"
        },
        "spool": {
            "path": "/var/spool/my-app-data",
            "type": "fs"
        }
    }
}
```

The JSON above is an application manifest, a description of the application you feed into Cocaine for it to be able to host it. In a distributed setup, this manifest will be sent to all the other nodes of the cluster automatically. Apart from this manifest, there is no other configuration needed to start serving the application.
