new MjpegDemux mjd
new JpegFiler jf

# Or go direct and save the CPU cycles.
connect mjd Jpeg_buffer jf

# connect mjd Wav_buffer ap


config mjd input dell3000:5103

config mjd enable 1

