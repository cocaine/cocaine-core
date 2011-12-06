What the hell is it?
====================

Cocained is a fast and lightweight multi-language event-driven task-based distributed hybrid application server built on top of ZeroMQ transport. Yeah, it __is__ cool.

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

The JSON above is an application manifest, a description of the application you feed into Cocaine for it to be able to host it. In a distributed setup, this manifest will be sent to all the other nodes of the cluster automatically. Apart from this manifest, there is _no other configuration needed to start serving the application_.

This JSON is kinda self-descriptive, I think. There are four parts:

* General stuff. Application __type__, which controls the plugin used to interpret the __args__ (in this example, the Python plugin will load the code from the location specified in the arguments) and the application __version__, used to distinguish different application releases in the cloud.

* Engine policy. __Backend__ type, which can be either _thread_ or _process_, different timeouts, namely __heartbeat__ timeout, which controls the default interval the engine will wait for response chunks from its slaves and __suicide__ timeout, which controls the amount of time a slave will stay idle before termination and limits, namely __pool__ limit, which controls the maximum number of slaves and __queue__ limit, which controls the maximum number of jobs waiting in the queue for processing.

* Publish-Subscribe. This part consists solely of one option, which is the __endpoint__ for engine publications.
 
* Tasks. Tasks are, basically, event sources for the engine. They are described by task __method__, which is some kind of callable name specific to the application plugin; task __type__ which determines the _driver_ used to schedule that callable and driver-specific __arguments__.

Trying it out
=============

In order for this application to come alive, you can either put the JSON manifest to the storage location specified in the command line (by default, it is __/var/lib/cocaine/instance/apps__), or dynamically push it into the server with the following easy steps:

* Connect to the server managment socket (__tcp://*:5000__ by default)

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

* As a result, if the query is well-formed, all the specified apps _will be saved to the storage_ (which can be shared among multiple servers) to recover them on the next start. The engines for your apps will be started and you'll get the initial runtime information and statistics in the response.

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

All these requests can be secured by RSA encryption, just bump the __protocol__ version to 3 (this can be forced in the command line parameters), specify the __username__ field, and send the RSA digital signature after the request. The signatures will be verified against the corresponding user public keys in the server storage.
