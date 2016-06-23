CTI
===

CTI is a hobby project I've been working on since 2010. During the 2000s, I had written several of one-off programs involving simple video and audio capture and processing, and network services. Applications included a birdfeeder webcam, recording analog TV programs, and converting DV tapes. Every time I needed a new, slightly different application, I'd copy and rename the previous one and change it slightly. I would add features to the new programs, and the old ones would [bit rot](https://en.wikipedia.org/wiki/Software_rot). This bothered me, so I came up with the idea of having *one* program that could be configured at runtime to handle any of the applications I might come up with, and if I fixed a bug or added a feature, it would be immediately included and available in all of these applications. This was the impetus for CTI.

The last one-off program I wrote was called `ncjpeg`, but I forget what the `nc` stood for, maybe something to do with `netcat`. From my notes,

	.2010-Jan-27 16:12:19 []

	Maybe, later, name this "CTI", for "C Templates and Instances".  For
	now, I'm going to build everything under "ncjpeg", and sort it out
	later.

	Things I need right now, for getting config values and ranges, are
	strings, lists of strings, maybe automatic cleanup.

The core concept in CTI is that there are a set of static **C** structures, with camel-case labels like `SocketServer`, that are used as **T**emplates. Any number of them can be **I**nstantiated, wherein a copy of the template is allocated, and a thread is created which runs in a loop calling the `.tick()` method of the instance, which usually blocks until it has something to do. Most instances have a set of Input and Output members, which can be connected in a many-to-one Output-to-Input graph, of sorts. Instances may thus pass (loosely runtime-typed) messages to other instances, and that is how a CTI "application" is built. Also, each instance has a table of configuration parameters that can be set using key/value strings.

So, CTI is *a modular, multi-threaded, message-passing, runtime-configurable program for video and audio capture and processing, networking, and various other applications*. That sounds impressive, but I chose to open this document describing it as a "hobby project", because I don't want to lose track of where it came from, I don't expect it to become a popular project, and I'm not looking to compete with other more developed projects in the same space. CTI exists primarily for my own use and entertainment, and as an exercise in programming.

If you're looking for bigger, well-established projects in the same space, that let you instantiate and plug parts together to do things, here are a few that come to mind,

* [PureData](https://puredata.info/)
* [gstreamer](https://gstreamer.freedesktop.org/)
* [ecasound](http://eca.cx/ecasound/index.php)
* [Processing](https://processing.org/)
* [Scratch](https://scratch.mit.edu/)

I am of course aware of C++, and I spent a few years as a serious aficionado of the language, but I came back to C and have mostly stuck with it. I won't get into the C vs. C++ debate here, but I do acknowledge that several features in CTI could have been implemented more concisely in C++. My philosophy is "each to his own", whatever tools and language a developer is most familiar and comfortable with, are the best.

### An example

CTI has a number of compiled-in Templates, each implemented in a separate C file (it can also load `.so` modules at runtime). Each Template is registered and added to a set of available templates. To create an Instance and start an associated thread, this function is used,

    Instance * Instantiate(const char *label);

where `label` is the name associated with the Template. While CTI could be used as a library, and applications hard-coded to call `Instantiate()`, my main design goal of CTI was to allow runtime configurability, so I came up with a simple configuration and command language. Thinking that I would probably come up with something better later on, but not wanting to break previous applications, I implemented it in a file named `ScriptV00.c`, allowing for later versions named `ScriptV01`, `ScriptV02`, etc. But as is often the case, the original worked good enough for my needs, and I haven't added any other versions. Despite the name, it doesn't have looping constructs and it is not turing complete, so I would not call it an actual *scripting* language.

Ok, now an example. `logitech.cmd` is a simple camera viewer application. It assumes a [UVC](https://en.wikipedia.org/wiki/USB_video_device_class) compatible USB camera is available on the computer. Oh, and since I hadn't mentioned it thus far, CTI is pretty Linux-centric, although I have occasionally ported it to other platforms, with mixed success.

    # Make instances for video capture, Jpeg decompression, and display using SDL.
    # syntax: new template-label instance-label
    new V4L2Capture vc
    new DJpeg dj
    new SDLstuff sdl

    # Connect outputs to inputs using runtime-tested labels.
    # syntax: config source-instance-label message-type-label destination-instance-label
    connect vc Jpeg_buffer dj
    connect dj RGB3_buffer sdl

    # Configure the video capture instance.
    # syntax: instance-label key value
    config vc device UVC
    config vc format MJPG
    config vc size 640x480
    config vc fps 30

    # Connect the SDL keyboard to the sdl instance itself to allow quitting with 'q',
    # and to the video capture instance, which uses 's' for snapshots.
    # Alternative connect syntax:
    #    connect source-label:message-type-label destination-label:message-type-label
    connect sdl:Keycode_msg sdl:Keycode_msg
    connect sdl:Keycode_msg_2 vc:Keycode_msg

    # Use OVERLAY mode for SDL. Other options are SOFTWARE and GL.
    config sdl mode OVERLAY

    # Some extra video capture parameters.
    config vc Exposure,.Auto.Priority 0
    config vc autoexpose 3

    # Start the video device capturing, which will set the whole set of instances running.
    config vc enable 1

The file can be loaded and run with,

    ./cti logitech.cmd

CTI (via the ScriptV00 module) will present a `cti> ` prompt after the file is loaded, from which further `new`, `config`, and other commands can be entered. `ctrl-d` or `exit` will quit the program.

### More examples

The `cmd/` subfolder here contains `logitech.cmd` and many other experimental files. I've had good experiences with Logitech cameras, specifically models 9000 and C310, so there are several `logitech-*.cmd` files. But they should work with most UVC compatible cameras.

### External C modules

There are a few files that I've imported into CTI that I did not write.

 * `serf_get.c` for handling HTTP and HTTPS operations. Writing an
   HTTP client is easy enough, but HTTPS is a big can of worms, so I'm
   using this example source from [Apache Serf](https://serf.apache.org/), 
   with some small modifications (use `return` instead of `exit()`, only
   initialize once). It works great, and is significantly faster than
   calling `system("wget ...")` on Raspberry Pi (85ms versus 300ms).

 * `OggOutput.c` is mostly `encoder_example.c` from the [Xiph Theora](https://www.theora.org/)
    library, with some wrapper code around it. I experimented with this 
    a few years ago, but Ogg Theora video never really caught on, and the
    only decent client support was in Firefox. Side note, if you say
    "Ogg Theora" to someone, even most tech-savvy people, they'll
    probably look at you funny.


### Some notes about the code

Many of the built-in template modules are incomplete, or just empty skeletons. For example `HTTPClient.c` seemed like a good idea one day, but I was lazy and ended up just calling `wget` (and then later importing `serf_get.c`). And `NVidiaCUDA.c`, yeah, never did anything with that.

There are a few C files that aren't part of CTI, which I wrote for testing, and may or may not have compiled in a long time, but I keep them in the project for possible future reference. I moved some of them into the `basement/` subfolder.

Since I use CTI modules in other projects, it has also become convenient place for modules that aren't (yet) built into CTI, but are used in more than one external project. `jsmn_misc.c`, `dbutil.c`, and a few others.

`String.c` handles strings and lists of strings. My favorite function there is `String_sprintf()`, which does pretty much what you would expect. There is a special value returned by the function `String_value_none()`, which can be used for,

 * initializing `String *` variables
 * as a return value from functions that failed to produce a result, and
 * for comparison via the function `String_is_none()`

The advantage over using `NULL` is that it points to an existing fixed `String` structure, so code that mistakenly accesses an "unset" string or fails to adequately check return values will see `"unset_string_or_empty_result"` instead of segfaulting. I don't pretend to write perfect code, and once in a while that `"unset_..."` string pops up, and it makes it much easier to go back and figure out where I went wrong.

This is similar in some ways to [NSULL](https://developer.apple.com/library/mac/documentation/Cocoa/Reference/Foundation/Classes/NSNull_Class/index.html#//apple_ref/occ/cl/NSNull) in Apple's Objective-C libraries. It provides a singleton object that can be assigned, and compared against. In my case, it protects me from segfaults in my code, and in Apple's case, it can be used in instances where `NULL` or `nil` is not allowed.

<del>Since this is C and not C++, there is no `auto_ptr` and no garbage collection. I keep my code close to the left margin (minimal levels of conditionals and loops), and I'm not averse to using `goto` to jump to the end of the function, where you may find `String_clear()` calls for each of the local `String *` variables in said function.</del>

In early 2016, I [read about](https://plus.google.com/+LennartPoetteringTheOneAndOnly/posts/jHPAdLJiw23) a C extension that allows code to define "cleanup" functions for local variables. Its been available in GCC probably for decades, and is also supported in CLANG. For my purposes, I came up with a one-line macro that I put in a header file called `localptr.h`,

    #define localptr(_type, _var) _type * _var __attribute__ ((__cleanup__( _type ## _free )))

and I use it to declare variables like this,

    localptr(String, result) = String_value_none();

which expands to,

    String * result __attribute__ ((__cleanup__( String_free ))) = String_value_none();

where previously I had done,

    String * result = String_value_none();
    /* ... */
    /* Code, possibly including 'goto out;' */
    /* ... */
    out:
    String_free(&result);

For `String *` variables, I simply need to provide this function,

    void String_free(String **s)

I love this. I still keep my code simple and close to the left margin, but I no longer have to use goto's and *one explicit cleanup call per local variable*. I sometimes think about writing a blog post explaing why I choose to use C over C++, I'll write more about this if I get around to it.

Moving on, the `Makefile` isn't very tidy. There is some scaffolding and artifacts left over from different Linux platforms that I've experimented with.

I had an experimental project called "modc" in the late 2000s, which implemented garbage collection by means of reference-counting allocations, keeping track of types, and leveraging the descending property of stack variable addresses (on most platforms) to periodically clean up dynamically allocated objects. It worked great, and I even wrote an sshfs-compatible (but non-encrypted) SFTP server completely from scratch with it, but the other goals of the modc project didn't pan out, so I abandoned it. I might try reviving some of the modc concepts in CTI one day.

### Using individual modules outside of CTI

Not every program lends itself to the "graph of connected instances" model, and I have several other projects that use various C modules from the CTI project, mostly `String.c`, `File.c`, and `SourceSink.c`. I've tried to minimize the dependencies between modules so they can be used independently without dragging in the entire CTI project.

