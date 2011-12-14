#!/usr/bin/env python
# encoding: utf-8

from distutils.core import setup

setup(
    name = "cocaine",
    version = "0.6",
    description = "Cocaine Python Framework",
    long_description = "A simple framework to ease the developing of Cocaine apps",
    url = "https://github.com/kobolog/cocaine",
    author = "Andrey Sibiryov",
    author_email = "me@kobology.ru",
    license = "BSD 2-Clause",
    platforms = ["Linux", "BSD", "MacOS"],
    packages = ["cocaine", "cocaine.decorators"],
    requires = ["msgpack", "json"]
)
