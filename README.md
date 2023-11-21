# mandelbrot-bench

This is a small test app I made to try out boost::compute. It spawns three windows, each rendering a Mandelbrot set. The first window renders on a single CPU core, the seconds uses all your cores, and the third offloads rendering to OpenCL (which generally means your GPU).

There is no warranty implied or included. If this has a bug, I'm sorry, but this was never meant to be a bug-free app. :)

I stole the basic Mandelbrot calculation from my other fractal app, [fracture](https://github.com/LorenDB/fracture).
