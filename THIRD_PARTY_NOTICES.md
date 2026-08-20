# Third-party notices

NovelOscUGens contains no incorporated third-party DSP source, firmware,
wavetable, preset, graphic, or data asset.

It includes and links against the public SuperCollider server-plugin API when
building. SuperCollider is distributed under the GNU General Public License,
version 3 or later:

- Project: https://github.com/supercollider/supercollider
- License: https://github.com/supercollider/supercollider/blob/develop/COPYING

Mathematical techniques named in the documentation are implemented
independently. They do not introduce a source-code dependency. The additional
oscillators were informed by these primary technical references:

- Paris Smaragdis, "Scanned Synthesis and Scanned Dynamic Wavetable
  Synthesis": https://www.mit.edu/~paris/pubs/smaragdis-icmc00.pdf
- Jari Kleimola, Victor Lazzarini, Joseph Timoney, and Vesa Välimäki,
  "Vector Phaseshaping Synthesis":
  https://mural.maynoothuniversity.ie/id/eprint/4096/1/vps_dafx11.pdf
- Csound scanned-synthesis reference:
  https://csound.com/docs/manual/SiggenScanTop.html

SuperCollider's current oscillator, trigger, delay, and buffer implementation
sources were consulted for API conventions and edge semantics. No DSP code was
copied from those files.
