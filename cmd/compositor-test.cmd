new JpegSource js
new DJpeg dj
new CJpeg cj
new JpegFiler jf
new Compositor cs

connect js Jpeg_buffer dj
connect dj YUV420P_buffer cs
connect cs YUV420P_buffer cj
connect cj Jpeg_buffer jf

config jf prefix cstest-

config cs size 1280x640
config cs require Test1,Test2,Test3
config cs paste Test1:0,0,640,360:0,640:270
config cs paste Test2:0,0,640,360:920,640:270
config cs paste Test3:40,0,560,360:920,640:180

config js label Test1
config js file Compositor-test01.jpg
config js run 1

config js label Test2
config js run 1

config js file Compositor-test02.jpg
config js label Test3
config js run 1
