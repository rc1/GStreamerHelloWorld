.PHONY: all
all: bin/hello-world bin/note bin/chord

CFLAGS = -Wall
LDFLAGS =

# GStreamer
# ==========
# List the gstreamer:
#      libraries with `pkg-config --list-all | grep gstreamer`
# Then add them below using pkg-config 
CFLAGS += `pkg-config --cflags gstreamer-1.0`
LDFLAGS += `pkg-config --libs gstreamer-1.0`

bin/hello-world: hello-world.c
	gcc $(CFLAGS) -o bin/hello-world hello-world.c $(LDFLAGS);

bin/note: note.c
	gcc $(CFLAGS) -o bin/note note.c $(LDFLAGS);

bin/chord: chord.c
	gcc $(CFLAGS) -o bin/chord chord.c $(LDFLAGS); 

.PHONY: clean
clean:
	rm bin/*
