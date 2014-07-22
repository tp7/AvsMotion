AvsMotion
=========

AviSynth plugin for animating clips with AAE motion tracking data.

### Why would you do such a terrible thing?
Previously if you wanted to animate a mask in avisynth using some complex tools like Mocha, you had to create this mask in Aegisub and using [Aegisub-Motion][1] to animate it. Unfortunately creating a vector mask might not always be possible or pleasant. This plugin simplifies the process, allowing you to animate any kind of avisynth clip using the same kind of AAE tracking data.

### How it works
This is actually a "proxy" plugin. It simply generates a bunch of calls to Spline36Resize, AddBorders and Crop internally to shift frames according to the data provided. In other words, you can replace this plugin with some avisynth script generator, but the plugin is a bit more efficient.

### Usage
```
AvsMotion(clip c, string file, int "offset", bool "mirror", int "pad_color")
```

* *file* - path to the file containing requited `Adobe After Effects 6.0 Keyframe Data`.
* *offset* - offset of the motion, in frames. For example if the tracked motion spans from frames 1546 to 1574, you should put value of 1546 here. 0 by default.
* *mirror* - specifies if the plugin should mirror border pixels or fill them with some predefined color. False by default.
* *pad_color* - color to fill the borders with. Used only when `mirror=false`. 0 by default

  [1]: https://github.com/torque/Aegisub-Motion
