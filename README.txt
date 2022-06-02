IMG TO SOUND

  A basic sequencer tool that converts images to music (as raw audio data).

QUICK START

  You first need an input image to convert. The "example.png" image is provided.
  Run the following in the command line:

      make tool
      ./tool example.png output.bin -x 6 -p 320

  The file "output.bin" now contains raw audio data in the format of 8-bit
  signed PWM at a sample rate of 48KHz.

  For more information, run the following in the command line:

      ./tool -h

IMAGE STRUCTURE

  An input image can represent audio. The X-axis represents time and the Y-axis
  represents pitch.

  An input image can be any size, but to use the full range of the piano
  (88 keys), the image must be at least 88 pixels tall. A shorter image will
  only be able to access the higher notes.

  Any pixels below the 88th row are ignored. The program can optionally ignore
  the first few columns of an image by passing the "-x" option in the command
  line.

  Each non-black pixel that is not ignored is considered to be a music note that
  should be played. If a note is mostly red, it uses a sine wave instrument,
  otherwise, it uses a sawtooth wave instrument. The higher up a pixel is, the
  higher the pitch is.
  
CONTRIBUTING
  
  Please feel free to contribute! This is open source.

LICENSE

  See the file: LICENSE.txt.

