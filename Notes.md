Audio Mixer With Delayed Starts
===============================

[Synchronized audio mixing in GStreamer](https://coaxion.net/blog/2013/11/synchronized-audio-mixing-in-gstreamer/)

`gst-launch-1.0 audiomixer name=mix mix. ! audioconvert ! audioresample ! autoaudiosink audiotestsrc num-buffers=400 volume=0.2 ! mix. audiotestsrc num-buffers=300 volume=0.2 freq=880 timestamp-offset=1000000000 ! mix. audiotestsrc num-buffers=100 volume=0.2 freq=660 timestamp-offset=2000000000 ! mix.`

