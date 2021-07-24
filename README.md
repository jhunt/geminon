Geminon
=======

This is Geminon, a small, lightweight server for the [Gemini
Protocol][gemini], a network communication system that stands
apart from the world-wide web.

I personally plan to use Geminon as part of an internal backplane
for configuration and orchestration, in my own bizarre
experiments.  If you find value in this, [come find me on
Twitter][social] so we can chat.

[gemini]: https://gemini.circumlunar.space/
[social]: https://twitter.com/iamjameshunt

Building it
-----------

This codebase ships with a Makefile:

    make test
    make geminon

will run the test suite, and build the `geminon` server binary,
respectively.
