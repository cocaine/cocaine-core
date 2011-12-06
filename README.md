What the hell is it?
====================

Cocained is a fast and lightweight multi-language event-driven task-based distributed hybrid application server built on top of ZeroMQ transport. Yeah, it _is_ cool.

Notable features:

* Apps are defined as a set of tasks, which trigger events in the app engine, which are then processed on a slave pool. Tasks can be servers, time-based jobs, filesystem monitors, etc.
* Dynamic self-managing slave pools (threads or processes) for each app with a rich configuration to suit the application needs in the best way.
* A single maintainance pubsub-based interface for each application for easy access to monitoring and runtime data.
* Optional secure communications using RSA encryption.
* Support for chunked responses and, soon, requests.
* Automatic node discovery and smart peer-to-peer balancing using the [LSD](https://github.com/tinybit/lsd) library. Note that ZeroMQ already offers built-in fair balancing features which you can use, although they do not consider real node load.
* Simple modular design to add new languages, task types and slave backends easily.

At the moment, Cocaine supports the following languages and specifications:

* C++
* Python
* [In Development] Perl
* [In Development] JavaScript

The application tasks can be driven by any of the following drivers:

* Recurring Timer
* [In Development] Cron
* [In Development] Manual Scheduler
* Filesystem Monitor
* ZeroMQ Server (Request-Response)
* [In Development] ZeroMQ Sink (Request-Publish)
* [In Development] ZeroMQ Subscriber (Publishing Chain)
* Native [LSD](https://github.com/tinybit/lsd) Server
* [Planned] Raw Socket Server

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

Trying it out
=============

In order for this application to come alive, you can either put the JSON manifest to the storage location specified in the command line (by default, it is /var/lib/cocaine/<instance>/apps), or dynamically push it into the server with the following easy steps:

* Connect to the server managment socket (tcp://*:5000 by default)

```python
import zmq

context = zmq.Context()
socket = context.socket(zmq.REQ)
```

* Build the JSON-RPC query

```python
query = {
    "version": 2,
    "action": "create",
    "apps": {
        "my-app": <manifest>
    }
}
```

* Send it out and receive the response

```python
socket.send_json(query)
print socket.recv_json()
```

* As a result, if the query is well-formed, all the specified apps will be saved to the storage (which can be shared among multiple servers) to recover them on the next start. The engines for your apps will be started and you'll get the initial runtime information and statistics in the response.

JSON-RPC Reference
==================

* Dynamically create engines

```python
query = {
    "version": 2,
    "action": "create"
    "apps": { ... }
}
```

* Dynamically destroy engines

```python
query = {
    "version": 2,
    "action": "delete"
    "apps": [ ... ]
}
```

* Dynamically reload engines

```python
query = {
    "version": 2,
    "action": "reload",
    "apps": [ ... ]
}
```

* Fetch the runtime information and statistics

```python
query = {
    "version": 2,
    "action": "info"
}
```

All these requests can be secured by RSA encryption, just bump the protocol version to 3 (this can be forced in the command line parameters), specify the "username" field, and send the RSA digital signature after the request.
