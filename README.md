 
OpenH264 VLC Plugin
===================
OpenH264 VLC Plugin is a plugin module for VLC enabling use of OpenH264 codec (encoder and decoder) in LibVLC and VLC media player.

Loading plug-in module in LibVLC and VLC media player
-----------------------------------------------------
For LibVLC < 2.0.0 you can add `--plugin-path` option to other options with the directory where compiled library is while creating LibVLC instance.

For LibVLC >= 2.0.0 you can set `VLC_PLUGIN_PATH` environment variable to the directory where compiled library is before creating LibVLC instance.

For all LibVLC you can just copy compiled lib in `plugins/` directory where LibVLC plugins are. Note: this will make module accessible system-wide.

Note: plugin file **must** be named as `libopenh264_plugin.[so/dll]`; please refer to [this guide](https://wiki.videolan.org/Documentation:VLC_Modules_Loading/#How_does_the_loading_of_modules_happen)  for more info.


Selecting encoder in LibVLC or VLC media player
-----------------------------------------------
When setting up `transcode` module in LibVLC or VLC media player use:

    #transcode{vcodec=h264,venc=OpenH264{}, ... }
to select OpenH264 encoder

Selecting decoder in LibVLC or VLC media player
-----------------------------------------------
When setting up media options in LibVLC use:
```
libvlc_media_add_option(media,":codec=OpenH264");
```
to select OpenH264 decoder.

You can add `:codec=OpenH264` option in VLC media player while opening file (use Media -> Open Multiple Files ...-> Show more options -> Edit Options section to add options).

License
-------
BSD, see `LICENSE` file for details.