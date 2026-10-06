# assets/

Put your own extracted copy of **Virtual-On Oratorio Tangram** (Xbox Live
Arcade, Title ID `58410985`) here. Nothing in this folder except this README is
committed to git.

The folder should look like this when you are done:

```
assets/
  default.xex
  ArcadeInfo.xml
  media/
    2d/ ...
    rom/data.farc
    sound/VO2_ALL.cpk
    sound/VO2_ALL.csb
    shader_hlsl.farc
    ...
```

## Getting the files

The game ships as an STFS (LIVE) package named
`2A944528D84678B7C9F0270A564B8E653520EF72`, found on the console under
`Content\0000000000000000\58410985\000D0000\`.

The easiest way to extract it is Xenia: **File > Install Content**, pick the
package, and Xenia unpacks it to
`<xenia>\content\0000000000000000\58410985\000D0000\2A944528D84678B7C9F0270A564B8E653520EF72\`.
Copy everything inside that folder into `assets/`.

Any other STFS extractor (Velocity, Horizon, Le Fluffie) works too.

Expected `default.xex`: version 0.0.1.3, built 2009-06-04.
