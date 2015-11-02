# RTDLSimulator

The Simulation environment is adapted from VisibleSim (https://github.com/claytronics/visiblesim)

The compiler is adapted from cl-meld (https://github.com/flavioc/cl-meld)

## New features

- Action supported.
- Initial position facts supported.
- Routing message between robots supported.

## Installation

Dependencies:
 - Boost C++ Libraries >= 1.47.0
 - GLUT
 - GLEW
 - SCBL

### Ubuntu Installation

```
sudo apt-get install libboost-all-dev
sudo apt-get install freeglut3-dev
sudo apt-get install libglew-dev
sudo apt-get install sbcl
make
cd applicationsBin/blinkyMeld/
./compile-meld.sh demo.meld 
./blinkyMeld

