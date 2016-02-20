CTI
===

CTI is a hobby project I've been working on since 2010. During the 2000s, I had written several of one-off programs involving simple video and audio capture and processing, and network services. Applications included: birdfeeder webcam, recording analog TV programs, converting DV tapes, and other such things. Every time I needed a new, slightly different application, I'd copy and rename the previous one and change it accordingly. I would add features to the new programs, and the old ones would [bit rot](https://en.wikipedia.org/wiki/Software_rot). This bothered me, so I came up with the idea of having *one* program that could be configured at runtime to handle any of the applications I might come up with, and if I fixed a bug or added a feature, it would be immediately included and available in all of these applications. This was the impetus for CTI.

The last one-off program I wrote was called `ncjpeg`, but I forget what the `nc` stood for, maybe something to do with `netcat`. From my notes,

	.2010-Jan-27 16:12:19 []

	Maybe, later, name this "CTI", for "C Templates and Instances".  For
	now, I'm going to build everything under "ncjpeg", and sort it out
	later.

	Things I need right now, for getting config values and ranges, are
	strings, lists of strings, maybe automatic cleanup.

The core concept in CTI is that there are a set of static **C** structures, with camel-case names like `SocketServer`, that are used as **T**emplates. Any number of them can be **I**nstantiated, wherein a copy of the template is allocated, and a thread is created which runs in a loop calling the `.tick()` method of the instance, which usually blocks until it has something to do. Most instances have a set of Input and Output members, which can be connected in a many-to-one Output-to-Input graph, of sorts. Instances may thus pass (loosely runtime-typed) messages to other instances, and that is how a CTI "application" works. Also, each instance has a table of configuration parameters that can be set using key/value strings.

So, CTI is *a modular, multi-threaded, message-passing, runtime-configurable program for video and audio capture and processing, networking, and various other applications*. That sounds impressive, but I chose to open this document describing it as a "hobby project", because I don't want to lose track of where it came from, I don't expect it to become a popular project, and I'm not looking to compete with other more developed projects in the same space. CTI exists primarily for my own use and entertainment, and as an exercise in programming.

If you're looking for established projects in the same space, that let you instantiate and plug parts together to do things, here are a few that come to mind,

* [ecasound](http://eca.cx/ecasound/index.php)
* [Processing](https://processing.org/)
* [Scratch](https://scratch.mit.edu/)

And you could probably find some interesting applications built on [Node.js](https://nodejs.org/).

I am of course aware of C++, and I spent a few years as a serious aficiando of the language, but I came back to C and have mostly stuck with it. I won't get into the C vs. C++ debate here, but I do acknowledge that several features in CTI could have been implemented more concisely in C++. My philosophy is "each to his own", whatever tools and language a developer is most familiar and comfortable with, are the best.

### An example

CTI has a number of compiled-in Templates, each implemented in a separate C file (it can also load `.so` modules at runtime). Each Template is registered and added to a set of available templates. To create an Instance and start an associated thread, this function is used,

    Instance * Instantiate(const char *label);

where `label` is the name associated with the Template. While CTI could be used as a library, and applications hard-coded to call `Instantiate()`, my main design goal of CTI was to allow runtime configurability, so I came up with a simple configuration language. Thinking that I would probably come up with something better later on, but not wanting to break previous applications, I implemented it in a file named `ScriptV00.c`, allowing for later versions named `ScriptV01`, `ScriptV02`, etc. But as is often the case, the original worked good enough for my needs, and I haven't added any other versions. Despite the name, it doesn't have looping constructs and it is not turing complete, so I would not call it an actual *scripting* language.

Ok, now an example. `logitech.cmd` is a simple camera viewer application. It assumes a [UVC](https://en.wikipedia.org/wiki/USB_video_device_class) compatible USB camera is available on the computer. Oh, and since I hadn't mentioned it thus far, CTI is pretty Linux-centric, although I have occasionally ported it to other platforms, with mixed success.

    # Make instances for video capture, Jpeg decompression, and display using SDL.
	# Each instance can be referenced by label, namely: vc, dj, sdl.
	new V4L2Capture vc
	new DJpeg dj
	new SDLstuff sdl

    # Connect outputs to inputs using runtime-tested labels. The generic
    # syntax is: config source-instance message-type destination-instance
	connect vc Jpeg_buffer dj
	connect dj RGB3_buffer sdl

    # Configure the video capture instance.
	config vc device UVC
	config vc format MJPG
	config vc size 640x480
	config vc fps 30

    # Connect the SDL keyboard to the sdl instance itself to allow quitting with 'q',
	# and to the video capture instance, which uses 's' for snapshots.
	connect sdl:Keycode_msg sdl:Keycode_msg
	connect sdl:Keycode_msg_2 vc:Keycode_msg

	# Use OVERLAY mode for SDL. Other options are SOFTWARE and GL.
	config sdl mode OVERLAY

    # Some extra video capture parameters.
	config vc Exposure,.Auto.Priority 0
	config vc autoexpose 3

    # Start the video device capturing.
    config vc enable 1

The file can be loaded and run with,

    ./cti logitech.cmd

CTI (via the ScriptV00 module) will present a `cti> ` prompt after the file is loaded, from which further `new`, `config`, and other commands can be entered. `ctrl-d` or `exit` will quit the program.

### Some notes about the code

`String.c` takes care of one the more error-prone areas of C programming, handling strings and lists of strings. My favorite function there is `String_sprintf()`, which does pretty much what you would expect. There is a special value returned by the function `String_value_none()`, which can be used for initializing `String *` variables, returned by functions that failed to produced a result, and used for comparison via the function `String_is_none()`. The advantage over using `NULL` is that it points to a legitimate `String` structure, so code that accidentally accesses an "unset" string or fails to adequately check return values will see `"unset_string_or_empty_result"` instead of segfaulting. I don't pretend to write perfect code, and once in a while that `"unset_..."` string pops up, and it makes it much easier to go back and figure out where I went wrong.


### Using individual modules outside of CTI

Not every program lends itself to the "graph of connected instances" model, and I have several other projects that use various C modules from the CTI project, mostly `String.c`, `File.c`, and `SourceSink.c`. I've tried to minimize the dependencies between modules so they can be used independently without dragging in the entire CTI project.

