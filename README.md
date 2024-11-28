# AppleCore

**Deeply zoom into the Mandelbrot set and automatically create videos from your journey**

This software generates a series of images around a given center point of the Mandelbrot set and saves them to image files, e.g. PNG or JPG. Because it calculates with arbitrary precision floating-point numbers, theoretically any zoom level is possible.

## Build

### macOS, Linux

```bash
git clone https://github.com/607011/AppleCore.git
cd AppleCore
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

## Run

```bash
./mandelbrot config.yaml
```

config.yaml contains the configuration for your journey:

- `width` and `height`: the size of the resulting images.
- `zoom`: the zoom range. In each iteration the zoom is multiplied by `factor`, then `increment` is added to the result.
- `out_file`: template for the names of the images files generated; `%z` will be replaced by a 6-digit sequence number.
- `center`: the real (`r`) and imaginary (`i`) part of the center point of the images
- `palette`: a series of comma-separated RGB values to colorize the generated images; default is a grayscale palette [0–255].

```yaml
width: 3840
height: 2160
zoom:
  from: 0.5
  to: 10000
  factor: 1.1
  increment: 0
out_file: mandelbrot-%z.png
center:
  r: 0.25041256420084111635701059145359920563610567033825287726745220758979353221647098362292311491423201419124855889664791486802053514372495353184361382776108352724063135779957032143020010594904114445020489572866810435489590208032
  i: 0.00001271208042916202422432592307682294230943345688139118046259628555161099396571222351815779936411578902341792926856777472930106023932290150278193914336807532076818902748428279181727568041854049799687884398478342390628980752
palette:
  - 66, 30, 15
  - 25, 7, 26
  - 9, 1, 47
  - 4, 4, 73
  - 0, 7, 100
  - 12, 44, 138
  - 24, 82, 177
  - 57, 125, 209
  - 134, 181, 229
  - 211, 236, 248
  - 241, 233, 191
  - 248, 201, 95
  - 255, 170, 0
  - 204, 128, 0
  - 153, 87, 0
  - 106, 52, 3
```

## Postprocess

When `mandelbrot` has completed the generation of the image files, you can combine them into a video with the aid of FFMpeg, for instance:

```bash
ffmpeg -y -framerate 60 \
  -start_number 0 -i "mandelbrot-%06d.png" \
  -c:v libx264 -pix_fmt yuv420p \
  journey.mp4
```
