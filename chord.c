#include <gst/gst.h>
#include <glib.h>
#include <math.h>

static GstElement *low_source, *mid_source, *high_source;

gdouble wave( gdouble speed, gdouble offset ) {
    GstClock *clock = gst_system_clock_obtain ();
    gint64 millseconds = GST_TIME_AS_MSECONDS( gst_clock_get_time( clock ) );
    gdouble value = sin(millseconds * 0.001 * speed + offset );
    gst_object_unref(clock);
    return value;
}

gdouble map_interval( gdouble input, gdouble inputMin, gdouble inputMax, gdouble outputMin, gdouble outputMax, gboolean clamp ) {
    input = ( input - inputMin ) / ( inputMax - inputMin );
    gdouble output = input * ( outputMax - outputMin ) + outputMin;
    if ( clamp ) {
        if ( outputMax < outputMin ) {
            if ( output < outputMax ) {
                output = outputMax;
            }
            else if ( output > outputMin ) {
                output = outputMin;
            }
        } else {
            if ( output > outputMax ) {
                output = outputMax;
            }
            else if ( output < outputMin ) {
                output = outputMin;
            }
        }
    }
    return output;
}

// This is our looping function that changes the
// the levels of the audio
static gboolean
timeout_cb (gpointer user_data)
{

    double low_volume = map_interval( wave( 1.0, 0 ), -1.0f, 1.0f, 0.0, 0.3, TRUE );
    double mid_volume = map_interval( wave( 1.0, 2 ), -1.0f, 1.0f, 0.0, 0.4, TRUE );
    double high_volume = map_interval( wave( 1.0, 4 ), -1.0f, 1.0f, 0.0, 0.4, TRUE );

    g_object_set (G_OBJECT (low_source), "volume", low_volume, NULL);
    g_object_set (G_OBJECT (mid_source), "volume", mid_volume, NULL);
    g_object_set (G_OBJECT (high_source), "volume", high_volume, NULL);
    // g_print ( "Hello %f %f %f \n", low_volume, mid_volume, high_volume );
   
    return TRUE;
}

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
    GMainLoop *loop = (GMainLoop *) data;

    switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
        g_print ("End of stream\n");
        g_main_loop_quit (loop);
        break;

    case GST_MESSAGE_ERROR: {
        gchar  *debug;
        GError *error;

        gst_message_parse_error (msg, &error, &debug);
        g_free (debug);

        g_printerr ("Error: %s\n", error->message);
        g_error_free (error);

        g_main_loop_quit (loop);
        break;
    }
    default:
        break;
    }

    return TRUE;
}


static void
on_pad_added (GstElement *element,
              GstPad     *pad,
              gpointer    data)
{
    GstPad *sinkpad;
    GstElement *decoder = (GstElement *) data;

    /* We can now link this pad with the vorbis-decoder sink pad */
    g_print ("Dynamic pad created, linking demuxer/decoder\n");

    sinkpad = gst_element_get_static_pad (decoder, "sink");

    gst_pad_link (pad, sinkpad);

    gst_object_unref (sinkpad);
}



int
main (int   argc,
      char *argv[])
{
    GMainLoop *loop;
  
    GstBus *bus;
    guint bus_watch_id;

    /* Initialisation */
    gst_init (&argc, &argv);

    loop = g_main_loop_new (NULL, FALSE);

    // `gst-launch-1.0 audiomixer name=mix mix. ! audioconvert ! audioresample ! autoaudiosink audiotestsrc num-buffers=400 volume=0.2 ! mix. audiotestsrc num-buffers=300 volume=0.2 freq=880 timestamp-offset=1000000000 ! mix. audiotestsrc num-buffers=100 volume=0.2 freq=660 timestamp-offset=2000000000 ! mix.`

    /* Piplines */
    GstElement *pipeline;
    pipeline = gst_pipeline_new ("pipline");

    /* Elements */
    low_source  = gst_element_factory_make ("audiotestsrc",  "low-test-source");
    mid_source  = gst_element_factory_make ("audiotestsrc",  "mid-test-source");
    high_source = gst_element_factory_make ("audiotestsrc",  "high-test-source");
    GstElement *mixer, *conv, *resample, *sink;
    mixer    = gst_element_factory_make ("audiomixer",  "mixer");
    conv     = gst_element_factory_make ("audioconvert",  "converter");
    resample = gst_element_factory_make ("audioresample", "resampler");
    sink     = gst_element_factory_make ("autoaudiosink", "sink");

    if (!pipeline || !low_source || !high_source || !mid_source|| !mixer || !conv || !resample || !resample || !sink ) {
        g_printerr ("One element could not be created. Exiting.\n");
        return -1;
    }

    /* Set up the pipeline */

    /* Set the audio test source params*/
    g_object_set (G_OBJECT (low_source), "volume", 0.4, "freq", 220.0, NULL); // timestamp-offset not working on the pi. Perhaps 32?
    g_object_set (G_OBJECT (mid_source), "volume", 0.3, "freq", 440.0, NULL);
    g_object_set (G_OBJECT (high_source), "volume", 0.2, "freq", 600.0, NULL);

    /* we add a message handler THIS NEEDS TO ADD THE OTHER PIPELINES */
    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    /* we add all elements into the pipeline */
    /* file-source | ogg-demuxer | vorbis-decoder | converter | alsa-output */
    gst_bin_add_many (GST_BIN (pipeline), low_source, mid_source, high_source, mixer, conv, resample, sink, NULL);

    /* we link the elements together */
    /* file-source -> ogg-demuxer ~> vorbis-decoder -> converter -> alsa-output */
    gst_element_link (low_source, mixer);
    gst_element_link (mid_source, mixer);
    gst_element_link (high_source, mixer);
    gst_element_link_many (mixer, conv, resample, sink, NULL);
    //g_signal_connect (source, "pad-added", G_CALLBACK (on_pad_added), conv);

    /* note that the demuxer will be linked to the decoder dynamically.
       The reason is that Ogg may contain various streams (for example
       audio and video). The source pad(s) will be created at run time,
       by the demuxer when it detects the amount and nature of streams.
       Therefore we connect a callback function which will be executed
       when the "pad-added" is emitted.*/

    /* Set the pipeline to "playing" state*/
    g_print ("Now playing: %s\n", argv[1]);
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    /* Add our callback */
    g_timeout_add (10, timeout_cb, loop);
  
    /* Iterate */
    g_print ("Running...\n");
    g_main_loop_run (loop);

    /* Out of the main loop, clean up nicely */
    g_print ("Returned, stopping playback\n");
    gst_element_set_state (pipeline, GST_STATE_NULL);

    /* Set the pipeline to "playing" state*/
    g_print ("Now playing: %s\n", argv[1]);
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    g_print ("Deleting pipeline\n");
    gst_object_unref (GST_OBJECT (pipeline));
    g_source_remove (bus_watch_id);
    g_main_loop_unref (loop);

    return 0;
}
