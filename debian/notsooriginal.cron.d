#14 0 * * * www-data [ -x /usr/lib/gallery/thumbnailer ] && GALLERY_TMP=/srv/raii/gallery find /usr/share/pixmaps -type f -exec /usr/lib/gallery/thumbnailer {} \;
