What the hell is it? [![Build Status](https://secure.travis-ci.org/cocaine/cocaine-core.png)](http://travis-ci.org/cocaine/cocaine-core)
====================
__Your personal app engine.__ Technically speaking, it's an open-source cloud platform enabling you to build your own PaaS clouds using simple yet effective dynamic components.

Notable features:

* You are not restricted by a language or a framework. Language support is plugin-based, and we already support several common languages, so your needs are probably covered. Of course, if you want to write your apps in Whitespace, you'll need to write the language support plugin first, but it's easier to write the actual Whitespace code, we bet.
* Your apps are driven by events, which is fancy. In order for events to actually appear from somewhere, your app defines a set of event drivers. We got lots of predefined event drivers, so unless you need to handle clients via a PS/2 port, you're good.
* We got dynamic self-managing slave pools for each app with a rich but simple configuration and resource usage control to scale with the app needs. Yeah, it's scales automatically, you don't need to think about it.
* Even more, it scales automatically across your server cluser via automatic node discovery and smart peer-to-peer balancing.
* If your startup idea is about processing terabytes of pirated video, we got data streaming and pipelining for you, enjoy.

At the moment, Cocaine Core supports the following languages and specifications:

* C++
* Python
* Perl
* [In Development] JavaScript

Also, we have the following event drivers built-in:

* Recurring Timer (multiple jobs can be run if they are not finished in the timer intervals)
* Drifting Timer (only one job can be run, hence the drift)
* [In Development] Cron
* [In Development] Manual Scheduler
* Filesystem Monitor
* ZeroMQ Server (Request-Response)
* Native Server (Request-Response + Smart Balancing)
* ZeroMQ Sink (Push-Pull)
* Native Sink (Push-Pull + Smart Balancing)
* [In Development] Raw Socket Server

A motivating example
====================

```python
{
    "type": "python",
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
    "drivers": {
        "ultimate-aggregator": {
            "emit": "aggregte",
            "type": "recurring-timer",
            "interval": 60000
        },
        "event": {
            "emit": "event",
            "type" : "native-server"
        },
        "spool": {
            "emit": "on_spool_changed",
            "type": "filesystem-monitor",
            "path": "/var/spool/my-app-data"
        }
    }
}
```

The JSON above is an app manifest, a description of the application you feed into Cocaine Core for it to be able to host it. In a distributed setup, this manifest will be sent to all the other nodes of the cluster automatically. Apart from this manifest, there is __no other configuration needed to start serving the application__.

Okay, I want to try it!
=======================

Then it's time to read our [Wiki](https://github.com/cocaine/cocaine-core/wiki) for installation instructions, reference manuals and cookies!
