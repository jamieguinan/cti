new JpegSource js
new DJpeg dj
new CJpeg cj
new JpegFiler jf
new Compositor cs

system rm -vf cstest*jpg

connect js Jpeg_buffer dj
connect dj YUV420P_buffer cs
connect cs YUV420P_buffer cj
connect cj Jpeg_buffer jf

config jf prefix cstest-

config cj quality 99

config cs size 1280x720
config cs require Test1
config cs paste Test1:0,0,400,300:0,0:0

config js label Test1
config js file basement/red-arrow-1px-edge.jpg
config js run 1

# Rotate 0
system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:2,2:0
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:4,4:0
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:-2,-2:0
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:-190,-140:0
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:-398,0:0
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:0,-298:0
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:-400,-300:0
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:1278,0:0
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:0,718:0
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:1278,718:0
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:1280,720:0
config js run 1


# Rotate 90
system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:300,0:90
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:2,0:90
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:300,718:90
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:1280,0:90
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:1380,0:90
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:1480,0:90
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:1530,0:90
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:1578,0:90
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:1578,718:90
config js run 1

# Rotate 180
system sleep 0.25

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:2,2:180
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:402,2:180
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:400,300:180
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:402,302:180
config js run 1


# Rotate 270
system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:0,720:270
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:-298,720:270
config js run 1

system sleep 0.25
config cs clear 1
config cs paste Test1:0,0,400,300:200,150:270
config js run 1

system sleep 0.25
exit

